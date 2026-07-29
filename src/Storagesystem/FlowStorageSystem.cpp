#include "internal/StorageSystem/FlowStorageSystem.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include "Vulkan/Vk_Context.hpp"
#include "internal/Vma.hpp"

namespace FlowUi::detail::storage {
namespace {

constexpr uint64_t kCapabilityMask =
	static_cast<uint64_t>(StorageCapability::RuntimeGrowth) |
	static_cast<uint64_t>(StorageCapability::WindowScopes) |
	static_cast<uint64_t>(StorageCapability::WorkerArenas) |
	static_cast<uint64_t>(StorageCapability::GenerationalHandles) |
	static_cast<uint64_t>(StorageCapability::BatchedBindingResolution) |
	static_cast<uint64_t>(StorageCapability::SynchronousUploads) |
	static_cast<uint64_t>(StorageCapability::DeferredRetirement) |
	static_cast<uint64_t>(StorageCapability::VulkanInterop);

[[noreturn]] void storageError(const char* message) {
	throw std::runtime_error(std::string("FlowStorageSystem: ") + message);
}

void checkVk(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		storageError(message);
	}
}

size_t alignUp(size_t value, size_t alignment) {
	if (alignment == 0 || !std::has_single_bit(alignment)) {
		storageError("alignment must be a non-zero power of two");
	}
	if (value > std::numeric_limits<size_t>::max() - (alignment - 1u)) {
		storageError("allocation alignment overflow");
	}
	return (value + alignment - 1u) & ~(alignment - 1u);
}

uint32_t nextGeneration(uint32_t generation) noexcept {
	++generation;
	return generation == InvalidGeneration ? 1u : generation;
}

uint64_t nextCapacity(uint64_t current, uint64_t required, float factor) {
	uint64_t capacity = std::max<uint64_t>(current, 1u);
	const double clampedFactor = std::max(1.1, static_cast<double>(factor));
	while (capacity < required) {
		const uint64_t grown = static_cast<uint64_t>(std::ceil(static_cast<double>(capacity) * clampedFactor));
		if (grown <= capacity) {
			return required;
		}
		capacity = grown;
	}
	return capacity;
}

VkFormat toVkFormat(PixelFormat format) {
	switch (format) {
	case PixelFormat::R8Unorm: return VK_FORMAT_R8_UNORM;
	case PixelFormat::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
	case PixelFormat::Rgba8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
	case PixelFormat::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
	case PixelFormat::Bgra8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
	case PixelFormat::Rgba16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case PixelFormat::D32Float: return VK_FORMAT_D32_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

VkImageUsageFlags toVkImageUsage(ImageUsage usage) {
	VkImageUsageFlags flags = 0;
	if (hasFlag(usage, ImageUsage::Sampled)) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (hasFlag(usage, ImageUsage::TransferSource)) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (hasFlag(usage, ImageUsage::TransferDestination)) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (hasFlag(usage, ImageUsage::ColorAttachment)) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (hasFlag(usage, ImageUsage::DepthAttachment)) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (hasFlag(usage, ImageUsage::Storage)) flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	return flags;
}

VkBufferUsageFlags toVkBufferUsage(BufferUsage usage) {
	VkBufferUsageFlags flags = 0;
	if (hasFlag(usage, BufferUsage::TransferSource)) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (hasFlag(usage, BufferUsage::TransferDestination)) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (hasFlag(usage, BufferUsage::Vertex)) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Index)) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Uniform)) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Storage)) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Indirect)) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	return flags;
}

VmaMemoryUsage toVmaMemoryUsage(MemoryPreference preference) {
	switch (preference) {
	case MemoryPreference::DeviceLocal: return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	case MemoryPreference::HostVisible: return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	case MemoryPreference::HostCached: return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	default: return VMA_MEMORY_USAGE_AUTO;
	}
}

VkFilter toVkFilter(FilterMode mode) {
	return mode == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerAddressMode toVkAddressMode(AddressMode mode) {
	switch (mode) {
	case AddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case AddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
}

uint32_t bytesPerPixel(PixelFormat format) {
	switch (format) {
	case PixelFormat::R8Unorm: return 1u;
	case PixelFormat::Rgba16Float: return 8u;
	case PixelFormat::D32Float: return 4u;
	case PixelFormat::Rgba8Unorm:
	case PixelFormat::Rgba8Srgb:
	case PixelFormat::Bgra8Unorm:
	case PixelFormat::Bgra8Srgb: return 4u;
	default: return 0u;
	}
}

uint64_t estimateImageBytes(const ImageDesc& desc) {
	uint64_t bytes = 0;
	uint64_t width = desc.width;
	uint64_t height = desc.height;
	uint64_t depth = desc.depth;
	for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
		bytes += width * height * depth * desc.layers * bytesPerPixel(desc.format);
		width = std::max<uint64_t>(1, width / 2u);
		height = std::max<uint64_t>(1, height / 2u);
		depth = std::max<uint64_t>(1, depth / 2u);
	}
	return bytes;
}

template <typename Pointer>
uint64_t nativeHandle(Pointer pointer) noexcept {
	if constexpr (std::is_pointer_v<Pointer>) {
		return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
	} else {
		return static_cast<uint64_t>(pointer);
	}
}

struct StringViewHash {
	using is_transparent = void;
	size_t operator()(std::string_view value) const noexcept {
		return std::hash<std::string_view>{}(value);
	}
};

struct StringViewEqual {
	using is_transparent = void;
	bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
};

struct SamplerKey {
	FilterMode minFilter{};
	FilterMode magFilter{};
	AddressMode addressU{};
	AddressMode addressV{};
	AddressMode addressW{};
	uint32_t minLodBits = 0;
	uint32_t maxLodBits = 0;
	uint32_t anisotropyBits = 0;
	bool anisotropy = false;
	auto operator<=>(const SamplerKey&) const = default;
};

struct SamplerKeyHash {
	size_t operator()(const SamplerKey& key) const noexcept {
		uint64_t hash = static_cast<uint64_t>(key.minFilter) | (static_cast<uint64_t>(key.magFilter) << 8u);
		hash ^= static_cast<uint64_t>(key.addressU) << 16u;
		hash ^= static_cast<uint64_t>(key.addressV) << 24u;
		hash ^= static_cast<uint64_t>(key.addressW) << 32u;
		hash ^= static_cast<uint64_t>(key.minLodBits) * 0x9e3779b185ebca87ull;
		hash ^= static_cast<uint64_t>(key.maxLodBits) * 0xc2b2ae3d27d4eb4full;
		hash ^= static_cast<uint64_t>(key.anisotropyBits) << 1u;
		hash ^= static_cast<uint64_t>(key.anisotropy) << 63u;
		return static_cast<size_t>(hash ^ (hash >> 32u));
	}
};

SamplerKey makeSamplerKey(const SamplerDesc& desc) noexcept {
	return SamplerKey{
		.minFilter = desc.minFilter,
		.magFilter = desc.magFilter,
		.addressU = desc.addressU,
		.addressV = desc.addressV,
		.addressW = desc.addressW,
		.minLodBits = std::bit_cast<uint32_t>(desc.minLod),
		.maxLodBits = std::bit_cast<uint32_t>(desc.maxLod),
		.anisotropyBits = std::bit_cast<uint32_t>(desc.maxAnisotropy),
		.anisotropy = desc.anisotropy,
	};
}

class LinearArena {
public:
	void initialize(size_t initialBytes, float growthFactor, bool allowGrowth) {
		growthFactor_ = growthFactor;
		allowGrowth_ = allowGrowth;
		if (initialBytes > 0 && pages_.empty()) {
			addPage(initialBytes);
		}
	}

	void* allocate(size_t bytes, size_t alignment) {
		if (bytes == 0) {
			return nullptr;
		}
		for (size_t attempt = currentPage_; attempt < pages_.size(); ++attempt) {
			Page& page = pages_[attempt];
			const size_t offset = alignUp(page.offset, alignment);
			if (offset <= page.capacity && bytes <= page.capacity - offset) {
				page.offset = offset + bytes;
				currentPage_ = attempt;
				liveBytes_ += bytes;
				highWaterBytes_ = std::max(highWaterBytes_, liveBytes_);
				return page.memory.get() + offset;
			}
		}

		if (!allowGrowth_) {
			storageError("transient arena exhausted and runtime growth is disabled");
		}
		const size_t base = pages_.empty() ? bytes : pages_.back().capacity;
		const size_t capacity = static_cast<size_t>(nextCapacity(base, bytes + alignment, growthFactor_));
		addPage(capacity);
		++growthCount_;
		return allocate(bytes, alignment);
	}

	void reset() noexcept {
		for (Page& page : pages_) {
			page.offset = 0;
		}
		currentPage_ = 0;
		liveBytes_ = 0;
	}

	void trimOverflow() {
		if (pages_.size() > 1) {
			pages_.erase(pages_.begin() + 1, pages_.end());
		}
		currentPage_ = 0;
	}

	[[nodiscard]] uint64_t capacity() const noexcept {
		uint64_t result = 0;
		for (const Page& page : pages_) result += page.capacity;
		return result;
	}
	[[nodiscard]] uint64_t highWater() const noexcept { return highWaterBytes_; }
	[[nodiscard]] uint64_t growthCount() const noexcept { return growthCount_; }

	static void* arenaAllocate(void* context, size_t bytes, size_t alignment) {
		return static_cast<LinearArena*>(context)->allocate(bytes, alignment);
	}

private:
	struct Page {
		std::unique_ptr<std::byte[]> memory;
		size_t capacity = 0;
		size_t offset = 0;
	};

	void addPage(size_t capacity) {
		Page page{};
		page.memory = std::make_unique<std::byte[]>(capacity);
		page.capacity = capacity;
		pages_.push_back(std::move(page));
	}

	std::vector<Page> pages_;
	size_t currentPage_ = 0;
	uint64_t liveBytes_ = 0;
	uint64_t highWaterBytes_ = 0;
	uint64_t growthCount_ = 0;
	float growthFactor_ = 1.5f;
	bool allowGrowth_ = true;
};

class PersistentPool {
public:
	void initialize(size_t initialBytes, float growthFactor, bool allowGrowth) {
		growthFactor_ = growthFactor;
		allowGrowth_ = allowGrowth;
		if (initialBytes > 0 && slabs_.empty()) addSlab(initialBytes);
	}

	MemoryBlock allocate(size_t bytes, size_t alignment, AllocationTag tag) {
		if (bytes == 0) return {};
		for (uint32_t slabIndex = 0; slabIndex < slabs_.size(); ++slabIndex) {
			Slab& slab = slabs_[slabIndex];
			for (size_t blockIndex = 0; blockIndex < slab.free.size(); ++blockIndex) {
				FreeBlock block = slab.free[blockIndex];
				const size_t alignedOffset = alignUp(block.offset, alignment);
				const size_t prefix = alignedOffset - block.offset;
				if (prefix > block.size || bytes > block.size - prefix) continue;

				slab.free.erase(slab.free.begin() + static_cast<std::ptrdiff_t>(blockIndex));
				if (prefix > 0) slab.free.push_back({block.offset, prefix});
				const size_t consumed = prefix + bytes;
				if (consumed < block.size) slab.free.push_back({alignedOffset + bytes, block.size - consumed});

				const AllocationId id = nextId_++;
				allocations_.emplace(id, Allocation{slabIndex, alignedOffset, bytes, tag});
				liveBytes_ += bytes;
				peakLiveBytes_ = std::max(peakLiveBytes_, liveBytes_);
				return MemoryBlock{slab.memory.get() + alignedOffset, bytes, id, tag};
			}
		}

		if (!allowGrowth_) storageError("persistent pool exhausted and runtime growth is disabled");
		const size_t base = slabs_.empty() ? bytes : slabs_.back().capacity;
		addSlab(static_cast<size_t>(nextCapacity(base, bytes + alignment, growthFactor_)));
		++growthCount_;
		return allocate(bytes, alignment, tag);
	}

	void release(const MemoryBlock& block) noexcept {
		auto it = allocations_.find(block.id);
		if (it == allocations_.end()) return;
		const Allocation allocation = it->second;
		if (allocation.slab < slabs_.size()) {
			Slab& slab = slabs_[allocation.slab];
			slab.free.push_back({allocation.offset, allocation.size});
			merge(slab.free);
		}
		liveBytes_ -= std::min<uint64_t>(liveBytes_, allocation.size);
		allocations_.erase(it);
	}

	[[nodiscard]] uint64_t reservedBytes() const noexcept {
		uint64_t result = 0;
		for (const Slab& slab : slabs_) result += slab.capacity;
		return result;
	}
	[[nodiscard]] uint64_t liveBytes() const noexcept { return liveBytes_; }
	[[nodiscard]] uint64_t peakLiveBytes() const noexcept { return peakLiveBytes_; }
	[[nodiscard]] uint64_t allocationCount() const noexcept { return allocations_.size(); }
	[[nodiscard]] uint64_t growthCount() const noexcept { return growthCount_; }

private:
	struct FreeBlock { size_t offset = 0; size_t size = 0; };
	struct Slab {
		std::unique_ptr<std::byte[]> memory;
		size_t capacity = 0;
		std::vector<FreeBlock> free;
	};
	struct Allocation {
		uint32_t slab = 0;
		size_t offset = 0;
		size_t size = 0;
		AllocationTag tag{};
	};

	void addSlab(size_t capacity) {
		Slab slab{};
		slab.memory = std::make_unique<std::byte[]>(capacity);
		slab.capacity = capacity;
		slab.free.push_back({0, capacity});
		slabs_.push_back(std::move(slab));
	}

	static void merge(std::vector<FreeBlock>& free) noexcept {
		std::sort(free.begin(), free.end(), [](const FreeBlock& lhs, const FreeBlock& rhs) {
			return lhs.offset < rhs.offset;
		});
		size_t output = 0;
		for (const FreeBlock block : free) {
			if (output > 0 && free[output - 1].offset + free[output - 1].size == block.offset) {
				free[output - 1].size += block.size;
			} else {
				free[output++] = block;
			}
		}
		free.resize(output);
	}

	std::vector<Slab> slabs_;
	std::unordered_map<AllocationId, Allocation> allocations_;
	AllocationId nextId_ = 1;
	uint64_t liveBytes_ = 0;
	uint64_t peakLiveBytes_ = 0;
	uint64_t growthCount_ = 0;
	float growthFactor_ = 1.5f;
	bool allowGrowth_ = true;
};

struct UsedResource {
	ResourceKind kind = ResourceKind::Invalid;
	uint64_t packedHandle = 0;
	auto operator<=>(const UsedResource&) const = default;
};

} // namespace

