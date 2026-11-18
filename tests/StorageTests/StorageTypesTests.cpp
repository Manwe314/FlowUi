#include "TestHarness.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "FlowUi/Private/storageSystem/AlignedRecord.hpp"
#include "FlowUi/Private/storageSystem/PersistentRecord.hpp"
#include "FlowUi/Private/storageSystem/StorageCalculations.hpp"
#include "FlowUi/Private/storageSystem/StorageContractMetadata.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include "FlowUi/Private/storageSystem/vulkan/StorageFormatConversions.hpp"

namespace {

using namespace FlowUi::detail::storage;

static_assert(std::is_same_v<std::underlying_type_t<ResourceDomain>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<PixelFormat>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(ResourceDomain::Image) == 0u);
static_assert(static_cast<std::uint8_t>(ResourceDomain::Action) == 9u);
static_assert(static_cast<std::uint8_t>(PixelFormat::Undefined) == 0u);
static_assert(static_cast<std::uint8_t>(PixelFormat::D32Float) == 7u);
static_assert(sizeof(SubmissionSerial) == sizeof(std::uint64_t));
static_assert(sizeof(FrameEpoch) == sizeof(std::uint64_t));

struct FixedRecordHeader {
	uint32_t marker = 0;
};
struct alignas(128) FixedRecordPayload {
	std::array<std::byte, 65> bytes{};
};

static_assert(typedRecordLayout<FixedRecordHeader, uint64_t>.headerBytes == 4);
static_assert(typedRecordLayout<FixedRecordHeader, uint64_t>.payloadOffset == 8);
static_assert(typedRecordLayout<FixedRecordHeader, uint64_t>.allocationBytes == 16);
static_assert(typedRecordLayout<FixedRecordHeader, FixedRecordPayload>.payloadOffset == 128);
static_assert(typedRecordLayout<FixedRecordHeader, FixedRecordPayload>.payloadBytes == 128);
static_assert(typedRecordLayout<FixedRecordHeader, FixedRecordPayload>.allocationBytes == 256);
static_assert(typedRecordLayout<FixedRecordHeader, FixedRecordPayload>.allocationAlignment == 128);
static_assert(makeAlignedRecordLayout(4, 4, 128, 128).payloadOffset ==
			  typedRecordLayout<FixedRecordHeader, FixedRecordPayload>.payloadOffset);
static_assert(checkedAdd(0, UINT64_MAX) == UINT64_MAX);
static_assert(checkedAdd(17, 25) == 42);
static_assert(checkedMultiply(UINT64_MAX, 0) == 0);
static_assert(checkedMultiply(6, 7) == 42);
static_assert(checkedSize(42) == 42);
static_assert(alignUp(0, 64) == 0);
static_assert(alignUp(65, 64) == 128);
static_assert(nextGeneration(0) == 1);
static_assert(nextGeneration(6) == 7);
static_assert(nextGeneration(UINT32_MAX) == 1);
static_assert(estimateImageBytes(ImageDesc{.width = 16, .height = 8}) == 512);
static_assert(estimateImageBytes(ImageDesc{
				  .width = 8, .height = 4, .depth = 2, .layers = 3, .mipLevels = 3}) == 888);
static_assert(estimateImageBytes(ImageDesc{.mipLevels = 4, .format = PixelFormat::R8Unorm}) == 4);
static_assert(estimateImageBytes(ImageDesc{.format = PixelFormat::Undefined}) == 0);
static_assert(estimateImageBytes(ImageDesc{.width = 0}) == 0);
static_assert(ResourceKeyHash{}(ResourceKey{ResourceDomain::Image, 0, 0}) ==
			  static_cast<size_t>(0x9e3779b97f4a7c15ull));
static_assert(RendererLayoutKeyHash{}(RendererLayoutKey{}) == 0);
static_assert(RendererLayoutKeyHash{}(RendererLayoutKey{.textureDescriptorCapacity = 16}) == 16);
static_assert(RendererPipelineKeyHash{}(RendererPipelineKey{.sampleCount = 1}) !=
			  RendererPipelineKeyHash{}(RendererPipelineKey{.sampleCount = 2}));
static_assert(BufferHandle::fromPacked(0x12345678abcdef01ull).packed() == 0x12345678abcdef01ull);
static_assert(useOf(BufferHandle{3, 4}).packedHandle == BufferHandle{3, 4}.packed());
static_assert(!PersistentRecordView{} && !ConstPersistentRecordView{} && !ManagerFrameView{});
static_assert(!MemoryBlock{} && !FrameToken{} && !UploadTicket{} && !SubmissionToken{});
static_assert(!BufferWriteView{} &&
			  !TypedPersistentRecordView<FixedRecordHeader, FixedRecordPayload>{});
static_assert(!ConstTypedPersistentRecordView<FixedRecordHeader, FixedRecordPayload>{});
static_assert(FrameToken{.window = 1, .epoch = 1});
static_assert(UploadTicket{.value = 1} && SubmissionToken{.serial = 1});
static_assert(supportsStorageCapabilities(flowStorageSupportedCapabilities,
										  StorageCapability::None));
static_assert(supportsStorageCapabilities(flowStorageSupportedCapabilities,
										  StorageCapability::RuntimeGrowth |
											  StorageCapability::VulkanInterop));
static_assert(!supportsStorageCapabilities(storageCapabilityMask(StorageCapability::RuntimeGrowth),
										   StorageCapability::RuntimeGrowth |
											   StorageCapability::VulkanInterop));
static_assert(storageContractRevision == 1u);
static_assert(currentStorageInterfaceVersion == 8u);
#if FLOW_UI_DEV_MODE
static_assert(flowStorageSupportedCapabilities == 0x3bfffull);
#else
static_assert(flowStorageSupportedCapabilities == 0x39fffull);
#endif
static_assert(FlowUi::detail::validateInitialCapacityDefinitions());
static_assert(FlowUi::detail::InitialMemoryCapacities{}.get(
				  FlowUi::detail::InitialCapacityTarget::storage_windows) == 2);

[[nodiscard]] constexpr bool fixedVulkanMappingsMatch() noexcept {
	constexpr std::array formats{
		std::pair{PixelFormat::Undefined, VK_FORMAT_UNDEFINED},
		std::pair{PixelFormat::R8Unorm, VK_FORMAT_R8_UNORM},
		std::pair{PixelFormat::Rgba8Unorm, VK_FORMAT_R8G8B8A8_UNORM},
		std::pair{PixelFormat::Rgba8Srgb, VK_FORMAT_R8G8B8A8_SRGB},
		std::pair{PixelFormat::Bgra8Unorm, VK_FORMAT_B8G8R8A8_UNORM},
		std::pair{PixelFormat::Bgra8Srgb, VK_FORMAT_B8G8R8A8_SRGB},
		std::pair{PixelFormat::Rgba16Float, VK_FORMAT_R16G16B16A16_SFLOAT},
		std::pair{PixelFormat::D32Float, VK_FORMAT_D32_SFLOAT},
	};
	constexpr std::array<uint32_t, formats.size()> pixelBytes{0, 1, 4, 4, 4, 4, 8, 4};
	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex) {
		if (toVkFormat(formats[formatIndex].first) != formats[formatIndex].second ||
			bytesPerPixel(formats[formatIndex].first) != pixelBytes[formatIndex])
			return false;
	}
	constexpr std::array imageUsages{
		std::pair{ImageUsage::None, VkImageUsageFlags{0}},
		std::pair{ImageUsage::Sampled, VkImageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT}},
		std::pair{ImageUsage::TransferSource, VkImageUsageFlags{VK_IMAGE_USAGE_TRANSFER_SRC_BIT}},
		std::pair{ImageUsage::TransferDestination,
				  VkImageUsageFlags{VK_IMAGE_USAGE_TRANSFER_DST_BIT}},
		std::pair{ImageUsage::ColorAttachment,
				  VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT}},
		std::pair{ImageUsage::DepthAttachment,
				  VkImageUsageFlags{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}},
		std::pair{ImageUsage::Storage, VkImageUsageFlags{VK_IMAGE_USAGE_STORAGE_BIT}},
	};
	for (const auto& [usage, expected] : imageUsages) {
		if (toVkImageUsage(usage) != expected)
			return false;
	}
	constexpr std::array bufferUsages{
		std::pair{BufferUsage::None, VkBufferUsageFlags{0}},
		std::pair{BufferUsage::TransferSource,
				  VkBufferUsageFlags{VK_BUFFER_USAGE_TRANSFER_SRC_BIT}},
		std::pair{BufferUsage::TransferDestination,
				  VkBufferUsageFlags{VK_BUFFER_USAGE_TRANSFER_DST_BIT}},
		std::pair{BufferUsage::Vertex, VkBufferUsageFlags{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}},
		std::pair{BufferUsage::Index, VkBufferUsageFlags{VK_BUFFER_USAGE_INDEX_BUFFER_BIT}},
		std::pair{BufferUsage::Uniform, VkBufferUsageFlags{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT}},
		std::pair{BufferUsage::Storage, VkBufferUsageFlags{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT}},
		std::pair{BufferUsage::Indirect, VkBufferUsageFlags{VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT}},
	};
	for (const auto& [usage, expected] : bufferUsages) {
		if (toVkBufferUsage(usage) != expected)
			return false;
	}
	return toVkFormat(static_cast<PixelFormat>(255)) == VK_FORMAT_UNDEFINED &&
		   bytesPerPixel(static_cast<PixelFormat>(255)) == 0 &&
		   toVkImageUsage(ImageUsage::Sampled | ImageUsage::TransferDestination) ==
			   (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) &&
		   toVkBufferUsage(BufferUsage::Vertex | BufferUsage::TransferDestination) ==
			   (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) &&
		   toVkFilter(FilterMode::Nearest) == VK_FILTER_NEAREST &&
		   toVkFilter(FilterMode::Linear) == VK_FILTER_LINEAR &&
		   toVkFilter(static_cast<FilterMode>(255)) == VK_FILTER_LINEAR &&
		   toVkAddressMode(AddressMode::Repeat) == VK_SAMPLER_ADDRESS_MODE_REPEAT &&
		   toVkAddressMode(AddressMode::MirroredRepeat) ==
			   VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT &&
		   toVkAddressMode(AddressMode::ClampToEdge) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE &&
		   toVkAddressMode(AddressMode::ClampToBorder) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER &&
		   toVkAddressMode(static_cast<AddressMode>(255)) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}
