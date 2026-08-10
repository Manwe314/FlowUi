#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "internal/StorageSystem/PersistentRecord.hpp"
#include "Ui/Vk_UiRenderer.hpp"

namespace {

using FlowUi::detail::storage::AccessMode;
using FlowUi::detail::storage::AllocationTag;
using FlowUi::detail::storage::BlobHandle;
using FlowUi::detail::storage::BufferDesc;
using FlowUi::detail::storage::BufferHandle;
using FlowUi::detail::storage::BufferUsage;
using FlowUi::detail::storage::BufferWriteView;
using FlowUi::detail::storage::BufferWriteMode;
using FlowUi::detail::storage::DescriptorWriteRecord;
using FlowUi::detail::storage::FlowStorageSystem;
using FlowUi::detail::storage::FrameStorageDesc;
using FlowUi::detail::storage::IStorageSystem;
using FlowUi::detail::storage::ImageDesc;
using FlowUi::detail::storage::ImageHandle;
using FlowUi::detail::storage::ImageUsage;
using FlowUi::detail::storage::ImageViewDesc;
using FlowUi::detail::storage::ImageViewHandle;
using FlowUi::detail::storage::NativeImageViewInfo;
using FlowUi::detail::storage::NativeRendererLayout;
using FlowUi::detail::storage::NativeRendererPipelineBundle;
using FlowUi::detail::storage::NativeWindowDescriptorBundle;
using FlowUi::detail::storage::MemoryClass;
using FlowUi::detail::storage::MemoryPreference;
using FlowUi::detail::storage::ManagerRecordDesc;
using FlowUi::detail::storage::ManagerRecordHandle;
using FlowUi::detail::storage::PixelFormat;
using FlowUi::detail::storage::PersistentRecordCreateInfo;
using FlowUi::detail::storage::PersistentRecordHandle;
using FlowUi::detail::storage::RendererLayoutHandle;
using FlowUi::detail::storage::RendererLayoutKey;
using FlowUi::detail::storage::RendererPipelineBundleHandle;
using FlowUi::detail::storage::RendererPipelineKey;
using FlowUi::detail::storage::ResourceDomain;
using FlowUi::detail::storage::ResourceKey;
using FlowUi::detail::storage::ResourceKind;
using FlowUi::detail::storage::ResourceState;
using FlowUi::detail::storage::ResourceSharing;
using FlowUi::detail::storage::ResourceUse;
using FlowUi::detail::storage::SamplerDesc;
using FlowUi::detail::storage::SamplerHandle;
using FlowUi::detail::storage::StorageCapability;
using FlowUi::detail::storage::StorageConfig;
using FlowUi::detail::storage::TextureHandle;
using FlowUi::detail::storage::TextureViewDesc;
using FlowUi::detail::storage::WindowDescriptorBundleDesc;
using FlowUi::detail::storage::WindowDescriptorBundleHandle;
using FlowUi::detail::storage::WindowStorageDesc;
using FlowUi::detail::storage::createTypedPersistentRecord;
using FlowUi::detail::storage::typedPersistentRecord;

struct ManagerRecordTestState {
	int value = 0;
	int* destroyed = nullptr;
	~ManagerRecordTestState() { if (destroyed) ++*destroyed; }
};

struct ManagerRecordTestArgs { int value = 0; int* destroyed = nullptr; };

ManagerRecordDesc managerRecordDesc(ResourceKey key, ManagerRecordTestArgs& args) {
	return ManagerRecordDesc{
		.key = key,
		.kind = ResourceKind::ManagerRoot,
		.bytes = sizeof(ManagerRecordTestState),
		.alignment = alignof(ManagerRecordTestState),
		.construct = +[](void* destination, void* userData) {
			const auto& values = *static_cast<ManagerRecordTestArgs*>(userData);
			::new (destination) ManagerRecordTestState{values.value, values.destroyed};
		},
		.destroy = +[](void* object) noexcept { static_cast<ManagerRecordTestState*>(object)->~ManagerRecordTestState(); },
		.userData = &args,
	};
}

struct PersistentRecordTestHeader {
	static inline int constructions = 0;
	static inline int destructions = 0;

	uint64_t marker = 0;
	size_t payloadOffset = 0;

	PersistentRecordTestHeader() noexcept { ++constructions; }
	~PersistentRecordTestHeader() noexcept { ++destructions; }

	static void resetCounts() noexcept {
		constructions = 0;
		destructions = 0;
	}
};

struct alignas(64) PersistentRecordTestPayload {
	static inline int constructions = 0;
	static inline int destructions = 0;

	int value = 0;
	int* destructionCounter = nullptr;

	PersistentRecordTestPayload(int initialValue, int* destroyed) noexcept
		: value(initialValue), destructionCounter(destroyed) {
		++constructions;
	}
	PersistentRecordTestPayload(const PersistentRecordTestPayload&) = delete;
	PersistentRecordTestPayload& operator=(const PersistentRecordTestPayload&) = delete;
	PersistentRecordTestPayload(PersistentRecordTestPayload&&) = delete;
	PersistentRecordTestPayload& operator=(PersistentRecordTestPayload&&) = delete;
	~PersistentRecordTestPayload() noexcept {
		++destructions;
		if (destructionCounter) ++*destructionCounter;
	}

	static void resetCounts() noexcept {
		constructions = 0;
		destructions = 0;
	}
};

struct ThrowingPersistentPayload {
	ThrowingPersistentPayload() { throw std::runtime_error("injected payload construction failure"); }
	~ThrowingPersistentPayload() noexcept = default;
};

template <typename Handle>
uint64_t nativeBits(Handle handle) noexcept {
	if constexpr (std::is_pointer_v<Handle>) {
		return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
	} else {
		return static_cast<uint64_t>(handle);
	}
}

void requireVk(VkResult result, std::string_view operation) {
	if (result == VK_SUCCESS) return;
	throw std::runtime_error(
		std::string(operation) + " failed with VkResult " + std::to_string(static_cast<int>(result)));
}

class OwnedRendererLayout {
public:
	OwnedRendererLayout(VulkanContext& context, uint32_t textureCapacity)
		: context_(&context) {
		try {
			VkDescriptorSetLayoutBinding globalsBinding{};
			globalsBinding.binding = 0;
			globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			globalsBinding.descriptorCount = 1;
			globalsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			VkDescriptorSetLayoutCreateInfo globalsInfo{};
			globalsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			globalsInfo.bindingCount = 1;
			globalsInfo.pBindings = &globalsBinding;
			requireVk(vkCreateDescriptorSetLayout(
				context_->device, &globalsInfo, nullptr, &globalsSetLayout_),
				"vkCreateDescriptorSetLayout(globals)");

			VkDescriptorSetLayoutBinding texturesBinding{};
			texturesBinding.binding = 0;
			texturesBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			texturesBinding.descriptorCount = textureCapacity;
			texturesBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo texturesInfo{};
			texturesInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			texturesInfo.bindingCount = 1;
			texturesInfo.pBindings = &texturesBinding;
			requireVk(vkCreateDescriptorSetLayout(
				context_->device, &texturesInfo, nullptr, &texturesSetLayout_),
				"vkCreateDescriptorSetLayout(textures)");

			const std::array setLayouts{globalsSetLayout_, texturesSetLayout_};
			VkPipelineLayoutCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
			pipelineInfo.pSetLayouts = setLayouts.data();
			requireVk(vkCreatePipelineLayout(
				context_->device, &pipelineInfo, nullptr, &pipelineLayout_),
				"vkCreatePipelineLayout");
		} catch (...) {
			reset();
			throw;
		}
	}

	~OwnedRendererLayout() { reset(); }

	OwnedRendererLayout(const OwnedRendererLayout&) = delete;
	OwnedRendererLayout& operator=(const OwnedRendererLayout&) = delete;

	[[nodiscard]] NativeRendererLayout native() const noexcept {
		return NativeRendererLayout{
			.globalsSetLayout = nativeBits(globalsSetLayout_),
			.texturesSetLayout = nativeBits(texturesSetLayout_),
			.pipelineLayout = nativeBits(pipelineLayout_),
		};
	}

	[[nodiscard]] VkDescriptorSetLayout globalsSetLayout() const noexcept { return globalsSetLayout_; }
	[[nodiscard]] VkDescriptorSetLayout texturesSetLayout() const noexcept { return texturesSetLayout_; }
	[[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return pipelineLayout_; }
	void releaseToStorage() noexcept { owns_ = false; }

private:
	void reset() noexcept {
		if (!owns_ || context_ == nullptr || context_->device == VK_NULL_HANDLE) return;
		if (pipelineLayout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context_->device, pipelineLayout_, nullptr);
		}
		if (texturesSetLayout_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context_->device, texturesSetLayout_, nullptr);
		}
		if (globalsSetLayout_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context_->device, globalsSetLayout_, nullptr);
		}
		pipelineLayout_ = VK_NULL_HANDLE;
		texturesSetLayout_ = VK_NULL_HANDLE;
		globalsSetLayout_ = VK_NULL_HANDLE;
	}

	VulkanContext* context_ = nullptr;
	VkDescriptorSetLayout globalsSetLayout_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout texturesSetLayout_ = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
	bool owns_ = true;
};

class OwnedDescriptorPool {
public:
	OwnedDescriptorPool(
		VulkanContext& context,
		VkDescriptorSetLayout globalsLayout,
		VkDescriptorSetLayout texturesLayout,
		uint32_t framesInFlight,
		uint32_t textureCapacity)
		: context_(&context) {
		const std::array poolSizes{
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
			VkDescriptorPoolSize{
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight * textureCapacity},
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = framesInFlight * 2u;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		requireVk(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &pool_),
			"vkCreateDescriptorPool");

		try {
			std::vector<VkDescriptorSetLayout> layouts(framesInFlight * 2u, globalsLayout);
			std::fill(layouts.begin() + framesInFlight, layouts.end(), texturesLayout);
			std::vector<VkDescriptorSet> sets(layouts.size(), VK_NULL_HANDLE);
			VkDescriptorSetAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocateInfo.descriptorPool = pool_;
			allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
			allocateInfo.pSetLayouts = layouts.data();
			requireVk(vkAllocateDescriptorSets(context_->device, &allocateInfo, sets.data()),
				"vkAllocateDescriptorSets");
			globalsSets_.reserve(framesInFlight);
			textureSets_.reserve(framesInFlight);
			for (uint32_t index = 0; index < framesInFlight; ++index) {
				globalsSets_.push_back(nativeBits(sets[index]));
				textureSets_.push_back(nativeBits(sets[index + framesInFlight]));
			}
		} catch (...) {
			reset();
			throw;
		}
	}

