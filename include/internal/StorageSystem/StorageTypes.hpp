#pragma once

#include <array>
#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#include "FlowUi/BuildConfig.hpp"

namespace FlowUi::detail::storage {

using WindowId = uint64_t;
using SubmissionSerial = uint64_t;
using StringId = uint32_t;
using AllocationId = uint32_t;
using UploadId = uint64_t;
using FrameEpoch = uint64_t;

inline constexpr uint32_t InvalidIndex = 0u;
inline constexpr uint32_t InvalidGeneration = 0u;
inline constexpr uint32_t InvalidFrameSlot = std::numeric_limits<uint32_t>::max();

enum class ResourceKind : uint8_t {
	Invalid = 0,
	CpuBlob,
	GpuBuffer,
	GpuImage,
	ImageView,
	Sampler,
	TextureView,
	FontFace,
	FontAtlas,
	SvgDocument,
	IconVariant,
	ViewportTarget,
	RendererLayout,
	RendererPipelineBundle,
	WindowDescriptorBundle,
	Count,
};

enum class ResourceDomain : uint16_t {
	Image = 0,
	Icon,
	Font,
	Viewport,
	Renderer,
	Layout,
	Input,
	Development,
	Internal,
};

enum class MemoryClass : uint8_t {
	Persistent = 0,
	ResourceMetadata,
	StringPool,
	WindowPersistent,
	FrameTransient,
	WorkerTransient,
	DecodeTransient,
	UploadStaging,
	Count,
};

enum class ResourceState : uint8_t {
	Invalid = 0,
	Queued,
	Decoding,
	Uploading,
	Ready,
	Failed,
	Retiring,
};

enum class AccessMode : uint8_t {
	ReadOnly = 0,
	CpuWrite,
	GpuWrite,
	CpuAndGpuWrite,
};

enum class ResourceSharing : uint8_t {
	AppShared = 0,
	WindowLocal,
	// Owned by one frames-in-flight slot and reusable across that slot's epochs.
	FrameLocal,
};

enum class MemoryPreference : uint8_t {
	Automatic = 0,
	DeviceLocal,
	HostVisible,
	HostCached,
};

enum class PixelFormat : uint16_t {
	Undefined = 0,
	R8Unorm,
	Rgba8Unorm,
	Rgba8Srgb,
	Bgra8Unorm,
	Bgra8Srgb,
	Rgba16Float,
	D32Float,
};

enum class ImageType : uint8_t {
	Image2D = 0,
	Image2DArray,
};

enum class ImageUsage : uint32_t {
	None = 0,
	Sampled = 1u << 0u,
	TransferSource = 1u << 1u,
	TransferDestination = 1u << 2u,
	ColorAttachment = 1u << 3u,
	DepthAttachment = 1u << 4u,
	Storage = 1u << 5u,
};

enum class BufferUsage : uint32_t {
	None = 0,
	TransferSource = 1u << 0u,
	TransferDestination = 1u << 1u,
	Vertex = 1u << 2u,
	Index = 1u << 3u,
	Uniform = 1u << 4u,
	Storage = 1u << 5u,
	Indirect = 1u << 6u,
};

template <typename Enum>
constexpr Enum operator|(Enum lhs, Enum rhs) noexcept
	requires std::is_enum_v<Enum>
{
	using U = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename Enum>
constexpr bool hasFlag(Enum value, Enum flag) noexcept
	requires std::is_enum_v<Enum>
{
	using U = std::underlying_type_t<Enum>;
	return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
}

enum class FilterMode : uint8_t { Nearest = 0, Linear };
enum class AddressMode : uint8_t { Repeat = 0, MirroredRepeat, ClampToEdge, ClampToBorder };

enum class StorageCapability : uint64_t {
	None = 0,
	RuntimeGrowth = 1ull << 0u,
	WindowScopes = 1ull << 1u,
	WorkerArenas = 1ull << 2u,
	GenerationalHandles = 1ull << 3u,
	BatchedBindingResolution = 1ull << 4u,
	SynchronousUploads = 1ull << 5u,
	DeferredRetirement = 1ull << 6u,
	VulkanInterop = 1ull << 7u,
	DirectMappedWrites = 1ull << 8u,
	HostScratchBufferWrites = 1ull << 9u,
	BindingWriteBatches = 1ull << 10u,
	FrameReadLeases = 1ull << 11u,
	RendererResourceBundles = 1ull << 12u,
	DevelopmentTelemetry = 1ull << 13u,
};

template <ResourceKind KindValue>
struct Handle {
	static constexpr ResourceKind Kind = KindValue;

	uint32_t index = InvalidIndex;
	uint32_t generation = InvalidGeneration;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return index != InvalidIndex && generation != InvalidGeneration;
	}

	[[nodiscard]] constexpr uint64_t packed() const noexcept {
		return (static_cast<uint64_t>(generation) << 32u) | index;
	}

	static constexpr Handle fromPacked(uint64_t value) noexcept {
		return Handle{
			.index = static_cast<uint32_t>(value),
			.generation = static_cast<uint32_t>(value >> 32u),
		};
	}
	auto operator<=>(const Handle&) const = default;
};

using BlobHandle = Handle<ResourceKind::CpuBlob>;
using BufferHandle = Handle<ResourceKind::GpuBuffer>;
using ImageHandle = Handle<ResourceKind::GpuImage>;
using ImageViewHandle = Handle<ResourceKind::ImageView>;
using SamplerHandle = Handle<ResourceKind::Sampler>;
using TextureHandle = Handle<ResourceKind::TextureView>;
using FontFaceHandle = Handle<ResourceKind::FontFace>;
using FontAtlasHandle = Handle<ResourceKind::FontAtlas>;
using SvgDocumentHandle = Handle<ResourceKind::SvgDocument>;
using IconVariantHandle = Handle<ResourceKind::IconVariant>;
using ViewportHandle = Handle<ResourceKind::ViewportTarget>;
using RendererLayoutHandle = Handle<ResourceKind::RendererLayout>;
using RendererPipelineBundleHandle = Handle<ResourceKind::RendererPipelineBundle>;
using WindowDescriptorBundleHandle = Handle<ResourceKind::WindowDescriptorBundle>;

struct ResourceKey {
	ResourceDomain domain = ResourceDomain::Internal;
	StringId name = 0;
	WindowId window = 0;
	auto operator<=>(const ResourceKey&) const = default;
};

struct ResourceKeyHash {
	size_t operator()(const ResourceKey& key) const noexcept {
		uint64_t value = static_cast<uint64_t>(key.name);
		value ^= static_cast<uint64_t>(key.domain) << 32u;
		value ^= key.window + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
		return static_cast<size_t>(value);
	}
};

struct AllocationTag {
	MemoryClass memoryClass = MemoryClass::Persistent;
	ResourceKind resourceKind = ResourceKind::Invalid;
	WindowId window = 0;
	uint32_t frameSlot = InvalidFrameSlot;
	StringId debugName = 0;
};

struct MemoryBlock {
	void* data = nullptr;
	size_t size = 0;
	AllocationId id = 0;
	AllocationTag tag{};

