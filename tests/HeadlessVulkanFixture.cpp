#include "HeadlessVulkanFixture.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "internal/Vma.hpp"

namespace FlowUi::test {
namespace {

void requireVk(VkResult result, const char* operation) {
	if (result == VK_SUCCESS) return;
	throw VulkanUnavailable(
		std::string(operation) + " failed with VkResult " + std::to_string(static_cast<int>(result)));
}

int devicePreference(VkPhysicalDeviceType type) noexcept {
	switch (type) {
	case VK_PHYSICAL_DEVICE_TYPE_CPU: return 3;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1;
	default: return 0;
	}
}

struct DeviceCandidate {
	VkPhysicalDevice device = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
	VkPhysicalDeviceProperties properties{};
};

} // namespace

HeadlessVulkanFixture::HeadlessVulkanFixture() {
	try {
		create();
	} catch (...) {
		reset();
		throw;
	}
}

HeadlessVulkanFixture::~HeadlessVulkanFixture() {
	reset();
}

void HeadlessVulkanFixture::create() {
	uint32_t loaderVersion = VK_API_VERSION_1_0;
	if (const auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
			vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))) {
		requireVk(enumerateVersion(&loaderVersion), "vkEnumerateInstanceVersion");
	}
	if (loaderVersion < VK_API_VERSION_1_3) {
		throw VulkanUnavailable("FlowUi storage tests require a Vulkan 1.3 loader");
	}

	VkApplicationInfo applicationInfo{};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "FlowUi storage tests";
	applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	applicationInfo.pEngineName = "FlowUi tests";
	applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &applicationInfo;
	requireVk(vkCreateInstance(&instanceInfo, nullptr, &context_.instance), "vkCreateInstance");

	uint32_t deviceCount = 0;
	requireVk(vkEnumeratePhysicalDevices(context_.instance, &deviceCount, nullptr),
		"vkEnumeratePhysicalDevices(count)");
	if (deviceCount == 0) throw VulkanUnavailable("no Vulkan physical device is available");
	std::vector<VkPhysicalDevice> devices(deviceCount);
	requireVk(vkEnumeratePhysicalDevices(context_.instance, &deviceCount, devices.data()),
		"vkEnumeratePhysicalDevices(list)");

	std::vector<DeviceCandidate> candidates;
	for (const VkPhysicalDevice device : devices) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device, &properties);
		if (properties.apiVersion < VK_API_VERSION_1_3) continue;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		for (uint32_t index = 0; index < queueFamilyCount; ++index) {
			if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
			candidates.push_back(DeviceCandidate{device, index, properties});
			break;
		}
	}
	if (candidates.empty()) {
		throw VulkanUnavailable("no Vulkan 1.3 device with a graphics queue is available");
	}
	const auto selected = std::max_element(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
		return devicePreference(lhs.properties.deviceType) < devicePreference(rhs.properties.deviceType);
	});
	context_.phys = selected->device;
	context_.graphicsQFamily = selected->graphicsQueueFamily;
	context_.presentQFamily = selected->graphicsQueueFamily;
	deviceName_ = selected->properties.deviceName;

	const float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = context_.graphicsQFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	requireVk(vkCreateDevice(context_.phys, &deviceInfo, nullptr, &context_.device), "vkCreateDevice");
	vkGetDeviceQueue(context_.device, context_.graphicsQFamily, 0, &context_.graphicsQ);
	context_.presentQ = context_.graphicsQ;

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.instance = context_.instance;
	allocatorInfo.physicalDevice = context_.phys;
	allocatorInfo.device = context_.device;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	requireVk(vmaCreateAllocator(&allocatorInfo, &context_.allocator), "vmaCreateAllocator");

	VkPhysicalDeviceMemoryProperties memoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(context_.phys, &memoryProperties);
	for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
		const VkMemoryPropertyFlags flags = memoryProperties.memoryTypes[index].propertyFlags;
		if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
			(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
			hasNonCoherentHostVisibleMemory_ = true;
			break;
		}
	}
}

void HeadlessVulkanFixture::reset() noexcept {
	context_.destroy();
	deviceName_.clear();
	hasNonCoherentHostVisibleMemory_ = false;
}

} // namespace FlowUi::test