	~OwnedDescriptorPool() { reset(); }

	OwnedDescriptorPool(const OwnedDescriptorPool&) = delete;
	OwnedDescriptorPool& operator=(const OwnedDescriptorPool&) = delete;

	[[nodiscard]] NativeWindowDescriptorBundle native() const noexcept {
		return NativeWindowDescriptorBundle{
			.descriptorPool = nativeBits(pool_),
			.globalsSets = globalsSets_,
			.textureSets = textureSets_,
		};
	}

	[[nodiscard]] uint64_t poolBits() const noexcept { return nativeBits(pool_); }
	[[nodiscard]] std::vector<uint64_t>& globalsSets() noexcept { return globalsSets_; }
	[[nodiscard]] std::vector<uint64_t>& textureSets() noexcept { return textureSets_; }
	void releaseToStorage() noexcept { owns_ = false; }

private:
	void reset() noexcept {
		if (!owns_ || context_ == nullptr || context_->device == VK_NULL_HANDLE) return;
		if (pool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(context_->device, pool_, nullptr);
		pool_ = VK_NULL_HANDLE;
	}

	VulkanContext* context_ = nullptr;
	VkDescriptorPool pool_ = VK_NULL_HANDLE;
	std::vector<uint64_t> globalsSets_;
	std::vector<uint64_t> textureSets_;
	bool owns_ = true;
};

class OwnedPipelineBundle {
public:
	OwnedPipelineBundle(VulkanContext& context, VkPipelineLayout layout)
		: context_(&context), layout_(layout) {
		try {
			std::ifstream input(FLOWUI_STORAGE_TEST_COMPUTE_SPV, std::ios::binary | std::ios::ate);
			if (!input) throw std::runtime_error("failed to open the storage test compute shader");
			const std::streamoff end = input.tellg();
			if (end <= 0 || (end % static_cast<std::streamoff>(sizeof(uint32_t))) != 0) {
				throw std::runtime_error("storage test compute shader has an invalid SPIR-V size");
			}
			std::vector<uint32_t> code(static_cast<size_t>(end) / sizeof(uint32_t));
			input.seekg(0);
			input.read(reinterpret_cast<char*>(code.data()), end);
			if (!input) throw std::runtime_error("failed to read the storage test compute shader");

			VkShaderModuleCreateInfo moduleInfo{};
			moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleInfo.codeSize = static_cast<size_t>(end);
			moduleInfo.pCode = code.data();
			requireVk(vkCreateShaderModule(context_->device, &moduleInfo, nullptr, &shaderModule_),
				"vkCreateShaderModule(storage pipeline test)");

			constexpr std::array<uint32_t, 3> workgroupSizes{1u, 2u, 4u};
			for (size_t index = 0; index < pipelines_.size(); ++index) {
				VkSpecializationMapEntry mapEntry{};
				mapEntry.constantID = 0;
				mapEntry.offset = 0;
				mapEntry.size = sizeof(uint32_t);
				VkSpecializationInfo specialization{};
				specialization.mapEntryCount = 1;
				specialization.pMapEntries = &mapEntry;
				specialization.dataSize = sizeof(uint32_t);
				specialization.pData = &workgroupSizes[index];

				VkPipelineShaderStageCreateInfo stage{};
				stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
				stage.module = shaderModule_;
				stage.pName = "main";
				stage.pSpecializationInfo = &specialization;
				VkComputePipelineCreateInfo pipelineInfo{};
				pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
				pipelineInfo.stage = stage;
				pipelineInfo.layout = layout_;
				requireVk(vkCreateComputePipelines(
					context_->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines_[index]),
					"vkCreateComputePipelines(storage pipeline test)");
			}
			vkDestroyShaderModule(context_->device, shaderModule_, nullptr);
			shaderModule_ = VK_NULL_HANDLE;
		} catch (...) {
			reset();
			throw;
		}
	}

	~OwnedPipelineBundle() { reset(); }

	OwnedPipelineBundle(const OwnedPipelineBundle&) = delete;
	OwnedPipelineBundle& operator=(const OwnedPipelineBundle&) = delete;

	[[nodiscard]] NativeRendererPipelineBundle native() const noexcept {
		return NativeRendererPipelineBundle{
			.pipelineLayout = nativeBits(layout_),
			.pipelines = {
				nativeBits(pipelines_[0]),
				nativeBits(pipelines_[1]),
				nativeBits(pipelines_[2]),
			},
		};
	}

	[[nodiscard]] bool distinct() const noexcept {
		return pipelines_[0] != VK_NULL_HANDLE && pipelines_[1] != VK_NULL_HANDLE &&
			pipelines_[2] != VK_NULL_HANDLE && pipelines_[0] != pipelines_[1] &&
			pipelines_[0] != pipelines_[2] && pipelines_[1] != pipelines_[2];
	}

	void releaseToStorage() noexcept { owns_ = false; }

private:
	void reset() noexcept {
		if (context_ == nullptr || context_->device == VK_NULL_HANDLE) return;
		if (owns_) {
			for (VkPipeline pipeline : pipelines_) {
				if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(context_->device, pipeline, nullptr);
			}
		}
		if (shaderModule_ != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context_->device, shaderModule_, nullptr);
		}
		pipelines_.fill(VK_NULL_HANDLE);
		shaderModule_ = VK_NULL_HANDLE;
	}

	VulkanContext* context_ = nullptr;
	VkPipelineLayout layout_ = VK_NULL_HANDLE;
	VkShaderModule shaderModule_ = VK_NULL_HANDLE;
	std::array<VkPipeline, 3> pipelines_{};
	bool owns_ = true;
};

StorageConfig testConfig() {
	StorageConfig config{};
	config.initialPersistentCpuBytes = 512;
	config.initialStringBytes = 256;
	config.initialDecodeScratchBytes = 128;
	config.initialUploadStagingBytes = 256;
	config.transientBytesPerFramePerWindow = 128;
	config.transientBytesPerWorker = 64;
	config.initialInstanceBytesPerFrame = 256;
	config.cpuSoftBudgetBytes = 1024 * 1024;
	config.gpuSoftBudgetBytes = 64 * 1024 * 1024;
	config.expectedBlobs = 8;
	config.expectedBuffers = 8;
	config.expectedImages = 4;
	config.expectedImageViews = 4;
	config.expectedSamplers = 4;
	config.expectedTextureViews = 8;
	config.expectedWindows = 2;
	config.expectedBindingsPerWindow = 16;
	config.framesInFlight = 2;
	config.expectedWorkerCount = 2;
	config.growthFactor = 1.5f;
	config.allowRuntimeGrowth = true;
	config.detailedTracking = true;
	return config;
}

WindowStorageDesc windowDesc(uint32_t framesInFlight = 2, uint32_t workers = 2) {
	WindowStorageDesc desc{};
	desc.framesInFlight = framesInFlight;
	desc.workerCount = workers;
	desc.initialTextureBindings = 8;
	desc.maxTextureBindings = 32;
	desc.transientBytesPerFrame = 64;
	desc.transientBytesPerWorker = 32;
	return desc;
}

bool hasCapability(const IStorageSystem& storage, StorageCapability capability) {
	return (storage.capabilities() & static_cast<uint64_t>(capability)) != 0;
}

void testInitialization(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	StorageConfig invalid = testConfig();
	invalid.growthFactor = 1.0f;
	{
		FlowStorageSystem storage(vulkan.context());
		FLOWUI_CHECK_THROWS(storage.initialize(invalid));
	}

	FlowStorageSystem storage(vulkan.context());
	const StorageConfig config = testConfig();
	storage.initialize(config);
	FLOWUI_CHECK(storage.interfaceVersion() == IStorageSystem::CurrentInterfaceVersion);
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::WindowScopes));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::WorkerArenas));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::GenerationalHandles));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::PersistentRecords));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::DirectMappedWrites));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::HostScratchBufferWrites));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::FrameReadLeases));
	FLOWUI_CHECK_THROWS(storage.initialize(config));
	storage.shutdown();
	storage.shutdown();
	FLOWUI_CHECK_THROWS(storage.initialize(config));
}