static_assert(fixedVulkanMappingsMatch());

[[nodiscard]] consteval bool malformedCapacityDefinitionsRejected() noexcept {
	using namespace FlowUi::detail;
	auto definitions = initial_capacity_definitions;
	definitions[0].target = InitialCapacityTarget::storage_windows;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions = initial_capacity_definitions;
	definitions[0].alignment = 0;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions[0].alignment = 3;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions = initial_capacity_definitions;
	definitions[0].default_value = definitions[0].maximum + 1;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions = initial_capacity_definitions;
	++definitions[0].default_value;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions = initial_capacity_definitions;
	definitions[1].key = definitions[0].key;
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	definitions = initial_capacity_definitions;
	definitions[0].key = {};
	if (validateInitialCapacityDefinitions(definitions))
		return false;
	return !validateInitialCapacityDefinitions(std::array<InitialCapacityDefinition, 0>{});
}
static_assert(malformedCapacityDefinitionsRejected());

template <typename Operation>
void checkStorageFailure(Operation&& operation, FlowUi::ErrorCode expectedCode,
						 FlowUi::ErrorSite expectedSite) {
	bool failed = false;
	try {
		std::invoke(std::forward<Operation>(operation));
	} catch (const FlowUi::FlowUiException& exception) {
		failed = true;
		FLOWUI_CHECK(exception.error().code == expectedCode);
		FLOWUI_CHECK(exception.error().site == expectedSite);
	}
	FLOWUI_CHECK(failed);
}