struct FlowStorageSystem::Impl {
	explicit Impl(VulkanContext& context) : vk(context) {}

	struct BlobRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		MemoryBlock memory{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		StringId debugName = 0;
	};

	struct BufferRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		void* mapped = nullptr;
		uint64_t size = 0;
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		BufferDesc desc{};
	};

	struct ImageRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		uint64_t byteSize = 0;
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		ImageDesc desc{};
	};

	struct ImageViewRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		VkImageView view = VK_NULL_HANDLE;
		ImageHandle image{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		ImageViewDesc desc{};
	};

	struct SamplerRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		VkSampler sampler = VK_NULL_HANDLE;
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		SamplerDesc desc{};
		SamplerKey key{};
	};

	struct TextureColdRecord {
		ResourceKey key{};
		TextureViewDesc desc{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
	};

	struct UploadRecord {
		UploadTicket ticket{};
		UploadRequest request{};
		ResourceState state = ResourceState::Queued;
	};

	struct RetirementRecord {
		RetirementRequest request{};
		uint64_t byteSize = 0;
	};

	struct FrameState {
		FrameEpoch epoch = 0;
		uint64_t frameNumber = 0;
		bool active = false;
		bool sealed = false;
		LinearArena transient;
		LinearArena decode;
		std::vector<std::unique_ptr<LinearArena>> workers;
		std::vector<UsedResource> used;
	};

	struct WindowState {
		WindowId id = 0;
		WindowStorageDesc desc{};
		std::vector<std::unique_ptr<FrameState>> frames;
		std::vector<BindingHotRecord> bindingsByTextureIndex;
		std::vector<uint32_t> freeDescriptorIndices;
		uint32_t nextDescriptorIndex = 1;
		uint32_t bindingRevision = 0;
		uint32_t liveBindings = 0;
		uint32_t retiredBindings = 0;
		bool closing = false;
		SubmissionSerial retireAfter = 0;
	};

	VulkanContext& vk;
	StorageConfig config{};
	bool initialized = false;
	mutable std::recursive_mutex mutex;

	PersistentPool persistentPool;
	PersistentPool stringPool;
	std::vector<std::string_view> strings{std::string_view{}};
	std::unordered_map<std::string_view, StringId, StringViewHash, StringViewEqual> stringIds;

	std::vector<BlobRecord> blobs{BlobRecord{}};
	std::vector<uint32_t> freeBlobs;
	std::vector<BufferRecord> buffers{BufferRecord{}};
	std::vector<uint32_t> freeBuffers;
	std::vector<ImageRecord> images{ImageRecord{}};
	std::vector<uint32_t> freeImages;
	std::vector<ImageViewRecord> imageViews{ImageViewRecord{}};
	std::vector<ImageViewHotRecord> imageViewHot{ImageViewHotRecord{}};
	std::vector<uint32_t> freeImageViews;
	std::vector<SamplerRecord> samplers{SamplerRecord{}};
	std::vector<SamplerHotRecord> samplerHot{SamplerHotRecord{}};
	std::vector<uint32_t> freeSamplers;
	std::unordered_map<SamplerKey, SamplerHandle, SamplerKeyHash> samplerByKey;
	std::vector<TextureHotRecord> textureHot{TextureHotRecord{.generation = 1, .revision = 1, .state = ResourceState::Ready}};
	std::vector<TextureColdRecord> textureCold{TextureColdRecord{}};
	std::vector<uint32_t> freeTextures;
	std::unordered_map<ResourceKey, TextureHandle, ResourceKeyHash> textureByKey;

	std::unordered_map<WindowId, std::unique_ptr<WindowState>> windows;
	std::deque<UploadRecord> uploads;
	std::unordered_map<UploadId, ResourceState> uploadStates;
	std::deque<RetirementRecord> retirements;

	VkCommandPool uploadCommandPool = VK_NULL_HANDLE;
	UploadId nextUploadId = 1;
	FrameEpoch nextFrameEpoch = 1;
	SubmissionSerial nextSubmissionSerial = 1;
	SubmissionSerial completedWatermark = 0;
	std::unordered_set<SubmissionSerial> completedOutOfOrder;
	StorageStats telemetry{};

	template <typename Record>
	static uint32_t acquireIndex(std::vector<Record>& records, std::vector<uint32_t>& freeIndices) {
		if (!freeIndices.empty()) {
			const uint32_t index = freeIndices.back();
			freeIndices.pop_back();
			return index;
		}
		if (records.size() >= std::numeric_limits<uint32_t>::max()) storageError("resource handle table exhausted");
		records.emplace_back();
		return static_cast<uint32_t>(records.size() - 1u);
	}

	uint32_t acquireImageViewIndex() {
		const uint32_t index = acquireIndex(imageViews, freeImageViews);
		if (imageViewHot.size() <= index) imageViewHot.resize(static_cast<size_t>(index) + 1u);
		return index;
	}

	uint32_t acquireSamplerIndex() {
		const uint32_t index = acquireIndex(samplers, freeSamplers);
		if (samplerHot.size() <= index) samplerHot.resize(static_cast<size_t>(index) + 1u);
		return index;
	}

	uint32_t acquireTextureIndex() {
		uint32_t index = 0;
		if (!freeTextures.empty()) {
			index = freeTextures.back();
			freeTextures.pop_back();
		} else {
			if (textureHot.size() >= std::numeric_limits<uint32_t>::max()) storageError("texture handle table exhausted");
			index = static_cast<uint32_t>(textureHot.size());
			textureHot.emplace_back();
			textureCold.emplace_back();
		}
		return index;
	}

	void requireInitialized() const {
		if (!initialized) storageError("system is not initialized");
	}

	[[nodiscard]] bool hasSealedFrames() const noexcept {
		for (const auto& [_, window] : windows) {
			for (const auto& frame : window->frames) {
				if (frame->active && frame->sealed) return true;
			}
		}
		return false;
	}

	void requireResourceMutationPhase() const {
		if (hasSealedFrames()) {
			storageError("resource-table mutation is not allowed while a sealed frame snapshot is active");
		}
	}

	WindowState& requireWindow(WindowId id) {
		auto it = windows.find(id);
		if (it == windows.end() || it->second->closing) storageError("window storage scope was not found");
		return *it->second;
	}

	const WindowState& requireWindow(WindowId id) const {
		auto it = windows.find(id);
		if (it == windows.end() || it->second->closing) storageError("window storage scope was not found");
		return *it->second;
	}

	FrameState& requireFrame(const FrameToken& token, bool allowSealed = true) {
		WindowState& window = requireWindow(token.window);
		if (token.frameSlot >= window.frames.size()) storageError("frame slot is out of range");
		FrameState& frame = *window.frames[token.frameSlot];
		if (!frame.active || frame.epoch != token.epoch) storageError("frame token is stale or inactive");
		if (!allowSealed && frame.sealed) storageError("frame storage is sealed");
		return frame;
	}

	const FrameState& requireFrame(const FrameToken& token) const {
		const WindowState& window = requireWindow(token.window);
		if (token.frameSlot >= window.frames.size()) storageError("frame slot is out of range");
		const FrameState& frame = *window.frames[token.frameSlot];
		if (!frame.active || frame.epoch != token.epoch) storageError("frame token is stale or inactive");
		return frame;
	}

	void addUse(FrameState& frame, ResourceKind kind, uint64_t packed) {
		if (packed == 0) return;
		const UsedResource used{kind, packed};
		if (std::find(frame.used.begin(), frame.used.end(), used) == frame.used.end()) frame.used.push_back(used);
	}

	void noteInvalidHandle() noexcept { ++telemetry.invalidHandleCount; }

	void updateGpuPeak() noexcept {
		telemetry.gpuPeakBytes = std::max(
			telemetry.gpuPeakBytes,
			telemetry.gpuLiveBytes + telemetry.gpuRetiredBytes);
	}

	void enqueueRetirement(RetirementKind kind, uint64_t packed, SubmissionSerial serial, uint64_t bytes = 0) {
		retirements.push_back(RetirementRecord{
			.request = RetirementRequest{kind, packed, serial},
			.byteSize = bytes,
		});
		telemetry.gpuRetiredBytes += bytes;
		updateGpuPeak();
	}

	void retainImage(ImageHandle handle) {
		if (!validImage(handle)) storageError("cannot retain invalid image handle");
		++images[handle.index].referenceCount;
	}

	void retainBlob(BlobHandle handle) {
		if (!validBlob(handle)) storageError("cannot retain invalid blob handle");
		++blobs[handle.index].referenceCount;
	}

	void retainBuffer(BufferHandle handle) {
		if (!validBuffer(handle)) storageError("cannot retain invalid buffer handle");
		++buffers[handle.index].referenceCount;
	}

	void retainImageView(ImageViewHandle handle) {
		if (!validImageView(handle)) storageError("cannot retain invalid image-view handle");
		++imageViews[handle.index].referenceCount;
	}

	void retainSampler(SamplerHandle handle) {
		if (!validSampler(handle)) storageError("cannot retain invalid sampler handle");
		++samplers[handle.index].referenceCount;
	}

	void refreshTexturesForImage(ImageHandle image) {
		if (!validImage(image)) return;
		const ResourceState imageState = images[image.index].state;
		for (size_t textureIndex = 1; textureIndex < textureHot.size(); ++textureIndex) {
			TextureHotRecord& hot = textureHot[textureIndex];
			if (hot.state == ResourceState::Invalid || hot.imageViewIndex >= imageViews.size()) continue;
			const ImageViewRecord& view = imageViews[hot.imageViewIndex];
			if (view.generation != hot.imageViewGeneration || view.image != image) continue;
			if (hot.state != imageState) {
				hot.state = imageState;
				hot.revision = nextGeneration(hot.revision);
			}
		}
	}

	bool validBlob(BlobHandle handle) const noexcept {
		return handle && handle.index < blobs.size() && blobs[handle.index].generation == handle.generation &&
			blobs[handle.index].state != ResourceState::Invalid;
	}
	bool validBuffer(BufferHandle handle) const noexcept {
		return handle && handle.index < buffers.size() && buffers[handle.index].generation == handle.generation &&
			buffers[handle.index].state != ResourceState::Invalid;
	}
	bool validImage(ImageHandle handle) const noexcept {
		return handle && handle.index < images.size() && images[handle.index].generation == handle.generation &&
			images[handle.index].state != ResourceState::Invalid;
	}
	bool validImageView(ImageViewHandle handle) const noexcept {
		return handle && handle.index < imageViews.size() && imageViews[handle.index].generation == handle.generation &&
			imageViews[handle.index].state != ResourceState::Invalid;
	}
	bool validSampler(SamplerHandle handle) const noexcept {
		return handle && handle.index < samplers.size() && samplers[handle.index].generation == handle.generation &&
			samplers[handle.index].state != ResourceState::Invalid;
	}
	bool validTexture(TextureHandle handle) const noexcept {
		return handle && handle.index < textureHot.size() && textureHot[handle.index].generation == handle.generation &&
			textureHot[handle.index].state != ResourceState::Invalid;
	}

	void stampUse(const UsedResource& used, SubmissionSerial serial) {
		switch (used.kind) {
		case ResourceKind::GpuBuffer: {
			const BufferHandle handle = BufferHandle::fromPacked(used.packedHandle);
			if (validBuffer(handle)) buffers[handle.index].lastUse = std::max(buffers[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::GpuImage: {
			const ImageHandle handle = ImageHandle::fromPacked(used.packedHandle);
			if (validImage(handle)) images[handle.index].lastUse = std::max(images[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::ImageView: {
			const ImageViewHandle handle = ImageViewHandle::fromPacked(used.packedHandle);
			if (validImageView(handle)) imageViews[handle.index].lastUse = std::max(imageViews[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::Sampler: {
			const SamplerHandle handle = SamplerHandle::fromPacked(used.packedHandle);
			if (validSampler(handle)) samplers[handle.index].lastUse = std::max(samplers[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::TextureView: {
			const TextureHandle handle = TextureHandle::fromPacked(used.packedHandle);
			if (validTexture(handle)) {
				TextureColdRecord& cold = textureCold[handle.index];
				cold.lastUse = std::max(cold.lastUse, serial);
				stampUse({ResourceKind::ImageView, cold.desc.imageView.packed()}, serial);
				stampUse({ResourceKind::Sampler, cold.desc.sampler.packed()}, serial);
			}
			break;
		}
		default: break;
		}
	}

	ResolvedTextureBinding resolve(WindowState& window, FrameState& frame, TextureHandle texture) {
		if (!validTexture(texture)) {
			noteInvalidHandle();
			return {};
		}
		if (window.bindingsByTextureIndex.size() <= texture.index) {
			window.bindingsByTextureIndex.resize(static_cast<size_t>(texture.index) + 1u);
		}

		const TextureHotRecord& hot = textureHot[texture.index];
		BindingHotRecord& binding = window.bindingsByTextureIndex[texture.index];
		const bool hit = binding.textureGeneration == texture.generation &&
			binding.textureRevision == hot.revision && binding.descriptorIndex != 0;
		if (hit) {
			++telemetry.bindingCacheHits;
		} else {
			++telemetry.bindingCacheMisses;
			if (binding.descriptorIndex == 0) {
				if (!window.freeDescriptorIndices.empty()) {
					binding.descriptorIndex = window.freeDescriptorIndices.back();
					window.freeDescriptorIndices.pop_back();
				} else {
					binding.descriptorIndex = window.nextDescriptorIndex++;
				}
				++window.liveBindings;
			}

			binding.textureGeneration = texture.generation;
			binding.textureRevision = hot.revision;
			binding.bindingRevision = ++window.bindingRevision;
			binding.state = hot.state;
			binding.nativeImageView = 0;
			binding.nativeSampler = 0;
			if (hot.state == ResourceState::Ready && hot.imageViewIndex < imageViewHot.size()) {
				const ImageViewHotRecord& view = imageViewHot[hot.imageViewIndex];
				if (view.generation == hot.imageViewGeneration) binding.nativeImageView = view.nativeImageView;
			}
			if (hot.state == ResourceState::Ready && hot.samplerIndex < samplerHot.size()) {
				const SamplerHotRecord& sampler = samplerHot[hot.samplerIndex];
				if (sampler.generation == hot.samplerGeneration) binding.nativeSampler = sampler.nativeSampler;
			}
		}

		addUse(frame, ResourceKind::TextureView, texture.packed());
		return ResolvedTextureBinding{
			.descriptorIndex = binding.descriptorIndex,
			.bindingRevision = binding.bindingRevision,
			.state = binding.state,
			.nativeImageView = binding.nativeImageView,
			.nativeSampler = binding.nativeSampler,
		};
	}

	void releaseImageReference(ImageHandle handle, SubmissionSerial lastUse) {
		if (!validImage(handle)) return;
		ImageRecord& record = images[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount > 0) --record.referenceCount;
		if (record.referenceCount == 0 && record.state != ResourceState::Retiring) {
			record.state = ResourceState::Retiring;
			enqueueRetirement(RetirementKind::Image, handle.packed(), record.lastUse, record.byteSize);
		}
	}

	void releaseImageViewReference(ImageViewHandle handle, SubmissionSerial lastUse) {
		if (!validImageView(handle)) return;
		ImageViewRecord& record = imageViews[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount > 0) --record.referenceCount;
		if (record.referenceCount == 0 && record.state != ResourceState::Retiring) {
			record.state = ResourceState::Retiring;
			enqueueRetirement(RetirementKind::ImageView, handle.packed(), record.lastUse);
		}
	}

	void releaseSamplerReference(SamplerHandle handle, SubmissionSerial lastUse) {
		if (!validSampler(handle)) return;
		SamplerRecord& record = samplers[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount > 0) --record.referenceCount;
		if (record.referenceCount == 0 && record.state != ResourceState::Retiring) {
			record.state = ResourceState::Retiring;
			samplerByKey.erase(record.key);
			enqueueRetirement(RetirementKind::Sampler, handle.packed(), record.lastUse);
		}
	}

	void destroyRetired(const RetirementRecord& retired) {
		const RetirementRequest& request = retired.request;
		switch (request.kind) {
		case RetirementKind::Blob: {
			const BlobHandle handle = BlobHandle::fromPacked(request.packedHandle);
			if (handle.index >= blobs.size()) break;
			BlobRecord& record = blobs[handle.index];
			if (record.generation != handle.generation) break;
			persistentPool.release(record.memory);
			record.memory = {};
			record.state = ResourceState::Invalid;
			record.generation = nextGeneration(record.generation);
			freeBlobs.push_back(handle.index);
			break;
		}
		case RetirementKind::Buffer: {
			const BufferHandle handle = BufferHandle::fromPacked(request.packedHandle);
			if (handle.index >= buffers.size()) break;
			BufferRecord& record = buffers[handle.index];
			if (record.generation != handle.generation) break;
			if (record.buffer != VK_NULL_HANDLE) vmaDestroyBuffer(vk.allocator, record.buffer, record.allocation);
			telemetry.gpuLiveBytes -= std::min(telemetry.gpuLiveBytes, record.size);
			record = BufferRecord{.generation = nextGeneration(record.generation)};
			freeBuffers.push_back(handle.index);
			break;
		}
		case RetirementKind::Image: {
			const ImageHandle handle = ImageHandle::fromPacked(request.packedHandle);
			if (handle.index >= images.size()) break;
			ImageRecord& record = images[handle.index];
			if (record.generation != handle.generation) break;
			if (record.image != VK_NULL_HANDLE) vmaDestroyImage(vk.allocator, record.image, record.allocation);
			telemetry.gpuLiveBytes -= std::min(telemetry.gpuLiveBytes, record.byteSize);
			record = ImageRecord{.generation = nextGeneration(record.generation)};
			freeImages.push_back(handle.index);
			break;
		}
		case RetirementKind::ImageView: {
			const ImageViewHandle handle = ImageViewHandle::fromPacked(request.packedHandle);
			if (handle.index >= imageViews.size()) break;
			ImageViewRecord& record = imageViews[handle.index];
			if (record.generation != handle.generation) break;
			const ImageHandle image = record.image;
			if (record.view != VK_NULL_HANDLE) vkDestroyImageView(vk.device, record.view, nullptr);
			record = ImageViewRecord{.generation = nextGeneration(record.generation)};
			imageViewHot[handle.index] = ImageViewHotRecord{.generation = record.generation};
			freeImageViews.push_back(handle.index);
			releaseImageReference(image, request.retireAfter);
			break;
		}
		case RetirementKind::Sampler: {
			const SamplerHandle handle = SamplerHandle::fromPacked(request.packedHandle);
			if (handle.index >= samplers.size()) break;
			SamplerRecord& record = samplers[handle.index];
			if (record.generation != handle.generation) break;
			if (record.sampler != VK_NULL_HANDLE) vkDestroySampler(vk.device, record.sampler, nullptr);
			record = SamplerRecord{.generation = nextGeneration(record.generation)};
			samplerHot[handle.index] = SamplerHotRecord{.generation = record.generation};
			freeSamplers.push_back(handle.index);
			break;
		}
		case RetirementKind::Texture: {
			const TextureHandle handle = TextureHandle::fromPacked(request.packedHandle);
			if (handle.index >= textureHot.size()) break;
			TextureHotRecord& hot = textureHot[handle.index];
			if (hot.generation != handle.generation) break;
			TextureColdRecord& cold = textureCold[handle.index];
			const ImageViewHandle view = cold.desc.imageView;
			const SamplerHandle sampler = cold.desc.sampler;
			hot = TextureHotRecord{.generation = nextGeneration(hot.generation)};
			cold = {};
			freeTextures.push_back(handle.index);
			releaseImageViewReference(view, request.retireAfter);
			releaseSamplerReference(sampler, request.retireAfter);
			for (auto& [_, window] : windows) {
				if (window->bindingsByTextureIndex.size() <= handle.index) continue;
				BindingHotRecord& binding = window->bindingsByTextureIndex[handle.index];
				if (binding.descriptorIndex != 0) {
					window->freeDescriptorIndices.push_back(binding.descriptorIndex);
					if (window->liveBindings > 0) --window->liveBindings;
					if (window->retiredBindings > 0) --window->retiredBindings;
				}
				binding = {};
			}
			break;
		}
		}
		telemetry.gpuRetiredBytes -= std::min(telemetry.gpuRetiredBytes, retired.byteSize);
	}

	void immediateDestroyAll() noexcept {
		try {
			for (size_t i = 1; i < imageViews.size(); ++i) {
				if (imageViews[i].view != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE)
					vkDestroyImageView(vk.device, imageViews[i].view, nullptr);
			}
			for (size_t i = 1; i < samplers.size(); ++i) {
				if (samplers[i].sampler != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE)
					vkDestroySampler(vk.device, samplers[i].sampler, nullptr);
			}
			if (vk.allocator) {
				for (size_t i = 1; i < buffers.size(); ++i) {
					if (buffers[i].buffer != VK_NULL_HANDLE) vmaDestroyBuffer(vk.allocator, buffers[i].buffer, buffers[i].allocation);
				}
				for (size_t i = 1; i < images.size(); ++i) {
					if (images[i].image != VK_NULL_HANDLE) vmaDestroyImage(vk.allocator, images[i].image, images[i].allocation);
				}
			}
			if (uploadCommandPool != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE)
				vkDestroyCommandPool(vk.device, uploadCommandPool, nullptr);
		} catch (...) {
		}
		uploadCommandPool = VK_NULL_HANDLE;
	}
};

FlowStorageSystem::FlowStorageSystem(VulkanContext& vulkanContext)
	: impl_(std::make_unique<Impl>(vulkanContext)) {}

FlowStorageSystem::~FlowStorageSystem() {
	shutdown();
}

void FlowStorageSystem::initialize(const StorageConfig& config) {
	std::scoped_lock lock(impl_->mutex);
	if (impl_->initialized) storageError("initialize called more than once");
	if (impl_->vk.device == VK_NULL_HANDLE || impl_->vk.allocator == nullptr) {
		storageError("a valid Vulkan device and VMA allocator are required");
	}
	if (config.growthFactor <= 1.0f) storageError("growthFactor must be greater than one");

	impl_->config = config;
	impl_->config.framesInFlight = std::max(1u, config.framesInFlight);
	impl_->config.expectedWorkerCount = std::max(1u, config.expectedWorkerCount);
	impl_->persistentPool.initialize(
		static_cast<size_t>(config.initialPersistentCpuBytes), config.growthFactor, config.allowRuntimeGrowth);
	impl_->stringPool.initialize(
		static_cast<size_t>(config.initialStringBytes), config.growthFactor, config.allowRuntimeGrowth);
	impl_->blobs.reserve(static_cast<size_t>(config.expectedBlobs) + 1u);
	impl_->buffers.reserve(static_cast<size_t>(config.expectedBuffers) + 1u);
	impl_->images.reserve(static_cast<size_t>(config.expectedImages) + 1u);
	impl_->imageViews.reserve(static_cast<size_t>(config.expectedImageViews) + 1u);
	impl_->imageViewHot.reserve(static_cast<size_t>(config.expectedImageViews) + 1u);
	impl_->samplers.reserve(static_cast<size_t>(config.expectedSamplers) + 1u);
	impl_->samplerHot.reserve(static_cast<size_t>(config.expectedSamplers) + 1u);
	impl_->textureHot.reserve(static_cast<size_t>(config.expectedTextureViews) + 1u);
	impl_->textureCold.reserve(static_cast<size_t>(config.expectedTextureViews) + 1u);
	impl_->windows.reserve(config.expectedWindows);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = impl_->vk.graphicsQFamily;
	checkVk(vkCreateCommandPool(impl_->vk.device, &poolInfo, nullptr, &impl_->uploadCommandPool),
		"failed to create central upload command pool");

	impl_->telemetry.cpuSoftBudgetBytes = config.cpuSoftBudgetBytes;
	impl_->telemetry.gpuSoftBudgetBytes = config.gpuSoftBudgetBytes;
	impl_->initialized = true;
}

void FlowStorageSystem::shutdown() noexcept {
	if (!impl_) return;
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->initialized) return;
	if (impl_->vk.device != VK_NULL_HANDLE) vkDeviceWaitIdle(impl_->vk.device);
	impl_->immediateDestroyAll();
	impl_->uploads.clear();
	impl_->uploadStates.clear();
	impl_->retirements.clear();
	impl_->windows.clear();
	impl_->textureByKey.clear();
	impl_->samplerByKey.clear();
	impl_->initialized = false;
}

uint32_t FlowStorageSystem::interfaceVersion() const noexcept { return CurrentInterfaceVersion; }
uint64_t FlowStorageSystem::capabilities() const noexcept { return kCapabilityMask; }

void FlowStorageSystem::registerWindow(WindowId id, const WindowStorageDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (id == 0) storageError("window id zero is reserved");
	if (impl_->windows.contains(id)) storageError("window id is already registered");

	auto window = std::make_unique<Impl::WindowState>();
	window->id = id;
	window->desc = desc;
	window->desc.framesInFlight = std::max(1u, desc.framesInFlight);
	window->desc.workerCount = std::max(1u, desc.workerCount);
	window->bindingsByTextureIndex.reserve(std::max<size_t>(desc.initialTextureBindings, impl_->textureHot.size()));
	window->bindingsByTextureIndex.resize(impl_->textureHot.size());
	window->frames.reserve(window->desc.framesInFlight);
	for (uint32_t slot = 0; slot < window->desc.framesInFlight; ++slot) {
		auto frame = std::make_unique<Impl::FrameState>();
		frame->transient.initialize(static_cast<size_t>(desc.transientBytesPerFrame), impl_->config.growthFactor,
			impl_->config.allowRuntimeGrowth);
		frame->decode.initialize(static_cast<size_t>(impl_->config.initialDecodeScratchBytes), impl_->config.growthFactor,
			impl_->config.allowRuntimeGrowth);
		frame->workers.reserve(window->desc.workerCount);
		for (uint32_t worker = 0; worker < window->desc.workerCount; ++worker) {
			auto arena = std::make_unique<LinearArena>();
			arena->initialize(static_cast<size_t>(desc.transientBytesPerWorker), impl_->config.growthFactor,
				impl_->config.allowRuntimeGrowth);
			frame->workers.push_back(std::move(arena));
		}
		window->frames.push_back(std::move(frame));
	}
	impl_->windows.emplace(id, std::move(window));
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
}

void FlowStorageSystem::unregisterWindow(WindowId id, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	auto it = impl_->windows.find(id);
	if (it == impl_->windows.end()) return;
	if (lastUse <= impl_->completedWatermark) {
		impl_->windows.erase(it);
	} else {
		it->second->closing = true;
		it->second->retireAfter = lastUse;
	}
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
}

FrameToken FlowStorageSystem::beginFrame(WindowId id, const FrameStorageDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	Impl::WindowState& window = impl_->requireWindow(id);
	if (desc.frameSlot >= window.frames.size()) storageError("beginFrame frame slot is out of range");
	Impl::FrameState& frame = *window.frames[desc.frameSlot];
	if (frame.active) storageError("frame slot is already active; completion/reuse protocol was violated");
	frame.transient.reset();
	frame.decode.reset();
	for (const auto& worker : frame.workers) worker->reset();
	frame.used.clear();
	frame.frameNumber = desc.frameNumber;
	frame.epoch = impl_->nextFrameEpoch++;
	if (frame.epoch == 0) frame.epoch = impl_->nextFrameEpoch++;
	frame.active = true;
	frame.sealed = false;
	return FrameToken{id, desc.frameSlot, desc.frameNumber, frame.epoch};
}

void FlowStorageSystem::sealFrame(const FrameToken& frame) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireFrame(frame, false).sealed = true;
}

void FlowStorageSystem::cancelFrame(const FrameToken& frame) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		Impl::FrameState& state = impl_->requireFrame(frame);
		state.used.clear();
		state.active = false;
		state.sealed = false;
	} catch (...) {
	}
}

MemoryBlock FlowStorageSystem::allocatePersistent(
	size_t bytes, size_t alignment, MemoryClass memoryClass, StringId debugName) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (memoryClass == MemoryClass::FrameTransient || memoryClass == MemoryClass::WorkerTransient ||
		memoryClass == MemoryClass::DecodeTransient) {
		storageError("transient memory classes must use an arena view");
	}
	PersistentPool& pool = memoryClass == MemoryClass::StringPool ? impl_->stringPool : impl_->persistentPool;
	return pool.allocate(bytes, alignment, AllocationTag{memoryClass, ResourceKind::Invalid, 0, InvalidFrameSlot, debugName});
}

void FlowStorageSystem::releasePersistent(MemoryBlock block) noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (block.tag.memoryClass == MemoryClass::StringPool) impl_->stringPool.release(block);
	else impl_->persistentPool.release(block);
}

ArenaView FlowStorageSystem::frameArena(const FrameToken& frame, MemoryClass memoryClass) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	LinearArena* arena = nullptr;
	if (memoryClass == MemoryClass::FrameTransient) arena = &state.transient;
	else if (memoryClass == MemoryClass::DecodeTransient || memoryClass == MemoryClass::UploadStaging) arena = &state.decode;
	else storageError("requested memory class is not available from a frame arena");
	return ArenaView{arena, &LinearArena::arenaAllocate, frame.epoch};
}

ArenaView FlowStorageSystem::workerArena(const FrameToken& frame, uint32_t workerIndex) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	if (workerIndex >= state.workers.size()) storageError("worker arena index is out of range");
	return ArenaView{state.workers[workerIndex].get(), &LinearArena::arenaAllocate, frame.epoch};
}

StringId FlowStorageSystem::intern(std::string_view value) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (value.empty()) return 0;
	if (const auto it = impl_->stringIds.find(value); it != impl_->stringIds.end()) return it->second;
	MemoryBlock block = impl_->stringPool.allocate(
		value.size() + 1u, alignof(char), AllocationTag{MemoryClass::StringPool, ResourceKind::Invalid, 0, InvalidFrameSlot, 0});
	auto* chars = static_cast<char*>(block.data);
	std::memcpy(chars, value.data(), value.size());
	chars[value.size()] = '\0';
	const std::string_view stable(chars, value.size());
	if (impl_->strings.size() >= std::numeric_limits<StringId>::max()) storageError("string id table exhausted");
	const StringId id = static_cast<StringId>(impl_->strings.size());
	impl_->strings.push_back(stable);
	impl_->stringIds.emplace(stable, id);
	return id;
}

std::string_view FlowStorageSystem::string(StringId id) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return id < impl_->strings.size() ? impl_->strings[id] : std::string_view{};
}

BlobHandle FlowStorageSystem::createBlob(std::span<const std::byte> bytes, StringId debugName) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	const uint32_t index = Impl::acquireIndex(impl_->blobs, impl_->freeBlobs);
	Impl::BlobRecord& record = impl_->blobs[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.state = ResourceState::Ready;
	record.referenceCount = 1;
	record.debugName = debugName;
	record.memory = impl_->persistentPool.allocate(bytes.size(), alignof(std::max_align_t),
		AllocationTag{MemoryClass::Persistent, ResourceKind::CpuBlob, 0, InvalidFrameSlot, debugName});
	if (!bytes.empty()) std::memcpy(record.memory.data, bytes.data(), bytes.size());
	return BlobHandle{index, record.generation};
}

std::span<const std::byte> FlowStorageSystem::readBlob(BlobHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validBlob(handle)) return {};
	const Impl::BlobRecord& record = impl_->blobs[handle.index];
	return {static_cast<const std::byte*>(record.memory.data), record.memory.size};
}

void FlowStorageSystem::releaseBlob(BlobHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validBlob(handle)) return;
	Impl::BlobRecord& record = impl_->blobs[handle.index];
	record.lastUse = std::max(record.lastUse, lastUse);
	if (record.referenceCount > 0) --record.referenceCount;
	if (record.referenceCount == 0 && record.state != ResourceState::Retiring) {
		record.state = ResourceState::Retiring;
		impl_->enqueueRetirement(RetirementKind::Blob, handle.packed(), record.lastUse);
	}
}

