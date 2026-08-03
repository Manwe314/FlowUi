#include "internal/StorageSystem/FlowStorageSystem.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <condition_variable>
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
	static_cast<uint64_t>(StorageCapability::VulkanInterop) |
	static_cast<uint64_t>(StorageCapability::DirectMappedWrites) |
	static_cast<uint64_t>(StorageCapability::HostScratchBufferWrites) |
	static_cast<uint64_t>(StorageCapability::BindingWriteBatches) |
	static_cast<uint64_t>(StorageCapability::FrameReadLeases) |
	static_cast<uint64_t>(StorageCapability::RendererResourceBundles) |
	static_cast<uint64_t>(StorageCapability::BorrowedNativeTextures)
#if FLOW_UI_DEV_MODE
	| static_cast<uint64_t>(StorageCapability::DevelopmentTelemetry)
#endif
	;

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

size_t checkedSize(uint64_t value, const char* message) {
	if (value > std::numeric_limits<size_t>::max()) storageError(message);
	return static_cast<size_t>(value);
}

uint64_t checkedMultiply(uint64_t lhs, uint64_t rhs, const char* message) {
	if (lhs != 0 && rhs > std::numeric_limits<uint64_t>::max() / lhs) storageError(message);
	return lhs * rhs;
}

uint64_t checkedAdd(uint64_t lhs, uint64_t rhs, const char* message) {
	if (rhs > std::numeric_limits<uint64_t>::max() - lhs) storageError(message);
	return lhs + rhs;
}

uint32_t nextGeneration(uint32_t generation) noexcept {
	++generation;
	return generation == InvalidGeneration ? 1u : generation;
}