void testCalculationFailures() {
	using FlowUi::ErrorCode;
	using FlowUi::ErrorSite;
	checkStorageFailure([] { (void)checkedAdd(UINT64_MAX, 1); }, ErrorCode::ArithmeticOverflow,
						ErrorSite::StorageCalculateCapacity);
	checkStorageFailure([] { (void)checkedMultiply(UINT64_MAX, 2); }, ErrorCode::ArithmeticOverflow,
						ErrorSite::StorageCalculateCapacity);
	checkStorageFailure([] { (void)alignUp(4, 0); }, ErrorCode::StorageConfigurationInvalid,
						ErrorSite::StorageCalculateCapacity);
	checkStorageFailure([] { (void)alignUp(4, 3); }, ErrorCode::StorageConfigurationInvalid,
						ErrorSite::StorageCalculateCapacity);
	checkStorageFailure([] { (void)alignUp(SIZE_MAX, 64); }, ErrorCode::ArithmeticOverflow,
						ErrorSite::StorageCalculateCapacity);
	if constexpr (sizeof(size_t) < sizeof(uint64_t)) {
		checkStorageFailure([] { (void)checkedSize(UINT64_MAX); }, ErrorCode::ArithmeticOverflow,
							ErrorSite::StorageCalculateCapacity);
	}
	checkStorageFailure(
		[] {
			(void)estimateImageBytes(
				ImageDesc{.width = UINT32_MAX, .height = UINT32_MAX, .depth = UINT32_MAX});
		},
		ErrorCode::ArithmeticOverflow, ErrorSite::StorageCalculateCapacity);
	for (const auto& arguments :
		 std::array{std::array<size_t, 4>{0, 1, 1, 1}, std::array<size_t, 4>{1, 1, 0, 1},
					std::array<size_t, 4>{1, 0, 1, 1}, std::array<size_t, 4>{1, 1, 1, 3}}) {
		checkStorageFailure(
			[&] {
				(void)makeAlignedRecordLayout(arguments[0], arguments[1], arguments[2],
											  arguments[3]);
			},
			ErrorCode::StorageConfigurationInvalid, ErrorSite::ResourceCreatePersistentRecord);
	}
	checkStorageFailure([] { (void)makeAlignedRecordLayout(SIZE_MAX, 1, 1, 64); },
						ErrorCode::ArithmeticOverflow, ErrorSite::ResourceCreatePersistentRecord);
	checkStorageFailure([] { (void)makeAlignedRecordLayout(SIZE_MAX - 7, 1, 8, 1); },
						ErrorCode::ArithmeticOverflow, ErrorSite::ResourceCreatePersistentRecord);
}