BufferHandle FlowStorageSystem::createBuffer(const BufferDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (desc.size == 0 || toVkBufferUsage(desc.usage) == 0) storageError("invalid buffer description");
	if (impl_->telemetry.gpuLiveBytes + impl_->telemetry.gpuRetiredBytes + desc.size > impl_->telemetry.gpuSoftBudgetBytes)
		storageError("GPU soft budget exceeded while creating buffer");

	const uint32_t index = Impl::acquireIndex(impl_->buffers, impl_->freeBuffers);
	Impl::BufferRecord& record = impl_->buffers[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.desc = desc;
	record.size = desc.size;
	record.referenceCount = 1;
	record.state = ResourceState::Queued;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = desc.size;
	bufferInfo.usage = toVkBufferUsage(desc.usage);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = toVmaMemoryUsage(desc.memory);
	if (desc.persistentlyMapped) {
		allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	VmaAllocationInfo resultInfo{};
	if (vmaCreateBuffer(impl_->vk.allocator, &bufferInfo, &allocationInfo, &record.buffer, &record.allocation, &resultInfo) != VK_SUCCESS) {
		record.state = ResourceState::Invalid;
		impl_->freeBuffers.push_back(index);
		storageError("failed to allocate Vulkan buffer");
	}
	record.mapped = resultInfo.pMappedData;
	record.state = ResourceState::Ready;
	impl_->telemetry.gpuLiveBytes += record.size;
	impl_->updateGpuPeak();
	return BufferHandle{index, record.generation};
}

ImageHandle FlowStorageSystem::createImage(const ImageDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	const VkFormat format = toVkFormat(desc.format);
	if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.layers == 0 || desc.mipLevels == 0 ||
		format == VK_FORMAT_UNDEFINED || toVkImageUsage(desc.usage) == 0) storageError("invalid image description");
	const uint64_t estimatedBytes = estimateImageBytes(desc);
	if (impl_->telemetry.gpuLiveBytes + impl_->telemetry.gpuRetiredBytes + estimatedBytes > impl_->telemetry.gpuSoftBudgetBytes)
		storageError("GPU soft budget exceeded while creating image");

	const uint32_t index = Impl::acquireIndex(impl_->images, impl_->freeImages);
	Impl::ImageRecord& record = impl_->images[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.desc = desc;
	record.referenceCount = 1;
	record.byteSize = estimatedBytes;
	record.state = ResourceState::Queued;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent = VkExtent3D{desc.width, desc.height, desc.depth};
	imageInfo.mipLevels = desc.mipLevels;
	imageInfo.arrayLayers = desc.layers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = toVkImageUsage(desc.usage);
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = toVmaMemoryUsage(desc.memory);
	VmaAllocationInfo resultInfo{};
	if (vmaCreateImage(impl_->vk.allocator, &imageInfo, &allocationInfo, &record.image, &record.allocation, &resultInfo) != VK_SUCCESS) {
		record.state = ResourceState::Invalid;
		impl_->freeImages.push_back(index);
		storageError("failed to allocate Vulkan image");
	}
	if (resultInfo.size > 0) record.byteSize = resultInfo.size;
	record.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	record.state = hasFlag(desc.usage, ImageUsage::TransferDestination) ? ResourceState::Queued : ResourceState::Ready;
	impl_->telemetry.gpuLiveBytes += record.byteSize;
	impl_->updateGpuPeak();
	return ImageHandle{index, record.generation};
}

ImageViewHandle FlowStorageSystem::createImageView(ImageHandle image, const ImageViewDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireResourceMutationPhase();
	if (!impl_->validImage(image)) storageError("createImageView received an invalid image handle");
	Impl::ImageRecord& imageRecord = impl_->images[image.index];
	if (desc.baseMipLevel + desc.mipLevelCount > imageRecord.desc.mipLevels ||
		desc.baseArrayLayer + desc.arrayLayerCount > imageRecord.desc.layers) storageError("image-view range exceeds image");

	const uint32_t index = impl_->acquireImageViewIndex();
	Impl::ImageViewRecord& record = impl_->imageViews[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.image = image;
	record.desc = desc;
	record.referenceCount = 1;
	record.state = ResourceState::Queued;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = imageRecord.image;
	viewInfo.viewType = desc.type == ImageType::Image2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = desc.format == PixelFormat::Undefined ? toVkFormat(imageRecord.desc.format) : toVkFormat(desc.format);
	viewInfo.subresourceRange.aspectMask = desc.depthAspect ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = desc.baseMipLevel;
	viewInfo.subresourceRange.levelCount = desc.mipLevelCount;
	viewInfo.subresourceRange.baseArrayLayer = desc.baseArrayLayer;
	viewInfo.subresourceRange.layerCount = desc.arrayLayerCount;
	checkVk(vkCreateImageView(impl_->vk.device, &viewInfo, nullptr, &record.view), "failed to create Vulkan image view");
	record.state = ResourceState::Ready;
	impl_->retainImage(image);
	impl_->imageViewHot[index] = ImageViewHotRecord{
		.generation = record.generation,
		.imageIndex = image.index,
		.imageGeneration = image.generation,
		.nativeImageView = nativeHandle(record.view),
	};
	return ImageViewHandle{index, record.generation};
}

SamplerHandle FlowStorageSystem::acquireSampler(const SamplerDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireResourceMutationPhase();
	const SamplerKey key = makeSamplerKey(desc);
	if (const auto found = impl_->samplerByKey.find(key); found != impl_->samplerByKey.end() && impl_->validSampler(found->second)) {
		impl_->retainSampler(found->second);
		return found->second;
	}

	const uint32_t index = impl_->acquireSamplerIndex();
	Impl::SamplerRecord& record = impl_->samplers[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.desc = desc;
	record.key = key;
	record.referenceCount = 1;
	record.state = ResourceState::Queued;

	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.minFilter = toVkFilter(desc.minFilter);
	info.magFilter = toVkFilter(desc.magFilter);
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	info.addressModeU = toVkAddressMode(desc.addressU);
	info.addressModeV = toVkAddressMode(desc.addressV);
	info.addressModeW = toVkAddressMode(desc.addressW);
	info.minLod = desc.minLod;
	info.maxLod = desc.maxLod;
	info.anisotropyEnable = desc.anisotropy ? VK_TRUE : VK_FALSE;
	info.maxAnisotropy = std::max(1.0f, desc.maxAnisotropy);
	info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	checkVk(vkCreateSampler(impl_->vk.device, &info, nullptr, &record.sampler), "failed to create Vulkan sampler");
	record.state = ResourceState::Ready;
	const SamplerHandle handle{index, record.generation};
	impl_->samplerByKey.emplace(key, handle);
	impl_->samplerHot[index] = SamplerHotRecord{record.generation, 0, nativeHandle(record.sampler)};
	return handle;
}

void FlowStorageSystem::releaseBuffer(BufferHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validBuffer(handle)) return;
	Impl::BufferRecord& record = impl_->buffers[handle.index];
	record.lastUse = std::max(record.lastUse, lastUse);
	if (record.referenceCount > 0) --record.referenceCount;
	if (record.referenceCount == 0 && record.state != ResourceState::Retiring) {
		record.state = ResourceState::Retiring;
		impl_->enqueueRetirement(RetirementKind::Buffer, handle.packed(), record.lastUse, record.size);
	}
}

void FlowStorageSystem::releaseImage(ImageHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseImageReference(handle, lastUse);
}

void FlowStorageSystem::releaseImageView(ImageViewHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	impl_->releaseImageViewReference(handle, lastUse);
}

void FlowStorageSystem::releaseSampler(SamplerHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	impl_->releaseSamplerReference(handle, lastUse);
}

TextureHandle FlowStorageSystem::publishTexture(ResourceKey key, const TextureViewDesc& desc, bool* inserted) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireResourceMutationPhase();
	if (!impl_->validImageView(desc.imageView) || !impl_->validSampler(desc.sampler))
		storageError("texture publication requires valid image-view and sampler handles");
	if (const auto found = impl_->textureByKey.find(key); found != impl_->textureByKey.end()) {
		if (inserted) *inserted = false;
		return replaceTexture(key, desc);
	}

	const uint32_t index = impl_->acquireTextureIndex();
	TextureHotRecord& hot = impl_->textureHot[index];
	hot.generation = hot.generation == 0 ? 1u : hot.generation;
	hot.revision = 1u;
	hot.imageViewIndex = desc.imageView.index;
	hot.imageViewGeneration = desc.imageView.generation;
	hot.samplerIndex = desc.sampler.index;
	hot.samplerGeneration = desc.sampler.generation;
	const ImageHandle backingImage = impl_->imageViews[desc.imageView.index].image;
	hot.state = impl_->validImage(backingImage) ? impl_->images[backingImage.index].state : ResourceState::Invalid;
	hot.sourceWidth = desc.sourceWidth;
	hot.sourceHeight = desc.sourceHeight;
	impl_->textureCold[index] = Impl::TextureColdRecord{key, desc, 1u, 0};
	impl_->retainImageView(desc.imageView);
	impl_->retainSampler(desc.sampler);
	const TextureHandle handle{index, hot.generation};
	impl_->textureByKey.emplace(key, handle);
	if (inserted) *inserted = true;
	return handle;
}

TextureHandle FlowStorageSystem::replaceTexture(ResourceKey key, const TextureViewDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireResourceMutationPhase();
	if (!impl_->validImageView(desc.imageView) || !impl_->validSampler(desc.sampler))
		storageError("texture replacement requires valid image-view and sampler handles");
	const auto found = impl_->textureByKey.find(key);
	if (found == impl_->textureByKey.end()) return publishTexture(key, desc, nullptr);
	const TextureHandle handle = found->second;
	if (!impl_->validTexture(handle)) storageError("texture key points to an invalid handle");

	TextureHotRecord& hot = impl_->textureHot[handle.index];
	Impl::TextureColdRecord& cold = impl_->textureCold[handle.index];
	const ImageViewHandle oldView = cold.desc.imageView;
	const SamplerHandle oldSampler = cold.desc.sampler;
	const SubmissionSerial oldLastUse = cold.lastUse;
	impl_->retainImageView(desc.imageView);
	impl_->retainSampler(desc.sampler);
	cold.desc = desc;
	hot.revision = nextGeneration(hot.revision);
	hot.imageViewIndex = desc.imageView.index;
	hot.imageViewGeneration = desc.imageView.generation;
	hot.samplerIndex = desc.sampler.index;
	hot.samplerGeneration = desc.sampler.generation;
	const ImageHandle backingImage = impl_->imageViews[desc.imageView.index].image;
	hot.state = impl_->validImage(backingImage) ? impl_->images[backingImage.index].state : ResourceState::Invalid;
	hot.sourceWidth = desc.sourceWidth;
	hot.sourceHeight = desc.sourceHeight;
	impl_->releaseImageViewReference(oldView, oldLastUse);
	impl_->releaseSamplerReference(oldSampler, oldLastUse);
	return handle;
}

bool FlowStorageSystem::removeTexture(ResourceKey key, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	const auto found = impl_->textureByKey.find(key);
	if (found == impl_->textureByKey.end()) return false;
	const TextureHandle handle = found->second;
	impl_->textureByKey.erase(found);
	if (!impl_->validTexture(handle)) return false;
	TextureHotRecord& hot = impl_->textureHot[handle.index];
	Impl::TextureColdRecord& cold = impl_->textureCold[handle.index];
	cold.lastUse = std::max(cold.lastUse, lastUse);
	hot.state = ResourceState::Retiring;
	for (auto& [_, window] : impl_->windows) {
		if (window->bindingsByTextureIndex.size() > handle.index) {
			const BindingHotRecord& binding = window->bindingsByTextureIndex[handle.index];
			if (binding.textureGeneration == handle.generation && binding.descriptorIndex != 0)
				++window->retiredBindings;
		}
	}
	impl_->enqueueRetirement(RetirementKind::Texture, handle.packed(), cold.lastUse);
	return true;
}

TextureHandle FlowStorageSystem::findTexture(ResourceKey key) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	const auto found = impl_->textureByKey.find(key);
	return found == impl_->textureByKey.end() ? TextureHandle{} : found->second;
}

TextureMetadata FlowStorageSystem::textureMetadata(TextureHandle texture) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validTexture(texture)) return {};
	const TextureHotRecord& hot = impl_->textureHot[texture.index];
	return TextureMetadata{hot.state, hot.sourceWidth, hot.sourceHeight, hot.revision};
}

