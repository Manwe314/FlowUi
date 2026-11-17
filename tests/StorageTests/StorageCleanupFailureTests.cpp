#include "HeadlessVulkanFixture.hpp"
#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <string_view>

namespace {
thread_local bool rejectAllocation = false;

struct AllocationRejection {
	AllocationRejection() noexcept { rejectAllocation = true; }
	~AllocationRejection() noexcept { rejectAllocation = false; }
};
} // namespace

void* operator new(std::size_t size) {
	if (!rejectAllocation) {
		if (void* allocation = std::malloc(size ? size : 1))
			return allocation;
	}
	throw std::bad_alloc{};
}
void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept { std::free(allocation); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* allocation) noexcept { ::operator delete(allocation); }
void operator delete[](void* allocation, std::size_t) noexcept { ::operator delete(allocation); }

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
		storage.initialize(FlowUi::detail::storage::StorageConfig{});
		constexpr std::array<std::string_view, 8> names{
			"cleanup/a", "cleanup/b", "cleanup/c", "cleanup/d",
			"cleanup/e", "cleanup/f", "cleanup/g", "cleanup/h"};
		std::array<FlowUi::detail::storage::ResourceKey, names.size()> keys{};
		uint32_t payload = 7;
		for (std::size_t index = 0; index < names.size(); ++index) {
			keys[index] = {.domain = FlowUi::detail::storage::ResourceDomain::Internal,
				.name = storage.intern(names[index]), .window = FlowUi::InvalidWindowId};
			const auto record = storage.createManagerRecord({
				.key = keys[index],
				.bytes = sizeof(uint32_t),
				.alignment = alignof(uint32_t),
				.construct = [](void* destination, void* source) {
					std::construct_at(static_cast<uint32_t*>(destination),
						*static_cast<const uint32_t*>(source));
				},
				.destroy = [](void* value) noexcept {
					std::destroy_at(static_cast<uint32_t*>(value));
				},
				.userData = &payload,
				.kind = FlowUi::detail::storage::ResourceKind::UiTheme,
			});
			if (!record)
				return 1;
		}
		{
			AllocationRejection rejection;
			for (const auto key : keys) {
				if (!storage.removeManagerRecord(key, FlowUi::detail::storage::ResourceKind::UiTheme))
					return 2;
			}
		}
		return storage.resourceStats(FlowUi::detail::storage::ResourceKind::UiTheme).live == 0 ? 0 : 3;
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cerr << error.what() << '\n';
#if defined(FLOWUI_TEST_REQUIRE_VULKAN_DEVICE)
		return 1;
#else
		return 77;
#endif
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 4;
	}
}