struct ArenaProbe {
	alignas(64) std::array<std::byte, 256> memory{};
	size_t offset = 0;
	size_t calls = 0;
};

void* probeAllocate(void* context, size_t bytes, size_t alignment) {
	auto& probe = *static_cast<ArenaProbe*>(context);
	++probe.calls;
	if (alignment == 0 || (alignment & (alignment - 1u)) != 0) return nullptr;
	const size_t aligned = (probe.offset + alignment - 1u) & ~(alignment - 1u);
	if (aligned > probe.memory.size() || bytes > probe.memory.size() - aligned) return nullptr;
	probe.offset = aligned + bytes;
	return probe.memory.data() + aligned;
}

void* rejectAllocation(void* context, size_t, size_t) {
	++*static_cast<size_t*>(context);
	return nullptr;
}

void testHandlePacking() {
	static_assert(sizeof(TextureHandle) == sizeof(uint64_t));
	static_assert(std::is_trivially_copyable_v<TextureHandle>);
	static_assert(std::is_same_v<TextureHandle, FlowUi::TextureHandle>);

	FLOWUI_CHECK(!TextureHandle{});
	const TextureHandle handle{0xfedcba98u, 0x76543210u};
	FLOWUI_CHECK(handle);
	FLOWUI_CHECK(handle.packed() == 0x76543210fedcba98ull);
	FLOWUI_CHECK(TextureHandle::fromPacked(handle.packed()) == handle);
	FLOWUI_CHECK(!TextureHandle::fromPacked(0));
	const TextureHandle maximum_handle{std::numeric_limits<std::uint32_t>::max(),
									   std::numeric_limits<std::uint32_t>::max()};
	FLOWUI_CHECK(maximum_handle.packed() == std::numeric_limits<std::uint64_t>::max());
	FLOWUI_CHECK(TextureHandle::fromPacked(maximum_handle.packed()) == maximum_handle);

	static_assert(sizeof(PersistentRecordHandle) == sizeof(uint64_t));
	static_assert(std::is_trivially_copyable_v<PersistentRecordHandle>);
	const PersistentRecordHandle record{0xabcdef01u, 0x12345678u};
	FLOWUI_CHECK(record);
	FLOWUI_CHECK(PersistentRecordHandle::fromPacked(record.packed()) == record);
	FLOWUI_CHECK(!PersistentRecordHandle::fromPacked(0));
}