void FlowStorageSystem::prepareTextureBindings(const FrameToken& frame, std::span<const TextureHandle> textures) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	Impl::WindowState& window = impl_->requireWindow(frame.window);
	for (TextureHandle texture : textures) impl_->resolve(window, state, texture);
}

ResolvedTextureBinding FlowStorageSystem::resolveTexture(const FrameToken& frame, TextureHandle texture) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	Impl::WindowState& window = impl_->requireWindow(frame.window);
	return impl_->resolve(window, state, texture);
}

void FlowStorageSystem::trackUse(const FrameToken& frame, BufferHandle buffer) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validBuffer(buffer)) storageError("trackUse received an invalid buffer");
	impl_->addUse(impl_->requireFrame(frame), ResourceKind::GpuBuffer, buffer.packed());
}

void FlowStorageSystem::trackUse(const FrameToken& frame, ImageHandle image) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validImage(image)) storageError("trackUse received an invalid image");
	impl_->addUse(impl_->requireFrame(frame), ResourceKind::GpuImage, image.packed());
}

void FlowStorageSystem::invalidateWindowBindings(WindowId id, TextureHandle texture) {
	std::scoped_lock lock(impl_->mutex);
	Impl::WindowState& window = impl_->requireWindow(id);
	if (texture.index >= window.bindingsByTextureIndex.size()) return;
	BindingHotRecord& binding = window.bindingsByTextureIndex[texture.index];
	if (binding.textureGeneration == texture.generation) {
		binding.textureRevision = 0;
		binding.bindingRevision = ++window.bindingRevision;
	}
}