	[[nodiscard]] explicit operator bool() const noexcept { return data != nullptr; }
};

#if FLOW_UI_DEV_MODE
struct ArenaLeaseState {
	std::atomic<bool> valid{true};
};
#endif

struct ArenaView {
	using AllocateFunction = void* (*)(void*, size_t, size_t);

	void* context = nullptr;
	AllocateFunction allocateFunction = nullptr;
	FrameEpoch epoch = 0;
#if FLOW_UI_DEV_MODE
	std::shared_ptr<const ArenaLeaseState> validation{};
#endif

	[[nodiscard]] void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) const {
#if FLOW_UI_DEV_MODE
		if (!validation || !validation->valid.load(std::memory_order_acquire)) return nullptr;
#endif
		return allocateFunction ? allocateFunction(context, bytes, alignment) : nullptr;
	}

	template <typename T>
	[[nodiscard]] std::span<T> allocateArray(size_t count) const {
		if (count == 0) {
			return {};
		}
		if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
			return {};
		}
		void* memory = allocate(sizeof(T) * count, alignof(T));
		return memory ? std::span<T>(static_cast<T*>(memory), count) : std::span<T>{};
	}
};

struct FrameToken {
	WindowId window = 0;
	uint32_t frameSlot = 0;
	uint64_t frameNumber = 0;
	FrameEpoch epoch = 0;

	[[nodiscard]] explicit operator bool() const noexcept { return window != 0 && epoch != 0; }
};