uint64_t nextCapacity(uint64_t current, uint64_t required, float factor) {
	uint64_t capacity = std::max<uint64_t>(current, 1u);
	const double clampedFactor = std::max(1.1, static_cast<double>(factor));
	while (capacity < required) {
		const double candidate = std::ceil(static_cast<double>(capacity) * clampedFactor);
		if (!std::isfinite(candidate) || candidate >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
			return required;
		}
		const uint64_t grown = static_cast<uint64_t>(candidate);
		if (grown <= capacity) return required;
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
		uint64_t mipBytes = checkedMultiply(width, height, "image byte-size overflow");
		mipBytes = checkedMultiply(mipBytes, depth, "image byte-size overflow");
		mipBytes = checkedMultiply(mipBytes, desc.layers, "image byte-size overflow");
		mipBytes = checkedMultiply(mipBytes, bytesPerPixel(desc.format), "image byte-size overflow");
		bytes = checkedAdd(bytes, mipBytes, "image byte-size overflow");
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

template <typename HandleType>
HandleType fromNativeHandle(uint64_t value) noexcept {
	if constexpr (std::is_pointer_v<HandleType>) {
		return reinterpret_cast<HandleType>(static_cast<uintptr_t>(value));
	} else {
		return static_cast<HandleType>(value);
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

struct AlignedByteDelete {
	size_t alignment = alignof(std::max_align_t);
	void operator()(std::byte* pointer) const noexcept {
		if (pointer) ::operator delete[](pointer, std::align_val_t(alignment));
	}
};

using AlignedByteArray = std::unique_ptr<std::byte[], AlignedByteDelete>;

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
			addPage(initialBytes, alignof(std::max_align_t));
		}
	}

	void* allocate(size_t bytes, size_t alignment) {
		if (bytes == 0) {
			return nullptr;
		}
		(void)alignUp(0, alignment);
		for (size_t attempt = currentPage_; attempt < pages_.size(); ++attempt) {
			Page& page = pages_[attempt];
			if (page.alignment < alignment) continue;
			const size_t offset = alignUp(page.offset, alignment);
			if (offset <= page.capacity && bytes <= page.capacity - offset) {
					page.offset = offset + bytes;
					currentPage_ = attempt;
#if FLOW_UI_DEV_MODE
					liveBytes_ += bytes;
					highWaterBytes_ = std::max(highWaterBytes_, liveBytes_);
#endif
					return page.memory.get() + offset;
			}
		}

		if (!allowGrowth_) {
			storageError("transient arena exhausted and runtime growth is disabled");
		}
		const size_t base = pages_.empty() ? bytes : pages_.back().capacity;
		const uint64_t required = checkedAdd(bytes, alignment, "transient arena growth overflow");
		const size_t capacity = checkedSize(nextCapacity(base, required, growthFactor_),
			"transient arena capacity exceeds the host address space");
		addPage(capacity, std::max(alignment, alignof(std::max_align_t)));
#if FLOW_UI_DEV_MODE
			++growthCount_;
#endif
			return allocate(bytes, alignment);
	}

	void reset() noexcept {
		for (Page& page : pages_) {
			page.offset = 0;
			}
			currentPage_ = 0;
#if FLOW_UI_DEV_MODE
			liveBytes_ = 0;
#endif
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
	[[nodiscard]] uint64_t highWater() const noexcept {
#if FLOW_UI_DEV_MODE
		return highWaterBytes_;
#else
		return 0;
#endif
	}
	[[nodiscard]] uint64_t growthCount() const noexcept {
#if FLOW_UI_DEV_MODE
		return growthCount_;
#else
		return 0;
#endif
	}

	static void* arenaAllocate(void* context, size_t bytes, size_t alignment) {
		return static_cast<LinearArena*>(context)->allocate(bytes, alignment);
	}

private:
	struct Page {
		AlignedByteArray memory{};
		size_t capacity = 0;
		size_t offset = 0;
		size_t alignment = alignof(std::max_align_t);
	};

	void addPage(size_t capacity, size_t alignment) {
		Page page{};
		page.memory = AlignedByteArray(
			static_cast<std::byte*>(::operator new[](capacity, std::align_val_t(alignment))),
			AlignedByteDelete{alignment});
		page.capacity = capacity;
		page.alignment = alignment;
		pages_.push_back(std::move(page));
	}

	std::vector<Page> pages_;
	size_t currentPage_ = 0;
#if FLOW_UI_DEV_MODE
	uint64_t liveBytes_ = 0;
	uint64_t highWaterBytes_ = 0;
	uint64_t growthCount_ = 0;
#endif
	float growthFactor_ = 1.5f;
	bool allowGrowth_ = true;
};

class PersistentPool {
public:
	void initialize(size_t initialBytes, float growthFactor, bool allowGrowth) {
		growthFactor_ = growthFactor;
		allowGrowth_ = allowGrowth;
		if (initialBytes > 0 && slabs_.empty()) addSlab(initialBytes, alignof(std::max_align_t));
	}

	MemoryBlock allocate(size_t bytes, size_t alignment, AllocationTag tag) {
		if (bytes == 0) return {};
		(void)alignUp(0, alignment);
		for (uint32_t slabIndex = 0; slabIndex < slabs_.size(); ++slabIndex) {
			Slab& slab = slabs_[slabIndex];
			if (slab.alignment < alignment) continue;
			for (size_t blockIndex = 0; blockIndex < slab.free.size(); ++blockIndex) {
				FreeBlock block = slab.free[blockIndex];
				const size_t alignedOffset = alignUp(block.offset, alignment);
				const size_t prefix = alignedOffset - block.offset;
				if (prefix > block.size || bytes > block.size - prefix) continue;

				// Reserve every metadata slot that this allocation and all future
				// releases can need while allocation is still allowed to throw.
				slab.free.reserve(slab.free.size() + slab.activeAllocations + 2u);
				if (nextId_ == 0 || allocations_.contains(nextId_)) {
					storageError("persistent allocation id space exhausted");
				}
				const AllocationId id = nextId_;
				allocations_.emplace(id, Allocation{slabIndex, alignedOffset, bytes, tag});

				slab.free.erase(slab.free.begin() + static_cast<std::ptrdiff_t>(blockIndex));
				if (prefix > 0) slab.free.push_back({block.offset, prefix});
				const size_t consumed = prefix + bytes;
				if (consumed < block.size) slab.free.push_back({alignedOffset + bytes, block.size - consumed});

				++nextId_;
				++slab.activeAllocations;
				liveBytes_ += bytes;
#if FLOW_UI_DEV_MODE
				peakLiveBytes_ = std::max(peakLiveBytes_, liveBytes_);
#endif
				return MemoryBlock{slab.memory.get() + alignedOffset, bytes, id, tag};
			}
		}

		if (!allowGrowth_) storageError("persistent pool exhausted and runtime growth is disabled");
		const size_t base = slabs_.empty() ? bytes : slabs_.back().capacity;
		const uint64_t required = checkedAdd(bytes, alignment, "persistent pool growth overflow");
		addSlab(checkedSize(nextCapacity(base, required, growthFactor_),
			"persistent pool capacity exceeds the host address space"),
			std::max(alignment, alignof(std::max_align_t)));
#if FLOW_UI_DEV_MODE
		++growthCount_;
#endif
		return allocate(bytes, alignment, tag);
	}

	void release(const MemoryBlock& block) noexcept {
		auto it = allocations_.find(block.id);
		if (it == allocations_.end()) return;
		const Allocation allocation = it->second;
		if (allocation.slab >= slabs_.size()) return;
		Slab& slab = slabs_[allocation.slab];
		const void* expectedData = slab.memory.get() + allocation.offset;
		const bool matchingTag =
			block.tag.memoryClass == allocation.tag.memoryClass &&
			block.tag.resourceKind == allocation.tag.resourceKind &&
			block.tag.window == allocation.tag.window &&
			block.tag.frameSlot == allocation.tag.frameSlot &&
			block.tag.debugName == allocation.tag.debugName;
		if (block.data != expectedData || block.size != allocation.size || !matchingTag) return;
		// allocate() reserves one release slot per active allocation, so this
		// push cannot allocate inside the noexcept release path.
		slab.free.push_back({allocation.offset, allocation.size});
		if (slab.activeAllocations > 0) --slab.activeAllocations;
		merge(slab.free);
		liveBytes_ -= std::min<uint64_t>(liveBytes_, allocation.size);
		allocations_.erase(it);
	}

	[[nodiscard]] uint64_t reservedBytes() const noexcept {
		uint64_t result = 0;
		for (const Slab& slab : slabs_) result += slab.capacity;
		return result;
	}
	[[nodiscard]] uint64_t liveBytes() const noexcept { return liveBytes_; }
	[[nodiscard]] uint64_t peakLiveBytes() const noexcept {
#if FLOW_UI_DEV_MODE
		return peakLiveBytes_;
#else
		return 0;
#endif
	}
	[[nodiscard]] uint64_t allocationCount() const noexcept { return allocations_.size(); }
	[[nodiscard]] uint64_t growthCount() const noexcept {
#if FLOW_UI_DEV_MODE
		return growthCount_;
#else
		return 0;
#endif
	}

private:
	struct FreeBlock { size_t offset = 0; size_t size = 0; };
	struct Slab {
		AlignedByteArray memory{};
		size_t capacity = 0;
		std::vector<FreeBlock> free;
		size_t activeAllocations = 0;
		size_t alignment = alignof(std::max_align_t);
	};
	struct Allocation {
		uint32_t slab = 0;
		size_t offset = 0;
		size_t size = 0;
		AllocationTag tag{};
	};

	void addSlab(size_t capacity, size_t alignment) {
		Slab slab{};
		slab.memory = AlignedByteArray(
			static_cast<std::byte*>(::operator new[](capacity, std::align_val_t(alignment))),
			AlignedByteDelete{alignment});
		slab.capacity = capacity;
		slab.alignment = alignment;
		slab.free.reserve(2u);
		slab.free.push_back({0, capacity});
		slabs_.push_back(std::move(slab));
	}

public:
	void clear() noexcept {
		slabs_.clear();
		allocations_.clear();
		nextId_ = 1;
		liveBytes_ = 0;
#if FLOW_UI_DEV_MODE
		peakLiveBytes_ = 0;
		growthCount_ = 0;
#endif
	}

private:

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
#if FLOW_UI_DEV_MODE
	uint64_t peakLiveBytes_ = 0;
	uint64_t growthCount_ = 0;
#endif
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
#if FLOW_UI_DEV_MODE
		StringId debugName = 0;
#endif
	};

	struct BufferRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		void* mapped = nullptr;
		VkMemoryPropertyFlags memoryProperties = 0;
		uint64_t size = 0;
		uint64_t allocationBytes = 0;
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
		ExternalTextureDesc externalDesc{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
		bool published = false;
		bool external = false;
	};

	struct RendererLayoutRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		RendererLayoutKey key{};
		NativeRendererLayout native{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
#if FLOW_UI_DEV_MODE
		StringId debugName = 0;
#endif
	};

	struct RendererPipelineBundleRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		RendererPipelineKey key{};
		NativeRendererPipelineBundle native{};
		uint32_t referenceCount = 0;
		SubmissionSerial lastUse = 0;
#if FLOW_UI_DEV_MODE
		StringId debugName = 0;
#endif
	};

	struct WindowDescriptorBundleRecord {
		uint32_t generation = 0;
		ResourceState state = ResourceState::Invalid;
		WindowDescriptorBundleDesc desc{};
		uint64_t nativePool = 0;
		std::vector<uint64_t> globalsSets;
		std::vector<uint64_t> textureSets;
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

	struct PendingBufferWrite {
		uint64_t id = 0;
		BufferHandle buffer{};
		std::byte* data = nullptr;
		uint64_t destinationOffset = 0;
		uint64_t capacity = 0;
		BufferWriteMode mode = BufferWriteMode::Default;
		bool committing = false;
	};

	struct UseEpochMarker {
		FrameEpoch epoch = 0;
		uint32_t generation = 0;
	};

	struct FrameState {
		FrameEpoch epoch = 0;
		uint64_t frameNumber = 0;
		bool active = false;
		bool sealed = false;
		uint64_t leaseId = 0;
		SubmissionSerial inFlightSerial = 0;
#if FLOW_UI_DEV_MODE
		std::shared_ptr<ReadLeaseState> leaseValidation{};
		std::shared_ptr<ArenaLeaseState> arenaValidation{};
#endif
		LinearArena transient;
		LinearArena decode;
		std::vector<std::unique_ptr<LinearArena>> workers;
		std::vector<UsedResource> used;
		std::vector<UseEpochMarker> usedBufferEpochs;
		std::vector<UseEpochMarker> usedImageEpochs;
		std::vector<UseEpochMarker> usedImageViewEpochs;
		std::vector<UseEpochMarker> usedSamplerEpochs;
		std::vector<UseEpochMarker> usedTextureEpochs;
		std::vector<UseEpochMarker> usedRendererLayoutEpochs;
		std::vector<UseEpochMarker> usedRendererPipelineEpochs;
		std::vector<UseEpochMarker> usedDescriptorBundleEpochs;
		std::vector<uint32_t> appliedBindingRevisions;
		std::vector<uint32_t> preparedBindingBatches;
		uint32_t currentBindingBatch = 0;
		std::vector<PendingBufferWrite> pendingBufferWrites;
		size_t activeBufferCommits = 0;
	};

	struct WindowState {
		WindowId id = 0;
		WindowStorageDesc desc{};
		std::vector<std::unique_ptr<FrameState>> frames;
		std::vector<BindingHotRecord> bindingsByTextureIndex;
		std::vector<uint32_t> freeDescriptorIndices;
		uint32_t nextDescriptorIndex = 1;
		uint32_t bindingRevision = 0;
#if FLOW_UI_DEV_MODE
		uint32_t liveBindings = 0;
		uint32_t retiredBindings = 0;
#endif
		WindowDescriptorBundleHandle activeDescriptorBundle{};
		bool closing = false;
		SubmissionSerial retireAfter = 0;
	};

	VulkanContext& vk;
	StorageConfig config{};
	bool initialized = false;
	bool terminated = false;
	mutable std::recursive_mutex mutex;
	std::condition_variable_any bufferCommitCondition;
	uint64_t cpuSoftBudgetBytes = 0;
	uint64_t gpuSoftBudgetBytes = 0;
	uint64_t gpuLiveBytes = 0;
	uint64_t gpuRetiredBytes = 0;

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
	TextureHandle fallbackTexture{};
	BindingHotRecord fallbackBinding{};
	std::vector<RendererLayoutRecord> rendererLayouts{RendererLayoutRecord{}};
	std::vector<uint32_t> freeRendererLayouts;
	std::unordered_map<RendererLayoutKey, RendererLayoutHandle, RendererLayoutKeyHash> rendererLayoutByKey;
	std::vector<RendererPipelineBundleRecord> rendererPipelineBundles{RendererPipelineBundleRecord{}};
	std::vector<uint32_t> freeRendererPipelineBundles;
	std::unordered_map<RendererPipelineKey, RendererPipelineBundleHandle, RendererPipelineKeyHash> rendererPipelineByKey;
	std::vector<WindowDescriptorBundleRecord> windowDescriptorBundles{WindowDescriptorBundleRecord{}};
	std::vector<uint32_t> freeWindowDescriptorBundles;

	std::unordered_map<WindowId, std::unique_ptr<WindowState>> windows;
	std::unordered_set<WindowId> registeredWindowIds;
	std::deque<UploadRecord> uploads;
	std::unordered_map<UploadId, ResourceState> uploadStates;
	std::vector<RetirementRecord> retirements;

	VkCommandPool uploadCommandPool = VK_NULL_HANDLE;
	UploadId nextUploadId = 1;
	FrameEpoch nextFrameEpoch = 1;
	uint64_t nextReadLeaseId = 1;
	uint64_t nextBufferWriteId = 1;
	SubmissionSerial nextSubmissionSerial = 1;
	SubmissionSerial completedWatermark = 0;
	std::unordered_set<SubmissionSerial> completedOutOfOrder;
#if FLOW_UI_DEV_MODE
	StorageStats telemetry{};
#endif

	template <typename Record>
	static uint32_t acquireIndex(std::vector<Record>& records, std::vector<uint32_t>& freeIndices) {
		if (!freeIndices.empty()) {
			const uint32_t index = freeIndices.back();
			freeIndices.pop_back();
			return index;
		}
		if (records.size() >= std::numeric_limits<uint32_t>::max()) storageError("resource handle table exhausted");
		// Reserve the rollback slot before growing the record table. Creation
		// failure can then return the index without a second allocation failure.
		freeIndices.reserve(freeIndices.size() + 1u);
		records.emplace_back();
		return static_cast<uint32_t>(records.size() - 1u);
	}

	uint32_t acquireImageViewIndex() {
		const uint32_t index = acquireIndex(imageViews, freeImageViews);
		try {
			if (imageViewHot.size() <= index) imageViewHot.resize(static_cast<size_t>(index) + 1u);
		} catch (...) {
			freeImageViews.push_back(index);
			throw;
		}
		return index;
	}

	uint32_t acquireSamplerIndex() {
		const uint32_t index = acquireIndex(samplers, freeSamplers);
		try {
			if (samplerHot.size() <= index) samplerHot.resize(static_cast<size_t>(index) + 1u);
		} catch (...) {
			freeSamplers.push_back(index);
			throw;
		}
		return index;
	}

	uint32_t acquireTextureIndex() {
		uint32_t index = 0;
		if (!freeTextures.empty()) {
			index = freeTextures.back();
			freeTextures.pop_back();
		} else {
			if (textureHot.size() >= std::numeric_limits<uint32_t>::max()) storageError("texture handle table exhausted");
			freeTextures.reserve(freeTextures.size() + 1u);
			textureHot.reserve(textureHot.size() + 1u);
			textureCold.reserve(textureCold.size() + 1u);
			index = static_cast<uint32_t>(textureHot.size());
			textureHot.emplace_back();
			textureCold.emplace_back();
		}
		return index;
	}

	void requireInitialized() const {
		if (!initialized) storageError("system is not initialized");
	}

	void requireCpuBudget(uint64_t bytes) const {
		const uint64_t live = persistentPool.liveBytes() + stringPool.liveBytes();
		if (bytes > cpuSoftBudgetBytes || live > cpuSoftBudgetBytes - bytes) {
			storageError("CPU soft budget exceeded while allocating persistent memory");
		}
	}

	[[nodiscard]] bool hasActiveFrames() const noexcept {
		for (const auto& [_, window] : windows) {
			for (const auto& frame : window->frames) {
				if (frame->active) return true;
			}
		}
		return false;
	}

	[[nodiscard]] bool hasSealedFrames() const noexcept {
		for (const auto& [_, window] : windows) {
			for (const auto& frame : window->frames) {
				if (frame->active && frame->sealed) return true;
			}
		}
		return false;
	}

	void requireSharedMutationPhase() const {
		if (hasActiveFrames()) {
			storageError("shared resource mutation is allowed only before window frames begin or after they terminate");
		}
	}

	void requireExternalTexturePublishPhase() const {
		if (hasSealedFrames()) {
			storageError("external texture publication is not allowed while a sealed frame is active");
		}
	}

	void requireWindowBindingMutationPhase(const WindowState& window, uint32_t currentFrameSlot) const {
		for (uint32_t slot = 0; slot < window.frames.size(); ++slot) {
			if (slot == currentFrameSlot) continue;
			const FrameState& frame = *window.frames[slot];
			if (frame.active && frame.sealed) {
				storageError("window binding mutation is not allowed while another frame read lease for that window is active");
			}
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

	void validateSharingAttribution(
		ResourceSharing sharing,
		WindowId window,
		uint32_t frameSlot,
		const char* resourceName) const {
		switch (sharing) {
		case ResourceSharing::AppShared:
			if (window != 0 || frameSlot != InvalidFrameSlot) {
				storageError("app-shared resource must use root window and invalid frame-slot attribution");
			}
			break;
		case ResourceSharing::WindowLocal: {
			const WindowState& owner = requireWindow(window);
			(void)owner;
			if (frameSlot != InvalidFrameSlot) {
				storageError("window-local resource must not carry a frame-slot attribution");
			}
			break;
		}
		case ResourceSharing::FrameLocal: {
			const WindowState& owner = requireWindow(window);
			if (frameSlot >= owner.frames.size()) storageError("frame-local resource frame-slot is out of range");
			break;
		}
		default:
			(void)resourceName;
			storageError("resource sharing mode is invalid");
		}
	}

	static bool visibleToFrame(
		ResourceSharing sharing,
		WindowId ownerWindow,
		uint32_t ownerFrameSlot,
		const FrameToken& frame) noexcept {
		switch (sharing) {
		case ResourceSharing::AppShared: return true;
		case ResourceSharing::WindowLocal: return ownerWindow == frame.window;
		case ResourceSharing::FrameLocal:
			return ownerWindow == frame.window && ownerFrameSlot == frame.frameSlot;
		default: return false;
		}
	}

	bool bufferVisibleToFrame(BufferHandle handle, const FrameToken& frame) const noexcept {
		if (!usableBuffer(handle)) return false;
		const BufferDesc& desc = buffers[handle.index].desc;
		return visibleToFrame(desc.sharing, desc.window, desc.frameSlot, frame);
	}

	bool imageVisibleToFrame(ImageHandle handle, const FrameToken& frame) const noexcept {
		if (!usableImage(handle)) return false;
		const ImageDesc& desc = images[handle.index].desc;
		return visibleToFrame(desc.sharing, desc.window, desc.frameSlot, frame);
	}

	ImageHandle textureBackingImage(TextureHandle handle) const noexcept {
		if (!usableTexture(handle) || textureCold[handle.index].external) return {};
		const ImageViewHandle view = textureCold[handle.index].desc.imageView;
		return usableImageView(view) ? imageViews[view.index].image : ImageHandle{};
	}

	bool textureVisibleToFrame(TextureHandle handle, const FrameToken& frame) const noexcept {
		if (!usableTexture(handle)) return false;
		const TextureColdRecord& cold = textureCold[handle.index];
		if (cold.external) {
			return visibleToFrame(
				cold.externalDesc.sharing,
				cold.externalDesc.window,
				cold.externalDesc.frameSlot,
				frame);
		}
		return imageVisibleToFrame(textureBackingImage(handle), frame);
	}

	void validateTextureOwnership(ResourceKey key, const TextureViewDesc& desc) const {
		if (!usableImageView(desc.imageView)) storageError("texture image view is invalid or retiring");
		const ImageHandle image = imageViews[desc.imageView.index].image;
		if (!usableImage(image)) storageError("texture backing image is invalid or retiring");
		const ImageDesc& imageDesc = images[image.index].desc;
		if (imageDesc.sharing == ResourceSharing::AppShared) {
			if (key.window != 0) storageError("app-shared texture must use root ResourceKey attribution");
		} else if (key.window != imageDesc.window) {
			storageError("texture ResourceKey window does not match its backing image owner");
		}
	}

	void validateExternalTextureOwnership(ResourceKey key, const ExternalTextureDesc& desc) const {
		if (desc.nativeImageView == 0 || desc.nativeSampler == 0) {
			storageError("external texture publication requires non-null native handles");
		}
		if (desc.sharing == ResourceSharing::AppShared) {
			if (key.window != 0 || desc.window != 0 || desc.frameSlot != InvalidFrameSlot) {
				storageError("app-shared external texture must use root attribution");
			}
			return;
		}
		if (desc.window == 0 || key.window != desc.window) {
			storageError("external texture key does not match its owning window");
		}
		if (desc.sharing == ResourceSharing::WindowLocal && desc.frameSlot != InvalidFrameSlot) {
			storageError("window-local external texture cannot name a frame slot");
		}
		if (desc.sharing == ResourceSharing::FrameLocal && desc.frameSlot == InvalidFrameSlot) {
			storageError("frame-local external texture requires a frame slot");
		}
	}

	FrameState& requireFrame(const FrameToken& token, bool allowSealed = true) {
		WindowState& window = requireWindow(token.window);
		if (token.frameSlot >= window.frames.size()) storageError("frame slot is out of range");
		FrameState& frame = *window.frames[token.frameSlot];
		if (!frame.active || frame.epoch != token.epoch) storageError("frame token is stale or inactive");
		if (!allowSealed && frame.sealed) storageError("frame storage is sealed");
		return frame;
	}

	FrameState& requireLease(const FrameReadLease& lease) {
		if (!lease) storageError("frame read lease is invalid");
		FrameState& frame = requireFrame(lease.frame);
		if (!frame.sealed || frame.leaseId != lease.leaseId) storageError("frame read lease is stale or inactive");
#if FLOW_UI_DEV_MODE
		if (!lease.valid() || frame.leaseValidation != lease.validation) storageError("frame read lease validation failed");
#endif
		return frame;
	}

	const FrameState& requireLease(const FrameReadLease& lease) const {
		if (!lease) storageError("frame read lease is invalid");
		const FrameState& frame = requireFrame(lease.frame);
		if (!frame.sealed || frame.leaseId != lease.leaseId) storageError("frame read lease is stale or inactive");
#if FLOW_UI_DEV_MODE
		if (!lease.valid() || frame.leaseValidation != lease.validation) storageError("frame read lease validation failed");
#endif
		return frame;
	}

	static void invalidateLease(FrameState& frame) noexcept {
#if FLOW_UI_DEV_MODE
		if (frame.leaseValidation) frame.leaseValidation->valid.store(false, std::memory_order_release);
		frame.leaseValidation.reset();
#endif
		frame.leaseId = 0;
	}

	static void invalidateArena(FrameState& frame) noexcept {
#if FLOW_UI_DEV_MODE
		if (frame.arenaValidation) frame.arenaValidation->valid.store(false, std::memory_order_release);
		frame.arenaValidation.reset();
#else
		(void)frame;
#endif
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
		const uint32_t index = static_cast<uint32_t>(packed);
		const uint32_t generation = static_cast<uint32_t>(packed >> 32u);
		std::vector<UseEpochMarker>* markers = nullptr;
		UseEpochMarker* marker = nullptr;
		switch (kind) {
		case ResourceKind::GpuBuffer: markers = &frame.usedBufferEpochs; break;
		case ResourceKind::GpuImage: markers = &frame.usedImageEpochs; break;
		case ResourceKind::ImageView: markers = &frame.usedImageViewEpochs; break;
		case ResourceKind::Sampler: markers = &frame.usedSamplerEpochs; break;
		case ResourceKind::TextureView: markers = &frame.usedTextureEpochs; break;
		case ResourceKind::RendererLayout: markers = &frame.usedRendererLayoutEpochs; break;
		case ResourceKind::RendererPipelineBundle: markers = &frame.usedRendererPipelineEpochs; break;
		case ResourceKind::WindowDescriptorBundle: markers = &frame.usedDescriptorBundleEpochs; break;
		default: break;
		}
		if (markers) {
			if (markers->size() <= index) markers->resize(static_cast<size_t>(index) + 1u);
			marker = &(*markers)[index];
			if (marker->epoch == frame.epoch && marker->generation == generation) return;
		}
		const UsedResource used{kind, packed};
		switch (kind) {
		case ResourceKind::GpuBuffer: retainBuffer(BufferHandle::fromPacked(packed)); break;
		case ResourceKind::GpuImage: retainImage(ImageHandle::fromPacked(packed)); break;
		case ResourceKind::ImageView: retainImageView(ImageViewHandle::fromPacked(packed)); break;
		case ResourceKind::Sampler: retainSampler(SamplerHandle::fromPacked(packed)); break;
		case ResourceKind::TextureView: retainTexture(TextureHandle::fromPacked(packed)); break;
		case ResourceKind::RendererLayout: retainRendererLayout(RendererLayoutHandle::fromPacked(packed)); break;
		case ResourceKind::RendererPipelineBundle:
			retainRendererPipelineBundle(RendererPipelineBundleHandle::fromPacked(packed));
			break;
		case ResourceKind::WindowDescriptorBundle:
			retainWindowDescriptorBundle(WindowDescriptorBundleHandle::fromPacked(packed));
			break;
		default: break;
		}
		try {
			frame.used.push_back(used);
			if (marker) *marker = UseEpochMarker{frame.epoch, generation};
		} catch (...) {
			releaseUsed(used, 0);
			throw;
		}
	}

	void noteInvalidHandle() noexcept {
#if FLOW_UI_DEV_MODE
		++telemetry.invalidHandleCount;
#endif
	}

	void updateGpuPeak() noexcept {
#if FLOW_UI_DEV_MODE
		telemetry.gpuPeakBytes = std::max(
			telemetry.gpuPeakBytes,
			gpuLiveBytes + gpuRetiredBytes);
#endif
	}

	void enqueueRetirement(RetirementKind kind, uint64_t packed, SubmissionSerial serial, uint64_t bytes = 0) {
		retirements.push_back(RetirementRecord{
			.request = RetirementRequest{kind, packed, serial},
			.byteSize = bytes,
		});
		gpuLiveBytes -= std::min(gpuLiveBytes, bytes);
		gpuRetiredBytes += bytes;
		updateGpuPeak();
	}

	void reserveRetirements(size_t additional) {
		if (additional > retirements.max_size() - retirements.size()) {
			storageError("retirement queue size overflow");
		}
		retirements.reserve(retirements.size() + additional);
	}

	void retainImage(ImageHandle handle) {
		if (!usableImage(handle)) storageError("cannot retain invalid or retiring image handle");
		incrementReference(images[handle.index].referenceCount);
	}

	void retainBlob(BlobHandle handle) {
		if (!validBlob(handle) || blobs[handle.index].state == ResourceState::Retiring)
			storageError("cannot retain invalid or retiring blob handle");
		incrementReference(blobs[handle.index].referenceCount);
	}

	void retainBuffer(BufferHandle handle) {
		if (!usableBuffer(handle)) storageError("cannot retain invalid or retiring buffer handle");
		incrementReference(buffers[handle.index].referenceCount);
	}

	void retainImageView(ImageViewHandle handle) {
		if (!usableImageView(handle)) storageError("cannot retain invalid or retiring image-view handle");
		incrementReference(imageViews[handle.index].referenceCount);
	}

	void retainSampler(SamplerHandle handle) {
		if (!usableSampler(handle)) storageError("cannot retain invalid or retiring sampler handle");
		incrementReference(samplers[handle.index].referenceCount);
	}

	void refreshTexturesForImage(ImageHandle image) {
		if (!validImage(image)) return;
		const ResourceState imageState = images[image.index].state;
		for (size_t textureIndex = 1; textureIndex < textureHot.size(); ++textureIndex) {
			TextureHotRecord& hot = textureHot[textureIndex];
			if (hot.external || hot.state == ResourceState::Invalid || hot.imageViewIndex >= imageViews.size()) continue;
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
	bool usableBuffer(BufferHandle handle) const noexcept {
		return validBuffer(handle) && buffers[handle.index].state != ResourceState::Retiring;
	}
	bool usableImage(ImageHandle handle) const noexcept {
		return validImage(handle) && images[handle.index].state != ResourceState::Retiring;
	}
	bool usableImageView(ImageViewHandle handle) const noexcept {
		return validImageView(handle) && imageViews[handle.index].state != ResourceState::Retiring;
	}
	bool usableSampler(SamplerHandle handle) const noexcept {
		return validSampler(handle) && samplers[handle.index].state != ResourceState::Retiring;
	}
	bool usableTexture(TextureHandle handle) const noexcept {
		return validTexture(handle) && textureHot[handle.index].state != ResourceState::Retiring &&
			(textureCold[handle.index].published || handle == fallbackTexture);
	}
	bool usableRendererLayout(RendererLayoutHandle handle) const noexcept {
		return validRendererLayout(handle) && rendererLayouts[handle.index].state != ResourceState::Retiring;
	}
	bool usableRendererPipelineBundle(RendererPipelineBundleHandle handle) const noexcept {
		return validRendererPipelineBundle(handle) &&
			rendererPipelineBundles[handle.index].state != ResourceState::Retiring;
	}
	bool usableWindowDescriptorBundle(WindowDescriptorBundleHandle handle) const noexcept {
		return validWindowDescriptorBundle(handle) &&
			windowDescriptorBundles[handle.index].state != ResourceState::Retiring;
	}

	static void incrementReference(uint32_t& count) {
		if (count == std::numeric_limits<uint32_t>::max()) storageError("resource reference count overflow");
		++count;
	}
	bool validRendererLayout(RendererLayoutHandle handle) const noexcept {
		return handle && handle.index < rendererLayouts.size() &&
			rendererLayouts[handle.index].generation == handle.generation &&
			rendererLayouts[handle.index].state != ResourceState::Invalid;
	}
	bool validRendererPipelineBundle(RendererPipelineBundleHandle handle) const noexcept {
		return handle && handle.index < rendererPipelineBundles.size() &&
			rendererPipelineBundles[handle.index].generation == handle.generation &&
			rendererPipelineBundles[handle.index].state != ResourceState::Invalid;
	}
	bool validWindowDescriptorBundle(WindowDescriptorBundleHandle handle) const noexcept {
		return handle && handle.index < windowDescriptorBundles.size() &&
			windowDescriptorBundles[handle.index].generation == handle.generation &&
			windowDescriptorBundles[handle.index].state != ResourceState::Invalid;
	}

	void retainTexture(TextureHandle handle) {
		if (!usableTexture(handle)) storageError("cannot retain invalid or retiring texture handle");
		incrementReference(textureCold[handle.index].referenceCount);
	}
	void retainRendererLayout(RendererLayoutHandle handle) {
		if (!usableRendererLayout(handle)) storageError("cannot retain invalid or retiring renderer-layout handle");
		incrementReference(rendererLayouts[handle.index].referenceCount);
	}
	void retainRendererPipelineBundle(RendererPipelineBundleHandle handle) {
		if (!usableRendererPipelineBundle(handle)) storageError("cannot retain invalid or retiring renderer-pipeline handle");
		incrementReference(rendererPipelineBundles[handle.index].referenceCount);
	}
	void retainWindowDescriptorBundle(WindowDescriptorBundleHandle handle) {
		if (!usableWindowDescriptorBundle(handle)) storageError("cannot retain invalid or retiring descriptor-bundle handle");
		incrementReference(windowDescriptorBundles[handle.index].referenceCount);
	}

	void releaseRendererLayoutReference(RendererLayoutHandle handle, SubmissionSerial lastUse) {
		if (!validRendererLayout(handle)) return;
		RendererLayoutRecord& record = rendererLayouts[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::RendererLayout, handle.packed(), record.lastUse);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
		rendererLayoutByKey.erase(record.key);
	}
	void releaseRendererPipelineBundleReference(RendererPipelineBundleHandle handle, SubmissionSerial lastUse) {
		if (!validRendererPipelineBundle(handle)) return;
		RendererPipelineBundleRecord& record = rendererPipelineBundles[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::RendererPipelineBundle, handle.packed(), record.lastUse);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
		rendererPipelineByKey.erase(record.key);
	}
	void releaseWindowDescriptorBundleReference(WindowDescriptorBundleHandle handle, SubmissionSerial lastUse) {
		if (!validWindowDescriptorBundle(handle)) return;
		WindowDescriptorBundleRecord& record = windowDescriptorBundles[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::WindowDescriptorBundle, handle.packed(), record.lastUse);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
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
		case ResourceKind::RendererLayout: {
			const RendererLayoutHandle handle = RendererLayoutHandle::fromPacked(used.packedHandle);
			if (validRendererLayout(handle)) rendererLayouts[handle.index].lastUse =
				std::max(rendererLayouts[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::RendererPipelineBundle: {
			const RendererPipelineBundleHandle handle = RendererPipelineBundleHandle::fromPacked(used.packedHandle);
			if (validRendererPipelineBundle(handle)) rendererPipelineBundles[handle.index].lastUse =
				std::max(rendererPipelineBundles[handle.index].lastUse, serial);
			break;
		}
		case ResourceKind::WindowDescriptorBundle: {
			const WindowDescriptorBundleHandle handle = WindowDescriptorBundleHandle::fromPacked(used.packedHandle);
			if (validWindowDescriptorBundle(handle)) windowDescriptorBundles[handle.index].lastUse =
				std::max(windowDescriptorBundles[handle.index].lastUse, serial);
			break;
		}
		default: break;
		}
	}

	ResolvedTextureBinding resolve(
		WindowState& window,
		FrameState& frame,
		const FrameToken& frameToken,
		TextureHandle texture) {
		if (!fallbackTexture || fallbackBinding.nativeImageView == 0 || fallbackBinding.nativeSampler == 0) {
			storageError("a ready fallback texture must be configured before resolving window bindings");
		}
		if (!usableTexture(texture)) {
			noteInvalidHandle();
			addUse(frame, ResourceKind::TextureView, fallbackTexture.packed());
			return ResolvedTextureBinding{
				.descriptorIndex = 0,
				.bindingRevision = fallbackBinding.bindingRevision,
				.state = fallbackBinding.state,
				.nativeImageView = fallbackBinding.nativeImageView,
				.nativeSampler = fallbackBinding.nativeSampler,
			};
		}
		if (!textureVisibleToFrame(texture, frameToken)) {
			storageError("texture is not visible to the resolving window/frame scope");
		}
		if (texture == fallbackTexture) {
			addUse(frame, ResourceKind::TextureView, fallbackTexture.packed());
			return ResolvedTextureBinding{
				.descriptorIndex = 0,
				.bindingRevision = fallbackBinding.bindingRevision,
				.state = fallbackBinding.state,
				.nativeImageView = fallbackBinding.nativeImageView,
				.nativeSampler = fallbackBinding.nativeSampler,
			};
		}
		if (window.bindingsByTextureIndex.size() <= texture.index) {
			window.bindingsByTextureIndex.resize(static_cast<size_t>(texture.index) + 1u);
		}

		const TextureHotRecord& hot = textureHot[texture.index];
		BindingHotRecord& binding = window.bindingsByTextureIndex[texture.index];
		const bool hit = binding.textureGeneration == texture.generation &&
			binding.textureRevision == hot.revision && binding.descriptorIndex != 0;
		if (hit) {
#if FLOW_UI_DEV_MODE
			++telemetry.bindingCacheHits;
#endif
		} else {
#if FLOW_UI_DEV_MODE
			++telemetry.bindingCacheMisses;
#endif
			if (binding.descriptorIndex == 0) {
				if (!window.freeDescriptorIndices.empty()) {
					binding.descriptorIndex = window.freeDescriptorIndices.back();
					window.freeDescriptorIndices.pop_back();
				} else {
					if (window.nextDescriptorIndex >= window.desc.maxTextureBindings) {
						storageError("window texture descriptor capacity was exhausted");
					}
					binding.descriptorIndex = window.nextDescriptorIndex++;
				}
#if FLOW_UI_DEV_MODE
				++window.liveBindings;
#endif
			}

			binding.textureGeneration = texture.generation;
			binding.textureRevision = hot.revision;
			if (window.bindingRevision == std::numeric_limits<uint32_t>::max()) {
				storageError("window binding revision space exhausted");
			}
			binding.bindingRevision = ++window.bindingRevision;
			binding.state = hot.state;
			binding.nativeImageView = fallbackBinding.nativeImageView;
			binding.nativeSampler = fallbackBinding.nativeSampler;
			if (hot.external && hot.state == ResourceState::Ready) {
				binding.nativeImageView = hot.nativeImageView;
				binding.nativeSampler = hot.nativeSampler;
			} else if (hot.state == ResourceState::Ready && hot.imageViewIndex < imageViewHot.size()) {
				const ImageViewHotRecord& view = imageViewHot[hot.imageViewIndex];
				if (view.generation == hot.imageViewGeneration) binding.nativeImageView = view.nativeImageView;
				if (hot.samplerIndex < samplerHot.size()) {
				const SamplerHotRecord& sampler = samplerHot[hot.samplerIndex];
				if (sampler.generation == hot.samplerGeneration) binding.nativeSampler = sampler.nativeSampler;
				}
			}
		}

		addUse(frame, ResourceKind::TextureView, texture.packed());
		if (binding.nativeImageView == fallbackBinding.nativeImageView &&
			binding.nativeSampler == fallbackBinding.nativeSampler && texture != fallbackTexture) {
			addUse(frame, ResourceKind::TextureView, fallbackTexture.packed());
		}
		return ResolvedTextureBinding{
			.descriptorIndex = binding.descriptorIndex,
			.bindingRevision = binding.bindingRevision,
			.state = binding.state,
			.nativeImageView = binding.nativeImageView,
			.nativeSampler = binding.nativeSampler,
		};
	}

	void releaseBufferReference(BufferHandle handle, SubmissionSerial lastUse) {
		if (!validBuffer(handle)) return;
		BufferRecord& record = buffers[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::Buffer, handle.packed(), record.lastUse, record.allocationBytes);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
	}

	void releaseImageReference(ImageHandle handle, SubmissionSerial lastUse) {
		if (!validImage(handle)) return;
		ImageRecord& record = images[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::Image, handle.packed(), record.lastUse, record.byteSize);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
	}

	void releaseImageViewReference(ImageViewHandle handle, SubmissionSerial lastUse) {
		if (!validImageView(handle)) return;
		ImageViewRecord& record = imageViews[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::ImageView, handle.packed(), record.lastUse);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
	}

	void releaseSamplerReference(SamplerHandle handle, SubmissionSerial lastUse) {
		if (!validSampler(handle)) return;
		SamplerRecord& record = samplers[handle.index];
		record.lastUse = std::max(record.lastUse, lastUse);
		if (record.referenceCount == 0) return;
		if (record.referenceCount > 1) { --record.referenceCount; return; }
		enqueueRetirement(RetirementKind::Sampler, handle.packed(), record.lastUse);
		record.referenceCount = 0;
		record.state = ResourceState::Retiring;
		samplerByKey.erase(record.key);
	}

	void releaseTextureReference(TextureHandle handle, SubmissionSerial lastUse) {
		if (!validTexture(handle)) return;
		TextureHotRecord& hot = textureHot[handle.index];
		TextureColdRecord& cold = textureCold[handle.index];
		cold.lastUse = std::max(cold.lastUse, lastUse);
		if (cold.referenceCount == 0) return;
		if (cold.referenceCount > 1) { --cold.referenceCount; return; }
		enqueueRetirement(RetirementKind::Texture, handle.packed(), cold.lastUse);
		cold.referenceCount = 0;
		hot.state = ResourceState::Retiring;
		for (auto& [_, window] : windows) {
			if (window->bindingsByTextureIndex.size() > handle.index) {
				const BindingHotRecord& binding = window->bindingsByTextureIndex[handle.index];
				if (binding.textureGeneration == handle.generation && binding.descriptorIndex != 0) {
#if FLOW_UI_DEV_MODE
					++window->retiredBindings;
#endif
				}
			}
		}
	}

	void releaseUsed(const UsedResource& used, SubmissionSerial lastUse) {
		switch (used.kind) {
		case ResourceKind::GpuBuffer:
			releaseBufferReference(BufferHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::GpuImage:
			releaseImageReference(ImageHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::ImageView:
			releaseImageViewReference(ImageViewHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::Sampler:
			releaseSamplerReference(SamplerHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::TextureView:
			releaseTextureReference(TextureHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::RendererLayout:
			releaseRendererLayoutReference(RendererLayoutHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::RendererPipelineBundle:
			releaseRendererPipelineBundleReference(
				RendererPipelineBundleHandle::fromPacked(used.packedHandle), lastUse);
			break;
		case ResourceKind::WindowDescriptorBundle:
			releaseWindowDescriptorBundleReference(
				WindowDescriptorBundleHandle::fromPacked(used.packedHandle), lastUse);
			break;
		default:
			break;
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
			const bool external = cold.external;
			hot = TextureHotRecord{.generation = nextGeneration(hot.generation)};
			cold = {};
			freeTextures.push_back(handle.index);
			if (!external) {
				releaseImageViewReference(view, request.retireAfter);
				releaseSamplerReference(sampler, request.retireAfter);
			}
			for (auto& [_, window] : windows) {
				if (window->bindingsByTextureIndex.size() <= handle.index) continue;
				BindingHotRecord& binding = window->bindingsByTextureIndex[handle.index];
				if (binding.descriptorIndex != 0) {
					window->freeDescriptorIndices.push_back(binding.descriptorIndex);
#if FLOW_UI_DEV_MODE
					if (window->liveBindings > 0) --window->liveBindings;
					if (window->retiredBindings > 0) --window->retiredBindings;
#endif
				}
				binding = {};
			}
			break;
		}
		case RetirementKind::RendererPipelineBundle: {
			const RendererPipelineBundleHandle handle =
				RendererPipelineBundleHandle::fromPacked(request.packedHandle);
			if (handle.index >= rendererPipelineBundles.size()) break;
			RendererPipelineBundleRecord& record = rendererPipelineBundles[handle.index];
			if (record.generation != handle.generation) break;
			const RendererLayoutHandle layout = record.key.layout;
			if (vk.device != VK_NULL_HANDLE) {
				for (uint64_t native : record.native.pipelines) {
					if (native != 0) vkDestroyPipeline(vk.device, fromNativeHandle<VkPipeline>(native), nullptr);
				}
			}
			record = RendererPipelineBundleRecord{.generation = nextGeneration(record.generation)};
			freeRendererPipelineBundles.push_back(handle.index);
			releaseRendererLayoutReference(layout, request.retireAfter);
			break;
		}
		case RetirementKind::WindowDescriptorBundle: {
			const WindowDescriptorBundleHandle handle =
				WindowDescriptorBundleHandle::fromPacked(request.packedHandle);
			if (handle.index >= windowDescriptorBundles.size()) break;
			WindowDescriptorBundleRecord& record = windowDescriptorBundles[handle.index];
			if (record.generation != handle.generation) break;
			const RendererLayoutHandle layout = record.desc.layout;
			if (record.nativePool != 0 && vk.device != VK_NULL_HANDLE) {
				vkDestroyDescriptorPool(vk.device, fromNativeHandle<VkDescriptorPool>(record.nativePool), nullptr);
			}
			record = WindowDescriptorBundleRecord{.generation = nextGeneration(record.generation)};
			freeWindowDescriptorBundles.push_back(handle.index);
			releaseRendererLayoutReference(layout, request.retireAfter);
			break;
		}
		case RetirementKind::RendererLayout: {
			const RendererLayoutHandle handle = RendererLayoutHandle::fromPacked(request.packedHandle);
			if (handle.index >= rendererLayouts.size()) break;
			RendererLayoutRecord& record = rendererLayouts[handle.index];
			if (record.generation != handle.generation) break;
			if (vk.device != VK_NULL_HANDLE) {
				if (record.native.pipelineLayout != 0) {
					vkDestroyPipelineLayout(
						vk.device, fromNativeHandle<VkPipelineLayout>(record.native.pipelineLayout), nullptr);
				}
				if (record.native.texturesSetLayout != 0) {
					vkDestroyDescriptorSetLayout(
						vk.device, fromNativeHandle<VkDescriptorSetLayout>(record.native.texturesSetLayout), nullptr);
				}
				if (record.native.globalsSetLayout != 0) {
					vkDestroyDescriptorSetLayout(
						vk.device, fromNativeHandle<VkDescriptorSetLayout>(record.native.globalsSetLayout), nullptr);
				}
			}
			record = RendererLayoutRecord{.generation = nextGeneration(record.generation)};
			freeRendererLayouts.push_back(handle.index);
			break;
		}
		}
		gpuRetiredBytes -= std::min(gpuRetiredBytes, retired.byteSize);
	}

	void immediateDestroyAll() noexcept {
		try {
			if (vk.device != VK_NULL_HANDLE) {
				for (size_t i = 1; i < windowDescriptorBundles.size(); ++i) {
					const WindowDescriptorBundleRecord& record = windowDescriptorBundles[i];
					if (record.state != ResourceState::Invalid && record.nativePool != 0) {
						vkDestroyDescriptorPool(
							vk.device, fromNativeHandle<VkDescriptorPool>(record.nativePool), nullptr);
					}
				}
				for (size_t i = 1; i < rendererPipelineBundles.size(); ++i) {
					const RendererPipelineBundleRecord& record = rendererPipelineBundles[i];
					if (record.state == ResourceState::Invalid) continue;
					for (uint64_t native : record.native.pipelines) {
						if (native != 0) vkDestroyPipeline(vk.device, fromNativeHandle<VkPipeline>(native), nullptr);
					}
				}
				for (size_t i = 1; i < rendererLayouts.size(); ++i) {
					const RendererLayoutRecord& record = rendererLayouts[i];
					if (record.state == ResourceState::Invalid) continue;
					if (record.native.pipelineLayout != 0) {
						vkDestroyPipelineLayout(
							vk.device, fromNativeHandle<VkPipelineLayout>(record.native.pipelineLayout), nullptr);
					}
					if (record.native.texturesSetLayout != 0) {
						vkDestroyDescriptorSetLayout(
							vk.device, fromNativeHandle<VkDescriptorSetLayout>(record.native.texturesSetLayout), nullptr);
					}
					if (record.native.globalsSetLayout != 0) {
						vkDestroyDescriptorSetLayout(
							vk.device, fromNativeHandle<VkDescriptorSetLayout>(record.native.globalsSetLayout), nullptr);
					}
				}
			}
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
	if (impl_->terminated) storageError("a shut-down FlowStorageSystem instance cannot be reinitialized");
	if (impl_->vk.device == VK_NULL_HANDLE || impl_->vk.allocator == nullptr) {
		storageError("a valid Vulkan device and VMA allocator are required");
	}
	if (config.growthFactor <= 1.0f) storageError("growthFactor must be greater than one");
	if (config.cpuSoftBudgetBytes == 0 || config.gpuSoftBudgetBytes == 0) {
		storageError("storage budgets must be non-zero");
	}
	if (config.defaultBufferWriteMode != BufferWriteMode::DirectMapped &&
		config.defaultBufferWriteMode != BufferWriteMode::HostScratchThenCopy) {
		storageError("defaultBufferWriteMode must select a concrete mapped write path");
	}

	impl_->config = config;
	impl_->config.framesInFlight = std::max(1u, config.framesInFlight);
	impl_->config.expectedWorkerCount = std::max(1u, config.expectedWorkerCount);
	impl_->persistentPool.initialize(
		checkedSize(config.initialPersistentCpuBytes, "initial persistent pool size exceeds the host address space"),
		config.growthFactor, config.allowRuntimeGrowth);
	impl_->stringPool.initialize(
		checkedSize(config.initialStringBytes, "initial string pool size exceeds the host address space"),
		config.growthFactor, config.allowRuntimeGrowth);
	impl_->blobs.reserve(static_cast<size_t>(config.expectedBlobs) + 1u);
	impl_->buffers.reserve(static_cast<size_t>(config.expectedBuffers) + 1u);
	impl_->images.reserve(static_cast<size_t>(config.expectedImages) + 1u);
	impl_->imageViews.reserve(static_cast<size_t>(config.expectedImageViews) + 1u);
	impl_->imageViewHot.reserve(static_cast<size_t>(config.expectedImageViews) + 1u);
	impl_->samplers.reserve(static_cast<size_t>(config.expectedSamplers) + 1u);
	impl_->samplerHot.reserve(static_cast<size_t>(config.expectedSamplers) + 1u);
	impl_->textureHot.reserve(static_cast<size_t>(config.expectedTextureViews) + 1u);
	impl_->textureCold.reserve(static_cast<size_t>(config.expectedTextureViews) + 1u);
	impl_->rendererLayouts.reserve(static_cast<size_t>(config.expectedRendererObjects) + 1u);
	impl_->rendererPipelineBundles.reserve(static_cast<size_t>(config.expectedRendererObjects) + 1u);
	impl_->windowDescriptorBundles.reserve(static_cast<size_t>(config.expectedRendererObjects) + 1u);
	impl_->retirements.reserve(
		static_cast<size_t>(config.expectedBlobs) + config.expectedBuffers + config.expectedImages +
		config.expectedImageViews + config.expectedSamplers + config.expectedTextureViews +
		static_cast<size_t>(config.expectedRendererObjects) * 3u);
	impl_->windows.reserve(config.expectedWindows);
	impl_->registeredWindowIds.reserve(config.expectedWindows);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = impl_->vk.graphicsQFamily;
	checkVk(vkCreateCommandPool(impl_->vk.device, &poolInfo, nullptr, &impl_->uploadCommandPool),
		"failed to create central upload command pool");

	impl_->cpuSoftBudgetBytes = config.cpuSoftBudgetBytes;
	impl_->gpuSoftBudgetBytes = config.gpuSoftBudgetBytes;
#if FLOW_UI_DEV_MODE
	impl_->telemetry.cpuSoftBudgetBytes = config.cpuSoftBudgetBytes;
	impl_->telemetry.gpuSoftBudgetBytes = config.gpuSoftBudgetBytes;
#endif
	impl_->initialized = true;
}

void FlowStorageSystem::shutdown() noexcept {
	if (!impl_) return;
	std::unique_lock lock(impl_->mutex);
	if (!impl_->initialized) return;
	impl_->bufferCommitCondition.wait(lock, [this] {
		for (const auto& [_, window] : impl_->windows) {
			for (const auto& frame : window->frames) {
				if (frame->activeBufferCommits != 0) return false;
			}
		}
		return true;
	});
	if (impl_->vk.device != VK_NULL_HANDLE) vkDeviceWaitIdle(impl_->vk.device);
	for (auto& [_, window] : impl_->windows) {
		for (auto& frame : window->frames) {
			Impl::invalidateLease(*frame);
			Impl::invalidateArena(*frame);
		}
	}
	impl_->immediateDestroyAll();
	impl_->uploads.clear();
	impl_->uploadStates.clear();
	impl_->retirements.clear();
	impl_->windows.clear();
	impl_->registeredWindowIds.clear();
	impl_->textureByKey.clear();
	impl_->samplerByKey.clear();
	impl_->rendererLayoutByKey.clear();
	impl_->rendererPipelineByKey.clear();
	impl_->persistentPool.clear();
	impl_->stringPool.clear();
	impl_->strings.assign(1u, std::string_view{});
	impl_->stringIds.clear();
	impl_->blobs.assign(1u, Impl::BlobRecord{});
	impl_->freeBlobs.clear();
	impl_->buffers.assign(1u, Impl::BufferRecord{});
	impl_->freeBuffers.clear();
	impl_->images.assign(1u, Impl::ImageRecord{});
	impl_->freeImages.clear();
	impl_->imageViews.assign(1u, Impl::ImageViewRecord{});
	impl_->imageViewHot.assign(1u, ImageViewHotRecord{});
	impl_->freeImageViews.clear();
	impl_->samplers.assign(1u, Impl::SamplerRecord{});
	impl_->samplerHot.assign(1u, SamplerHotRecord{});
	impl_->freeSamplers.clear();
	impl_->textureHot.assign(1u, TextureHotRecord{.generation = 1, .revision = 1, .state = ResourceState::Ready});
	impl_->textureCold.assign(1u, Impl::TextureColdRecord{});
	impl_->freeTextures.clear();
	impl_->fallbackTexture = {};
	impl_->fallbackBinding = {};
	impl_->rendererLayouts.assign(1u, Impl::RendererLayoutRecord{});
	impl_->freeRendererLayouts.clear();
	impl_->rendererPipelineBundles.assign(1u, Impl::RendererPipelineBundleRecord{});
	impl_->freeRendererPipelineBundles.clear();
	impl_->windowDescriptorBundles.assign(1u, Impl::WindowDescriptorBundleRecord{});
	impl_->freeWindowDescriptorBundles.clear();
	impl_->completedOutOfOrder.clear();
	impl_->gpuLiveBytes = 0;
	impl_->gpuRetiredBytes = 0;
	impl_->nextUploadId = 1;
	impl_->nextFrameEpoch = 1;
	impl_->nextReadLeaseId = 1;
	impl_->nextBufferWriteId = 1;
	impl_->nextSubmissionSerial = 1;
	impl_->completedWatermark = 0;
#if FLOW_UI_DEV_MODE
	impl_->telemetry = {};
#endif
	impl_->initialized = false;
	impl_->terminated = true;
}

uint32_t FlowStorageSystem::interfaceVersion() const noexcept { return CurrentInterfaceVersion; }
uint64_t FlowStorageSystem::capabilities() const noexcept { return kCapabilityMask; }

void FlowStorageSystem::registerWindow(WindowId id, const WindowStorageDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (id == 0) storageError("window id zero is reserved");
	if (impl_->registeredWindowIds.contains(id)) {
		storageError("window ids cannot be reused during one storage-system lifetime");
	}

	auto window = std::make_unique<Impl::WindowState>();
	window->id = id;
	window->desc = desc;
	window->desc.framesInFlight = std::max(1u, desc.framesInFlight);
	window->desc.workerCount = std::max(1u, desc.workerCount);
	window->desc.maxTextureBindings = std::max(1u, desc.maxTextureBindings);
	if (desc.initialTextureBindings > window->desc.maxTextureBindings) {
		storageError("initial texture binding count exceeds the window binding limit");
	}
	window->bindingsByTextureIndex.reserve(std::max({
		static_cast<size_t>(desc.initialTextureBindings),
		impl_->textureHot.size(),
		checkedSize(static_cast<uint64_t>(impl_->config.expectedTextureViews) + 1u,
			"expected texture-view count exceeds the host address space"),
	}));
	window->bindingsByTextureIndex.resize(impl_->textureHot.size());
	if (!window->bindingsByTextureIndex.empty() && impl_->fallbackTexture) {
		window->bindingsByTextureIndex[0] = impl_->fallbackBinding;
		if (window->bindingsByTextureIndex.size() <= impl_->fallbackTexture.index) {
			window->bindingsByTextureIndex.resize(static_cast<size_t>(impl_->fallbackTexture.index) + 1u);
		}
		window->bindingsByTextureIndex[impl_->fallbackTexture.index] = impl_->fallbackBinding;
	}
	window->frames.reserve(window->desc.framesInFlight);
	const auto expectedSlots = [](uint32_t expected) {
		return checkedSize(static_cast<uint64_t>(expected) + 1u,
			"expected resource count exceeds the host address space");
	};
	for (uint32_t slot = 0; slot < window->desc.framesInFlight; ++slot) {
		auto frame = std::make_unique<Impl::FrameState>();
		frame->transient.initialize(checkedSize(desc.transientBytesPerFrame,
			"window frame arena size exceeds the host address space"), impl_->config.growthFactor,
			impl_->config.allowRuntimeGrowth);
		frame->decode.initialize(checkedSize(impl_->config.initialDecodeScratchBytes,
			"decode arena size exceeds the host address space"), impl_->config.growthFactor,
			impl_->config.allowRuntimeGrowth);
		frame->workers.reserve(window->desc.workerCount);
		frame->pendingBufferWrites.reserve(4u);
		frame->used.reserve(std::max<size_t>(
			64u, expectedSlots(impl_->config.expectedBindingsPerWindow) + 8u));
		frame->usedBufferEpochs.resize(expectedSlots(impl_->config.expectedBuffers));
		frame->usedImageEpochs.resize(expectedSlots(impl_->config.expectedImages));
		frame->usedImageViewEpochs.resize(expectedSlots(impl_->config.expectedImageViews));
		frame->usedSamplerEpochs.resize(expectedSlots(impl_->config.expectedSamplers));
		frame->usedTextureEpochs.resize(expectedSlots(impl_->config.expectedTextureViews));
		frame->usedRendererLayoutEpochs.resize(expectedSlots(impl_->config.expectedRendererObjects));
		frame->usedRendererPipelineEpochs.resize(expectedSlots(impl_->config.expectedRendererObjects));
		frame->usedDescriptorBundleEpochs.resize(expectedSlots(impl_->config.expectedRendererObjects));
		// These arrays are indexed by descriptor slot. Reserve the hard window
		// bound once so dirty-batch preparation cannot reallocate in a hot frame.
		frame->appliedBindingRevisions.reserve(window->desc.maxTextureBindings);
		frame->preparedBindingBatches.reserve(window->desc.maxTextureBindings);
		for (uint32_t worker = 0; worker < window->desc.workerCount; ++worker) {
			auto arena = std::make_unique<LinearArena>();
			arena->initialize(checkedSize(desc.transientBytesPerWorker,
				"window worker arena size exceeds the host address space"), impl_->config.growthFactor,
				impl_->config.allowRuntimeGrowth);
			frame->workers.push_back(std::move(arena));
		}
		window->frames.push_back(std::move(frame));
	}
	auto [registered, inserted] = impl_->windows.emplace(id, std::move(window));
	if (!inserted) storageError("window id is already registered");
	try {
		if (!impl_->registeredWindowIds.insert(id).second) {
			storageError("window id tombstone insertion collided");
		}
	} catch (...) {
		impl_->windows.erase(registered);
		throw;
	}
#if FLOW_UI_DEV_MODE
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
#endif
}

void FlowStorageSystem::unregisterWindow(WindowId id, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	auto it = impl_->windows.find(id);
	if (it == impl_->windows.end()) return;
	SubmissionSerial highestOutstanding = 0;
	for (const auto& frame : it->second->frames) {
		if (frame->active) storageError("window cannot be unregistered while it has an active frame");
		highestOutstanding = std::max(highestOutstanding, frame->inFlightSerial);
	}
	if (lastUse < highestOutstanding) {
		storageError("window last-use serial precedes an outstanding frame submission");
	}
	lastUse = std::max(lastUse, highestOutstanding);
	if (it->second->activeDescriptorBundle) {
		impl_->releaseWindowDescriptorBundleReference(it->second->activeDescriptorBundle, lastUse);
		it->second->activeDescriptorBundle = {};
	}
	if (highestOutstanding == 0 && lastUse <= impl_->completedWatermark) {
		impl_->windows.erase(it);
	} else {
		it->second->closing = true;
		it->second->retireAfter = lastUse;
	}
#if FLOW_UI_DEV_MODE
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
#endif
}

FrameToken FlowStorageSystem::beginFrame(WindowId id, const FrameStorageDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	Impl::WindowState& window = impl_->requireWindow(id);
	if (desc.frameSlot >= window.frames.size()) storageError("beginFrame frame slot is out of range");
	Impl::FrameState& frame = *window.frames[desc.frameSlot];
	if (frame.active) storageError("frame slot is already active; completion/reuse protocol was violated");
	if (frame.inFlightSerial != 0) {
		storageError("frame slot cannot be reused before its exact submission token completes");
	}
#if FLOW_UI_DEV_MODE
	auto arenaValidation = std::make_shared<ArenaLeaseState>();
#endif
	frame.transient.reset();
	frame.decode.reset();
	for (const auto& worker : frame.workers) worker->reset();
	frame.used.clear();
	frame.pendingBufferWrites.clear();
	Impl::invalidateLease(frame);
	Impl::invalidateArena(frame);
	frame.frameNumber = desc.frameNumber;
	if (impl_->nextFrameEpoch == 0 || impl_->nextFrameEpoch == std::numeric_limits<FrameEpoch>::max()) {
		storageError("frame epoch space exhausted");
	}
	frame.epoch = impl_->nextFrameEpoch++;
	frame.currentBindingBatch = 0;
	frame.active = true;
	frame.sealed = false;
#if FLOW_UI_DEV_MODE
	frame.arenaValidation = std::move(arenaValidation);
#endif
	return FrameToken{id, desc.frameSlot, desc.frameNumber, frame.epoch};
}

FrameReadLease FlowStorageSystem::sealFrame(const FrameToken& frame) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	if (!state.pendingBufferWrites.empty()) storageError("all buffer writes must be committed before sealing a frame");
	const Impl::WindowState& window = impl_->requireWindow(frame.window);
	if (window.activeDescriptorBundle) {
		// The active per-window descriptor generation is an implicit dependency of
		// every submitted frame. Explicit trackUses calls remain harmlessly deduped.
		impl_->addUse(state, ResourceKind::WindowDescriptorBundle,
			window.activeDescriptorBundle.packed());
	}
	if (impl_->nextReadLeaseId == 0 || impl_->nextReadLeaseId == std::numeric_limits<uint64_t>::max()) {
		storageError("frame read lease id space exhausted");
	}
#if FLOW_UI_DEV_MODE
	auto leaseValidation = std::make_shared<ReadLeaseState>();
#endif
	Impl::invalidateArena(state);
	state.sealed = true;
	state.leaseId = impl_->nextReadLeaseId++;
	FrameReadLease lease{.frame = frame, .leaseId = state.leaseId};
#if FLOW_UI_DEV_MODE
	state.leaseValidation = std::move(leaseValidation);
	lease.validation = state.leaseValidation;
#endif
	return lease;
}

void FlowStorageSystem::cancelFrame(const FrameToken& frame) noexcept {
	try {
		std::unique_lock lock(impl_->mutex);
		Impl::FrameState& state = impl_->requireFrame(frame);
		impl_->bufferCommitCondition.wait(lock, [&state] { return state.activeBufferCommits == 0; });
		const size_t pendingReleases = state.pendingBufferWrites.size() + state.used.size();
		impl_->reserveRetirements(pendingReleases);
		for (const Impl::PendingBufferWrite& write : state.pendingBufferWrites) {
			impl_->releaseBufferReference(write.buffer, 0);
		}
		state.pendingBufferWrites.clear();
		for (const UsedResource& used : state.used) impl_->releaseUsed(used, 0);
		state.used.clear();
		Impl::invalidateLease(state);
		Impl::invalidateArena(state);
		state.active = false;
		state.sealed = false;
	} catch (...) {
	}
}

MemoryBlock FlowStorageSystem::allocatePersistent(
	size_t bytes, size_t alignment, MemoryClass memoryClass, StringId debugName) {
	return allocatePersistent(bytes, alignment,
		AllocationTag{memoryClass, ResourceKind::Invalid, 0, InvalidFrameSlot, debugName});
}

MemoryBlock FlowStorageSystem::allocatePersistent(
	size_t bytes, size_t alignment, const AllocationTag& tag) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (static_cast<uint8_t>(tag.memoryClass) >= static_cast<uint8_t>(MemoryClass::Count)) {
		storageError("persistent allocation uses an invalid memory class");
	}
	if (tag.memoryClass == MemoryClass::FrameTransient || tag.memoryClass == MemoryClass::WorkerTransient ||
		tag.memoryClass == MemoryClass::DecodeTransient) {
		storageError("transient memory classes must use an arena view");
	}
	if (tag.memoryClass == MemoryClass::WindowPersistent) {
		if (tag.window == 0 || !impl_->windows.contains(tag.window)) {
			storageError("window-persistent allocation requires a registered window attribution");
		}
	}
	impl_->requireCpuBudget(bytes);
	PersistentPool& pool = tag.memoryClass == MemoryClass::StringPool ? impl_->stringPool : impl_->persistentPool;
	return pool.allocate(bytes, alignment, tag);
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
	ArenaView view{.context = arena, .allocateFunction = &LinearArena::arenaAllocate, .epoch = frame.epoch};
#if FLOW_UI_DEV_MODE
	view.validation = state.arenaValidation;
#endif
	return view;
}

ArenaView FlowStorageSystem::workerArena(const FrameToken& frame, uint32_t workerIndex) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	if (workerIndex >= state.workers.size()) storageError("worker arena index is out of range");
	ArenaView view{
		.context = state.workers[workerIndex].get(),
		.allocateFunction = &LinearArena::arenaAllocate,
		.epoch = frame.epoch,
	};
#if FLOW_UI_DEV_MODE
	view.validation = state.arenaValidation;
#endif
	return view;
}

BufferWriteView FlowStorageSystem::beginBufferWrite(
	const FrameToken& frame,
	BufferHandle buffer,
	uint64_t destinationOffset,
	uint64_t bytes,
	BufferWriteMode mode) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	if (!impl_->usableBuffer(buffer)) storageError("beginBufferWrite received an invalid or retiring buffer handle");
	if (bytes == 0) storageError("buffer writes must contain at least one byte");
	Impl::BufferRecord& record = impl_->buffers[buffer.index];
	if (record.desc.access != AccessMode::CpuWrite && record.desc.access != AccessMode::CpuAndGpuWrite) {
		storageError("buffer write requires CpuWrite or CpuAndGpuWrite access");
	}
	if (!Impl::visibleToFrame(
		record.desc.sharing, record.desc.window, record.desc.frameSlot, frame)) {
		storageError("buffer write frame does not own the destination resource scope");
	}
	if (destinationOffset > record.size || bytes > record.size - destinationOffset) {
		storageError("buffer write range exceeds destination buffer");
	}
	if (record.mapped == nullptr) storageError("buffer writes require a persistently mapped destination buffer");
	for (const auto& [_, window] : impl_->windows) {
		for (const auto& frameState : window->frames) {
			for (const Impl::PendingBufferWrite& active : frameState->pendingBufferWrites) {
				if (active.buffer != buffer) continue;
				const uint64_t activeEnd = active.destinationOffset + active.capacity;
				const uint64_t requestedEnd = destinationOffset + bytes;
				if (destinationOffset < activeEnd && active.destinationOffset < requestedEnd) {
					storageError("overlapping active writes to the same buffer are not allowed across frame scopes");
				}
			}
		}
	}
	if (mode == BufferWriteMode::Default) mode = impl_->config.defaultBufferWriteMode;
	if (mode == BufferWriteMode::Default) mode = BufferWriteMode::DirectMapped;

	std::byte* writeData = nullptr;
	if (mode == BufferWriteMode::DirectMapped) {
		writeData = static_cast<std::byte*>(record.mapped) + destinationOffset;
	} else if (mode == BufferWriteMode::HostScratchThenCopy) {
		if (bytes > std::numeric_limits<size_t>::max()) storageError("host scratch buffer write is too large");
		writeData = static_cast<std::byte*>(state.transient.allocate(static_cast<size_t>(bytes), alignof(std::max_align_t)));
	} else {
		storageError("unsupported buffer write mode");
	}
	if (!writeData) storageError("failed to allocate buffer write memory");

	if (impl_->nextBufferWriteId == 0 || impl_->nextBufferWriteId == std::numeric_limits<uint64_t>::max()) {
		storageError("buffer write id space exhausted");
	}
	const uint64_t writeId = impl_->nextBufferWriteId++;
	impl_->retainBuffer(buffer);
	try {
		state.pendingBufferWrites.push_back(Impl::PendingBufferWrite{
			.id = writeId,
			.buffer = buffer,
			.data = writeData,
			.destinationOffset = destinationOffset,
			.capacity = bytes,
			.mode = mode,
		});
	} catch (...) {
		impl_->buffers[buffer.index].referenceCount--;
		throw;
	}

	return BufferWriteView{
		.buffer = buffer,
		.data = writeData,
		.destinationOffset = destinationOffset,
		.capacity = bytes,
		.epoch = frame.epoch,
		.writeId = writeId,
		.mode = mode,
	};
}

void FlowStorageSystem::commitBufferWrite(
	const FrameToken& frame,
	const BufferWriteView& write,
	uint64_t bytesWritten) {
	commitBufferWriteInternal(frame, write, bytesWritten, nullptr);
}

void FlowStorageSystem::commitBufferWriteInternal(
	const FrameToken& frame,
	const BufferWriteView& write,
	uint64_t bytesWritten,
	const std::byte* sourceData) {
	std::unique_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	if (!write || write.epoch != frame.epoch) storageError("buffer write view is invalid or stale");
	auto pendingIt = std::find_if(
		state.pendingBufferWrites.begin(), state.pendingBufferWrites.end(),
		[&](const Impl::PendingBufferWrite& pending) { return pending.id == write.writeId; });
	if (pendingIt == state.pendingBufferWrites.end()) storageError("buffer write was not found or was already committed");
	const Impl::PendingBufferWrite pending = *pendingIt;
	if (pending.buffer != write.buffer || pending.data != write.data || pending.capacity != write.capacity ||
		pending.destinationOffset != write.destinationOffset || pending.mode != write.mode) {
		storageError("buffer write view does not match the active write");
	}
	if (pending.committing) storageError("buffer write is already being committed");
	if (bytesWritten > pending.capacity) storageError("committed byte count exceeds buffer write capacity");
	if (!impl_->validBuffer(pending.buffer)) storageError("buffer was invalidated during an active write");

	Impl::BufferRecord& record = impl_->buffers[pending.buffer.index];
	void* destination = static_cast<std::byte*>(record.mapped) + pending.destinationOffset;
	const VmaAllocation allocation = record.allocation;
	const bool hostCoherent = (record.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
	impl_->retainBuffer(pending.buffer);
	pendingIt->committing = true;
	++state.activeBufferCommits;
	lock.unlock();
	try {
		if (bytesWritten > 0 && sourceData != nullptr) {
			std::memcpy(pending.data, sourceData, static_cast<size_t>(bytesWritten));
		}
		if (bytesWritten > 0 && pending.mode == BufferWriteMode::HostScratchThenCopy) {
			std::memcpy(destination, pending.data, static_cast<size_t>(bytesWritten));
		}
		if (bytesWritten > 0 && !hostCoherent) {
			checkVk(vmaFlushAllocation(
				impl_->vk.allocator, allocation, pending.destinationOffset, bytesWritten),
				"failed to flush mapped buffer write");
		}
	} catch (...) {
		lock.lock();
		auto currentWrite = std::find_if(
			state.pendingBufferWrites.begin(), state.pendingBufferWrites.end(),
			[&](const Impl::PendingBufferWrite& candidate) { return candidate.id == pending.id; });
		if (currentWrite != state.pendingBufferWrites.end()) currentWrite->committing = false;
		--state.activeBufferCommits;
		impl_->bufferCommitCondition.notify_all();
		// The pending write keeps its own reference, so releasing this temporary
		// commit pin cannot be the operation that queues retirement.
		impl_->releaseBufferReference(pending.buffer, 0);
		throw;
	}
	lock.lock();
	auto currentWrite = std::find_if(
		state.pendingBufferWrites.begin(), state.pendingBufferWrites.end(),
		[&](const Impl::PendingBufferWrite& candidate) { return candidate.id == pending.id; });
	if (currentWrite == state.pendingBufferWrites.end()) {
		--state.activeBufferCommits;
		impl_->bufferCommitCondition.notify_all();
		impl_->releaseBufferReference(pending.buffer, 0);
		storageError("buffer write disappeared while it was being committed");
	}
	try {
		impl_->addUse(state, ResourceKind::GpuBuffer, pending.buffer.packed());
	} catch (...) {
		currentWrite->committing = false;
		--state.activeBufferCommits;
		impl_->bufferCommitCondition.notify_all();
		impl_->releaseBufferReference(pending.buffer, 0);
		throw;
	}
	state.pendingBufferWrites.erase(currentWrite);
	// Drop the pending-write reference and then the temporary commit pin. The
	// generation-safe frame-use reference remains alive through submission.
	impl_->releaseBufferReference(pending.buffer, 0);
	impl_->releaseBufferReference(pending.buffer, 0);
	--state.activeBufferCommits;
	impl_->bufferCommitCondition.notify_all();
}

void FlowStorageSystem::writeBuffer(
	const FrameToken& frame,
	BufferHandle buffer,
	uint64_t destinationOffset,
	std::span<const std::byte> bytes) {
	if (bytes.empty()) return;
	// The caller's span is already host scratch. Use a direct lease and let the
	// common commit path pin the allocation while performing exactly one unlocked
	// copy, regardless of the producer default selected in StorageConfig.
	BufferWriteView write = beginBufferWrite(
		frame, buffer, destinationOffset, bytes.size(), BufferWriteMode::DirectMapped);
	commitBufferWriteInternal(frame, write, bytes.size(), bytes.data());
}

StringId FlowStorageSystem::intern(std::string_view value) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (value.empty()) return 0;
	if (const auto it = impl_->stringIds.find(value); it != impl_->stringIds.end()) return it->second;
	if (value.size() == std::numeric_limits<size_t>::max()) storageError("interned string size overflow");
	if (impl_->strings.size() >= std::numeric_limits<StringId>::max()) storageError("string id table exhausted");
	impl_->requireCpuBudget(value.size() + 1u);
	MemoryBlock block = impl_->stringPool.allocate(
		value.size() + 1u, alignof(char), AllocationTag{MemoryClass::StringPool, ResourceKind::Invalid, 0, InvalidFrameSlot, 0});
	auto* chars = static_cast<char*>(block.data);
	std::memcpy(chars, value.data(), value.size());
	chars[value.size()] = '\0';
	const std::string_view stable(chars, value.size());
	const StringId id = static_cast<StringId>(impl_->strings.size());
	try {
		impl_->strings.push_back(stable);
		try {
			impl_->stringIds.emplace(stable, id);
		} catch (...) {
			impl_->strings.pop_back();
			throw;
		}
	} catch (...) {
		impl_->stringPool.release(block);
		throw;
	}
	return id;
}

std::string_view FlowStorageSystem::string(StringId id) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return id < impl_->strings.size() ? impl_->strings[id] : std::string_view{};
}

BlobHandle FlowStorageSystem::createBlob(std::span<const std::byte> bytes, StringId debugName) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireCpuBudget(bytes.size());
	const uint32_t index = Impl::acquireIndex(impl_->blobs, impl_->freeBlobs);
	Impl::BlobRecord& record = impl_->blobs[index];
	const uint32_t generation = record.generation == 0 ? 1u : record.generation;
	MemoryBlock memory{};
	try {
		memory = impl_->persistentPool.allocate(bytes.size(), alignof(std::max_align_t),
			AllocationTag{MemoryClass::Persistent, ResourceKind::CpuBlob, 0, InvalidFrameSlot, debugName});
	} catch (...) {
		impl_->freeBlobs.push_back(index);
		throw;
	}
	record.generation = generation;
	record.state = ResourceState::Ready;
	record.referenceCount = 1;
#if FLOW_UI_DEV_MODE
	record.debugName = debugName;
#else
	(void)debugName;
#endif
	record.memory = memory;
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
	if (record.referenceCount == 0) return;
	if (record.referenceCount > 1) {
		--record.referenceCount;
		return;
	}
	impl_->enqueueRetirement(RetirementKind::Blob, handle.packed(), record.lastUse);
	record.referenceCount = 0;
	record.state = ResourceState::Retiring;
}

BufferHandle FlowStorageSystem::createBuffer(const BufferDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (desc.size == 0 || toVkBufferUsage(desc.usage) == 0) storageError("invalid buffer description");
	impl_->validateSharingAttribution(desc.sharing, desc.window, desc.frameSlot, "buffer");
	if (desc.persistentlyMapped && desc.access != AccessMode::CpuWrite && desc.access != AccessMode::CpuAndGpuWrite) {
		storageError("persistently mapped buffers require CPU-write access");
	}
	if (desc.size > impl_->gpuSoftBudgetBytes ||
		impl_->gpuLiveBytes + impl_->gpuRetiredBytes > impl_->gpuSoftBudgetBytes - desc.size)
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
	vmaGetAllocationMemoryProperties(impl_->vk.allocator, record.allocation, &record.memoryProperties);
	record.allocationBytes = std::max<uint64_t>(record.size, resultInfo.size);
	if (record.allocationBytes > impl_->gpuSoftBudgetBytes ||
		impl_->gpuLiveBytes + impl_->gpuRetiredBytes > impl_->gpuSoftBudgetBytes - record.allocationBytes) {
		vmaDestroyBuffer(impl_->vk.allocator, record.buffer, record.allocation);
		record = Impl::BufferRecord{.generation = record.generation};
		impl_->freeBuffers.push_back(index);
		storageError("actual Vulkan buffer allocation exceeds the GPU soft budget");
	}
	record.state = ResourceState::Ready;
	impl_->gpuLiveBytes += record.allocationBytes;
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
	impl_->validateSharingAttribution(desc.sharing, desc.window, desc.frameSlot, "image");
	if (estimatedBytes > impl_->gpuSoftBudgetBytes ||
		impl_->gpuLiveBytes + impl_->gpuRetiredBytes > impl_->gpuSoftBudgetBytes - estimatedBytes)
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
	if (record.byteSize > impl_->gpuSoftBudgetBytes ||
		impl_->gpuLiveBytes + impl_->gpuRetiredBytes > impl_->gpuSoftBudgetBytes - record.byteSize) {
		vmaDestroyImage(impl_->vk.allocator, record.image, record.allocation);
		record = Impl::ImageRecord{.generation = record.generation};
		impl_->freeImages.push_back(index);
		storageError("actual Vulkan image allocation exceeds the GPU soft budget");
	}
	impl_->gpuLiveBytes += record.byteSize;
	impl_->updateGpuPeak();
	return ImageHandle{index, record.generation};
}

ImageViewHandle FlowStorageSystem::createImageView(ImageHandle image, const ImageViewDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (!impl_->usableImage(image)) storageError("createImageView received an invalid or retiring image handle");
	Impl::ImageRecord& imageRecord = impl_->images[image.index];
	if (desc.mipLevelCount == 0 || desc.arrayLayerCount == 0 ||
		desc.baseMipLevel > imageRecord.desc.mipLevels ||
		desc.mipLevelCount > imageRecord.desc.mipLevels - desc.baseMipLevel ||
		desc.baseArrayLayer > imageRecord.desc.layers ||
		desc.arrayLayerCount > imageRecord.desc.layers - desc.baseArrayLayer) {
		storageError("image-view range exceeds image");
	}

	const uint32_t index = impl_->acquireImageViewIndex();
	Impl::ImageViewRecord& record = impl_->imageViews[index];
	record.generation = record.generation == 0 ? 1u : record.generation;
	record.image = image;
	record.desc = desc;
	record.referenceCount = 1;
	record.state = ResourceState::Queued;
	try {
		impl_->retainImage(image);
	} catch (...) {
		record = Impl::ImageViewRecord{.generation = record.generation};
		impl_->imageViewHot[index] = ImageViewHotRecord{.generation = record.generation};
		impl_->freeImageViews.push_back(index);
		throw;
	}

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
	if (vkCreateImageView(impl_->vk.device, &viewInfo, nullptr, &record.view) != VK_SUCCESS) {
		impl_->releaseImageReference(image, 0);
		record = Impl::ImageViewRecord{.generation = record.generation};
		impl_->imageViewHot[index] = ImageViewHotRecord{.generation = record.generation};
		impl_->freeImageViews.push_back(index);
		storageError("failed to create Vulkan image view");
	}
	record.state = ResourceState::Ready;
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
	impl_->requireSharedMutationPhase();
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
	if (vkCreateSampler(impl_->vk.device, &info, nullptr, &record.sampler) != VK_SUCCESS) {
		record = Impl::SamplerRecord{.generation = record.generation};
		impl_->samplerHot[index] = SamplerHotRecord{.generation = record.generation};
		impl_->freeSamplers.push_back(index);
		storageError("failed to create Vulkan sampler");
	}
	record.state = ResourceState::Ready;
	const SamplerHandle handle{index, record.generation};
	try {
		impl_->samplerByKey.emplace(key, handle);
	} catch (...) {
		vkDestroySampler(impl_->vk.device, record.sampler, nullptr);
		record = Impl::SamplerRecord{.generation = record.generation};
		impl_->samplerHot[index] = SamplerHotRecord{.generation = record.generation};
		impl_->freeSamplers.push_back(index);
		throw;
	}
	impl_->samplerHot[index] = SamplerHotRecord{record.generation, 0, nativeHandle(record.sampler)};
	return handle;
}

void FlowStorageSystem::releaseBuffer(BufferHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseBufferReference(handle, lastUse);
}

void FlowStorageSystem::releaseImage(ImageHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseImageReference(handle, lastUse);
}

void FlowStorageSystem::releaseImageView(ImageViewHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseImageViewReference(handle, lastUse);
}

void FlowStorageSystem::releaseSampler(SamplerHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseSamplerReference(handle, lastUse);
}

TextureHandle FlowStorageSystem::publishTexture(ResourceKey key, const TextureViewDesc& desc, bool* inserted) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (!impl_->usableImageView(desc.imageView) || !impl_->usableSampler(desc.sampler))
		storageError("texture publication requires valid image-view and sampler handles");
	impl_->validateTextureOwnership(key, desc);
	if (const auto found = impl_->textureByKey.find(key); found != impl_->textureByKey.end()) {
		if (inserted) *inserted = false;
		return replaceTexture(key, desc);
	}

	const uint32_t index = impl_->acquireTextureIndex();
	TextureHotRecord& hot = impl_->textureHot[index];
	const uint32_t generation = hot.generation == 0 ? 1u : hot.generation;
	try {
		impl_->retainImageView(desc.imageView);
		try {
			impl_->retainSampler(desc.sampler);
		} catch (...) {
			impl_->releaseImageViewReference(desc.imageView, 0);
			throw;
		}
	} catch (...) {
		impl_->freeTextures.push_back(index);
		throw;
	}
	hot.generation = generation;
	hot.revision = 1u;
	hot.imageViewIndex = desc.imageView.index;
	hot.imageViewGeneration = desc.imageView.generation;
	hot.samplerIndex = desc.sampler.index;
	hot.samplerGeneration = desc.sampler.generation;
	const ImageHandle backingImage = impl_->imageViews[desc.imageView.index].image;
	hot.state = impl_->validImage(backingImage) ? impl_->images[backingImage.index].state : ResourceState::Invalid;
	hot.sourceWidth = desc.sourceWidth;
	hot.sourceHeight = desc.sourceHeight;
	impl_->textureCold[index] = Impl::TextureColdRecord{
		.key = key,
		.desc = desc,
		.referenceCount = 1u,
		.published = true,
	};
	const TextureHandle handle{index, hot.generation};
	try {
		if (!impl_->textureByKey.emplace(key, handle).second) {
			storageError("texture key publication collided with a stale entry");
		}
	} catch (...) {
		impl_->releaseImageViewReference(desc.imageView, 0);
		impl_->releaseSamplerReference(desc.sampler, 0);
		hot = TextureHotRecord{.generation = generation};
		impl_->textureCold[index] = {};
		impl_->freeTextures.push_back(index);
		throw;
	}
	if (inserted) *inserted = true;
	return handle;
}

TextureHandle FlowStorageSystem::replaceTexture(ResourceKey key, const TextureViewDesc& desc) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (!impl_->usableImageView(desc.imageView) || !impl_->usableSampler(desc.sampler))
		storageError("texture replacement requires valid image-view and sampler handles");
	const auto found = impl_->textureByKey.find(key);
	if (found == impl_->textureByKey.end()) return publishTexture(key, desc, nullptr);
	impl_->validateTextureOwnership(key, desc);
	const TextureHandle handle = found->second;
	if (!impl_->validTexture(handle)) storageError("texture key points to an invalid handle");
	const ImageHandle newBackingImage = impl_->imageViews[desc.imageView.index].image;
	if (handle == impl_->fallbackTexture &&
		(!impl_->usableImage(newBackingImage) || impl_->images[newBackingImage.index].state != ResourceState::Ready)) {
		storageError("the configured fallback texture can only be replaced with ready native resources");
	}
	if (handle == impl_->fallbackTexture &&
		impl_->fallbackBinding.bindingRevision == std::numeric_limits<uint32_t>::max()) {
		storageError("fallback binding revision space exhausted");
	}

	TextureHotRecord& hot = impl_->textureHot[handle.index];
	Impl::TextureColdRecord& cold = impl_->textureCold[handle.index];
	const ImageViewHandle oldView = cold.desc.imageView;
	const SamplerHandle oldSampler = cold.desc.sampler;
	const SubmissionSerial oldLastUse = cold.lastUse;
	impl_->reserveRetirements(2u);
	if (handle == impl_->fallbackTexture) {
		for (auto& [_, window] : impl_->windows) {
			if (window->bindingsByTextureIndex.size() <= handle.index) {
				window->bindingsByTextureIndex.resize(static_cast<size_t>(handle.index) + 1u);
			}
		}
	}
	impl_->retainImageView(desc.imageView);
	try {
		impl_->retainSampler(desc.sampler);
	} catch (...) {
		impl_->releaseImageViewReference(desc.imageView, 0);
		throw;
	}
	cold.desc = desc;
	hot.revision = nextGeneration(hot.revision);
	hot.imageViewIndex = desc.imageView.index;
	hot.imageViewGeneration = desc.imageView.generation;
	hot.samplerIndex = desc.sampler.index;
	hot.samplerGeneration = desc.sampler.generation;
	hot.state = impl_->validImage(newBackingImage) ? impl_->images[newBackingImage.index].state : ResourceState::Invalid;
	hot.sourceWidth = desc.sourceWidth;
	hot.sourceHeight = desc.sourceHeight;
	impl_->releaseImageViewReference(oldView, oldLastUse);
	impl_->releaseSamplerReference(oldSampler, oldLastUse);
	if (handle == impl_->fallbackTexture) {
		const ImageViewHotRecord& view = impl_->imageViewHot[hot.imageViewIndex];
		const SamplerHotRecord& sampler = impl_->samplerHot[hot.samplerIndex];
		impl_->fallbackBinding = BindingHotRecord{
			.textureGeneration = handle.generation,
			.textureRevision = hot.revision,
			.descriptorIndex = 0,
			.bindingRevision = impl_->fallbackBinding.bindingRevision + 1u,
			.state = ResourceState::Ready,
			.nativeImageView = view.nativeImageView,
			.nativeSampler = sampler.nativeSampler,
		};
		for (auto& [_, window] : impl_->windows) {
			window->bindingsByTextureIndex[0] = impl_->fallbackBinding;
			window->bindingsByTextureIndex[handle.index] = impl_->fallbackBinding;
		}
	}
	return handle;
}

TextureHandle FlowStorageSystem::publishExternalTexture(
	ResourceKey key,
	const ExternalTextureDesc& desc,
	bool* inserted) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireExternalTexturePublishPhase();
	impl_->validateExternalTextureOwnership(key, desc);

	auto found = impl_->textureByKey.find(key);
	const bool isInsert = found == impl_->textureByKey.end();
	if (!isInsert) impl_->reserveRetirements(1u);
	const TextureHandle old = isInsert ? TextureHandle{} : found->second;

	const uint32_t index = impl_->acquireTextureIndex();
	TextureHotRecord& hot = impl_->textureHot[index];
	const uint32_t generation = hot.generation == 0 ? 1u : hot.generation;
	hot = TextureHotRecord{
		.generation = generation,
		.revision = 1u,
		.state = ResourceState::Ready,
		.external = true,
		.sourceWidth = desc.sourceWidth,
		.sourceHeight = desc.sourceHeight,
		.nativeImageView = desc.nativeImageView,
		.nativeSampler = desc.nativeSampler,
	};
	impl_->textureCold[index] = Impl::TextureColdRecord{
		.key = key,
		.externalDesc = desc,
		.referenceCount = 1u,
		.published = true,
		.external = true,
	};
	const TextureHandle handle{index, generation};
	if (isInsert) {
		try {
			if (!impl_->textureByKey.emplace(key, handle).second) {
				storageError("external texture key publication collided with a stale entry");
			}
		} catch (...) {
			hot = TextureHotRecord{.generation = generation};
			impl_->textureCold[index] = {};
			impl_->freeTextures.push_back(index);
			throw;
		}
	} else {
		found->second = handle;
		Impl::TextureColdRecord& oldCold = impl_->textureCold[old.index];
		oldCold.published = false;
		impl_->releaseTextureReference(old, oldCold.lastUse);
	}
	if (inserted) *inserted = isInsert;
	return handle;
}

bool FlowStorageSystem::removeTexture(ResourceKey key, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireExternalTexturePublishPhase();
	const auto found = impl_->textureByKey.find(key);
	if (found == impl_->textureByKey.end()) return false;
	const TextureHandle handle = found->second;
	if (!impl_->validTexture(handle)) {
		impl_->textureByKey.erase(found);
		return false;
	}
	Impl::TextureColdRecord& cold = impl_->textureCold[handle.index];
	if (!cold.published) return false;
	impl_->reserveRetirements(1u);
	impl_->textureByKey.erase(found);
	cold.published = false;
	cold.lastUse = std::max(cold.lastUse, lastUse);
	impl_->releaseTextureReference(handle, cold.lastUse);
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

bool FlowStorageSystem::textureRetirementComplete(TextureHandle texture) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return !impl_->validTexture(texture);
}

void FlowStorageSystem::setFallbackTexture(TextureHandle texture) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (texture == impl_->fallbackTexture) return;
	if (!impl_->usableTexture(texture) || impl_->textureHot[texture.index].state != ResourceState::Ready) {
		storageError("fallback texture must be a ready, non-retiring texture");
	}
	if (impl_->textureCold[texture.index].external) {
		storageError("the fallback texture must be storage-owned");
	}
	const ImageHandle fallbackImage = impl_->textureBackingImage(texture);
	if (!fallbackImage || impl_->textureCold[texture.index].key.window != 0 ||
		impl_->images[fallbackImage.index].desc.sharing != ResourceSharing::AppShared) {
		storageError("fallback texture must be backed by an app-shared root image");
	}
	const TextureHotRecord& hot = impl_->textureHot[texture.index];
	if (hot.imageViewIndex >= impl_->imageViewHot.size() || hot.samplerIndex >= impl_->samplerHot.size()) {
		storageError("fallback texture has invalid native dependencies");
	}
	const ImageViewHotRecord& view = impl_->imageViewHot[hot.imageViewIndex];
	const SamplerHotRecord& sampler = impl_->samplerHot[hot.samplerIndex];
	if (view.generation != hot.imageViewGeneration || sampler.generation != hot.samplerGeneration ||
		view.nativeImageView == 0 || sampler.nativeSampler == 0) {
		storageError("fallback texture does not resolve to valid native handles");
	}
	if (impl_->fallbackBinding.bindingRevision == std::numeric_limits<uint32_t>::max()) {
		storageError("fallback binding revision space exhausted");
	}
	for (auto& [_, window] : impl_->windows) {
		const size_t requiredSize = std::max<size_t>(1u, static_cast<size_t>(texture.index) + 1u);
		if (window->bindingsByTextureIndex.size() < requiredSize) {
			window->bindingsByTextureIndex.resize(requiredSize);
		}
	}
	impl_->reserveRetirements(impl_->fallbackTexture ? 1u : 0u);
	impl_->retainTexture(texture);
	const TextureHandle previous = impl_->fallbackTexture;
	impl_->fallbackTexture = texture;
	impl_->fallbackBinding = BindingHotRecord{
		.textureGeneration = texture.generation,
		.textureRevision = hot.revision,
		.descriptorIndex = 0,
		.bindingRevision = impl_->fallbackBinding.bindingRevision + 1u,
		.state = ResourceState::Ready,
		.nativeImageView = view.nativeImageView,
		.nativeSampler = sampler.nativeSampler,
	};
	for (auto& [_, window] : impl_->windows) {
		if (previous && previous.index < window->bindingsByTextureIndex.size()) {
			BindingHotRecord& previousBinding = window->bindingsByTextureIndex[previous.index];
			if (previousBinding.textureGeneration == previous.generation && previousBinding.descriptorIndex == 0) {
				previousBinding = {};
			}
		}
		window->bindingsByTextureIndex[0] = impl_->fallbackBinding;
		window->bindingsByTextureIndex[texture.index] = impl_->fallbackBinding;
	}
	if (previous) impl_->releaseTextureReference(previous, 0);
}

PreparedTextureBindings FlowStorageSystem::prepareTextureBindings(
	const FrameToken& frame,
	std::span<const TextureHandle> textures) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	Impl::WindowState& window = impl_->requireWindow(frame.window);
	impl_->requireWindowBindingMutationPhase(window, frame.frameSlot);
	if (!impl_->fallbackTexture) storageError("fallback texture must be configured before preparing bindings");
	if (textures.size() >= std::numeric_limits<size_t>::max() ||
		textures.size() + 1u > std::numeric_limits<size_t>::max() / sizeof(DescriptorWriteRecord)) {
		storageError("binding batch is too large");
	}
	const size_t maximumWrites = textures.size() + 1u;
	auto* writes = static_cast<DescriptorWriteRecord*>(state.transient.allocate(
		maximumWrites * sizeof(DescriptorWriteRecord), alignof(DescriptorWriteRecord)));
	if (maximumWrites > 0 && !writes) storageError("failed to allocate the descriptor write batch");

	// Preflight the complete batch before resolve() assigns any descriptor index.
	// This makes descriptor-capacity failure transactional.
	uint32_t* seen = nullptr;
	if (!impl_->textureHot.empty()) {
		seen = static_cast<uint32_t*>(state.transient.allocate(
			impl_->textureHot.size() * sizeof(uint32_t), alignof(uint32_t)));
		if (!seen) storageError("failed to allocate texture binding preflight markers");
		std::fill_n(seen, impl_->textureHot.size(), 0u);
	}
	uint32_t requiredNewBindings = 0;
	for (TextureHandle texture : textures) {
		if (!impl_->usableTexture(texture) || texture == impl_->fallbackTexture) continue;
		if (!impl_->textureVisibleToFrame(texture, frame)) {
			storageError("texture is not visible to the resolving window/frame scope");
		}
		if (seen[texture.index] != 0u) continue;
		seen[texture.index] = texture.generation;
		const bool alreadyAssigned = texture.index < window.bindingsByTextureIndex.size() &&
			window.bindingsByTextureIndex[texture.index].descriptorIndex != 0;
		if (!alreadyAssigned) ++requiredNewBindings;
	}
	const uint64_t reusable = window.freeDescriptorIndices.size();
	const uint64_t unassigned = requiredNewBindings > reusable ? requiredNewBindings - reusable : 0u;
	if (static_cast<uint64_t>(window.nextDescriptorIndex) + unassigned > window.desc.maxTextureBindings) {
		storageError("window texture descriptor capacity was exhausted");
	}

	if (state.currentBindingBatch == std::numeric_limits<uint32_t>::max()) {
		std::fill(state.preparedBindingBatches.begin(), state.preparedBindingBatches.end(), 0u);
		state.currentBindingBatch = 1u;
	} else {
		++state.currentBindingBatch;
	}
	const uint32_t batch = state.currentBindingBatch;
	size_t writeCount = 0;
	auto appendIfDirty = [&](TextureHandle texture, const ResolvedTextureBinding& resolved) {
		const size_t descriptorIndex = resolved.descriptorIndex;
		if (state.appliedBindingRevisions.size() <= descriptorIndex) {
			state.appliedBindingRevisions.resize(descriptorIndex + 1u, 0u);
		}
		if (state.preparedBindingBatches.size() <= descriptorIndex) {
			state.preparedBindingBatches.resize(descriptorIndex + 1u, 0u);
		}
		if (state.preparedBindingBatches[descriptorIndex] == batch ||
			state.appliedBindingRevisions[descriptorIndex] == resolved.bindingRevision) return;
		state.preparedBindingBatches[descriptorIndex] = batch;
		writes[writeCount++] = DescriptorWriteRecord{
			.texture = texture,
			.descriptorIndex = resolved.descriptorIndex,
			.bindingRevision = resolved.bindingRevision,
			.state = resolved.state,
			.nativeImageView = resolved.nativeImageView,
			.nativeSampler = resolved.nativeSampler,
		};
	};
	appendIfDirty(impl_->fallbackTexture, ResolvedTextureBinding{
		.descriptorIndex = 0,
		.bindingRevision = impl_->fallbackBinding.bindingRevision,
		.state = impl_->fallbackBinding.state,
		.nativeImageView = impl_->fallbackBinding.nativeImageView,
		.nativeSampler = impl_->fallbackBinding.nativeSampler,
	});
	impl_->addUse(state, ResourceKind::TextureView, impl_->fallbackTexture.packed());
	for (TextureHandle texture : textures) {
		appendIfDirty(texture, impl_->resolve(window, state, frame, texture));
	}
	return PreparedTextureBindings{
		.dirtyBindings = std::span<const DescriptorWriteRecord>(writes, writeCount),
		.bindingsByTextureIndex = window.bindingsByTextureIndex,
		.requiredDescriptorCapacity = std::max(1u, window.nextDescriptorIndex),
		.epoch = frame.epoch,
	};
}

void FlowStorageSystem::acknowledgeTextureBindings(
	const FrameToken& frame,
	std::span<const DescriptorWriteRecord> appliedBindings) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	const Impl::WindowState& window = impl_->requireWindow(frame.window);
	size_t requiredSize = state.appliedBindingRevisions.size();
	for (const DescriptorWriteRecord& applied : appliedBindings) {
		if (applied.descriptorIndex >= window.nextDescriptorIndex) {
			storageError("acknowledged descriptor index is outside the prepared capacity");
		}
		const BindingHotRecord* current = nullptr;
		if (applied.descriptorIndex == 0) {
			if (applied.texture != impl_->fallbackTexture) {
				storageError("fallback descriptor acknowledgement has the wrong texture handle");
			}
			current = &impl_->fallbackBinding;
		} else if (applied.texture && applied.texture.index < window.bindingsByTextureIndex.size()) {
			const BindingHotRecord& candidate = window.bindingsByTextureIndex[applied.texture.index];
			if (candidate.textureGeneration == applied.texture.generation &&
				candidate.descriptorIndex == applied.descriptorIndex) current = &candidate;
		}
		if (applied.descriptorIndex >= state.preparedBindingBatches.size() ||
			state.preparedBindingBatches[applied.descriptorIndex] != state.currentBindingBatch ||
			!current || current->bindingRevision != applied.bindingRevision ||
			current->nativeImageView != applied.nativeImageView || current->nativeSampler != applied.nativeSampler) {
			storageError("descriptor acknowledgement does not match the current prepared binding");
		}
		requiredSize = std::max(requiredSize, static_cast<size_t>(applied.descriptorIndex) + 1u);
	}
	state.appliedBindingRevisions.resize(requiredSize, 0u);
	for (const DescriptorWriteRecord& applied : appliedBindings) {
		state.appliedBindingRevisions[applied.descriptorIndex] = applied.bindingRevision;
	}
}

void FlowStorageSystem::resetTextureBindings(WindowId id, uint32_t frameSlot) {
	std::scoped_lock lock(impl_->mutex);
	Impl::WindowState& window = impl_->requireWindow(id);
	if (frameSlot >= window.frames.size()) storageError("texture-binding reset frame slot is out of range");
	Impl::FrameState& frame = *window.frames[frameSlot];
	if (frame.active || frame.inFlightSerial != 0) {
		storageError("texture bindings cannot be reset while the frame slot is active or in flight");
	}
	frame.appliedBindingRevisions.clear();
	frame.preparedBindingBatches.clear();
}

ResolvedTextureBinding FlowStorageSystem::resolveTexture(const FrameToken& frame, TextureHandle texture) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	Impl::WindowState& window = impl_->requireWindow(frame.window);
	impl_->requireWindowBindingMutationPhase(window, frame.frameSlot);
	return impl_->resolve(window, state, frame, texture);
}

void FlowStorageSystem::trackUse(const FrameToken& frame, BufferHandle buffer) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->bufferVisibleToFrame(buffer, frame)) {
		storageError("trackUse received an invalid, retiring, or cross-scope buffer");
	}
	impl_->addUse(impl_->requireFrame(frame, false), ResourceKind::GpuBuffer, buffer.packed());
}