StorageReadView FlowStorageSystem::readView(const FrameToken& frame) const {
	std::scoped_lock lock(impl_->mutex);
	const Impl::FrameState& state = impl_->requireFrame(frame);
	if (!state.sealed) storageError("storage read view requires a sealed frame");
	return StorageReadView{impl_->textureHot, impl_->imageViewHot, impl_->samplerHot, frame.epoch};
}

WindowBindingView FlowStorageSystem::windowBindingView(const FrameToken& frame) const {
	std::scoped_lock lock(impl_->mutex);
	const Impl::FrameState& state = impl_->requireFrame(frame);
	if (!state.sealed) storageError("window binding view requires a sealed frame");
	const Impl::WindowState& window = impl_->requireWindow(frame.window);
	return WindowBindingView{window.bindingsByTextureIndex, frame.epoch};
}

WindowStorageSnapshot FlowStorageSystem::windowSnapshot(WindowId id) const {
	std::scoped_lock lock(impl_->mutex);
	const Impl::WindowState& window = impl_->requireWindow(id);
	WindowStorageSnapshot result{};
	result.window = id;
	result.framesInFlight = static_cast<uint32_t>(window.frames.size());
	result.bindingCapacity = static_cast<uint32_t>(window.bindingsByTextureIndex.capacity());
	result.liveBindings = window.liveBindings;
	result.retiredBindings = window.retiredBindings;
	for (const auto& frame : window.frames) {
		result.transientCapacityBytes += frame->transient.capacity() + frame->decode.capacity();
		result.transientHighWaterBytes += frame->transient.highWater() + frame->decode.highWater();
		for (const auto& worker : frame->workers) {
			result.transientCapacityBytes += worker->capacity();
			result.transientHighWaterBytes += worker->highWater();
		}
	}
	return result;
}

