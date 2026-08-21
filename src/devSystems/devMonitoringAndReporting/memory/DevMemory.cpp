#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "internal/StorageSystem/IStorageSystem.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
#include "devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.hpp"
#endif

namespace FlowUi::devSystems {
namespace {

[[nodiscard]] constexpr MemoryMonitoringLevel compiledMemoryLevel() noexcept {
	return static_cast<MemoryMonitoringLevel>(FLOWUI_DEV_MEMORY_LEVEL);
}

[[nodiscard]] DevMemoryConfig normalizeConfig(DevMemoryConfig config) noexcept {
	const uint8_t requested = static_cast<uint8_t>(config.level);
	const uint8_t compiled = static_cast<uint8_t>(compiledMemoryLevel());
	config.level = static_cast<MemoryMonitoringLevel>(std::min(requested, compiled));
	config.producerEventCapacity = std::max(1u, config.producerEventCapacity);
	return config;
}

} // namespace

struct DevMemory::Impl {
	explicit Impl(DevMemoryConfig initialConfig)
		: config(normalizeConfig(initialConfig)), recorder(config.producerEventCapacity) {
		recorder.setLevel(config.level);
	}

	mutable std::mutex mutex{};
	DevMemoryConfig config{};
	DevMemoryRecorder recorder;
	std::unordered_map<MemorySourceId, MemorySourceDescriptor> descriptors{};
	std::unordered_map<MemoryTuningTargetId, MemoryTuningTargetDescriptor> tuningTargets{};
	std::vector<RegisteredMemoryProbe> probes{};
	::FlowUi::detail::storage::IStorageSystem* storage = nullptr;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	DevEnvironmentMemoryProbe environment{};
#endif
	std::atomic<uint64_t> sourceCollisions{0u};
};

DevMemory::DevMemory(DevMemoryConfig config)
	: impl_(std::make_unique<Impl>(config)) {
	for (const auto& source : memory_sources::kAll) (void)registerSource(source);
	for (auto& target : memory_sources::tuningTargets()) (void)registerTuningTarget(target);
}

DevMemory::~DevMemory() = default;

void DevMemory::setConfig(const DevMemoryConfig& config) noexcept {
	std::scoped_lock lock(impl_->mutex);
	DevMemoryConfig normalized = normalizeConfig(config);
	// Producer capacity is fixed at construction so recording never reallocates.
	normalized.producerEventCapacity = impl_->recorder.capacity();
	impl_->config = normalized;
	impl_->recorder.setLevel(normalized.level);
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	impl_->environment.setConfig(normalized);
#endif
}

#if FLOWUI_DEV_MEMORY_LEVEL >= 1
void DevMemory::initializeEnvironmentProbes(const ::VulkanContext& context) noexcept {
	impl_->environment.setConfig(config());
	impl_->environment.initializeVulkan(context);
}

void DevMemory::detachEnvironmentProbes() noexcept { impl_->environment.detachVulkan(); }

void DevMemory::advanceGpuFrameIndex(uint32_t frameIndex) noexcept {
	impl_->environment.advanceVmaFrameIndex(frameIndex);
}

MemoryEnvironmentSnapshot DevMemory::sampleEnvironment(
	uint64_t nowNs,
	uint64_t safelyAttributableFlowUiCpuBytes,
	bool force) noexcept {
	return impl_->environment.sample(nowNs, safelyAttributableFlowUiCpuBytes, force);
}
#endif

DevMemoryConfig DevMemory::config() const noexcept {
	std::scoped_lock lock(impl_->mutex);
	return impl_->config;
}

MemoryMonitoringLevel DevMemory::compiledLevel() const noexcept { return compiledMemoryLevel(); }

bool DevMemory::registerSource(const StaticMemorySourceDescriptor& descriptor) {
	if (descriptor.id == 0u || descriptor.name.empty()) return false;
	std::scoped_lock lock(impl_->mutex);
	const auto existing = impl_->descriptors.find(descriptor.id);
	if (existing != impl_->descriptors.end()) {
		const MemorySourceDescriptor& current = existing->second;
		const bool identical = current.parent == descriptor.parent &&
			current.domain == descriptor.domain && current.kind == descriptor.kind &&
			current.name == descriptor.name && current.accuracy == descriptor.accuracy &&
			current.tuningTarget == descriptor.tuningTarget;
		if (!identical) impl_->sourceCollisions.fetch_add(1u, std::memory_order_relaxed);
		return identical;
	}
	impl_->descriptors.emplace(descriptor.id, MemorySourceDescriptor{
		.id = descriptor.id,
		.parent = descriptor.parent,
		.domain = descriptor.domain,
		.kind = descriptor.kind,
		.name = std::string(descriptor.name),
		.accuracy = descriptor.accuracy,
		.tuningTarget = descriptor.tuningTarget,
	});
	return true;
}

bool DevMemory::registerTuningTarget(const MemoryTuningTargetDescriptor& descriptor) {
	if (descriptor.id == 0u || descriptor.source == 0u || descriptor.configKey.empty() ||
		descriptor.alignment == 0u || descriptor.minimum > descriptor.maximum) return false;
	std::scoped_lock lock(impl_->mutex);
	const auto [it, inserted] = impl_->tuningTargets.emplace(descriptor.id, descriptor);
	if (inserted) return true;
	return it->second.source == descriptor.source && it->second.metric == descriptor.metric &&
		it->second.unit == descriptor.unit && it->second.configKey == descriptor.configKey;
}

bool DevMemory::registerProbe(const RegisteredMemoryProbe& probe) {
	if (probe.source == 0u || probe.owner == nullptr || probe.sample == nullptr) return false;
	std::scoped_lock lock(impl_->mutex);
	const auto duplicate = std::find_if(impl_->probes.begin(), impl_->probes.end(),
		[&](const RegisteredMemoryProbe& current) {
			return current.source == probe.source && current.owner == probe.owner;
		});
	if (duplicate != impl_->probes.end()) return duplicate->sample == probe.sample;
	impl_->probes.push_back(probe);
	return true;
}

void DevMemory::unregisterProbe(MemorySourceId source, const void* owner) noexcept {
	std::scoped_lock lock(impl_->mutex);
	std::erase_if(impl_->probes, [&](const RegisteredMemoryProbe& probe) {
		return probe.source == source && probe.owner == owner;
	});
}

std::vector<MemorySourceDescriptor> DevMemory::descriptorSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	std::vector<MemorySourceDescriptor> result;
	result.reserve(impl_->descriptors.size());
	for (const auto& [_, descriptor] : impl_->descriptors) result.push_back(descriptor);
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.id < right.id;
	});
	return result;
}