void testPersistentStringsAndBlobs(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::ManagerRecords));
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::ManagerFrameSnapshots));
	storage.registerWindow(7u, windowDesc());

	storage.setBudget(128u, 64u * 1024u * 1024u);
	const AllocationTag tag{
		.memoryClass = MemoryClass::WindowPersistent,
		.resourceKind = ResourceKind::RendererLayout,
		.window = 7u,
		.frameSlot = FlowUi::detail::storage::InvalidFrameSlot,
		.debugName = 0,
	};
	const auto block = storage.allocatePersistent(64, 64, tag);
	FLOWUI_CHECK(block);
	FLOWUI_CHECK(block.size == 64);
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(block.data) % 64u == 0);
	FLOWUI_CHECK(block.tag.window == 7u);
	FLOWUI_CHECK(block.tag.memoryClass == MemoryClass::WindowPersistent);
	std::memset(block.data, 0xa5, block.size);
	auto wrongData = block;
	wrongData.data = static_cast<std::byte*>(wrongData.data) + 1;
	storage.releasePersistent(wrongData);
	auto wrongTag = block;
	wrongTag.tag.resourceKind = ResourceKind::GpuBuffer;
	storage.releasePersistent(wrongTag);
	const auto forgedReleaseProbe = storage.allocatePersistent(64, 64, tag);
	FLOWUI_CHECK(forgedReleaseProbe.data != block.data);
	FLOWUI_CHECK(std::all_of(
		static_cast<const std::byte*>(block.data),
		static_cast<const std::byte*>(block.data) + block.size,
		[](std::byte value) { return value == std::byte{0xa5}; }));
	storage.releasePersistent(forgedReleaseProbe);
	FLOWUI_CHECK_THROWS(storage.allocatePersistent(65, alignof(std::max_align_t), tag));
	storage.releasePersistent(block);
	storage.releasePersistent(block);
	FLOWUI_CHECK_THROWS(storage.allocatePersistent(8, 3, tag));
	storage.setBudget(1024u * 1024u, 64u * 1024u * 1024u);

	const auto firstString = storage.intern("shared renderer layout");
	const auto sameString = storage.intern("shared renderer layout");
	FLOWUI_CHECK(firstString != 0);
	FLOWUI_CHECK(firstString == sameString);
	FLOWUI_CHECK(storage.string(firstString) == "shared renderer layout");
	FLOWUI_CHECK(storage.string(999999u).empty());

	const std::array bytes{
		std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
	};
	const BlobHandle firstBlob = storage.createBlob(bytes, firstString);
	FLOWUI_CHECK(firstBlob);
	FLOWUI_CHECK(storage.validateHandle(ResourceKind::CpuBlob, firstBlob.index, firstBlob.generation));
	const auto read = storage.readBlob(firstBlob);
	FLOWUI_CHECK(read.size() == bytes.size());
	FLOWUI_CHECK(std::equal(read.begin(), read.end(), bytes.begin(), bytes.end()));

	storage.releaseBlob(firstBlob, 0);
	storage.collect();
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::CpuBlob, firstBlob.index, firstBlob.generation));
	FLOWUI_CHECK(storage.readBlob(firstBlob).empty());
	const BlobHandle secondBlob = storage.createBlob(bytes, firstString);
	FLOWUI_CHECK(secondBlob.index == firstBlob.index);
	FLOWUI_CHECK(secondBlob.generation != firstBlob.generation);
	storage.releaseBlob(secondBlob, 0);
	storage.collect();

	storage.unregisterWindow(7u, 0);
}

void testFrameArenasAndCompletion(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(11u, windowDesc(2, 2));

	const auto frame = storage.beginFrame(11u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	FLOWUI_CHECK(frame);
	const auto transient = storage.frameArena(frame, MemoryClass::FrameTransient);
	auto first = transient.allocateArray<std::byte>(48);
	FLOWUI_CHECK(first.size() == 48);
	std::fill(first.begin(), first.end(), std::byte{0x5a});
	auto overflow = transient.allocateArray<std::byte>(128);
	FLOWUI_CHECK(overflow.size() == 128);
	FLOWUI_CHECK(std::all_of(first.begin(), first.end(), [](std::byte value) {
		return value == std::byte{0x5a};
	}));

	const auto worker0 = storage.workerArena(frame, 0);
	const auto worker1 = storage.workerArena(frame, 1);
	auto worker0Bytes = worker0.allocateArray<uint64_t>(4);
	auto worker1Bytes = worker1.allocateArray<uint64_t>(4);
	FLOWUI_CHECK(!worker0Bytes.empty());
	FLOWUI_CHECK(!worker1Bytes.empty());
	FLOWUI_CHECK(worker0Bytes.data() != worker1Bytes.data());
	FLOWUI_CHECK_THROWS(storage.workerArena(frame, 2));

	const auto lease = storage.sealFrame(frame);
	FLOWUI_CHECK(lease);
	FLOWUI_CHECK(lease.valid());
	FLOWUI_CHECK(storage.readView(lease).valid());
	FLOWUI_CHECK(storage.windowBindingView(lease).valid());
	FLOWUI_CHECK_THROWS(storage.frameArena(frame, MemoryClass::FrameTransient));
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(transient.allocate(1) == nullptr);
	FLOWUI_CHECK(worker0.allocate(1) == nullptr);
#endif

	const auto submission = storage.noteSubmission(lease);
	FLOWUI_CHECK(submission);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(!lease.valid());
#endif
	FLOWUI_CHECK_THROWS(storage.beginFrame(11u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 2}));
	storage.noteCompleted(submission);
	FLOWUI_CHECK(storage.completedSerial() == submission.serial);
	FLOWUI_CHECK_THROWS(storage.noteCompleted(submission));

	const auto reused = storage.beginFrame(11u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 3});
	FLOWUI_CHECK(reused.epoch != frame.epoch);
	FLOWUI_CHECK_THROWS(storage.unregisterWindow(11u, 0));
	storage.cancelFrame(reused);
	storage.unregisterWindow(11u, 0);
}

void testMultiWindowProgress(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(21u, windowDesc(1, 1));
	storage.registerWindow(22u, windowDesc(1, 1));

	const auto frameA = storage.beginFrame(21u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 10});
	const auto frameB = storage.beginFrame(22u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 90});
	const auto leaseA = storage.sealFrame(frameA);
	const auto leaseB = storage.sealFrame(frameB);
	FLOWUI_CHECK(leaseA.frame.window != leaseB.frame.window);
	FLOWUI_CHECK(storage.readView(leaseA).epoch == frameA.epoch);
	FLOWUI_CHECK(storage.readView(leaseB).epoch == frameB.epoch);

	const auto submissionA = storage.noteSubmission(leaseA);
	const auto submissionB = storage.noteSubmission(leaseB);
	FLOWUI_CHECK(submissionB.serial == submissionA.serial + 1u);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(!leaseA.valid());
	FLOWUI_CHECK(!leaseB.valid());
#endif
	FLOWUI_CHECK_THROWS(storage.beginFrame(22u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 91}));

	storage.noteCompleted(submissionB);
	FLOWUI_CHECK(storage.completedSerial() < submissionB.serial);
	storage.noteCompleted(submissionA);
	FLOWUI_CHECK(storage.completedSerial() == submissionB.serial);

	const auto nextB = storage.beginFrame(22u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 92});
	storage.cancelFrame(nextB);
	storage.unregisterWindow(21u, submissionA.serial);
	storage.unregisterWindow(22u, submissionB.serial);
}

void testMappedWriteModes(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	StorageConfig config = testConfig();
	config.defaultBufferWriteMode = BufferWriteMode::HostScratchThenCopy;
	storage.initialize(config);
	storage.registerWindow(31u, windowDesc(1, 1));

	storage.setBudget(1024u * 1024u, 32u);
	BufferDesc desc{};
	desc.size = 64;
	desc.usage = BufferUsage::Storage | BufferUsage::TransferDestination;
	desc.memory = MemoryPreference::HostVisible;
	desc.sharing = ResourceSharing::WindowLocal;
	desc.access = AccessMode::CpuWrite;
	desc.persistentlyMapped = true;
	desc.window = 31u;
	FLOWUI_CHECK_THROWS(storage.createBuffer(desc));
	storage.setBudget(1024u * 1024u, 64u * 1024u * 1024u);

	const BufferHandle buffer = storage.createBuffer(desc);
	FLOWUI_CHECK(buffer);
	const auto native = storage.nativeBuffer(buffer);
	FLOWUI_CHECK(native.nativeBuffer != 0);
	FLOWUI_CHECK(native.size == desc.size);

	const auto frame = storage.beginFrame(31u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	auto direct = storage.beginBufferWrite(frame, buffer, 0, 16, BufferWriteMode::DirectMapped);
	FLOWUI_CHECK(direct);
	FLOWUI_CHECK(direct.mode == BufferWriteMode::DirectMapped);
	std::fill_n(direct.data, 16, std::byte{0x11});
	FLOWUI_CHECK_THROWS(storage.beginBufferWrite(frame, buffer, 8, 8, BufferWriteMode::DirectMapped));
	storage.commitBufferWrite(frame, direct, 16);
	FLOWUI_CHECK_THROWS(storage.commitBufferWrite(frame, direct, 16));
	auto verifyDirect = storage.beginBufferWrite(frame, buffer, 0, 16, BufferWriteMode::DirectMapped);
	FLOWUI_CHECK(std::all_of(verifyDirect.data, verifyDirect.data + 16, [](std::byte value) {
		return value == std::byte{0x11};
	}));
	storage.commitBufferWrite(frame, verifyDirect, 0);

	auto scratch = storage.beginBufferWrite(frame, buffer, 16, 16, BufferWriteMode::HostScratchThenCopy);
	FLOWUI_CHECK(scratch);
	FLOWUI_CHECK(scratch.mode == BufferWriteMode::HostScratchThenCopy);
	std::fill_n(scratch.data, 16, std::byte{0x22});
	storage.commitBufferWrite(frame, scratch, 16);
	auto verifyScratch = storage.beginBufferWrite(frame, buffer, 16, 16, BufferWriteMode::DirectMapped);
	FLOWUI_CHECK(std::all_of(verifyScratch.data, verifyScratch.data + 16, [](std::byte value) {
		return value == std::byte{0x22};
	}));
	storage.commitBufferWrite(frame, verifyScratch, 0);
	auto configuredDefault = storage.beginBufferWrite(frame, buffer, 48, 8, BufferWriteMode::Default);
	FLOWUI_CHECK(configuredDefault.mode == BufferWriteMode::HostScratchThenCopy);
	std::fill_n(configuredDefault.data, 8, std::byte{0x4d});
	storage.commitBufferWrite(frame, configuredDefault, 8);

	const std::array tail{
		std::byte{0x31}, std::byte{0x32}, std::byte{0x33}, std::byte{0x34},
	};
	storage.writeBuffer(frame, buffer, 32, tail);

	auto pending = storage.beginBufferWrite(frame, buffer, 40, 8, BufferWriteMode::DirectMapped);
	FLOWUI_CHECK_THROWS(storage.sealFrame(frame));
	std::fill_n(pending.data, 8, std::byte{0x44});
	storage.commitBufferWrite(frame, pending, 8);
	const auto lease = storage.sealFrame(frame);
	const auto submission = storage.noteSubmission(lease);
	storage.releaseBuffer(buffer, submission.serial);
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, buffer.index, buffer.generation));
	FLOWUI_CHECK(storage.nativeBuffer(buffer).nativeBuffer == 0);
	storage.noteCompleted(submission);
	storage.collect();
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, buffer.index, buffer.generation));
	storage.unregisterWindow(31u, submission.serial);
}