UploadTicket FlowStorageSystem::enqueueUpload(const UploadRequest& request) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (!impl_->validBlob(request.source)) storageError("upload source blob is invalid");
	const auto bytes = readBlob(request.source);
	if (request.sourceOffset > bytes.size() || request.byteCount > bytes.size() - request.sourceOffset)
		storageError("upload source range exceeds blob");
	if (request.destination == UploadDestination::Buffer && !impl_->validBuffer(request.destinationBuffer))
		storageError("upload destination buffer is invalid");
	if (request.destination == UploadDestination::Image && !impl_->validImage(request.destinationImage))
		storageError("upload destination image is invalid");
	impl_->retainBlob(request.source);
	if (request.destination == UploadDestination::Buffer) impl_->retainBuffer(request.destinationBuffer);
	else impl_->retainImage(request.destinationImage);
	const UploadTicket ticket{impl_->nextUploadId++};
	impl_->uploads.push_back(Impl::UploadRecord{ticket, request, ResourceState::Queued});
	impl_->uploadStates[ticket.value] = ResourceState::Queued;
	impl_->telemetry.queuedUploadBytes += request.byteCount;
	return ticket;
}

ResourceState FlowStorageSystem::uploadState(UploadTicket ticket) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	const auto found = impl_->uploadStates.find(ticket.value);
	return found == impl_->uploadStates.end() ? ResourceState::Invalid : found->second;
}

