#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

#include "internal/MemoryCapacityProfile.hpp"
#include "HeadlessVulkanFixture.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"

using namespace FlowUi;
namespace storage = FlowUi::detail::storage;
namespace {
[[nodiscard]] std::pair<size_t, size_t> run_string_workload(VulkanContext& vulkan, uint64_t hint) {
	MemoryCapacityProfile profile;
	profile.settings = {
		{makeMemoryCapacityTargetId("flowui.tuning.storage.strings_entries"), hint},
		{makeMemoryCapacityTargetId("flowui.tuning.storage.string_ids_entries"), hint}};
	auto resolved = FlowUi::detail::resolve_memory_capacity_profile(profile);
	assert(resolved);
	storage::StorageConfig config;
	assert(storage::applyMemoryCapacityProfile(config, *resolved) >= 2);
	storage::FlowStorageSystem system(vulkan);
	system.initialize(config);
	storage::StorageMemorySnapshot snapshot;
	const auto capacities = [&] {
		system.appendMemorySnapshot({}, snapshot);
		const auto find = [&](std::string_view key) {
			const auto id = memory_capacity_target_id(key);
			const auto entry =
				std::find_if(snapshot.capacity_demands.begin(), snapshot.capacity_demands.end(),
							 [&](const auto& demand) { return demand.target == id; });
			assert(entry != snapshot.capacity_demands.end());
			return entry->capacity;
		};
		return std::pair{find("flowui.tuning.storage.strings_entries"),
						 find("flowui.tuning.storage.string_ids_entries")};
	};
	auto previous = capacities();
	if (hint) {
		assert(previous.first >= hint + 1 && previous.second >= hint);
	}
	size_t vector_growth = 0, hash_growth = 0;
	for (uint64_t index = 0; index < 112; ++index) {
		const auto id = system.intern("capacity-workload-" + std::to_string(index));
		assert(id);
		const auto current = capacities();
		if (current.first != previous.first)
			++vector_growth;
		if (current.second != previous.second)
			++hash_growth;
		previous = current;
	}
	system.shutdown();
	return {vector_growth, hash_growth};
}
} // namespace
int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture fixture;
		const auto baseline = run_string_workload(fixture.context(), 0);
		const auto reserved = run_string_workload(fixture.context(), 128);
		assert(baseline.first > 0 && baseline.second > 0);
		assert(reserved.first == 0 && reserved.second == 0);
		std::cout << "Real storage string workload: vector growth " << baseline.first << " -> "
				  << reserved.first << ", hash rehashes " << baseline.second << " -> "
				  << reserved.second << ".\n";
		return 0;
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cerr << error.what() << '\n';
		return 77;
	}
}