void FlowStorageSystem::trackUse(const FrameToken& frame, ImageHandle image) {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->imageVisibleToFrame(image, frame)) {
		storageError("trackUse received an invalid, retiring, or cross-scope image");
	}
	impl_->addUse(impl_->requireFrame(frame, false), ResourceKind::GpuImage, image.packed());
}

void FlowStorageSystem::trackUses(const FrameToken& frame, std::span<const ResourceUse> resources) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& state = impl_->requireFrame(frame, false);
	for (const ResourceUse& resource : resources) {
		switch (resource.kind) {
		case ResourceKind::GpuBuffer:
			if (!impl_->bufferVisibleToFrame(BufferHandle::fromPacked(resource.packedHandle), frame))
				storageError("trackUses received an invalid or cross-scope buffer");
			break;
		case ResourceKind::GpuImage:
			if (!impl_->imageVisibleToFrame(ImageHandle::fromPacked(resource.packedHandle), frame))
				storageError("trackUses received an invalid or cross-scope image");
			break;
		case ResourceKind::ImageView: {
			const ImageViewHandle handle = ImageViewHandle::fromPacked(resource.packedHandle);
			if (!impl_->usableImageView(handle) ||
				!impl_->imageVisibleToFrame(impl_->imageViews[handle.index].image, frame)) {
				storageError("trackUses received an invalid or cross-scope image view");
			}
			break;
		}
		case ResourceKind::Sampler:
			if (!impl_->usableSampler(SamplerHandle::fromPacked(resource.packedHandle)))
				storageError("trackUses received an invalid sampler");
			break;
		case ResourceKind::TextureView:
			if (!impl_->textureVisibleToFrame(TextureHandle::fromPacked(resource.packedHandle), frame))
				storageError("trackUses received an invalid or cross-scope texture");
			break;
		case ResourceKind::RendererLayout:
			if (!impl_->usableRendererLayout(RendererLayoutHandle::fromPacked(resource.packedHandle)))
				storageError("trackUses received an invalid renderer layout");
			break;
		case ResourceKind::RendererPipelineBundle:
			if (!impl_->usableRendererPipelineBundle(
				RendererPipelineBundleHandle::fromPacked(resource.packedHandle)))
				storageError("trackUses received an invalid renderer pipeline bundle");
			break;
		case ResourceKind::WindowDescriptorBundle: {
			const WindowDescriptorBundleHandle handle =
				WindowDescriptorBundleHandle::fromPacked(resource.packedHandle);
			if (!impl_->usableWindowDescriptorBundle(handle) ||
				impl_->windowDescriptorBundles[handle.index].desc.window != frame.window) {
				storageError("trackUses received an invalid or cross-window descriptor bundle");
			}
			break;
		}
		default:
			storageError("trackUses received an unsupported resource kind");
		}
	}
	for (const ResourceUse& resource : resources) {
		impl_->addUse(state, resource.kind, resource.packedHandle);
	}
}

