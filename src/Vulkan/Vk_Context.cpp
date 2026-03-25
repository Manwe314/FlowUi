#include "Vulkan/Vk_Context.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

namespace {

static void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

static bool hasExtension(const char* name, const std::vector<VkExtensionProperties>& exts) {
	for (const auto& ext : exts) {
		if (std::strcmp(name, ext.extensionName) == 0) {
			return true;
		}
	}
	return false;
}

static bool hasLayer(const char* name, const std::vector<VkLayerProperties>& layers) {
	for (const auto& layer : layers) {
		if (std::strcmp(name, layer.layerName) == 0) {
			return true;
		}
	}
	return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData) {
	(void)severity;
	(void)type;
	(void)userData;
	std::fprintf(stderr, "[Vulkan] %s\n", callbackData->pMessage);
	return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info) {
	info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = debugCallback;
}

static VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* messenger) {
	auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (func) {
		return func(instance, createInfo, allocator, messenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks* allocator) {
	auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (func) {
		func(instance, messenger, allocator);
	}
}

struct QueueFamilyIndices {
	uint32_t graphics = UINT32_MAX;
	uint32_t present = UINT32_MAX;

	bool complete() const {
		return graphics != UINT32_MAX && present != UINT32_MAX;
	}
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilyIndices indices{};

	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

	for (uint32_t i = 0; i < count; ++i) {
		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics = i;
		}

		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport == VK_TRUE) {
			indices.present = i;
		}

		if (indices.complete()) {
			break;
		}
	}

	return indices;
}

static bool deviceHasExtension(VkPhysicalDevice device, const char* name) {
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> exts(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());
	return hasExtension(name, exts);
}

struct SwapchainSupport {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

static SwapchainSupport querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
	SwapchainSupport support{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount > 0) {
		support.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());
	}

	uint32_t presentCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
	if (presentCount > 0) {
		support.presentModes.resize(presentCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, support.presentModes.data());
	}

	return support;
}

} // namespace

void VulkanContext::createInstance(const FlowUi::AppConfig& config, const std::vector<const char*>& requiredExts) {
	if (instance != VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan instance already created.");
	}

	uint32_t instanceVersion = VK_API_VERSION_1_0;
	auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
		vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
	if (enumerateInstanceVersion) {
		vkCheck(enumerateInstanceVersion(&instanceVersion), "Failed to query Vulkan instance version.");
	}
	if (instanceVersion < VK_API_VERSION_1_3) {
		throw std::runtime_error("Vulkan 1.3 is required but not supported by the loader.");
	}

	uint32_t extCount = 0;
	vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr),
		"Failed to enumerate instance extensions.");
	std::vector<VkExtensionProperties> availableExts(extCount);
	vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data()),
		"Failed to enumerate instance extensions.");

	std::vector<const char*> extensions = requiredExts;
	bool enableDebugUtils = config.vk.enableDebugUtils;
	bool enableValidation = config.vk.enableValidation;
	auto appendUnique = [&](const char* ext) {
		if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) {
			extensions.push_back(ext);
		}
	};

	VkInstanceCreateFlags createFlags = 0;

	if (enableDebugUtils) {
		if (hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availableExts)) {
			appendUnique(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		} else {
			enableDebugUtils = false;
			std::fprintf(
				stderr,
				"[FlowUi] VK_EXT_debug_utils requested but not available. Continuing without debug messenger.\n");
		}
	}

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
	if (hasExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, availableExts)) {
		appendUnique(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		createFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
#endif

	for (const char* ext : extensions) {
		if (!hasExtension(ext, availableExts)) {
			throw std::runtime_error(std::string("Missing required instance extension: ") + ext);
		}
	}

	std::vector<const char*> layers;
	if (enableValidation) {
		uint32_t layerCount = 0;
		vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, nullptr),
			"Failed to enumerate instance layers.");
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkCheck(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()),
			"Failed to enumerate instance layers.");
		if (hasLayer("VK_LAYER_KHRONOS_validation", availableLayers)) {
			layers.push_back("VK_LAYER_KHRONOS_validation");
		} else {
			std::fprintf(
				stderr,
				"[FlowUi] VK_LAYER_KHRONOS_validation requested but not available. Continuing without validation layers.\n");
		}
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = config.window.title.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "FlowUi";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.flags = createFlags;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableDebugUtils) {
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}

	vkCheck(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create Vulkan instance.");

	if (enableDebugUtils) {
		vkCheck(CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger),
			"Failed to create Vulkan debug messenger.");
	}
}

void VulkanContext::createSurface(IWindowBackend& window) {
	if (instance == VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan instance must be created before surface.");
	}
	if (surface != VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan surface already created.");
	}
	surface = window.createSurface(instance);
	if (surface == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to create Vulkan surface.");
	}
}