void testAlignedRecordLayout() {
	struct Header {
		uint32_t marker = 0;
	};
	struct alignas(64) Payload {
		std::byte bytes[65]{};
	};

	const AlignedRecordLayout layout = makeAlignedRecordLayout<Header, Payload>();
	FLOWUI_CHECK(layout.headerBytes == sizeof(Header));
	FLOWUI_CHECK(layout.payloadBytes == sizeof(Payload));
	FLOWUI_CHECK(layout.payloadOffset >= sizeof(Header));
	FLOWUI_CHECK(layout.payloadOffset % alignof(Payload) == 0);
	FLOWUI_CHECK(layout.allocationBytes == layout.payloadOffset + sizeof(Payload));
	FLOWUI_CHECK(layout.allocationAlignment == alignof(Payload));

	alignas(64) std::byte allocation[256]{};
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(layout.payload(allocation)) % alignof(Payload) == 0);
	FLOWUI_CHECK_THROWS(makeAlignedRecordLayout(0, 1, 1, 1));
	FLOWUI_CHECK_THROWS(makeAlignedRecordLayout(1, 3, 1, 1));
	FLOWUI_CHECK_THROWS(makeAlignedRecordLayout(
		std::numeric_limits<size_t>::max(), 1, 1, 64));
}

void testWindowIdentity() {
	static_assert(FlowUi::InvalidWindowId == 0u);
	static_assert(FlowUi::MainWindowId == 1u);
	static_assert(std::is_same_v<WindowId, FlowUi::WindowId>);
	FLOWUI_CHECK(FlowUi::MainWindowId != FlowUi::InvalidWindowId);
}

void testFlagsAndKeys() {
	const ImageUsage imageUsage = ImageUsage::Sampled | ImageUsage::TransferDestination;
	FLOWUI_CHECK(hasFlag(imageUsage, ImageUsage::Sampled));
	FLOWUI_CHECK(hasFlag(imageUsage, ImageUsage::TransferDestination));
	FLOWUI_CHECK(!hasFlag(imageUsage, ImageUsage::Storage));

	const BufferUsage bufferUsage = BufferUsage::Vertex | BufferUsage::TransferDestination;
	FLOWUI_CHECK(hasFlag(bufferUsage, BufferUsage::Vertex));
	FLOWUI_CHECK(!hasFlag(bufferUsage, BufferUsage::Index));

	const ResourceKey first{ResourceDomain::Renderer, 17u, 9u};
	const ResourceKey same{ResourceDomain::Renderer, 17u, 9u};
	const ResourceKey otherWindow{ResourceDomain::Renderer, 17u, 10u};
	const ResourceKey other_domain{ResourceDomain::Action, 17u, 9u};
	FLOWUI_CHECK(first == same);
	FLOWUI_CHECK(first != otherWindow);
	FLOWUI_CHECK(first != other_domain);
	FLOWUI_CHECK(ResourceKeyHash{}(first) == ResourceKeyHash{}(same));
}

void testArenaView() {
	ArenaProbe probe{};
	ArenaView arena{};
	arena.context = &probe;
	arena.allocateFunction = &probeAllocate;
	arena.epoch = 41u;
#if FLOW_UI_DEV_MODE
	arena.validation = std::make_shared<ArenaLeaseState>();
#endif

	auto values = arena.allocateArray<uint32_t>(8);
	FLOWUI_CHECK(values.size() == 8);
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(values.data()) % alignof(uint32_t) == 0);
	for (uint32_t index = 0; index < values.size(); ++index) values[index] = index * 3u;
	for (uint32_t index = 0; index < values.size(); ++index) FLOWUI_CHECK(values[index] == index * 3u);
	FLOWUI_CHECK(arena.allocateArray<uint64_t>(0).empty());

	size_t rejectedCalls = 0;
	ArenaView rejecting{};
	rejecting.context = &rejectedCalls;
	rejecting.allocateFunction = &rejectAllocation;
	rejecting.epoch = 42u;