void testNativeImageViewAndSamplerQueries(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());

	ImageDesc imageDesc{};
	imageDesc.format = PixelFormat::Rgba8Unorm;
	imageDesc.usage = ImageUsage::Sampled;
	imageDesc.memory = MemoryPreference::DeviceLocal;
	const ImageHandle image = storage.createImage(imageDesc);
	const ImageViewHandle view = storage.createImageView(image, ImageViewDesc{});
	const SamplerHandle sampler = storage.acquireSampler(SamplerDesc{});

	const NativeImageViewInfo nativeView = storage.nativeImageView(view);
	FLOWUI_CHECK(nativeView.nativeImageView != 0);
	FLOWUI_CHECK(nativeView.image == image);
	FLOWUI_CHECK(storage.nativeSampler(sampler).nativeSampler != 0);

	FLOWUI_CHECK(storage.nativeImageView(ImageViewHandle{view.index, view.generation + 1u}).nativeImageView == 0);
	FLOWUI_CHECK(storage.nativeSampler(SamplerHandle{sampler.index, sampler.generation + 1u}).nativeSampler == 0);

	storage.releaseImageView(view, 0);
	storage.releaseSampler(sampler, 0);
	FLOWUI_CHECK(storage.nativeImageView(view).nativeImageView == 0);
	FLOWUI_CHECK(storage.nativeSampler(sampler).nativeSampler == 0);
	storage.releaseImage(image, 0);
	storage.collect();
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::ImageView, view.index, view.generation));
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::Sampler, sampler.index, sampler.generation));
}

void testSharedUiByteResourceLifecycle(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	SharedUiByteResources resources{};
	initSharedUiByteResources(storage, resources);

	FLOWUI_CHECK(resources.quadBuffer);
	FLOWUI_CHECK(resources.placeholderFontImage);
	FLOWUI_CHECK(resources.placeholderFontView);
	FLOWUI_CHECK(resources.placeholderUiImage);
	FLOWUI_CHECK(resources.placeholderUiView);
	FLOWUI_CHECK(resources.linearSampler);
	FLOWUI_CHECK(resources.nativeQuadBuffer != VK_NULL_HANDLE);
	FLOWUI_CHECK(resources.nativePlaceholderFontView != VK_NULL_HANDLE);
	FLOWUI_CHECK(resources.nativePlaceholderUiView != VK_NULL_HANDLE);
	FLOWUI_CHECK(resources.nativeLinearSampler != VK_NULL_HANDLE);

	const BufferHandle quad = resources.quadBuffer;
	const ImageHandle fontImage = resources.placeholderFontImage;
	const ImageViewHandle fontView = resources.placeholderFontView;
	const SamplerHandle sampler = resources.linearSampler;
	destroySharedUiByteResources(storage, resources);
	storage.collect();
	FLOWUI_CHECK(!resources.quadBuffer);
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, quad.index, quad.generation));
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuImage, fontImage.index, fontImage.generation));
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::ImageView, fontView.index, fontView.generation));
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::Sampler, sampler.index, sampler.generation));
}

BufferDesc trackedBufferDesc(FlowUi::detail::storage::WindowId window) {
	BufferDesc desc{};
	desc.size = 64;
	desc.usage = BufferUsage::Storage;
	desc.memory = MemoryPreference::HostVisible;
	desc.sharing = ResourceSharing::WindowLocal;
	desc.access = AccessMode::CpuWrite;
	desc.persistentlyMapped = true;
	desc.window = window;
	return desc;
}

void testFrameHeldResourceLifetimes(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(51u, windowDesc(1, 1));

	const BufferDesc desc = trackedBufferDesc(51u);
	const BufferHandle cancelled = storage.createBuffer(desc);
	const auto cancelFrame = storage.beginFrame(51u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array duplicateCancelUses{
		ResourceUse{ResourceKind::GpuBuffer, cancelled.packed()},
		ResourceUse{ResourceKind::GpuBuffer, cancelled.packed()},
	};
	storage.trackUses(cancelFrame, duplicateCancelUses);
	storage.releaseBuffer(cancelled, 0);
	FLOWUI_CHECK(storage.validateHandle(ResourceKind::GpuBuffer, cancelled.index, cancelled.generation));
	storage.cancelFrame(cancelFrame);
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, cancelled.index, cancelled.generation));
	storage.collect();

	const BufferHandle recycledAfterCancel = storage.createBuffer(desc);
	FLOWUI_CHECK(recycledAfterCancel.index == cancelled.index);
	FLOWUI_CHECK(recycledAfterCancel.generation != cancelled.generation);
	storage.releaseBuffer(recycledAfterCancel, 0);
	storage.collect();

	const BufferHandle submitted = storage.createBuffer(desc);
	const auto submitFrame = storage.beginFrame(51u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 2});
	const std::array duplicateSubmitUses{
		ResourceUse{ResourceKind::GpuBuffer, submitted.packed()},
		ResourceUse{ResourceKind::GpuBuffer, submitted.packed()},
	};
	storage.trackUses(submitFrame, duplicateSubmitUses);
	storage.releaseBuffer(submitted, 0);
	FLOWUI_CHECK(storage.validateHandle(ResourceKind::GpuBuffer, submitted.index, submitted.generation));
	const auto submission = storage.noteSubmission(storage.sealFrame(submitFrame));
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, submitted.index, submitted.generation));
	storage.collect();

	const BufferHandle whileInFlight = storage.createBuffer(desc);
	FLOWUI_CHECK(whileInFlight.index != submitted.index);
	storage.releaseBuffer(whileInFlight, 0);
	storage.collect();

	storage.noteCompleted(submission);
	FLOWUI_CHECK_THROWS(storage.noteCompleted(submission));
	storage.collect();
	const BufferHandle recycledAfterSubmission = storage.createBuffer(desc);
	FLOWUI_CHECK(recycledAfterSubmission.index == submitted.index);
	FLOWUI_CHECK(recycledAfterSubmission.generation != submitted.generation);
	storage.releaseBuffer(recycledAfterSubmission, 0);
	storage.collect();
	storage.unregisterWindow(51u, submission.serial);
}

void testConcurrentNonOverlappingWrites(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(52u, windowDesc(2, 1));
	const BufferHandle buffer = storage.createBuffer(trackedBufferDesc(52u));
	const std::array frames{
		storage.beginFrame(52u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1}),
		storage.beginFrame(52u, FrameStorageDesc{.frameSlot = 1, .frameNumber = 1}),
	};
	std::barrier synchronizationPoint(2);
	std::array<std::exception_ptr, 2> errors{};
	std::array<std::jthread, 2> workers;
	for (size_t index = 0; index < workers.size(); ++index) {
		workers[index] = std::jthread([&, index] {
			BufferWriteView write{};
			synchronizationPoint.arrive_and_wait();
			try {
				write = storage.beginBufferWrite(
					frames[index], buffer, index * 16u, 16, BufferWriteMode::DirectMapped);
			} catch (...) {
				errors[index] = std::current_exception();
			}
			synchronizationPoint.arrive_and_wait();
			if (write) {
				std::fill_n(write.data, 16, index == 0 ? std::byte{0x31} : std::byte{0x72});
			}
			synchronizationPoint.arrive_and_wait();
			if (write) {
				try {
					storage.commitBufferWrite(frames[index], write, 16);
				} catch (...) {
					errors[index] = std::current_exception();
				}
			}
		});
	}
	for (auto& worker : workers) worker.join();
	for (const auto& error : errors) {
		if (error) std::rethrow_exception(error);
	}

	const auto submission0 = storage.noteSubmission(storage.sealFrame(frames[0]));
	const auto submission1 = storage.noteSubmission(storage.sealFrame(frames[1]));
	storage.releaseBuffer(buffer, submission1.serial);
	storage.noteCompleted(submission1);
	storage.noteCompleted(submission0);
	storage.collect();
	FLOWUI_CHECK(!storage.validateHandle(ResourceKind::GpuBuffer, buffer.index, buffer.generation));
	storage.unregisterWindow(52u, submission1.serial);
}