void VulkanContext::pickPhysicalDevice(const FlowUi::AppConfig& config) {
	if (instance == VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan instance must be created before picking a device.");
	}
	if (surface == VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan surface must be created before picking a device.");
	}

	uint32_t deviceCount = 0;
	vkCheck(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr),
		"Failed to enumerate Vulkan physical devices.");
	if (deviceCount == 0) {
		throw std::runtime_error("No Vulkan-capable GPU found.");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkCheck(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()),
		"Failed to enumerate Vulkan physical devices.");

	VkPhysicalDevice selected = VK_NULL_HANDLE;
	QueueFamilyIndices selectedQueues{};
	bool selectedDiscrete = false;

	for (VkPhysicalDevice device : devices) {
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(device, &props);
		if (props.apiVersion < VK_API_VERSION_1_3) {
			continue;
		}

		if (!deviceHasExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
			continue;
		}

		QueueFamilyIndices indices = findQueueFamilies(device, surface);
		if (!indices.complete()) {
			continue;
		}

		SwapchainSupport support = querySwapchainSupport(device, surface);
		if (support.formats.empty() || support.presentModes.empty()) {
			continue;
		}

		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &features13;
		vkGetPhysicalDeviceFeatures2(device, &features2);

		if (!features13.dynamicRendering) {
			continue;
		}

		const bool isDiscrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

		if (config.vk.preferDiscreteGPU) {
			if (isDiscrete && !selectedDiscrete) {
				selected = device;
				selectedQueues = indices;
				selectedDiscrete = true;
			} else if (selected == VK_NULL_HANDLE) {
				selected = device;
				selectedQueues = indices;
			}
		} else if (selected == VK_NULL_HANDLE) {
			selected = device;
			selectedQueues = indices;
			selectedDiscrete = isDiscrete;
		}
	}

	if (selected == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable Vulkan 1.3 GPU.");
	}

	phys = selected;
	graphicsQFamily = selectedQueues.graphics;
	presentQFamily = selectedQueues.present;
}

void VulkanContext::createDevice(const FlowUi::AppConfig& config) {
	(void)config;
	if (phys == VK_NULL_HANDLE) {
		throw std::runtime_error("Physical device must be selected before creating logical device.");
	}
	if (graphicsQFamily == UINT32_MAX || presentQFamily == UINT32_MAX) {
		throw std::runtime_error("Queue family indices not set.");
	}

	std::vector<uint32_t> uniqueFamilies;
	uniqueFamilies.push_back(graphicsQFamily);
	if (presentQFamily != graphicsQFamily) {
		uniqueFamilies.push_back(presentQFamily);
	}

	float priority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	queueInfos.reserve(uniqueFamilies.size());
	for (uint32_t family : uniqueFamilies) {
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = family;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &priority;
		queueInfos.push_back(queueInfo);
	}

	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
	if (deviceHasExtension(phys, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
		deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	}
#endif

	VkPhysicalDeviceVulkan12Features supported12{};
	supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	VkPhysicalDeviceVulkan13Features supported13{};
	supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	supported13.pNext = &supported12;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &supported13;
	vkGetPhysicalDeviceFeatures2(phys, &features2);

	VkPhysicalDeviceVulkan12Features enabled12{};
	enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabled12.descriptorIndexing = supported12.descriptorIndexing ? VK_TRUE : VK_FALSE;
	enabled12.runtimeDescriptorArray = supported12.runtimeDescriptorArray ? VK_TRUE : VK_FALSE;
	enabled12.shaderSampledImageArrayNonUniformIndexing =
		supported12.shaderSampledImageArrayNonUniformIndexing ? VK_TRUE : VK_FALSE;
	enabled12.descriptorBindingPartiallyBound = supported12.descriptorBindingPartiallyBound ? VK_TRUE : VK_FALSE;
	enabled12.descriptorBindingSampledImageUpdateAfterBind =
		supported12.descriptorBindingSampledImageUpdateAfterBind ? VK_TRUE : VK_FALSE;

	if (!enabled12.descriptorIndexing || !enabled12.runtimeDescriptorArray ||
		!enabled12.shaderSampledImageArrayNonUniformIndexing || !enabled12.descriptorBindingPartiallyBound ||
		!enabled12.descriptorBindingSampledImageUpdateAfterBind) {
		throw std::runtime_error("Selected device does not support descriptor indexing features required by UI renderer.");
	}

	VkPhysicalDeviceVulkan13Features enabled13{};
	enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabled13.pNext = &enabled12;
	enabled13.dynamicRendering = supported13.dynamicRendering ? VK_TRUE : VK_FALSE;
	enabled13.synchronization2 = supported13.synchronization2 ? VK_TRUE : VK_FALSE;

	if (!enabled13.dynamicRendering) {
		throw std::runtime_error("Selected device does not support dynamic rendering.");
	}

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	createInfo.pQueueCreateInfos = queueInfos.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.pNext = &enabled13;
	createInfo.pEnabledFeatures = nullptr;

	vkCheck(vkCreateDevice(phys, &createInfo, nullptr, &device), "Failed to create Vulkan device.");

	vkGetDeviceQueue(device, graphicsQFamily, 0, &graphicsQ);
	vkGetDeviceQueue(device, presentQFamily, 0, &presentQ);

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.instance = instance;
	allocatorInfo.physicalDevice = phys;
	allocatorInfo.device = device;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	vkCheck(vmaCreateAllocator(&allocatorInfo, &allocator), "Failed to create VMA allocator.");
}

void VulkanContext::destroy() {
	if (device != VK_NULL_HANDLE) {
		if (allocator) {
			vmaDestroyAllocator(allocator);
			allocator = nullptr;
		}
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (debugMessenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		debugMessenger = VK_NULL_HANDLE;
	}

	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}

	phys = VK_NULL_HANDLE;
	graphicsQFamily = UINT32_MAX;
	presentQFamily = UINT32_MAX;
	graphicsQ = VK_NULL_HANDLE;
	presentQ = VK_NULL_HANDLE;
}