#if FLOW_UI_DEV_MODE
struct ReadLeaseState {
	std::atomic<bool> valid{true};
};
#endif

struct FrameReadLease {
	FrameToken frame{};
	uint64_t leaseId = 0;
#if FLOW_UI_DEV_MODE
	std::shared_ptr<const ReadLeaseState> validation{};
#endif

	[[nodiscard]] explicit operator bool() const noexcept {
		return static_cast<bool>(frame) && leaseId != 0;
	}

	[[nodiscard]] bool valid() const noexcept {
		if (!static_cast<bool>(*this)) return false;
#if FLOW_UI_DEV_MODE
		return validation && validation->valid.load(std::memory_order_acquire);
#else
		return true;
#endif
	}
};

struct UploadTicket {
	UploadId value = 0;
	[[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
};

struct SubmissionToken {
	WindowId window = 0;
	SubmissionSerial serial = 0;
	uint32_t frameSlot = 0;
	[[nodiscard]] explicit operator bool() const noexcept { return serial != 0; }
};

enum class BufferWriteMode : uint8_t {
	Default = 0,
	DirectMapped,
	HostScratchThenCopy,
};

struct StorageConfig {
	uint64_t initialPersistentCpuBytes = 4ull * 1024ull * 1024ull;
	uint64_t initialStringBytes = 1ull * 1024ull * 1024ull;
	uint64_t initialDecodeScratchBytes = 8ull * 1024ull * 1024ull;
	uint64_t initialUploadStagingBytes = 16ull * 1024ull * 1024ull;
	uint64_t transientBytesPerFramePerWindow = 1ull * 1024ull * 1024ull;
	uint64_t transientBytesPerWorker = 1ull * 1024ull * 1024ull;
	uint64_t initialInstanceBytesPerFrame = 256ull * 1024ull;
	uint64_t cpuSoftBudgetBytes = 256ull * 1024ull * 1024ull;
	uint64_t gpuSoftBudgetBytes = 512ull * 1024ull * 1024ull;
	uint32_t expectedBlobs = 128;
	uint32_t expectedBuffers = 64;
	uint32_t expectedImages = 256;
	uint32_t expectedImageViews = 384;
	uint32_t expectedSamplers = 16;
	uint32_t expectedTextureViews = 512;
	uint32_t expectedRendererObjects = 32;
	uint32_t expectedWindows = 2;
	uint32_t expectedBindingsPerWindow = 512;
	uint32_t framesInFlight = 2;
	uint32_t expectedWorkerCount = 1;
	float growthFactor = 1.5f;
	BufferWriteMode defaultBufferWriteMode = BufferWriteMode::DirectMapped;
	bool allowRuntimeGrowth = true;
	bool detailedTracking = false;
};

struct WindowStorageDesc {
	uint32_t framesInFlight = 2;
	uint32_t workerCount = 1;
	uint32_t initialTextureBindings = 512;
	uint32_t maxTextureBindings = 4096;
	uint64_t transientBytesPerFrame = 1ull * 1024ull * 1024ull;
	uint64_t transientBytesPerWorker = 1ull * 1024ull * 1024ull;
	StringId debugName = 0;
};

struct FrameStorageDesc {
	uint32_t frameSlot = 0;
	uint64_t frameNumber = 0;
};

struct BufferDesc {
	uint64_t size = 0;
	BufferUsage usage = BufferUsage::None;
	MemoryPreference memory = MemoryPreference::Automatic;
	ResourceSharing sharing = ResourceSharing::AppShared;
	AccessMode access = AccessMode::ReadOnly;
	bool persistentlyMapped = false;
	bool evictable = false;
	WindowId window = 0;
	uint32_t frameSlot = InvalidFrameSlot;
	StringId debugName = 0;
};

struct ImageDesc {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t layers = 1;
	uint32_t mipLevels = 1;
	PixelFormat format = PixelFormat::Rgba8Srgb;
	ImageType type = ImageType::Image2D;
	ImageUsage usage = ImageUsage::Sampled | ImageUsage::TransferDestination;
	MemoryPreference memory = MemoryPreference::DeviceLocal;
	ResourceSharing sharing = ResourceSharing::AppShared;
	AccessMode access = AccessMode::ReadOnly;
	bool evictable = false;
	WindowId window = 0;
	uint32_t frameSlot = InvalidFrameSlot;
	StringId debugName = 0;
};

struct ImageViewDesc {
	ImageType type = ImageType::Image2D;
	PixelFormat format = PixelFormat::Undefined;
	uint32_t baseMipLevel = 0;
	uint32_t mipLevelCount = 1;
	uint32_t baseArrayLayer = 0;
	uint32_t arrayLayerCount = 1;
	bool depthAspect = false;
	StringId debugName = 0;
};

struct SamplerDesc {
	FilterMode minFilter = FilterMode::Linear;
	FilterMode magFilter = FilterMode::Linear;
	AddressMode addressU = AddressMode::ClampToEdge;
	AddressMode addressV = AddressMode::ClampToEdge;
	AddressMode addressW = AddressMode::ClampToEdge;
	float minLod = 0.0f;
	float maxLod = 0.0f;
	float maxAnisotropy = 1.0f;
	bool anisotropy = false;
	StringId debugName = 0;
};

struct TextureViewDesc {
	ImageViewHandle imageView{};
	SamplerHandle sampler{};
	float uv0x = 0.0f;
	float uv0y = 0.0f;
	float uv1x = 1.0f;
	float uv1y = 1.0f;
	int32_t sourceWidth = 0;
	int32_t sourceHeight = 0;
};

struct ImageRegion {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t mipLevel = 0;
	uint32_t baseArrayLayer = 0;
	uint32_t layerCount = 1;
};

enum class UploadDestination : uint8_t { Buffer = 0, Image };

struct UploadRequest {
	UploadDestination destination = UploadDestination::Image;
	BlobHandle source{};
	uint64_t sourceOffset = 0;
	uint64_t byteCount = 0;
	BufferHandle destinationBuffer{};
	uint64_t destinationBufferOffset = 0;
	ImageHandle destinationImage{};
	ImageRegion imageRegion{};
	ResourceState finalState = ResourceState::Ready;
	bool releaseSourceWhenComplete = false;
};

struct TextureMetadata {
	ResourceState state = ResourceState::Invalid;
	int32_t sourceWidth = 0;
	int32_t sourceHeight = 0;
	uint32_t revision = 0;
};

struct BufferWriteView {
	BufferHandle buffer{};
	std::byte* data = nullptr;
	uint64_t destinationOffset = 0;
	uint64_t capacity = 0;
	FrameEpoch epoch = 0;
	uint64_t writeId = 0;
	BufferWriteMode mode = BufferWriteMode::Default;

	[[nodiscard]] explicit operator bool() const noexcept {
		return static_cast<bool>(buffer) && data != nullptr && writeId != 0;
	}
};

struct ResolvedTextureBinding {
	uint32_t descriptorIndex = 0;
	uint32_t bindingRevision = 0;
	ResourceState state = ResourceState::Invalid;
	uint64_t nativeImageView = 0;
	uint64_t nativeSampler = 0;
};

struct DescriptorWriteRecord {
	TextureHandle texture{};
	uint32_t descriptorIndex = 0;
	uint32_t bindingRevision = 0;
	ResourceState state = ResourceState::Invalid;
	uint8_t reserved[3]{};
	uint64_t nativeImageView = 0;
	uint64_t nativeSampler = 0;
};

struct PreparedTextureBindings {
	std::span<const DescriptorWriteRecord> dirtyBindings{};
	uint32_t requiredDescriptorCapacity = 1;
	FrameEpoch epoch = 0;
};

struct TextureHotRecord {
	uint32_t generation = 0;
	uint32_t revision = 0;
	uint32_t imageViewIndex = 0;
	uint32_t imageViewGeneration = 0;
	uint32_t samplerIndex = 0;
	uint32_t samplerGeneration = 0;
	ResourceState state = ResourceState::Invalid;
	uint8_t reserved[3]{};
	int32_t sourceWidth = 0;
	int32_t sourceHeight = 0;
};

struct ImageViewHotRecord {
	uint32_t generation = 0;
	uint32_t imageIndex = 0;
	uint32_t imageGeneration = 0;
	uint32_t reserved = 0;
	uint64_t nativeImageView = 0;
};

struct SamplerHotRecord {
	uint32_t generation = 0;
	uint32_t reserved = 0;
	uint64_t nativeSampler = 0;
};

struct BindingHotRecord {
	uint32_t textureGeneration = 0;
	uint32_t textureRevision = 0;
	uint32_t descriptorIndex = 0;
	uint32_t bindingRevision = 0;
	ResourceState state = ResourceState::Invalid;
	uint8_t reserved[3]{};
	uint64_t nativeImageView = 0;
	uint64_t nativeSampler = 0;
};

struct StorageReadView {
	std::span<const TextureHotRecord> textures{};
	std::span<const ImageViewHotRecord> imageViews{};
	std::span<const SamplerHotRecord> samplers{};
	FrameEpoch epoch = 0;
#if FLOW_UI_DEV_MODE
	std::shared_ptr<const ReadLeaseState> validation{};
#endif

	[[nodiscard]] bool valid() const noexcept {
#if FLOW_UI_DEV_MODE
		return validation && validation->valid.load(std::memory_order_acquire);
#else
		return epoch != 0;
#endif
	}

	[[nodiscard]] const TextureHotRecord* texture(TextureHandle handle) const noexcept {
		if (!valid()) return nullptr;
		if (!handle || handle.index >= textures.size()) {
			return nullptr;
		}
		const TextureHotRecord& record = textures[handle.index];
		return record.generation == handle.generation ? &record : nullptr;
	}

	[[nodiscard]] const ImageViewHotRecord* imageView(ImageViewHandle handle) const noexcept {
		if (!valid()) return nullptr;
		if (!handle || handle.index >= imageViews.size()) return nullptr;
		const ImageViewHotRecord& record = imageViews[handle.index];
		return record.generation == handle.generation ? &record : nullptr;
	}

	[[nodiscard]] const SamplerHotRecord* sampler(SamplerHandle handle) const noexcept {
		if (!valid()) return nullptr;
		if (!handle || handle.index >= samplers.size()) return nullptr;
		const SamplerHotRecord& record = samplers[handle.index];
		return record.generation == handle.generation ? &record : nullptr;
	}
};

struct WindowBindingView {
	std::span<const BindingHotRecord> bindingsByTextureIndex{};
	FrameEpoch epoch = 0;
#if FLOW_UI_DEV_MODE
	std::shared_ptr<const ReadLeaseState> validation{};
#endif

	[[nodiscard]] bool valid() const noexcept {
#if FLOW_UI_DEV_MODE
		return validation && validation->valid.load(std::memory_order_acquire);
#else
		return epoch != 0;
#endif
	}

	[[nodiscard]] const BindingHotRecord* binding(TextureHandle handle) const noexcept {
		if (!valid()) return nullptr;
		if (!handle || handle.index >= bindingsByTextureIndex.size()) {
			return nullptr;
		}
		const BindingHotRecord& record = bindingsByTextureIndex[handle.index];
		return record.textureGeneration == handle.generation ? &record : nullptr;
	}
};

struct WindowStorageSnapshot {
	WindowId window = 0;
	uint32_t framesInFlight = 0;
	uint32_t bindingCapacity = 0;
	uint32_t liveBindings = 0;
	uint32_t retiredBindings = 0;
	uint64_t transientCapacityBytes = 0;
	uint64_t transientHighWaterBytes = 0;
};

struct MemoryClassStats {
	uint64_t reservedBytes = 0;
	uint64_t committedBytes = 0;
	uint64_t liveBytes = 0;
	uint64_t peakLiveBytes = 0;
	uint64_t allocationCount = 0;
	uint64_t growthCount = 0;
};

struct StorageStats {
	std::array<MemoryClassStats, static_cast<size_t>(MemoryClass::Count)> cpu{};
	uint64_t gpuLiveBytes = 0;
	uint64_t gpuRetiredBytes = 0;
	uint64_t gpuPeakBytes = 0;
	uint64_t cpuSoftBudgetBytes = 0;
	uint64_t gpuSoftBudgetBytes = 0;
	uint64_t queuedUploadBytes = 0;
	uint64_t completedUploadBytes = 0;
	uint64_t bindingCacheHits = 0;
	uint64_t bindingCacheMisses = 0;
	uint64_t invalidHandleCount = 0;
	uint64_t currentSubmissionSerial = 0;
	uint64_t completedSubmissionSerial = 0;
	uint32_t windowCount = 0;
};

struct ResourceStats {
	ResourceKind kind = ResourceKind::Invalid;
	uint64_t liveBytes = 0;
	uint64_t retiredBytes = 0;
	uint32_t slots = 0;
	uint32_t live = 0;
	uint32_t free = 0;
	uint32_t queued = 0;
	uint32_t ready = 0;
	uint32_t failed = 0;
	uint32_t retiring = 0;
};

enum class RetirementKind : uint8_t {
	Blob = 0,
	Buffer,
	Image,
	ImageView,
	Sampler,
	Texture,
	RendererLayout,
	RendererPipelineBundle,
	WindowDescriptorBundle,
};

struct RetirementRequest {
	RetirementKind kind = RetirementKind::Blob;
	uint64_t packedHandle = 0;
	SubmissionSerial retireAfter = 0;
};

struct NativeBufferView {
	uint64_t nativeBuffer = 0;
	uint64_t size = 0;
	bool hostCoherent = false;
};

struct NativeImageView {
	uint64_t nativeImage = 0;
	uint64_t nativeImageView = 0;
	PixelFormat format = PixelFormat::Undefined;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t layers = 0;
};

struct RendererLayoutKey {
	uint32_t textureDescriptorCapacity = 0;
	uint32_t shaderInterfaceRevision = 0;
	uint32_t pushConstantBytes = 0;
	uint32_t descriptorFeatureFlags = 0;
	auto operator<=>(const RendererLayoutKey&) const = default;
};

struct RendererLayoutKeyHash {
	size_t operator()(const RendererLayoutKey& key) const noexcept {
		uint64_t value = key.textureDescriptorCapacity;
		value ^= static_cast<uint64_t>(key.shaderInterfaceRevision) << 16u;
		value ^= static_cast<uint64_t>(key.pushConstantBytes) << 32u;
		value ^= static_cast<uint64_t>(key.descriptorFeatureFlags) << 48u;
		return static_cast<size_t>(value ^ (value >> 33u));
	}
};

struct NativeRendererLayout {
	uint64_t globalsSetLayout = 0;
	uint64_t texturesSetLayout = 0;
	uint64_t pipelineLayout = 0;
};

struct RendererPipelineKey {
	RendererLayoutHandle layout{};
	uint32_t nativeColorFormat = 0;
	uint32_t sampleCount = 1;
	uint32_t pipelineStateRevision = 0;
	uint64_t shaderSetFingerprint = 0;
	auto operator<=>(const RendererPipelineKey&) const = default;
};

struct RendererPipelineKeyHash {
	size_t operator()(const RendererPipelineKey& key) const noexcept {
		uint64_t value = key.layout.packed();
		value ^= static_cast<uint64_t>(key.nativeColorFormat) << 7u;
		value ^= static_cast<uint64_t>(key.sampleCount) << 23u;
		value ^= static_cast<uint64_t>(key.pipelineStateRevision) << 39u;
		value ^= key.shaderSetFingerprint + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
		return static_cast<size_t>(value ^ (value >> 32u));
	}
};

struct NativeRendererPipelineBundle {
	uint64_t pipelineLayout = 0;
	std::array<uint64_t, 3> pipelines{};
};

struct WindowDescriptorBundleDesc {
	WindowId window = 0;
	RendererLayoutHandle layout{};
	uint32_t framesInFlight = 0;
	uint32_t descriptorCapacity = 0;
	StringId debugName = 0;
};

struct NativeWindowDescriptorBundle {
	uint64_t descriptorPool = 0;
	std::span<const uint64_t> globalsSets{};
	std::span<const uint64_t> textureSets{};
};

struct NativeWindowDescriptorView {
	uint64_t descriptorPool = 0;
	std::span<const uint64_t> globalsSets{};
	std::span<const uint64_t> textureSets{};
	uint32_t descriptorCapacity = 0;
};

template <typename HandleType>
struct NativePublishResult {
	HandleType handle{};
	bool ownershipTransferred = false;
};

struct ResourceUse {
	ResourceKind kind = ResourceKind::Invalid;
	uint64_t packedHandle = 0;
};

template <ResourceKind Kind>
[[nodiscard]] constexpr ResourceUse useOf(Handle<Kind> handle) noexcept {
	return ResourceUse{Kind, handle.packed()};
}

} // namespace FlowUi::detail::storage