void FlowStorageSystem::invalidateWindowBindings(WindowId id, TextureHandle texture) {
	std::scoped_lock lock(impl_->mutex);
	Impl::WindowState& window = impl_->requireWindow(id);
	impl_->requireWindowBindingMutationPhase(window, InvalidFrameSlot);
	if (texture.index >= window.bindingsByTextureIndex.size()) return;
	BindingHotRecord& binding = window.bindingsByTextureIndex[texture.index];
	if (binding.textureGeneration == texture.generation) {
		binding.textureRevision = 0;
		if (window.bindingRevision == std::numeric_limits<uint32_t>::max()) {
			storageError("window binding revision space exhausted");
		}
		binding.bindingRevision = ++window.bindingRevision;
	}
}

StorageReadView FlowStorageSystem::readView(const FrameReadLease& lease) const {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireLease(lease);
	StorageReadView view{
		.textures = impl_->textureHot,
		.imageViews = impl_->imageViewHot,
		.samplers = impl_->samplerHot,
		.epoch = lease.frame.epoch,
	};
#if FLOW_UI_DEV_MODE
	view.validation = lease.validation;
#endif
	return view;
}

WindowBindingView FlowStorageSystem::windowBindingView(const FrameReadLease& lease) const {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireLease(lease);
	const Impl::WindowState& window = impl_->requireWindow(lease.frame.window);
	WindowBindingView view{
		.bindingsByTextureIndex = window.bindingsByTextureIndex,
		.epoch = lease.frame.epoch,
	};
#if FLOW_UI_DEV_MODE
	view.validation = lease.validation;
#endif
	return view;
}