void testTextureBindingProtocol(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());

	ImageDesc imageDesc{};
	imageDesc.width = 1;
	imageDesc.height = 1;
	imageDesc.format = PixelFormat::Rgba8Unorm;
	imageDesc.usage = ImageUsage::Sampled;
	imageDesc.memory = MemoryPreference::DeviceLocal;
	const ImageHandle image = storage.createImage(imageDesc);
	const auto imageView = storage.createImageView(image, ImageViewDesc{});
	const SamplerHandle sampler = storage.acquireSampler(SamplerDesc{});
	const TextureViewDesc textureDesc{
		.imageView = imageView,
		.sampler = sampler,
		.sourceWidth = 1,
		.sourceHeight = 1,
	};
	auto publish = [&](std::string_view name) {
		return storage.publishTexture(ResourceKey{
			.domain = ResourceDomain::Image,
			.name = storage.intern(name),
			.window = 0,
		}, textureDesc, nullptr);
	};

	const ResourceKey oldFallbackKey{
		.domain = ResourceDomain::Image,
		.name = storage.intern("test fallback old"),
		.window = 0,
	};
	const TextureHandle oldFallback = storage.publishTexture(oldFallbackKey, textureDesc, nullptr);
	const TextureHandle texture = publish("test texture one");
	const TextureHandle capacityTexture = publish("test texture capacity");
	const TextureHandle newFallback = publish("test fallback new");
	storage.setFallbackTexture(oldFallback);
	storage.releaseImageView(imageView, 0);
	storage.releaseSampler(sampler, 0);
	storage.releaseImage(image, 0);

	FLOWUI_CHECK(storage.removeTexture(oldFallbackKey, 0));
	FLOWUI_CHECK(!storage.findTexture(oldFallbackKey));
	FLOWUI_CHECK(!storage.removeTexture(oldFallbackKey, 0));

	WindowStorageDesc desc = windowDesc(1, 1);
	desc.initialTextureBindings = 1;
	desc.maxTextureBindings = 2;
	storage.registerWindow(61u, desc);

	const auto frame = storage.beginFrame(61u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const TextureHandle invalidTexture{999999u, 17u};
	const auto invalidResolved = storage.resolveTexture(frame, invalidTexture);
	FLOWUI_CHECK(invalidResolved.descriptorIndex == 0);
	FLOWUI_CHECK(invalidResolved.state == ResourceState::Ready);
	FLOWUI_CHECK(invalidResolved.nativeImageView != 0);
	FLOWUI_CHECK(invalidResolved.nativeSampler != 0);

	const std::array requested{texture, invalidTexture, texture};
	const auto prepared = storage.prepareTextureBindings(frame, requested);
	FLOWUI_CHECK(prepared.epoch == frame.epoch);
	FLOWUI_CHECK(prepared.requiredDescriptorCapacity == 2);
	FLOWUI_CHECK(prepared.dirtyBindings.size() == 2);
	std::vector<DescriptorWriteRecord> applied(
		prepared.dirtyBindings.begin(), prepared.dirtyBindings.end());
	FLOWUI_CHECK(std::any_of(applied.begin(), applied.end(), [](const auto& write) {
		return write.descriptorIndex == 0;
	}));
	FLOWUI_CHECK(std::any_of(applied.begin(), applied.end(), [](const auto& write) {
		return write.descriptorIndex == 1;
	}));
	auto mismatched = applied;
	++mismatched.front().bindingRevision;
	FLOWUI_CHECK_THROWS(storage.acknowledgeTextureBindings(frame, mismatched));
	storage.acknowledgeTextureBindings(frame, applied);
	const auto alreadyApplied = storage.prepareTextureBindings(frame, requested);
	FLOWUI_CHECK(alreadyApplied.dirtyBindings.empty());

	const auto lease = storage.sealFrame(frame);
	const auto bindings = storage.windowBindingView(lease);
	const auto* textureBinding = bindings.binding(texture);
	FLOWUI_CHECK(textureBinding != nullptr);
	FLOWUI_CHECK(textureBinding->descriptorIndex == 1);
	const auto submission = storage.noteSubmission(lease);

	storage.setFallbackTexture(newFallback);
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::TextureView, oldFallback.index, oldFallback.generation));
	storage.collect();
	const TextureHandle allocatedWhileInFlight = publish("test texture while fallback in flight");
	FLOWUI_CHECK(allocatedWhileInFlight.index != oldFallback.index);

	storage.noteCompleted(submission);
	FLOWUI_CHECK_THROWS(storage.noteCompleted(submission));
	storage.collect();
	const TextureHandle recycledFallbackSlot = publish("test texture after fallback completion");
	FLOWUI_CHECK(recycledFallbackSlot.index == oldFallback.index);
	FLOWUI_CHECK(recycledFallbackSlot.generation != oldFallback.generation);

	storage.resetTextureBindings(61u, 0);
	const auto nextFrame = storage.beginFrame(61u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 2});
	const std::array nextRequested{texture};
	const auto afterReset = storage.prepareTextureBindings(nextFrame, nextRequested);
	FLOWUI_CHECK(afterReset.dirtyBindings.size() == 2);
	FLOWUI_CHECK(afterReset.requiredDescriptorCapacity == 2);
	std::vector<DescriptorWriteRecord> resetWrites(
		afterReset.dirtyBindings.begin(), afterReset.dirtyBindings.end());
	storage.acknowledgeTextureBindings(nextFrame, resetWrites);
	FLOWUI_CHECK_THROWS(storage.resetTextureBindings(61u, 0));
	const std::array overCapacity{capacityTexture};
	FLOWUI_CHECK_THROWS(storage.prepareTextureBindings(nextFrame, overCapacity));
	const auto preservedAfterFailure = storage.prepareTextureBindings(nextFrame, nextRequested);
	FLOWUI_CHECK(preservedAfterFailure.binding(texture) != nullptr);
	FLOWUI_CHECK(preservedAfterFailure.binding(texture)->descriptorIndex == 1u);
	storage.cancelFrame(nextFrame);
	storage.resetTextureBindings(61u, 0);
	storage.unregisterWindow(61u, submission.serial);
}

 void testWindowAndFrameLocality(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(81u, windowDesc(2, 1));
	storage.registerWindow(82u, windowDesc(2, 1));

	BufferDesc windowBufferDesc = trackedBufferDesc(81u);
	windowBufferDesc.sharing = ResourceSharing::WindowLocal;
	const BufferHandle windowBuffer = storage.createBuffer(windowBufferDesc);
	BufferDesc frameBufferDesc = trackedBufferDesc(81u);
	frameBufferDesc.sharing = ResourceSharing::FrameLocal;
	frameBufferDesc.frameSlot = 0;
	const BufferHandle frameBuffer = storage.createBuffer(frameBufferDesc);

	ImageDesc windowImageDesc{};
	windowImageDesc.format = PixelFormat::Rgba8Unorm;
	windowImageDesc.usage = ImageUsage::Sampled;
	windowImageDesc.memory = MemoryPreference::DeviceLocal;
	windowImageDesc.sharing = ResourceSharing::WindowLocal;
	windowImageDesc.window = 81u;
	const ImageHandle windowImage = storage.createImage(windowImageDesc);
	ImageDesc frameImageDesc = windowImageDesc;
	frameImageDesc.sharing = ResourceSharing::FrameLocal;
	frameImageDesc.frameSlot = 0;
	const ImageHandle frameImage = storage.createImage(frameImageDesc);
	const auto windowImageView = storage.createImageView(windowImage, ImageViewDesc{});
	const auto frameImageView = storage.createImageView(frameImage, ImageViewDesc{});
	const SamplerHandle sampler = storage.acquireSampler(SamplerDesc{});
	const ResourceKey windowTextureKey{
		.domain = ResourceDomain::Image,
		.name = storage.intern("window-local texture"),
		.window = 81u,
	};
	const ResourceKey frameTextureKey{
		.domain = ResourceDomain::Image,
		.name = storage.intern("frame-slot-local texture"),
		.window = 81u,
	};
	const TextureHandle windowTexture = storage.publishTexture(
		windowTextureKey, TextureViewDesc{.imageView = windowImageView, .sampler = sampler}, nullptr);
	const TextureHandle frameTexture = storage.publishTexture(
		frameTextureKey, TextureViewDesc{.imageView = frameImageView, .sampler = sampler}, nullptr);

	const auto ownerSlot0 = storage.beginFrame(81u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const auto ownerSlot1 = storage.beginFrame(81u, FrameStorageDesc{.frameSlot = 1, .frameNumber = 1});
	const auto otherWindow = storage.beginFrame(82u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array validOwnerUses{
		ResourceUse{ResourceKind::GpuBuffer, windowBuffer.packed()},
		ResourceUse{ResourceKind::GpuBuffer, frameBuffer.packed()},
		ResourceUse{ResourceKind::GpuImage, windowImage.packed()},
		ResourceUse{ResourceKind::GpuImage, frameImage.packed()},
		ResourceUse{ResourceKind::TextureView, windowTexture.packed()},
		ResourceUse{ResourceKind::TextureView, frameTexture.packed()},
	};
	storage.trackUses(ownerSlot0, validOwnerUses);
	storage.trackUse(ownerSlot1, windowBuffer);
	storage.trackUse(ownerSlot1, windowImage);
	const std::array validWindowTextureUse{
		ResourceUse{ResourceKind::TextureView, windowTexture.packed()},
	};
	storage.trackUses(ownerSlot1, validWindowTextureUse);

	auto expectRejected = [&](const auto& frame, ResourceKind kind, uint64_t packedHandle) {
		const std::array use{ResourceUse{kind, packedHandle}};
		FLOWUI_CHECK_THROWS(storage.trackUses(frame, use));
	};
	expectRejected(otherWindow, ResourceKind::GpuBuffer, windowBuffer.packed());
	expectRejected(otherWindow, ResourceKind::GpuImage, windowImage.packed());
	expectRejected(otherWindow, ResourceKind::TextureView, windowTexture.packed());
	expectRejected(ownerSlot1, ResourceKind::GpuBuffer, frameBuffer.packed());
	expectRejected(ownerSlot1, ResourceKind::GpuImage, frameImage.packed());
	expectRejected(ownerSlot1, ResourceKind::TextureView, frameTexture.packed());

	FLOWUI_CHECK_THROWS(storage.beginBufferWrite(
		otherWindow, windowBuffer, 0, 4, BufferWriteMode::Default));
	FLOWUI_CHECK_THROWS(storage.beginBufferWrite(
		ownerSlot1, frameBuffer, 0, 4, BufferWriteMode::Default));

	storage.cancelFrame(ownerSlot0);
	storage.cancelFrame(ownerSlot1);
	storage.cancelFrame(otherWindow);
	FLOWUI_CHECK(storage.removeTexture(windowTextureKey, 0));
	FLOWUI_CHECK(storage.removeTexture(frameTextureKey, 0));
	storage.releaseBuffer(windowBuffer, 0);
	storage.releaseBuffer(frameBuffer, 0);
	storage.releaseImageView(windowImageView, 0);
	storage.releaseImageView(frameImageView, 0);
	storage.releaseSampler(sampler, 0);
	storage.releaseImage(windowImage, 0);
	storage.releaseImage(frameImage, 0);
	storage.collect();
	storage.unregisterWindow(81u, 0);
	FLOWUI_CHECK_THROWS(storage.registerWindow(81u, windowDesc(1, 1)));
	storage.unregisterWindow(82u, 0);
}

void testRendererResourceOwnership(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	WindowStorageDesc rendererWindow = windowDesc(2, 1);
	rendererWindow.maxTextureBindings = 8;
	storage.registerWindow(71u, rendererWindow);
	storage.registerWindow(72u, windowDesc(1, 1));

	constexpr uint32_t descriptorCapacity = 8;
	const RendererLayoutKey layoutKey{
		.textureDescriptorCapacity = descriptorCapacity,
		.shaderInterfaceRevision = 3,
		.pushConstantBytes = 0,
		.descriptorFeatureFlags = 0,
	};
	OwnedRendererLayout firstNative(vulkan.context(), descriptorCapacity);
	const auto firstPublication = storage.publishRendererLayout(layoutKey, firstNative.native(), 0);
	FLOWUI_CHECK(firstPublication.handle);
	FLOWUI_CHECK(firstPublication.ownershipTransferred);
	firstNative.releaseToStorage();

	OwnedRendererLayout duplicateNative(vulkan.context(), descriptorCapacity);
	const auto duplicatePublication = storage.publishRendererLayout(layoutKey, duplicateNative.native(), 0);
	FLOWUI_CHECK(duplicatePublication.handle == firstPublication.handle);
	FLOWUI_CHECK(!duplicatePublication.ownershipTransferred);
	const RendererLayoutHandle acquired = storage.acquireRendererLayout(layoutKey);
	FLOWUI_CHECK(acquired == firstPublication.handle);
	const NativeRendererLayout storedLayout = storage.nativeRendererLayout(firstPublication.handle);
	FLOWUI_CHECK(storedLayout.globalsSetLayout == firstNative.native().globalsSetLayout);
	FLOWUI_CHECK(storedLayout.texturesSetLayout == firstNative.native().texturesSetLayout);
	FLOWUI_CHECK(storedLayout.pipelineLayout == firstNative.native().pipelineLayout);

	const RendererPipelineKey pipelineKey{
		.layout = firstPublication.handle,
		.nativeColorFormat = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM),
		.sampleCount = 1,
		.pipelineStateRevision = 5,
		.shaderSetFingerprint = 0x5a17u,
	};
	OwnedPipelineBundle firstPipelines(vulkan.context(), firstNative.pipelineLayout());
	FLOWUI_CHECK(firstPipelines.distinct());
	const auto firstPipelinePublication =
		storage.publishRendererPipelineBundle(pipelineKey, firstPipelines.native(), 0);
	FLOWUI_CHECK(firstPipelinePublication.handle);
	FLOWUI_CHECK(firstPipelinePublication.ownershipTransferred);
	firstPipelines.releaseToStorage();

	OwnedPipelineBundle duplicatePipelines(vulkan.context(), firstNative.pipelineLayout());
	FLOWUI_CHECK(duplicatePipelines.distinct());
	const auto duplicatePipelinePublication =
		storage.publishRendererPipelineBundle(pipelineKey, duplicatePipelines.native(), 0);
	FLOWUI_CHECK(duplicatePipelinePublication.handle == firstPipelinePublication.handle);
	FLOWUI_CHECK(!duplicatePipelinePublication.ownershipTransferred);
	const RendererPipelineBundleHandle acquiredPipelines =
		storage.acquireRendererPipelineBundle(pipelineKey);
	FLOWUI_CHECK(acquiredPipelines == firstPipelinePublication.handle);
	const NativeRendererPipelineBundle storedPipelines =
		storage.nativeRendererPipelineBundle(firstPipelinePublication.handle);
	FLOWUI_CHECK(storedPipelines.pipelineLayout == firstPipelines.native().pipelineLayout);
	FLOWUI_CHECK(storedPipelines.pipelines == firstPipelines.native().pipelines);

	OwnedDescriptorPool descriptorPool(
		vulkan.context(), firstNative.globalsSetLayout(), firstNative.texturesSetLayout(), 2, descriptorCapacity);
	const uint64_t originalGlobalsSet = descriptorPool.globalsSets().front();
	const uint64_t originalTextureSet = descriptorPool.textureSets().front();
	const WindowDescriptorBundleDesc bundleDesc{
		.window = 71u,
		.layout = firstPublication.handle,
		.framesInFlight = 2,
		.descriptorCapacity = descriptorCapacity,
	};
	const WindowDescriptorBundleHandle bundle =
		storage.adoptWindowDescriptorBundle(bundleDesc, descriptorPool.native());
	FLOWUI_CHECK(bundle);
	const uint64_t descriptorPoolBits = descriptorPool.poolBits();
	descriptorPool.releaseToStorage();
	descriptorPool.globalsSets().front() = 0;
	descriptorPool.textureSets().front() = 0;
	const auto storedBundle = storage.nativeWindowDescriptorBundle(bundle);
	FLOWUI_CHECK(storedBundle.descriptorPool == descriptorPoolBits);
	FLOWUI_CHECK(storedBundle.globalsSets.size() == 2);
	FLOWUI_CHECK(storedBundle.textureSets.size() == 2);
	FLOWUI_CHECK(storedBundle.globalsSets.front() == originalGlobalsSet);
	FLOWUI_CHECK(storedBundle.textureSets.front() == originalTextureSet);
	FLOWUI_CHECK(storedBundle.descriptorCapacity == descriptorCapacity);

	OwnedDescriptorPool rejectedPool(
		vulkan.context(), firstNative.globalsSetLayout(), firstNative.texturesSetLayout(), 2, descriptorCapacity);
	WindowDescriptorBundleDesc rejectedDesc = bundleDesc;
	rejectedDesc.descriptorCapacity = descriptorCapacity - 1u;
	FLOWUI_CHECK_THROWS(storage.adoptWindowDescriptorBundle(rejectedDesc, rejectedPool.native()));

	const auto wrongWindowFrame = storage.beginFrame(
		72u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array wrongWindowUse{
		ResourceUse{ResourceKind::WindowDescriptorBundle, bundle.packed()},
	};
	FLOWUI_CHECK_THROWS(storage.trackUses(wrongWindowFrame, wrongWindowUse));
	storage.cancelFrame(wrongWindowFrame);

	const auto frame = storage.beginFrame(71u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array duplicateUses{
		ResourceUse{ResourceKind::RendererLayout, firstPublication.handle.packed()},
		ResourceUse{ResourceKind::RendererLayout, firstPublication.handle.packed()},
		ResourceUse{ResourceKind::RendererPipelineBundle, firstPipelinePublication.handle.packed()},
		ResourceUse{ResourceKind::RendererPipelineBundle, firstPipelinePublication.handle.packed()},
		ResourceUse{ResourceKind::WindowDescriptorBundle, bundle.packed()},
		ResourceUse{ResourceKind::WindowDescriptorBundle, bundle.packed()},
	};
	storage.trackUses(frame, duplicateUses);
	storage.releaseWindowDescriptorBundle(bundle, 0);
	storage.releaseRendererPipelineBundle(firstPipelinePublication.handle, 0);
	storage.releaseRendererPipelineBundle(duplicatePipelinePublication.handle, 0);
	storage.releaseRendererPipelineBundle(acquiredPipelines, 0);
	storage.releaseRendererLayout(firstPublication.handle, 0);
	storage.releaseRendererLayout(duplicatePublication.handle, 0);
	storage.releaseRendererLayout(acquired, 0);
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(bundle).descriptorPool != 0);
	FLOWUI_CHECK(storage.nativeRendererPipelineBundle(
		firstPipelinePublication.handle).pipelines[0] != 0);
	FLOWUI_CHECK(storage.nativeRendererLayout(firstPublication.handle).pipelineLayout != 0);

	const auto submission = storage.noteSubmission(storage.sealFrame(frame));
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(bundle).descriptorPool == 0);
	FLOWUI_CHECK(storage.nativeRendererPipelineBundle(
		firstPipelinePublication.handle).pipelines[0] == 0);
	FLOWUI_CHECK(storage.nativeRendererLayout(firstPublication.handle).pipelineLayout != 0);
	storage.collect();
	FLOWUI_CHECK(storage.nativeRendererLayout(firstPublication.handle).pipelineLayout != 0);
	storage.noteCompleted(submission);
	FLOWUI_CHECK_THROWS(storage.noteCompleted(submission));
	storage.collect();
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(bundle).descriptorPool == 0);
	FLOWUI_CHECK(storage.nativeRendererPipelineBundle(
		firstPipelinePublication.handle).pipelines[0] == 0);
	FLOWUI_CHECK(storage.nativeRendererLayout(firstPublication.handle).pipelineLayout == 0);
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::WindowDescriptorBundle, bundle.index, bundle.generation));
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::RendererLayout, firstPublication.handle.index, firstPublication.handle.generation));
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::RendererPipelineBundle,
		firstPipelinePublication.handle.index,
		firstPipelinePublication.handle.generation));

	storage.unregisterWindow(71u, submission.serial);
	storage.unregisterWindow(72u, 0);
}