std::vector<MemoryTuningTargetDescriptor> DevMemory::tuningTargetSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	std::vector<MemoryTuningTargetDescriptor> result;
	result.reserve(impl_->tuningTargets.size());
	for (const auto& [_, descriptor] : impl_->tuningTargets) result.push_back(descriptor);
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.id < right.id;
	});
	return result;
}

std::vector<RegisteredMemoryProbe> DevMemory::probeSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	return impl_->probes;
}

DevMemoryRecorder& DevMemory::recorder() noexcept { return impl_->recorder; }
const DevMemoryRecorder& DevMemory::recorder() const noexcept { return impl_->recorder; }

MemoryQualitySnapshot DevMemory::qualitySnapshot() const noexcept {
	MemoryQualitySnapshot result = impl_->recorder.qualitySnapshot();
	result.sourceCollisions = impl_->sourceCollisions.load(std::memory_order_relaxed);
	return result;
}

void DevMemory::setStorageSystem(::FlowUi::detail::storage::IStorageSystem* storage) noexcept {
	std::scoped_lock lock(impl_->mutex);
	impl_->storage = storage;
}

bool DevMemory::appendStorageSnapshot(
	const ::FlowUi::detail::storage::StorageMemorySnapshotRequest& request,
	::FlowUi::detail::storage::StorageMemorySnapshot& destination) const noexcept {
	try {
		::FlowUi::detail::storage::IStorageSystem* storage = nullptr;
		{
			std::scoped_lock lock(impl_->mutex);
			storage = impl_->storage;
		}
		if (!storage) return false;
		storage->appendMemorySnapshot(request, destination);
		return true;
	} catch (...) {
		return false;
	}
}

} // namespace FlowUi::devSystems

#endif