void FlowStorageSystem::flushUploads() {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireResourceMutationPhase();
	while (!impl_->uploads.empty()) {
		Impl::UploadRecord upload = impl_->uploads.front();
		impl_->uploads.pop_front();
		impl_->uploadStates[upload.ticket.value] = ResourceState::Uploading;
		const std::span<const std::byte> source = readBlob(upload.request.source);
		const void* sourceData = source.data() + upload.request.sourceOffset;
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAllocation = nullptr;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		try {
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = upload.request.byteCount;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VmaAllocationCreateInfo allocationInfo{};
			allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo resultInfo{};
			checkVk(vmaCreateBuffer(impl_->vk.allocator, &bufferInfo, &allocationInfo,
				&stagingBuffer, &stagingAllocation, &resultInfo), "failed to create upload staging buffer");
			std::memcpy(resultInfo.pMappedData, sourceData, static_cast<size_t>(upload.request.byteCount));
			checkVk(vmaFlushAllocation(impl_->vk.allocator, stagingAllocation, 0, upload.request.byteCount),
				"failed to flush upload staging allocation");

			VkCommandBufferAllocateInfo commandAlloc{};
			commandAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandAlloc.commandPool = impl_->uploadCommandPool;
			commandAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandAlloc.commandBufferCount = 1;
			checkVk(vkAllocateCommandBuffers(impl_->vk.device, &commandAlloc, &commandBuffer),
				"failed to allocate upload command buffer");
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin upload command buffer");

			if (upload.request.destination == UploadDestination::Buffer) {
				Impl::BufferRecord& destination = impl_->buffers[upload.request.destinationBuffer.index];
				if (upload.request.destinationBufferOffset + upload.request.byteCount > destination.size)
					storageError("upload exceeds destination buffer");
				VkBufferCopy copy{};
				copy.srcOffset = 0;
				copy.dstOffset = upload.request.destinationBufferOffset;
				copy.size = upload.request.byteCount;
				vkCmdCopyBuffer(commandBuffer, stagingBuffer, destination.buffer, 1, &copy);
				destination.state = upload.request.finalState;
			} else {
				Impl::ImageRecord& destination = impl_->images[upload.request.destinationImage.index];
				VkImageMemoryBarrier toTransfer{};
				toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				toTransfer.oldLayout = destination.layout;
				toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toTransfer.image = destination.image;
				toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				toTransfer.subresourceRange.baseMipLevel = upload.request.imageRegion.mipLevel;
				toTransfer.subresourceRange.levelCount = 1;
				toTransfer.subresourceRange.baseArrayLayer = upload.request.imageRegion.baseArrayLayer;
				toTransfer.subresourceRange.layerCount = upload.request.imageRegion.layerCount;
				toTransfer.srcAccessMask = 0;
				toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &toTransfer);

				VkBufferImageCopy copy{};
				copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copy.imageSubresource.mipLevel = upload.request.imageRegion.mipLevel;
				copy.imageSubresource.baseArrayLayer = upload.request.imageRegion.baseArrayLayer;
				copy.imageSubresource.layerCount = upload.request.imageRegion.layerCount;
				copy.imageOffset = VkOffset3D{
					static_cast<int32_t>(upload.request.imageRegion.x),
					static_cast<int32_t>(upload.request.imageRegion.y),
					static_cast<int32_t>(upload.request.imageRegion.z)};
				copy.imageExtent = VkExtent3D{upload.request.imageRegion.width, upload.request.imageRegion.height,
					upload.request.imageRegion.depth};
				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, destination.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

				VkImageMemoryBarrier toRead{};
				toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				toRead.image = destination.image;
				toRead.subresourceRange = toTransfer.subresourceRange;
				toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &toRead);
				destination.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				destination.state = upload.request.finalState;
			}

			checkVk(vkEndCommandBuffer(commandBuffer), "failed to end upload command buffer");
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			checkVk(vkQueueSubmit(impl_->vk.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE), "failed to submit upload");
			checkVk(vkQueueWaitIdle(impl_->vk.graphicsQ), "failed waiting for synchronous upload");
			vkFreeCommandBuffers(impl_->vk.device, impl_->uploadCommandPool, 1, &commandBuffer);
			commandBuffer = VK_NULL_HANDLE;
			vmaDestroyBuffer(impl_->vk.allocator, stagingBuffer, stagingAllocation);
			stagingBuffer = VK_NULL_HANDLE;
			impl_->uploadStates[upload.ticket.value] = ResourceState::Ready;
			impl_->telemetry.queuedUploadBytes -= std::min(impl_->telemetry.queuedUploadBytes, upload.request.byteCount);
			impl_->telemetry.completedUploadBytes += upload.request.byteCount;
			if (upload.request.destination == UploadDestination::Buffer) {
				releaseBuffer(upload.request.destinationBuffer, 0);
			} else {
				impl_->refreshTexturesForImage(upload.request.destinationImage);
				releaseImage(upload.request.destinationImage, 0);
			}
			releaseBlob(upload.request.source, 0);
			if (upload.request.releaseSourceWhenComplete) releaseBlob(upload.request.source, 0);
		} catch (...) {
			if (commandBuffer != VK_NULL_HANDLE)
				vkFreeCommandBuffers(impl_->vk.device, impl_->uploadCommandPool, 1, &commandBuffer);
			if (stagingBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(impl_->vk.allocator, stagingBuffer, stagingAllocation);
			impl_->uploadStates[upload.ticket.value] = ResourceState::Failed;
			if (upload.request.destination == UploadDestination::Buffer && impl_->validBuffer(upload.request.destinationBuffer))
				impl_->buffers[upload.request.destinationBuffer.index].state = ResourceState::Failed;
			if (upload.request.destination == UploadDestination::Image && impl_->validImage(upload.request.destinationImage))
				impl_->images[upload.request.destinationImage.index].state = ResourceState::Failed;
			if (upload.request.destination == UploadDestination::Buffer) releaseBuffer(upload.request.destinationBuffer, 0);
			else {
				impl_->refreshTexturesForImage(upload.request.destinationImage);
				releaseImage(upload.request.destinationImage, 0);
			}
			releaseBlob(upload.request.source, 0);
			throw;
		}
	}
}

