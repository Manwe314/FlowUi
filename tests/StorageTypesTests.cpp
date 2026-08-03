#include "TestHarness.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

#include "internal/StorageSystem/StorageTypes.hpp"

namespace {

using namespace FlowUi::detail::storage;

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
	FLOWUI_CHECK(first == same);
	FLOWUI_CHECK(first != otherWindow);
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
	runner.run("flag helpers and resource keys", testFlagsAndKeys);
	runner.run("arena view allocation and overflow", testArenaView);
	runner.run("generation-checked read views", testReadViews);
	runner.run("frame read lease validity", testFrameLease);
	return runner.finish();
}