void testDescriptorBundleReplacement(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());

	ImageDesc imageDesc{};
	imageDesc.format = PixelFormat::Rgba8Unorm;
	imageDesc.usage = ImageUsage::Sampled;
	imageDesc.memory = MemoryPreference::DeviceLocal;
	const ImageHandle image = storage.createImage(imageDesc);
	const auto imageView = storage.createImageView(image, ImageViewDesc{});
	const SamplerHandle sampler = storage.acquireSampler(SamplerDesc{});
	const TextureViewDesc textureDesc{.imageView = imageView, .sampler = sampler};
	const TextureHandle fallback = storage.publishTexture(ResourceKey{
		.domain = ResourceDomain::Image,
		.name = storage.intern("descriptor replacement fallback"),
		.window = 0,
	}, textureDesc, nullptr);
	const TextureHandle texture = storage.publishTexture(ResourceKey{
		.domain = ResourceDomain::Image,
		.name = storage.intern("descriptor replacement texture"),
		.window = 0,
	}, textureDesc, nullptr);
	storage.setFallbackTexture(fallback);
	storage.releaseImageView(imageView, 0);
	storage.releaseSampler(sampler, 0);
	storage.releaseImage(image, 0);

	constexpr uint32_t descriptorCapacity = 4;
	WindowStorageDesc window = windowDesc(1, 1);
	window.initialTextureBindings = descriptorCapacity;
	window.maxTextureBindings = descriptorCapacity;
	storage.registerWindow(91u, window);
	const RendererLayoutKey layoutKey{
		.textureDescriptorCapacity = descriptorCapacity,
		.shaderInterfaceRevision = 9,
		.pushConstantBytes = 0,
		.descriptorFeatureFlags = 0,
	};
	OwnedRendererLayout nativeLayout(vulkan.context(), descriptorCapacity);
	const auto layoutPublication = storage.publishRendererLayout(layoutKey, nativeLayout.native(), 0);
	FLOWUI_CHECK(layoutPublication.ownershipTransferred);
	nativeLayout.releaseToStorage();
	const WindowDescriptorBundleDesc descriptorDesc{
		.window = 91u,
		.layout = layoutPublication.handle,
		.framesInFlight = 1,
		.descriptorCapacity = descriptorCapacity,
	};

	OwnedDescriptorPool firstPool(
		vulkan.context(), nativeLayout.globalsSetLayout(), nativeLayout.texturesSetLayout(), 1, descriptorCapacity);
	const WindowDescriptorBundleHandle firstBundle =
		storage.adoptWindowDescriptorBundle(descriptorDesc, firstPool.native());
	firstPool.releaseToStorage();

	const auto firstFrame = storage.beginFrame(91u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array textures{texture};
	const auto firstWrites = storage.prepareTextureBindings(firstFrame, textures);
	FLOWUI_CHECK(firstWrites.dirtyBindings.size() == 2);
	std::vector<DescriptorWriteRecord> firstApplied(
		firstWrites.dirtyBindings.begin(), firstWrites.dirtyBindings.end());
	storage.acknowledgeTextureBindings(firstFrame, firstApplied);
	FLOWUI_CHECK(storage.prepareTextureBindings(firstFrame, textures).dirtyBindings.empty());
	const std::array firstUse{
		ResourceUse{ResourceKind::WindowDescriptorBundle, firstBundle.packed()},
	};
	storage.trackUses(firstFrame, firstUse);
	const auto firstSubmission = storage.noteSubmission(storage.sealFrame(firstFrame));

	OwnedDescriptorPool replacementPool(
		vulkan.context(), nativeLayout.globalsSetLayout(), nativeLayout.texturesSetLayout(), 1, descriptorCapacity);
	const WindowDescriptorBundleHandle replacementBundle =
		storage.adoptWindowDescriptorBundle(descriptorDesc, replacementPool.native());
	replacementPool.releaseToStorage();
	FLOWUI_CHECK(replacementBundle != firstBundle);
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(firstBundle).descriptorPool == 0);
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(replacementBundle).descriptorPool != 0);
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::WindowDescriptorBundle, firstBundle.index, firstBundle.generation));
	storage.collect();
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(replacementBundle).descriptorPool != 0);

	storage.noteCompleted(firstSubmission);
	storage.collect();
	const auto replacementFrame = storage.beginFrame(
		91u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 2});
	const auto replacementWrites = storage.prepareTextureBindings(replacementFrame, textures);
	FLOWUI_CHECK(replacementWrites.dirtyBindings.size() == 2);
	std::vector<DescriptorWriteRecord> replacementApplied(
		replacementWrites.dirtyBindings.begin(), replacementWrites.dirtyBindings.end());
	storage.acknowledgeTextureBindings(replacementFrame, replacementApplied);
	const std::array replacementUse{
		ResourceUse{ResourceKind::WindowDescriptorBundle, replacementBundle.packed()},
	};
	storage.trackUses(replacementFrame, replacementUse);
	storage.releaseWindowDescriptorBundle(replacementBundle, 0);
	storage.releaseRendererLayout(layoutPublication.handle, 0);
	const auto replacementSubmission =
		storage.noteSubmission(storage.sealFrame(replacementFrame));
	FLOWUI_CHECK(storage.nativeWindowDescriptorBundle(replacementBundle).descriptorPool == 0);
	FLOWUI_CHECK(storage.nativeRendererLayout(layoutPublication.handle).pipelineLayout != 0);
	storage.noteCompleted(replacementSubmission);
	storage.collect();
	FLOWUI_CHECK(storage.nativeRendererLayout(layoutPublication.handle).pipelineLayout == 0);
	storage.unregisterWindow(91u, replacementSubmission.serial);
}

