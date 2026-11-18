#include "FlowUi/Private/storageSystem/StorageConfigurationValidation.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "RecordingProvider/RecordingStorageSystem.hpp"
#include "TestHarness.hpp"

namespace {
using namespace FlowUi;
using namespace FlowUi::detail::storage;

struct BoundedStorage : test::RecordingStorageSystem {
	using RecordingStorageSystem::RecordingStorageSystem;
	static constexpr uint64_t supportedCapabilities =
		flowStorageSupportedCapabilities & ~storageCapabilityMask(StorageCapability::RuntimeGrowth);
	[[nodiscard]] uint64_t capabilities() const noexcept { return supportedCapabilities; }
	void initialize(const StorageConfig& config) {
		const auto compatible = validateStorageConfiguration<BoundedStorage>(config);
		if (!compatible)
			throw FlowUiException(compatible.error());
		RecordingStorageSystem::initialize(config);
	}
};
static_assert(validateStorageSystem<BoundedStorage>());
static_assert(!storageSupportsCapability<BoundedStorage>(StorageCapability::RuntimeGrowth));
static_assert(storageSupportsCapability<FlowStorageSystem>(StorageCapability::RuntimeGrowth));

void testSelectedGrowthCompatibility() {
	StorageConfig configuration{};
	configuration.allowRuntimeGrowth = false;
	configuration.allowPersistentGrowth = false;
	configuration.allowTransientGrowth = false;
	FLOWUI_CHECK(validateStorageConfiguration<BoundedStorage>(configuration));
	for (auto member : {&StorageConfig::allowRuntimeGrowth, &StorageConfig::allowPersistentGrowth,
						&StorageConfig::allowTransientGrowth}) {
		configuration.*member = true;
		const auto bounded = validateStorageConfiguration<BoundedStorage>(configuration);
		FLOWUI_CHECK(!bounded);
		FLOWUI_CHECK(bounded.error().code == ErrorCode::StorageConfigurationInvalid);
		FLOWUI_CHECK(bounded.error().site == ErrorSite::StorageInitialize);
		FLOWUI_CHECK(validateStorageConfiguration<FlowStorageSystem>(configuration));
		configuration.*member = false;
	}
}
} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("selected static growth support and runtime flags", testSelectedGrowthCompatibility);
	return runner.finish();
}