#if FLOW_UI_DEV_MODE
	rejecting.validation = std::make_shared<ArenaLeaseState>();
#endif
	const size_t overflowingCount = std::numeric_limits<size_t>::max() / sizeof(uint64_t) + 1u;
	FLOWUI_CHECK(rejecting.allocateArray<uint64_t>(overflowingCount).empty());
	FLOWUI_CHECK(rejectedCalls == 0);

	FLOWUI_CHECK(ArenaView{}.allocate(16) == nullptr);
}

void testReadViews() {
	std::array<TextureHotRecord, 2> textures{};
	textures[1].generation = 7u;
	textures[1].revision = 3u;
	textures[1].state = ResourceState::Ready;

	std::array<ImageViewHotRecord, 2> imageViews{};
	imageViews[1].generation = 4u;
	std::array<SamplerHotRecord, 2> samplers{};
	samplers[1].generation = 5u;

	StorageReadView readView{};
	readView.textures = textures;
	readView.imageViews = imageViews;
	readView.samplers = samplers;
	readView.epoch = 23u;
#if FLOW_UI_DEV_MODE
	auto readValidation = std::make_shared<ReadLeaseState>();
	readView.validation = readValidation;
#endif

	FLOWUI_CHECK(readView.valid());
	FLOWUI_CHECK(readView.texture(TextureHandle{1u, 7u}) == &textures[1]);
	FLOWUI_CHECK(readView.texture(TextureHandle{1u, 8u}) == nullptr);
	FLOWUI_CHECK(readView.texture(TextureHandle{2u, 7u}) == nullptr);
	FLOWUI_CHECK(readView.imageView(ImageViewHandle{1u, 4u}) == &imageViews[1]);
	FLOWUI_CHECK(readView.sampler(SamplerHandle{1u, 5u}) == &samplers[1]);

	std::array<BindingHotRecord, 2> bindings{};
	bindings[1].textureGeneration = 7u;
	bindings[1].descriptorIndex = 11u;
	WindowBindingView bindingView{};
	bindingView.bindingsByTextureIndex = bindings;
	bindingView.epoch = 23u;
#if FLOW_UI_DEV_MODE
	auto bindingValidation = std::make_shared<ReadLeaseState>();
	bindingView.validation = bindingValidation;
#endif
	FLOWUI_CHECK(bindingView.valid());
	FLOWUI_CHECK(bindingView.binding(TextureHandle{1u, 7u}) == &bindings[1]);
	FLOWUI_CHECK(bindingView.binding(TextureHandle{1u, 8u}) == nullptr);

#if FLOW_UI_DEV_MODE
	readValidation->valid.store(false, std::memory_order_release);
	bindingValidation->valid.store(false, std::memory_order_release);
	FLOWUI_CHECK(!readView.valid());
	FLOWUI_CHECK(readView.texture(TextureHandle{1u, 7u}) == nullptr);
	FLOWUI_CHECK(!bindingView.valid());
#endif
}

void testFrameLease() {
	FrameReadLease lease{};
	FLOWUI_CHECK(!lease);
	FLOWUI_CHECK(!lease.valid());

	lease.frame = FrameToken{8u, 1u, 52u, 19u};
	lease.leaseId = 3u;
#if FLOW_UI_DEV_MODE
	auto validation = std::make_shared<ReadLeaseState>();
	lease.validation = validation;
#endif
	FLOWUI_CHECK(lease);
	FLOWUI_CHECK(lease.valid());
#if FLOW_UI_DEV_MODE
	validation->valid.store(false, std::memory_order_release);
	FLOWUI_CHECK(!lease.valid());
#endif
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("canonical main window identity", testWindowIdentity);
	runner.run("handle packing and generations", testHandlePacking);
	runner.run("aligned header and payload record layout", testAlignedRecordLayout);
	runner.run("checked calculation and layout error equivalence", testCalculationFailures);
	runner.run("flag helpers and resource keys", testFlagsAndKeys);
	runner.run("arena view allocation and overflow", testArenaView);
	runner.run("generation-checked read views", testReadViews);
	runner.run("frame read lease validity", testFrameLease);
	return runner.finish();
}