SubmissionToken FlowStorageSystem::noteSubmission(WindowId id, uint32_t frameSlot) {
	std::scoped_lock lock(impl_->mutex);
	Impl::WindowState& window = impl_->requireWindow(id);
	if (frameSlot >= window.frames.size()) storageError("submission frame slot is out of range");
	Impl::FrameState& frame = *window.frames[frameSlot];
	if (!frame.active || !frame.sealed) storageError("frame must be active and sealed before submission");
	const SubmissionSerial serial = impl_->nextSubmissionSerial++;
	for (const UsedResource& used : frame.used) impl_->stampUse(used, serial);
	frame.active = false;
	frame.sealed = false;
	impl_->telemetry.currentSubmissionSerial = serial;
	return SubmissionToken{id, serial, frameSlot};
}

void FlowStorageSystem::noteCompleted(SubmissionToken submission) {
	std::scoped_lock lock(impl_->mutex);
	if (!submission || submission.serial >= impl_->nextSubmissionSerial) return;
	if (submission.serial <= impl_->completedWatermark) return;
	impl_->completedOutOfOrder.insert(submission.serial);
	while (impl_->completedOutOfOrder.erase(impl_->completedWatermark + 1u) > 0) ++impl_->completedWatermark;
	impl_->telemetry.completedSubmissionSerial = impl_->completedWatermark;
	for (auto it = impl_->windows.begin(); it != impl_->windows.end();) {
		if (it->second->closing && it->second->retireAfter <= impl_->completedWatermark) it = impl_->windows.erase(it);
		else ++it;
	}
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
}

SubmissionSerial FlowStorageSystem::completedSerial() const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return impl_->completedWatermark;
}

void FlowStorageSystem::retire(const RetirementRequest& request) {
	switch (request.kind) {
	case RetirementKind::Blob: releaseBlob(BlobHandle::fromPacked(request.packedHandle), request.retireAfter); break;
	case RetirementKind::Buffer: releaseBuffer(BufferHandle::fromPacked(request.packedHandle), request.retireAfter); break;
	case RetirementKind::Image: releaseImage(ImageHandle::fromPacked(request.packedHandle), request.retireAfter); break;
	case RetirementKind::ImageView: releaseImageView(ImageViewHandle::fromPacked(request.packedHandle), request.retireAfter); break;
	case RetirementKind::Sampler: releaseSampler(SamplerHandle::fromPacked(request.packedHandle), request.retireAfter); break;
	case RetirementKind::Texture: {
		std::scoped_lock lock(impl_->mutex);
		impl_->requireResourceMutationPhase();
		const TextureHandle handle = TextureHandle::fromPacked(request.packedHandle);
		if (!impl_->validTexture(handle)) return;
		TextureHotRecord& hot = impl_->textureHot[handle.index];
		if (hot.state == ResourceState::Retiring) return;
		impl_->textureByKey.erase(impl_->textureCold[handle.index].key);
		hot.state = ResourceState::Retiring;
		for (auto& [_, window] : impl_->windows) {
			if (window->bindingsByTextureIndex.size() > handle.index) {
				const BindingHotRecord& binding = window->bindingsByTextureIndex[handle.index];
				if (binding.textureGeneration == handle.generation && binding.descriptorIndex != 0)
					++window->retiredBindings;
			}
		}
		impl_->enqueueRetirement(RetirementKind::Texture, handle.packed(),
			std::max(request.retireAfter, impl_->textureCold[handle.index].lastUse));
		break;
	}
	}
}

void FlowStorageSystem::collect() {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireResourceMutationPhase();
	for (;;) {
		std::vector<Impl::RetirementRecord> ready;
		for (auto it = impl_->retirements.begin(); it != impl_->retirements.end();) {
			if (it->request.retireAfter <= impl_->completedWatermark) {
				ready.push_back(*it);
				it = impl_->retirements.erase(it);
			} else {
				++it;
			}
		}
		if (ready.empty()) break;
		for (const Impl::RetirementRecord& retired : ready) impl_->destroyRetired(retired);
	}
}

void FlowStorageSystem::trim(uint64_t targetBytes) {
	std::scoped_lock lock(impl_->mutex);
	uint64_t released = 0;
	for (auto& [_, window] : impl_->windows) {
		for (auto& frame : window->frames) {
			const uint64_t before = frame->transient.capacity() + frame->decode.capacity();
			frame->transient.trimOverflow();
			frame->decode.trimOverflow();
			for (auto& worker : frame->workers) worker->trimOverflow();
			const uint64_t after = frame->transient.capacity() + frame->decode.capacity();
			released += before - std::min(before, after);
			if (released >= targetBytes) return;
		}
	}
}

StorageStats FlowStorageSystem::stats() const {
	std::scoped_lock lock(impl_->mutex);
	StorageStats result = impl_->telemetry;
	auto persistent = result.cpu[static_cast<size_t>(MemoryClass::Persistent)];
	persistent.reservedBytes = impl_->persistentPool.reservedBytes();
	persistent.committedBytes = persistent.reservedBytes;
	persistent.liveBytes = impl_->persistentPool.liveBytes();
	persistent.peakLiveBytes = impl_->persistentPool.peakLiveBytes();
	persistent.allocationCount = impl_->persistentPool.allocationCount();
	persistent.growthCount = impl_->persistentPool.growthCount();
	result.cpu[static_cast<size_t>(MemoryClass::Persistent)] = persistent;
	auto strings = result.cpu[static_cast<size_t>(MemoryClass::StringPool)];
	strings.reservedBytes = impl_->stringPool.reservedBytes();
	strings.committedBytes = strings.reservedBytes;
	strings.liveBytes = impl_->stringPool.liveBytes();
	strings.peakLiveBytes = impl_->stringPool.peakLiveBytes();
	strings.allocationCount = impl_->stringPool.allocationCount();
	strings.growthCount = impl_->stringPool.growthCount();
	result.cpu[static_cast<size_t>(MemoryClass::StringPool)] = strings;
	return result;
}

ResourceStats FlowStorageSystem::resourceStats(ResourceKind kind) const {
	std::scoped_lock lock(impl_->mutex);
	ResourceStats result{};
	result.kind = kind;
	auto add = [&](ResourceState state, uint64_t bytes) {
		if (state == ResourceState::Invalid) { ++result.free; return; }
		++result.live;
		result.liveBytes += bytes;
		switch (state) {
		case ResourceState::Queued:
		case ResourceState::Decoding:
		case ResourceState::Uploading: ++result.queued; break;
		case ResourceState::Ready: ++result.ready; break;
		case ResourceState::Failed: ++result.failed; break;
		case ResourceState::Retiring: ++result.retiring; result.retiredBytes += bytes; break;
		default: break;
		}
	};
	switch (kind) {
	case ResourceKind::CpuBlob:
		result.slots = static_cast<uint32_t>(impl_->blobs.size() - 1u);
		for (size_t i = 1; i < impl_->blobs.size(); ++i) add(impl_->blobs[i].state, impl_->blobs[i].memory.size);
		break;
	case ResourceKind::GpuBuffer:
		result.slots = static_cast<uint32_t>(impl_->buffers.size() - 1u);
		for (size_t i = 1; i < impl_->buffers.size(); ++i) add(impl_->buffers[i].state, impl_->buffers[i].size);
		break;
	case ResourceKind::GpuImage:
		result.slots = static_cast<uint32_t>(impl_->images.size() - 1u);
		for (size_t i = 1; i < impl_->images.size(); ++i) add(impl_->images[i].state, impl_->images[i].byteSize);
		break;
	case ResourceKind::ImageView:
		result.slots = static_cast<uint32_t>(impl_->imageViews.size() - 1u);
		for (size_t i = 1; i < impl_->imageViews.size(); ++i) add(impl_->imageViews[i].state, 0);
		break;
	case ResourceKind::Sampler:
		result.slots = static_cast<uint32_t>(impl_->samplers.size() - 1u);
		for (size_t i = 1; i < impl_->samplers.size(); ++i) add(impl_->samplers[i].state, 0);
		break;
	case ResourceKind::TextureView:
		result.slots = static_cast<uint32_t>(impl_->textureHot.size() - 1u);
		for (size_t i = 1; i < impl_->textureHot.size(); ++i) add(impl_->textureHot[i].state, 0);
		break;
	default: break;
	}
	return result;
}

bool FlowStorageSystem::validateHandle(ResourceKind kind, uint32_t index, uint32_t generation) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	switch (kind) {
	case ResourceKind::CpuBlob: return impl_->validBlob({index, generation});
	case ResourceKind::GpuBuffer: return impl_->validBuffer({index, generation});
	case ResourceKind::GpuImage: return impl_->validImage({index, generation});
	case ResourceKind::ImageView: return impl_->validImageView({index, generation});
	case ResourceKind::Sampler: return impl_->validSampler({index, generation});
	case ResourceKind::TextureView: return impl_->validTexture({index, generation});
	default: return false;
	}
}

void FlowStorageSystem::setBudget(uint64_t cpuBytes, uint64_t gpuBytes) {
	std::scoped_lock lock(impl_->mutex);
	if (cpuBytes == 0 || gpuBytes == 0) storageError("storage budgets must be non-zero");
	impl_->telemetry.cpuSoftBudgetBytes = cpuBytes;
	impl_->telemetry.gpuSoftBudgetBytes = gpuBytes;
}

NativeBufferView FlowStorageSystem::nativeBuffer(BufferHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validBuffer(handle)) return {};
	const Impl::BufferRecord& record = impl_->buffers[handle.index];
	return NativeBufferView{nativeHandle(record.buffer), record.size, record.mapped};
}

NativeImageView FlowStorageSystem::nativeImage(ImageHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->validImage(handle)) return {};
	const Impl::ImageRecord& record = impl_->images[handle.index];
	return NativeImageView{
		.nativeImage = nativeHandle(record.image),
		.nativeImageView = 0,
		.format = record.desc.format,
		.width = record.desc.width,
		.height = record.desc.height,
		.layers = record.desc.layers,
	};
}

} // namespace FlowUi::detail::storage