WindowStorageSnapshot FlowStorageSystem::windowSnapshot(WindowId id) const {
#if !FLOW_UI_DEV_MODE
	(void)id;
	return {};
#else
	std::scoped_lock lock(impl_->mutex);
	const Impl::WindowState& window = impl_->requireWindow(id);
	WindowStorageSnapshot result{};
	result.window = id;
	result.framesInFlight = static_cast<uint32_t>(window.frames.size());
	result.bindingCapacity = window.desc.maxTextureBindings;
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
#endif
}

UploadTicket FlowStorageSystem::enqueueUpload(const UploadRequest& request) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	if (!impl_->validBlob(request.source) || impl_->blobs[request.source.index].state != ResourceState::Ready) {
		storageError("upload source blob is invalid or not ready");
	}
	if (request.byteCount == 0) storageError("upload byte count must be non-zero");
	if (impl_->nextUploadId == 0 || impl_->nextUploadId == std::numeric_limits<UploadId>::max()) {
		storageError("upload id space exhausted");
	}
	const auto bytes = readBlob(request.source);
	if (request.sourceOffset > bytes.size() || request.byteCount > bytes.size() - request.sourceOffset)
		storageError("upload source range exceeds blob");
	if (request.destination == UploadDestination::Buffer) {
		if (!impl_->usableBuffer(request.destinationBuffer)) storageError("upload destination buffer is invalid");
		const Impl::BufferRecord& destination = impl_->buffers[request.destinationBuffer.index];
		if (!hasFlag(destination.desc.usage, BufferUsage::TransferDestination) ||
			request.destinationBufferOffset > destination.size ||
			request.byteCount > destination.size - request.destinationBufferOffset) {
			storageError("buffer upload destination range or usage is invalid");
		}
	} else if (request.destination == UploadDestination::Image) {
		if (!impl_->usableImage(request.destinationImage)) storageError("upload destination image is invalid");
		const Impl::ImageRecord& destination = impl_->images[request.destinationImage.index];
		const ImageRegion& region = request.imageRegion;
		if (!hasFlag(destination.desc.usage, ImageUsage::TransferDestination) || region.width == 0 ||
			region.height == 0 || region.depth == 0 || region.layerCount == 0 ||
			region.mipLevel >= destination.desc.mipLevels ||
			region.baseArrayLayer > destination.desc.layers ||
			region.layerCount > destination.desc.layers - region.baseArrayLayer) {
			storageError("image upload subresource or usage is invalid");
		}
		const uint32_t mipWidth = region.mipLevel >= 31u
			? 1u : std::max(1u, destination.desc.width >> region.mipLevel);
		const uint32_t mipHeight = region.mipLevel >= 31u
			? 1u : std::max(1u, destination.desc.height >> region.mipLevel);
		const uint32_t mipDepth = region.mipLevel >= 31u
			? 1u : std::max(1u, destination.desc.depth >> region.mipLevel);
		if (region.x > mipWidth || region.width > mipWidth - region.x ||
			region.y > mipHeight || region.height > mipHeight - region.y ||
			region.z > mipDepth || region.depth > mipDepth - region.z) {
			storageError("image upload region exceeds its mip extent");
		}
		uint64_t requiredBytes = checkedMultiply(region.width, region.height, "image upload byte-size overflow");
		requiredBytes = checkedMultiply(requiredBytes, region.depth, "image upload byte-size overflow");
		requiredBytes = checkedMultiply(requiredBytes, region.layerCount, "image upload byte-size overflow");
		requiredBytes = checkedMultiply(
			requiredBytes, bytesPerPixel(destination.desc.format), "image upload byte-size overflow");
		if (request.byteCount != requiredBytes) storageError("image upload byte count does not match its tightly packed region");
	} else {
		storageError("upload destination kind is invalid");
	}
	const UploadTicket ticket{impl_->nextUploadId++};
	impl_->retainBlob(request.source);
	try {
		if (request.destination == UploadDestination::Buffer) impl_->retainBuffer(request.destinationBuffer);
		else impl_->retainImage(request.destinationImage);
	} catch (...) {
		Impl::BlobRecord& source = impl_->blobs[request.source.index];
		if (source.referenceCount > 0) --source.referenceCount;
		throw;
	}
	try {
		impl_->uploads.push_back(Impl::UploadRecord{ticket, request, ResourceState::Queued});
		try {
			impl_->uploadStates.emplace(ticket.value, ResourceState::Queued);
		} catch (...) {
			impl_->uploads.pop_back();
			throw;
		}
	} catch (...) {
		if (request.destination == UploadDestination::Buffer) {
			impl_->releaseBufferReference(request.destinationBuffer, 0);
		} else {
			impl_->releaseImageReference(request.destinationImage, 0);
		}
		Impl::BlobRecord& source = impl_->blobs[request.source.index];
		if (source.referenceCount > 0) --source.referenceCount;
		throw;
	}