void testManagerRecordsAndFailureRollback(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(301u, windowDesc(1, 1));
	int destroyed = 0;
	ManagerRecordTestArgs sharedArgs{17, &destroyed};
	const ResourceKey sharedKey{
		.domain = ResourceDomain::Internal,
		.name = storage.intern("phase5 shared manager record"),
	};
	const uint64_t initialRevision = storage.managerSharedRevision();
	for (uint32_t checkpoint = 1; checkpoint <= 4; ++checkpoint) {
		storage.setManagerFailureCountdown(checkpoint);
		FLOWUI_CHECK_THROWS(storage.createManagerRecord(managerRecordDesc(sharedKey, sharedArgs)));
		FLOWUI_CHECK(!storage.findManagerRecord(sharedKey, ResourceKind::ManagerRoot));
		FLOWUI_CHECK(storage.managerSharedRevision() == initialRevision);
	}
	FLOWUI_CHECK(destroyed == 1); // the final injected failure occurs after construction

	storage.setManagerFailureCountdown(0);
	const ManagerRecordHandle shared = storage.createManagerRecord(managerRecordDesc(sharedKey, sharedArgs));
	FLOWUI_CHECK(shared);
	auto* sharedState = static_cast<ManagerRecordTestState*>(
		storage.managerRecordData(shared, ResourceKind::ManagerRoot));
	FLOWUI_CHECK(sharedState != nullptr && sharedState->value == 17);
	FLOWUI_CHECK(storage.managerSharedRevision() > initialRevision);

	const auto frame = storage.beginFrame(301u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const auto managerView = storage.managerFrameView(frame);
	FLOWUI_CHECK(managerView.sharedRevision == frame.managerSharedRevision);
	storage.cancelFrame(frame);
	storage.noteManagerMutation(301u);
	FLOWUI_CHECK(storage.managerWindowRevision(301u) > managerView.windowRevision);

	ManagerRecordTestArgs localArgs{29, &destroyed};
	const ResourceKey localKey{
		.domain = ResourceDomain::Viewport,
		.name = storage.intern("phase5 local manager record"),
		.window = 301u,
	};
	const ManagerRecordHandle local = storage.createManagerRecord(managerRecordDesc(localKey, localArgs));
	FLOWUI_CHECK(local);
	storage.releaseWindowManagerRecords(301u);
	FLOWUI_CHECK(!storage.managerRecordData(local, ResourceKind::ManagerRoot));
	FLOWUI_CHECK(destroyed == 2);
	FLOWUI_CHECK(storage.removeManagerRecord(sharedKey, ResourceKind::ManagerRoot));
	FLOWUI_CHECK(destroyed == 3);
	storage.unregisterWindow(301u, 0);
}

void testPersistentAlignedRecordsAndFailureRollback(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(401u, windowDesc(1, 1));
	PersistentRecordTestHeader::resetCounts();
	PersistentRecordTestPayload::resetCounts();
	int payloadDestructions = 0;

	const auto createStateRecord = [&](int value) {
		return createTypedPersistentRecord<
			PersistentRecordTestHeader,
			PersistentRecordTestPayload>(
			storage,
			PersistentRecordCreateInfo{
				.kind = ResourceKind::UiElementState,
				.window = 401u,
				.debugName = storage.intern("phase-b element state"),
			},
			[](PersistentRecordTestHeader& header, const auto& layout) noexcept {
				header.marker = 0xfeedbeefull;
				header.payloadOffset = layout.payloadOffset;
			},
			value,
			&payloadDestructions);
	};

	for (uint32_t checkpoint = 1; checkpoint <= 4; ++checkpoint) {
		storage.setRecordFailureCountdown(checkpoint);
		FLOWUI_CHECK_THROWS(createStateRecord(17));
		FLOWUI_CHECK(storage.resourceStats(ResourceKind::UiElementState).live == 0);
	}
	// Only checkpoint four reaches a fully constructed record before failing to
	// publish it; both objects must be rolled back exactly once.
	FLOWUI_CHECK(PersistentRecordTestHeader::constructions == 1);
	FLOWUI_CHECK(PersistentRecordTestHeader::destructions == 1);
	FLOWUI_CHECK(PersistentRecordTestPayload::constructions == 1);
	FLOWUI_CHECK(PersistentRecordTestPayload::destructions == 1);
	FLOWUI_CHECK(payloadDestructions == 1);

	storage.setRecordFailureCountdown(0);
	const PersistentRecordHandle first = createStateRecord(31);
	FLOWUI_CHECK(first);
	FLOWUI_CHECK(storage.validateHandle(
		ResourceKind::UiElementState, first.index, first.generation));
	auto firstView = typedPersistentRecord<
		PersistentRecordTestHeader,
		PersistentRecordTestPayload>(storage, first, ResourceKind::UiElementState);
	FLOWUI_CHECK(firstView);
	FLOWUI_CHECK(firstView.header->marker == 0xfeedbeefull);
	FLOWUI_CHECK(firstView.payload->value == 31);
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(firstView.payload) %
		alignof(PersistentRecordTestPayload) == 0);
	FLOWUI_CHECK(firstView.header->payloadOffset >= sizeof(PersistentRecordTestHeader));
	FLOWUI_CHECK(!storage.persistentRecord(first, ResourceKind::UiElementResources));

#if FLOW_UI_DEV_MODE
	const auto stateStats = storage.resourceStats(ResourceKind::UiElementState);
	FLOWUI_CHECK(stateStats.live == 1);
	FLOWUI_CHECK(stateStats.ready == 1);
	FLOWUI_CHECK(stateStats.liveBytes >=
		sizeof(PersistentRecordTestHeader) + sizeof(PersistentRecordTestPayload));
#endif

	FLOWUI_CHECK(storage.removePersistentRecord(first, ResourceKind::UiElementState));
	FLOWUI_CHECK(!storage.validateHandle(
		ResourceKind::UiElementState, first.index, first.generation));
	FLOWUI_CHECK(!storage.persistentRecord(first, ResourceKind::UiElementState));
	FLOWUI_CHECK(!storage.removePersistentRecord(first, ResourceKind::UiElementState));

	const PersistentRecordHandle replacement = createStateRecord(47);
	FLOWUI_CHECK(replacement.index == first.index);
	FLOWUI_CHECK(replacement.generation != first.generation);
	FLOWUI_CHECK(!storage.persistentRecord(first, ResourceKind::UiElementState));
	FLOWUI_CHECK(typedPersistentRecord<
		PersistentRecordTestHeader,
		PersistentRecordTestPayload>(
		storage, replacement, ResourceKind::UiElementState).payload->value == 47);

	const int headerConstructionsBeforeThrow = PersistentRecordTestHeader::constructions;
	const int headerDestructionsBeforeThrow = PersistentRecordTestHeader::destructions;
	FLOWUI_CHECK_THROWS((createTypedPersistentRecord<
		PersistentRecordTestHeader,
		ThrowingPersistentPayload>(
		storage,
		PersistentRecordCreateInfo{
			.kind = ResourceKind::UiElementState,
			.window = 401u,
		},
		[](PersistentRecordTestHeader&, const auto&) noexcept {})));
	FLOWUI_CHECK(PersistentRecordTestHeader::constructions == headerConstructionsBeforeThrow + 1);
	FLOWUI_CHECK(PersistentRecordTestHeader::destructions == headerDestructionsBeforeThrow + 1);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(storage.resourceStats(ResourceKind::UiElementState).live == 1);
#endif

	FLOWUI_CHECK_THROWS((createTypedPersistentRecord<
		PersistentRecordTestHeader,
		PersistentRecordTestPayload>(
		storage,
		PersistentRecordCreateInfo{.kind = ResourceKind::UiElementState},
		[](PersistentRecordTestHeader&, const auto&) noexcept {},
		1,
		&payloadDestructions)));
	FLOWUI_CHECK_THROWS((createTypedPersistentRecord<
		PersistentRecordTestHeader,
		PersistentRecordTestPayload>(
		storage,
		PersistentRecordCreateInfo{
			.kind = ResourceKind::UiElementResources,
			.window = 401u,
		},
		[](PersistentRecordTestHeader&, const auto&) noexcept {},
		1,
		&payloadDestructions)));

	const PersistentRecordHandle resources = createTypedPersistentRecord<
		PersistentRecordTestHeader,
		PersistentRecordTestPayload>(
		storage,
		PersistentRecordCreateInfo{
			.kind = ResourceKind::UiElementResources,
			.window = FlowUi::InvalidWindowId,
			.debugName = storage.intern("phase-b element resources"),
		},
		[](PersistentRecordTestHeader& header, const auto& layout) noexcept {
			header.payloadOffset = layout.payloadOffset;
		},
		73,
		&payloadDestructions);
	const auto resourceView = storage.persistentRecord(
		resources, ResourceKind::UiElementResources);
	FLOWUI_CHECK(resourceView);
	FLOWUI_CHECK(resourceView.window == FlowUi::InvalidWindowId);
	FLOWUI_CHECK(resourceView.kind == ResourceKind::UiElementResources);

	storage.releaseWindowPersistentRecords(401u);
	FLOWUI_CHECK(!storage.persistentRecord(replacement, ResourceKind::UiElementState));
	FLOWUI_CHECK(storage.persistentRecord(resources, ResourceKind::UiElementResources));
	FLOWUI_CHECK(storage.removePersistentRecord(resources, ResourceKind::UiElementResources));
	storage.unregisterWindow(401u, 0);
}