#if FLOW_UI_DEV_MODE
	impl_->telemetry.queuedUploadBytes += request.byteCount;
#endif
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
	impl_->requireSharedMutationPhase();
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
#if FLOW_UI_DEV_MODE
			impl_->telemetry.queuedUploadBytes -= std::min(impl_->telemetry.queuedUploadBytes, upload.request.byteCount);
			impl_->telemetry.completedUploadBytes += upload.request.byteCount;
#endif
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

SubmissionToken FlowStorageSystem::noteSubmission(const FrameReadLease& lease) {
	std::scoped_lock lock(impl_->mutex);
	Impl::FrameState& frame = impl_->requireLease(lease);
	if (frame.inFlightSerial != 0) storageError("frame slot already has an outstanding submission");
	if (impl_->nextSubmissionSerial == 0 ||
		impl_->nextSubmissionSerial == std::numeric_limits<SubmissionSerial>::max()) {
		storageError("submission serial space exhausted");
	}
	// Make the remaining submission transition failure-free: each retained use
	// can enqueue at most one retirement when its frame reference is released.
	impl_->reserveRetirements(frame.used.size());
	const SubmissionSerial serial = impl_->nextSubmissionSerial++;
	for (const UsedResource& used : frame.used) {
		impl_->stampUse(used, serial);
		impl_->releaseUsed(used, serial);
	}
	frame.used.clear();
	frame.inFlightSerial = serial;
	Impl::invalidateLease(frame);
	Impl::invalidateArena(frame);
	frame.active = false;
	frame.sealed = false;
#if FLOW_UI_DEV_MODE
	impl_->telemetry.currentSubmissionSerial = serial;
#endif
	return SubmissionToken{lease.frame.window, serial, lease.frame.frameSlot};
}