void testAnonymousTextureExactRetirement(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	ImageDesc imageDesc{};
	imageDesc.usage = ImageUsage::Sampled;
	imageDesc.format = PixelFormat::Rgba8Unorm;
	const ImageHandle image = storage.createImage(imageDesc);
	const ImageViewHandle view = storage.createImageView(image, ImageViewDesc{});
	const SamplerHandle sampler = storage.acquireSampler(SamplerDesc{});
	const ResourceKey fallbackKey{
		.domain = ResourceDomain::Internal,
		.name = storage.intern("phase5 fallback"),
	};
	const TextureHandle fallback = storage.publishTexture(
		fallbackKey, TextureViewDesc{.imageView = view, .sampler = sampler});
	storage.setFallbackTexture(fallback);
	const TextureHandle anonymous = storage.createAnonymousTexture(TextureViewDesc{
		.imageView = view, .sampler = sampler, .sourceWidth = 11, .sourceHeight = 7,
	});
	FLOWUI_CHECK(storage.textureMetadata(anonymous).sourceWidth == 11);

	storage.registerWindow(302u, windowDesc(1, 1));
	const auto frame = storage.beginFrame(302u, FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
	const std::array requested{anonymous};
	const auto prepared = storage.prepareTextureBindings(frame, requested);
	storage.acknowledgeTextureBindings(frame, prepared.dirtyBindings);
	const auto submission = storage.noteSubmission(storage.sealFrame(frame));
	storage.releaseAnonymousTexture(anonymous);
	storage.collect();
	FLOWUI_CHECK(!storage.textureRetirementComplete(anonymous));
	storage.noteCompleted(submission);
	storage.collect();
	FLOWUI_CHECK(storage.textureRetirementComplete(anonymous));
	storage.unregisterWindow(302u, submission.serial);
	storage.releaseImageView(view);
	storage.releaseSampler(sampler);
	storage.releaseImage(image);
}

void testTelemetryConfiguration(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowStorageSystem storage(vulkan.context());
	storage.initialize(testConfig());
	storage.registerWindow(41u, windowDesc(1, 1));
	const auto block = storage.allocatePersistent(32, alignof(std::max_align_t), MemoryClass::Persistent, 0);

#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(hasCapability(storage, StorageCapability::DevelopmentTelemetry));
	const auto stats = storage.stats();
	FLOWUI_CHECK(stats.cpuSoftBudgetBytes == testConfig().cpuSoftBudgetBytes);
	FLOWUI_CHECK(stats.gpuSoftBudgetBytes == testConfig().gpuSoftBudgetBytes);
	FLOWUI_CHECK(stats.windowCount == 1);
	FLOWUI_CHECK(stats.cpu[static_cast<size_t>(MemoryClass::Persistent)].liveBytes >= 32);
	const auto snapshot = storage.windowSnapshot(41u);
	FLOWUI_CHECK(snapshot.window == 41u);
	FLOWUI_CHECK(snapshot.framesInFlight == 1);
#else
	FLOWUI_CHECK(!hasCapability(storage, StorageCapability::DevelopmentTelemetry));
	const auto stats = storage.stats();
	FLOWUI_CHECK(stats.gpuLiveBytes == 0);
	FLOWUI_CHECK(stats.gpuRetiredBytes == 0);
	FLOWUI_CHECK(stats.cpuSoftBudgetBytes == 0);
	FLOWUI_CHECK(stats.gpuSoftBudgetBytes == 0);
	FLOWUI_CHECK(stats.windowCount == 0);
	for (const auto& memory : stats.cpu) {
		FLOWUI_CHECK(memory.reservedBytes == 0);
		FLOWUI_CHECK(memory.liveBytes == 0);
		FLOWUI_CHECK(memory.allocationCount == 0);
	}
	const auto resources = storage.resourceStats(ResourceKind::GpuBuffer);
	FLOWUI_CHECK(resources.kind == ResourceKind::GpuBuffer);
	FLOWUI_CHECK(resources.live == 0);
	FLOWUI_CHECK(resources.slots == 0);
#endif

	storage.releasePersistent(block);
	storage.unregisterWindow(41u, 0);
}

} // namespace

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		std::cout << "Using headless Vulkan device: " << vulkan.deviceName() << '\n';
		std::cout << "Host-visible non-coherent memory: "
			<< (vulkan.hasNonCoherentHostVisibleMemory() ? "available" : "not available") << '\n';

		FlowUi::test::Runner runner;
		runner.run("initialization and capabilities", [&] { testInitialization(vulkan); });
		runner.run("persistent pools, strings, and blobs", [&] { testPersistentStringsAndBlobs(vulkan); });
		runner.run("frame arenas, leases, and completion gating", [&] { testFrameArenasAndCompletion(vulkan); });
		runner.run("independent multi-window progress", [&] { testMultiWindowProgress(vulkan); });
		runner.run("direct and host-scratch buffer writes", [&] { testMappedWriteModes(vulkan); });
		runner.run("cold native image-view and sampler queries", [&] {
			testNativeImageViewAndSamplerQueries(vulkan);
		});
		runner.run("shared UI byte resource upload and retirement", [&] {
			testSharedUiByteResourceLifecycle(vulkan);
		});
		runner.run("frame-held resource lifetime and deduplication", [&] {
			testFrameHeldResourceLifetimes(vulkan);
		});
		runner.run("concurrent non-overlapping mapped writes", [&] {
			testConcurrentNonOverlappingWrites(vulkan);
		});
		runner.run("fallback and descriptor binding protocol", [&] { testTextureBindingProtocol(vulkan); });
		runner.run("window and frame-slot resource locality", [&] { testWindowAndFrameLocality(vulkan); });
		runner.run("typed renderer resource ownership", [&] { testRendererResourceOwnership(vulkan); });
		runner.run("descriptor bundle replacement", [&] { testDescriptorBundleReplacement(vulkan); });
		runner.run("manager records, revisions, destruction, and failure rollback", [&] {
			testManagerRecordsAndFailureRollback(vulkan);
		});
		runner.run("persistent aligned records, generations, and failure rollback", [&] {
			testPersistentAlignedRecordsAndFailureRollback(vulkan);
		});
		runner.run("anonymous logical texture exact retirement", [&] {
			testAnonymousTextureExactRetirement(vulkan);
		});
		runner.run("development and production telemetry contract", [&] { testTelemetryConfiguration(vulkan); });
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
#else
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
#endif
	}
}