void FlowStorageSystem::noteCompleted(SubmissionToken submission) {
	std::scoped_lock lock(impl_->mutex);
	if (!submission || submission.serial >= impl_->nextSubmissionSerial) {
		storageError("submission completion token is invalid or was never issued");
	}
	auto foundWindow = impl_->windows.find(submission.window);
	if (foundWindow == impl_->windows.end() || submission.frameSlot >= foundWindow->second->frames.size()) {
		storageError("submission completion token references an unknown window frame slot");
	}
	Impl::FrameState& frame = *foundWindow->second->frames[submission.frameSlot];
	if (frame.inFlightSerial != submission.serial) {
		storageError("submission completion token is stale, duplicate, or mismatched");
	}
	if (submission.serial == impl_->completedWatermark + 1u) {
		frame.inFlightSerial = 0;
		++impl_->completedWatermark;
		while (impl_->completedOutOfOrder.erase(impl_->completedWatermark + 1u) > 0) {
			++impl_->completedWatermark;
		}
	} else {
		// Allocation is needed only for a real completion gap. Insert before
		// clearing the frame slot so allocation failure leaves the token retryable.
		impl_->completedOutOfOrder.insert(submission.serial);
		frame.inFlightSerial = 0;
	}
#if FLOW_UI_DEV_MODE
	impl_->telemetry.completedSubmissionSerial = impl_->completedWatermark;
#endif
	for (auto it = impl_->windows.begin(); it != impl_->windows.end();) {
		bool anyOutstanding = false;
		for (const auto& slot : it->second->frames) anyOutstanding = anyOutstanding || slot->inFlightSerial != 0;
		if (it->second->closing && !anyOutstanding && it->second->retireAfter <= impl_->completedWatermark) {
			it = impl_->windows.erase(it);
		} else {
			++it;
		}
	}
#if FLOW_UI_DEV_MODE
	impl_->telemetry.windowCount = static_cast<uint32_t>(impl_->windows.size());
#endif
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
		impl_->requireSharedMutationPhase();
		const TextureHandle handle = TextureHandle::fromPacked(request.packedHandle);
		if (!impl_->validTexture(handle)) return;
		Impl::TextureColdRecord& cold = impl_->textureCold[handle.index];
		if (!cold.published) return;
		impl_->reserveRetirements(1u);
		impl_->textureByKey.erase(cold.key);
		cold.published = false;
		impl_->releaseTextureReference(handle, request.retireAfter);
		break;
	}
	case RetirementKind::RendererLayout:
		releaseRendererLayout(RendererLayoutHandle::fromPacked(request.packedHandle), request.retireAfter);
		break;
	case RetirementKind::RendererPipelineBundle:
		releaseRendererPipelineBundle(
			RendererPipelineBundleHandle::fromPacked(request.packedHandle), request.retireAfter);
		break;
	case RetirementKind::WindowDescriptorBundle:
		releaseWindowDescriptorBundle(
			WindowDescriptorBundleHandle::fromPacked(request.packedHandle), request.retireAfter);
		break;
	}
}

void FlowStorageSystem::collect() {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireSharedMutationPhase();
	for (;;) {
		size_t readyCount = 0;
		size_t blobCount = 0;
		size_t bufferCount = 0;
		size_t imageCount = 0;
		size_t imageViewCount = 0;
		size_t samplerCount = 0;
		size_t textureCount = 0;
		size_t rendererLayoutCount = 0;
		size_t rendererPipelineCount = 0;
		size_t descriptorBundleCount = 0;
		for (const Impl::RetirementRecord& retired : impl_->retirements) {
			if (retired.request.retireAfter > impl_->completedWatermark) continue;
			++readyCount;
			switch (retired.request.kind) {
			case RetirementKind::Blob: ++blobCount; break;
			case RetirementKind::Buffer: ++bufferCount; break;
			case RetirementKind::Image: ++imageCount; break;
			case RetirementKind::ImageView: ++imageViewCount; break;
			case RetirementKind::Sampler: ++samplerCount; break;
			case RetirementKind::Texture: ++textureCount; break;
			case RetirementKind::RendererLayout: ++rendererLayoutCount; break;
			case RetirementKind::RendererPipelineBundle: ++rendererPipelineCount; break;
			case RetirementKind::WindowDescriptorBundle: ++descriptorBundleCount; break;
			}
		}
		if (readyCount == 0) break;
		auto reserveAdditional = [](auto& values, size_t count) {
			if (count > values.max_size() - values.size()) {
				storageError("retirement free-list size overflow");
			}
			values.reserve(values.size() + count);
		};
		reserveAdditional(impl_->freeBlobs, blobCount);
		reserveAdditional(impl_->freeBuffers, bufferCount);
		reserveAdditional(impl_->freeImages, imageCount);
		reserveAdditional(impl_->freeImageViews, imageViewCount);
		reserveAdditional(impl_->freeSamplers, samplerCount);
		reserveAdditional(impl_->freeTextures, textureCount);
		reserveAdditional(impl_->freeRendererLayouts, rendererLayoutCount);
		reserveAdditional(impl_->freeRendererPipelineBundles, rendererPipelineCount);
		reserveAdditional(impl_->freeWindowDescriptorBundles, descriptorBundleCount);
		for (auto& [_, window] : impl_->windows) {
			reserveAdditional(window->freeDescriptorIndices, textureCount);
		}
		if (readyCount > std::numeric_limits<size_t>::max() / 2u) {
			storageError("retirement dependency queue size overflow");
		}
		impl_->reserveRetirements(readyCount * 2u);
		std::vector<Impl::RetirementRecord> ready;
		ready.reserve(readyCount);
		auto pending = std::remove_if(
			impl_->retirements.begin(), impl_->retirements.end(), [&](Impl::RetirementRecord& retired) {
				if (retired.request.retireAfter > impl_->completedWatermark) return false;
				ready.push_back(std::move(retired));
				return true;
			});
		impl_->retirements.erase(pending, impl_->retirements.end());
		for (const Impl::RetirementRecord& retired : ready) impl_->destroyRetired(retired);
	}
}

void FlowStorageSystem::trim(uint64_t targetBytes) {
	std::scoped_lock lock(impl_->mutex);
	if (targetBytes == 0) return;
	uint64_t released = 0;
	for (auto& [_, window] : impl_->windows) {
		for (auto& frame : window->frames) {
			if (frame->active || frame->inFlightSerial != 0) continue;
			uint64_t before = frame->transient.capacity() + frame->decode.capacity();
			for (const auto& worker : frame->workers) before += worker->capacity();
			frame->transient.trimOverflow();
			frame->decode.trimOverflow();
			for (auto& worker : frame->workers) worker->trimOverflow();
			uint64_t after = frame->transient.capacity() + frame->decode.capacity();
			for (const auto& worker : frame->workers) after += worker->capacity();
			released += before - std::min(before, after);
			if (released >= targetBytes) return;
		}
	}
}

StorageStats FlowStorageSystem::stats() const {
#if !FLOW_UI_DEV_MODE
	return {};
#else
	std::scoped_lock lock(impl_->mutex);
	StorageStats result = impl_->telemetry;
	result.gpuLiveBytes = impl_->gpuLiveBytes;
	result.gpuRetiredBytes = impl_->gpuRetiredBytes;
	result.cpuSoftBudgetBytes = impl_->cpuSoftBudgetBytes;
	result.gpuSoftBudgetBytes = impl_->gpuSoftBudgetBytes;
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
#endif
}

ResourceStats FlowStorageSystem::resourceStats(ResourceKind kind) const {
#if !FLOW_UI_DEV_MODE
	return ResourceStats{.kind = kind};
#else
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
		for (size_t i = 1; i < impl_->buffers.size(); ++i)
			add(impl_->buffers[i].state, impl_->buffers[i].allocationBytes);
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
	case ResourceKind::RendererLayout:
		result.slots = static_cast<uint32_t>(impl_->rendererLayouts.size() - 1u);
		for (size_t i = 1; i < impl_->rendererLayouts.size(); ++i) add(impl_->rendererLayouts[i].state, 0);
		break;
	case ResourceKind::RendererPipelineBundle:
		result.slots = static_cast<uint32_t>(impl_->rendererPipelineBundles.size() - 1u);
		for (size_t i = 1; i < impl_->rendererPipelineBundles.size(); ++i)
			add(impl_->rendererPipelineBundles[i].state, 0);
		break;
	case ResourceKind::WindowDescriptorBundle:
		result.slots = static_cast<uint32_t>(impl_->windowDescriptorBundles.size() - 1u);
		for (size_t i = 1; i < impl_->windowDescriptorBundles.size(); ++i)
			add(impl_->windowDescriptorBundles[i].state, 0);
		break;
	default: break;
	}
	return result;
#endif
}

bool FlowStorageSystem::validateHandle(ResourceKind kind, uint32_t index, uint32_t generation) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	switch (kind) {
	case ResourceKind::CpuBlob: {
		const BlobHandle handle{index, generation};
		return impl_->validBlob(handle) && impl_->blobs[index].state != ResourceState::Retiring;
	}
	case ResourceKind::GpuBuffer: return impl_->usableBuffer({index, generation});
	case ResourceKind::GpuImage: return impl_->usableImage({index, generation});
	case ResourceKind::ImageView: return impl_->usableImageView({index, generation});
	case ResourceKind::Sampler: return impl_->usableSampler({index, generation});
	case ResourceKind::TextureView: return impl_->usableTexture({index, generation});
	case ResourceKind::RendererLayout: return impl_->usableRendererLayout({index, generation});
	case ResourceKind::RendererPipelineBundle: return impl_->usableRendererPipelineBundle({index, generation});
	case ResourceKind::WindowDescriptorBundle: return impl_->usableWindowDescriptorBundle({index, generation});
	default: return false;
	}
}

void FlowStorageSystem::setBudget(uint64_t cpuBytes, uint64_t gpuBytes) {
	std::scoped_lock lock(impl_->mutex);
	if (cpuBytes == 0 || gpuBytes == 0) storageError("storage budgets must be non-zero");
	if (impl_->persistentPool.liveBytes() + impl_->stringPool.liveBytes() > cpuBytes ||
		impl_->gpuLiveBytes + impl_->gpuRetiredBytes > gpuBytes) {
		storageError("new storage budget is below current committed usage");
	}
	impl_->cpuSoftBudgetBytes = cpuBytes;
	impl_->gpuSoftBudgetBytes = gpuBytes;
#if FLOW_UI_DEV_MODE
	impl_->telemetry.cpuSoftBudgetBytes = cpuBytes;
	impl_->telemetry.gpuSoftBudgetBytes = gpuBytes;
#endif
}

NativePublishResult<RendererLayoutHandle> FlowStorageSystem::publishRendererLayout(
	const RendererLayoutKey& key,
	const NativeRendererLayout& native,
	StringId debugName) {
	std::scoped_lock lock(impl_->mutex);
#if !FLOW_UI_DEV_MODE
	(void)debugName;
#endif
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (key.textureDescriptorCapacity == 0 || native.globalsSetLayout == 0 ||
		native.texturesSetLayout == 0 || native.pipelineLayout == 0) {
		storageError("renderer layout publication requires a non-zero capacity and complete native handles");
	}
	if (const auto found = impl_->rendererLayoutByKey.find(key);
		found != impl_->rendererLayoutByKey.end() && impl_->usableRendererLayout(found->second)) {
		impl_->retainRendererLayout(found->second);
		return NativePublishResult<RendererLayoutHandle>{found->second, false};
	}
	const uint32_t index = Impl::acquireIndex(impl_->rendererLayouts, impl_->freeRendererLayouts);
	Impl::RendererLayoutRecord& record = impl_->rendererLayouts[index];
	const uint32_t generation = record.generation == 0 ? 1u : record.generation;
	record = Impl::RendererLayoutRecord{
		.generation = generation,
		.state = ResourceState::Ready,
		.key = key,
		.native = native,
		.referenceCount = 1,
		.lastUse = 0,
#if FLOW_UI_DEV_MODE
		.debugName = debugName,
#endif
	};
	const RendererLayoutHandle handle{index, generation};
	try {
		if (!impl_->rendererLayoutByKey.emplace(key, handle).second) {
			storageError("renderer layout key publication collided with a stale entry");
		}
	} catch (...) {
		record = Impl::RendererLayoutRecord{.generation = generation};
		impl_->freeRendererLayouts.push_back(index);
		throw;
	}
	return NativePublishResult<RendererLayoutHandle>{handle, true};
}

RendererLayoutHandle FlowStorageSystem::acquireRendererLayout(const RendererLayoutKey& key) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	const auto found = impl_->rendererLayoutByKey.find(key);
	if (found == impl_->rendererLayoutByKey.end() || !impl_->usableRendererLayout(found->second)) return {};
	impl_->retainRendererLayout(found->second);
	return found->second;
}

NativePublishResult<RendererPipelineBundleHandle> FlowStorageSystem::publishRendererPipelineBundle(
	const RendererPipelineKey& key,
	const NativeRendererPipelineBundle& native,
	StringId debugName) {
	std::scoped_lock lock(impl_->mutex);
#if !FLOW_UI_DEV_MODE
	(void)debugName;
#endif
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	if (!impl_->usableRendererLayout(key.layout) || key.sampleCount == 0) {
		storageError("renderer pipeline publication requires a valid layout and sample count");
	}
	const Impl::RendererLayoutRecord& layout = impl_->rendererLayouts[key.layout.index];
	if (native.pipelineLayout == 0 || native.pipelineLayout != layout.native.pipelineLayout ||
		std::any_of(native.pipelines.begin(), native.pipelines.end(), [](uint64_t handle) { return handle == 0; })) {
		storageError("renderer pipeline publication requires complete native handles matching its layout");
	}
	if (const auto found = impl_->rendererPipelineByKey.find(key);
		found != impl_->rendererPipelineByKey.end() && impl_->usableRendererPipelineBundle(found->second)) {
		impl_->retainRendererPipelineBundle(found->second);
		return NativePublishResult<RendererPipelineBundleHandle>{found->second, false};
	}
	impl_->retainRendererLayout(key.layout);
	uint32_t index = 0;
	try {
		index = Impl::acquireIndex(impl_->rendererPipelineBundles, impl_->freeRendererPipelineBundles);
		Impl::RendererPipelineBundleRecord& record = impl_->rendererPipelineBundles[index];
		const uint32_t generation = record.generation == 0 ? 1u : record.generation;
		record = Impl::RendererPipelineBundleRecord{
			.generation = generation,
			.state = ResourceState::Ready,
			.key = key,
			.native = native,
			.referenceCount = 1,
			.lastUse = 0,
#if FLOW_UI_DEV_MODE
			.debugName = debugName,
#endif
		};
		const RendererPipelineBundleHandle handle{index, generation};
		try {
			if (!impl_->rendererPipelineByKey.emplace(key, handle).second) {
				storageError("renderer pipeline key publication collided with a stale entry");
			}
		} catch (...) {
			record = Impl::RendererPipelineBundleRecord{.generation = generation};
			impl_->freeRendererPipelineBundles.push_back(index);
			throw;
		}
		return NativePublishResult<RendererPipelineBundleHandle>{handle, true};
	} catch (...) {
		impl_->releaseRendererLayoutReference(key.layout, 0);
		throw;
	}
}

RendererPipelineBundleHandle FlowStorageSystem::acquireRendererPipelineBundle(const RendererPipelineKey& key) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	const auto found = impl_->rendererPipelineByKey.find(key);
	if (found == impl_->rendererPipelineByKey.end() ||
		!impl_->usableRendererPipelineBundle(found->second)) return {};
	impl_->retainRendererPipelineBundle(found->second);
	return found->second;
}

WindowDescriptorBundleHandle FlowStorageSystem::adoptWindowDescriptorBundle(
	const WindowDescriptorBundleDesc& desc,
	const NativeWindowDescriptorBundle& native) {
	std::scoped_lock lock(impl_->mutex);
	impl_->requireInitialized();
	impl_->requireSharedMutationPhase();
	Impl::WindowState& window = impl_->requireWindow(desc.window);
	if (!impl_->usableRendererLayout(desc.layout) || native.descriptorPool == 0 ||
		desc.framesInFlight != window.frames.size() ||
		native.globalsSets.size() != desc.framesInFlight || native.textureSets.size() != desc.framesInFlight ||
		desc.descriptorCapacity == 0 || desc.descriptorCapacity != window.desc.maxTextureBindings ||
		desc.descriptorCapacity < window.nextDescriptorIndex ||
		desc.descriptorCapacity != impl_->rendererLayouts[desc.layout.index].key.textureDescriptorCapacity ||
		std::any_of(native.globalsSets.begin(), native.globalsSets.end(), [](uint64_t handle) { return handle == 0; }) ||
		std::any_of(native.textureSets.begin(), native.textureSets.end(), [](uint64_t handle) { return handle == 0; })) {
		storageError("window descriptor adoption failed validation");
	}
	std::vector<uint64_t> globals(native.globalsSets.begin(), native.globalsSets.end());
	std::vector<uint64_t> textures(native.textureSets.begin(), native.textureSets.end());
	const WindowDescriptorBundleHandle previous = window.activeDescriptorBundle;
	impl_->reserveRetirements(previous ? 1u : 0u);
	impl_->retainRendererLayout(desc.layout);
	try {
		const uint32_t index =
			Impl::acquireIndex(impl_->windowDescriptorBundles, impl_->freeWindowDescriptorBundles);
		Impl::WindowDescriptorBundleRecord& record = impl_->windowDescriptorBundles[index];
		const uint32_t generation = record.generation == 0 ? 1u : record.generation;
		record = Impl::WindowDescriptorBundleRecord{
			.generation = generation,
			.state = ResourceState::Ready,
			.desc = desc,
			.nativePool = native.descriptorPool,
			.globalsSets = std::move(globals),
			.textureSets = std::move(textures),
			.referenceCount = 1,
			.lastUse = 0,
		};
		const WindowDescriptorBundleHandle handle{index, generation};
		window.activeDescriptorBundle = handle;
		for (auto& frame : window.frames) {
			frame->appliedBindingRevisions.clear();
			frame->preparedBindingBatches.clear();
			frame->currentBindingBatch = 0;
		}
		if (previous) impl_->releaseWindowDescriptorBundleReference(previous, 0);
		return handle;
	} catch (...) {
		impl_->releaseRendererLayoutReference(desc.layout, 0);
		throw;
	}
}

NativeRendererLayout FlowStorageSystem::nativeRendererLayout(RendererLayoutHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return impl_->usableRendererLayout(handle) ? impl_->rendererLayouts[handle.index].native : NativeRendererLayout{};
}

NativeRendererPipelineBundle FlowStorageSystem::nativeRendererPipelineBundle(
	RendererPipelineBundleHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return impl_->usableRendererPipelineBundle(handle)
		? impl_->rendererPipelineBundles[handle.index].native
		: NativeRendererPipelineBundle{};
}

NativeWindowDescriptorView FlowStorageSystem::nativeWindowDescriptorBundle(
	WindowDescriptorBundleHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->usableWindowDescriptorBundle(handle)) return {};
	const Impl::WindowDescriptorBundleRecord& record = impl_->windowDescriptorBundles[handle.index];
	return NativeWindowDescriptorView{
		.descriptorPool = record.nativePool,
		.globalsSets = record.globalsSets,
		.textureSets = record.textureSets,
		.descriptorCapacity = record.desc.descriptorCapacity,
	};
}

void FlowStorageSystem::releaseRendererLayout(RendererLayoutHandle handle, SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseRendererLayoutReference(handle, lastUse);
}

void FlowStorageSystem::releaseRendererPipelineBundle(
	RendererPipelineBundleHandle handle,
	SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	impl_->releaseRendererPipelineBundleReference(handle, lastUse);
}

void FlowStorageSystem::releaseWindowDescriptorBundle(
	WindowDescriptorBundleHandle handle,
	SubmissionSerial lastUse) {
	std::scoped_lock lock(impl_->mutex);
	if (impl_->validWindowDescriptorBundle(handle)) {
		const WindowId windowId = impl_->windowDescriptorBundles[handle.index].desc.window;
		if (auto window = impl_->windows.find(windowId);
			window != impl_->windows.end() && window->second->activeDescriptorBundle == handle) {
			window->second->activeDescriptorBundle = {};
		}
	}
	impl_->releaseWindowDescriptorBundleReference(handle, lastUse);
}

NativeBufferView FlowStorageSystem::nativeBuffer(BufferHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->usableBuffer(handle)) return {};
	const Impl::BufferRecord& record = impl_->buffers[handle.index];
	return NativeBufferView{
		.nativeBuffer = nativeHandle(record.buffer),
		.size = record.size,
		.hostCoherent = (record.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0,
	};
}

NativeImageView FlowStorageSystem::nativeImage(ImageHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->usableImage(handle)) return {};
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

NativeImageViewInfo FlowStorageSystem::nativeImageView(ImageViewHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->usableImageView(handle)) return {};
	const Impl::ImageViewRecord& record = impl_->imageViews[handle.index];
	return NativeImageViewInfo{
		.nativeImageView = nativeHandle(record.view),
		.image = record.image,
	};
}

NativeSamplerInfo FlowStorageSystem::nativeSampler(SamplerHandle handle) const noexcept {
	std::scoped_lock lock(impl_->mutex);
	if (!impl_->usableSampler(handle)) return {};
	return NativeSamplerInfo{
		.nativeSampler = nativeHandle(impl_->samplers[handle.index].sampler),
	};
}

} // namespace FlowUi::detail::storage
