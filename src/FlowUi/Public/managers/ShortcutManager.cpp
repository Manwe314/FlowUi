#include "FlowUi/Public/managers/ShortcutManager.hpp"
#include "FlowUi/Public/defaults/ShortcutDefaults.hpp"
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <array>


#include "FlowUi/Private/storageSystem/ManagerStateAccess.hpp"
#include "FlowUi/Private/inputService/ShortcutManagerState.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace {

constexpr uint8_t kModCtrl = 1u << 0u;
constexpr uint8_t kModShift = 1u << 1u;
constexpr uint8_t kModAlt = 1u << 2u;
constexpr uint8_t kModSuper = 1u << 3u;

uint8_t modsMaskFromChord(const FlowUi::ShortcutChord& chord) {
	uint8_t mask = 0u;
	if (chord.ctrl) mask |= kModCtrl;
	if (chord.shift) mask |= kModShift;
	if (chord.alt) mask |= kModAlt;
	if (chord.super) mask |= kModSuper;
	return mask;
}

uint8_t modsMaskFromInput(const FlowUi::FrameInput& input) {
	uint8_t mask = 0u;
	if (input.ctrl) mask |= kModCtrl;
	if (input.shift) mask |= kModShift;
	if (input.alt) mask |= kModAlt;
	if (input.super) mask |= kModSuper;
	return mask;
}

bool keyDown(const FlowUi::FrameInput& input, int key) {
	return key >= 0 && key < static_cast<int>(FlowUi::FrameInput::kKeyboardKeyCount) &&
		input.keyDown[static_cast<size_t>(key)];
}

uint32_t packChord(int key, uint8_t modsMask, FlowUi::ShortcutTrigger trigger) {
	const uint32_t keyBits = static_cast<uint32_t>(std::max(0, key)) & 0x3FFu;
	const uint32_t modBits = static_cast<uint32_t>(modsMask & 0x0Fu) << 10u;
	const uint32_t triggerBits = (static_cast<uint32_t>(trigger) & 0x03u) << 14u;
	return keyBits | modBits | triggerBits;
}

int unpackKey(uint32_t packedChord) { return static_cast<int>(packedChord & 0x3FFu); }

bool scopeEligible(FlowUi::ShortcutScope scope, bool focused_input, bool focused_element) noexcept {
	switch (scope) {
	case FlowUi::ShortcutScope::FocusedInput: return focused_input;
	case FlowUi::ShortcutScope::FocusedElement: return focused_element;
	case FlowUi::ShortcutScope::Global: return true;
	}
	return false;
}
bool precedenceLess(FlowUi::ShortcutScope left_scope, int32_t left_priority, uint64_t left_order,
	FlowUi::ShortcutScope right_scope, int32_t right_priority, uint64_t right_order) noexcept {
	if (left_scope != right_scope) return left_scope < right_scope;
	if (left_priority != right_priority) return left_priority > right_priority;
	return left_order < right_order;
}

bool executableOrderLess(
	const FlowUi::detail::manager_storage::ShortcutRegistration& a,
	const FlowUi::detail::manager_storage::ShortcutRegistration& b) {
	return precedenceLess(a->scope, a->priority, a->registrationOrder, b->scope, b->priority, b->registrationOrder);
}

bool scopeIsActive(
	const FlowUi::detail::manager_storage::ShortcutRegistrationRecord& executable,
	const FlowUi::ShortcutContext& context,
	FlowUi::UiManager& ui) {
	return scopeEligible(executable.scope, ui.inputFields().hasPrimaryFieldFocus(), context.focusedElementId.id != 0u);
}

} // namespace

namespace FlowUi {
#if FLOW_UI_DEV_MODE
bool ShortcutManager::devMatches(const DevShortcutView& candidate, const ShortcutChord& chord,
	bool focused_input, bool focused_element) noexcept {
	return scopeEligible(candidate.scope, focused_input, focused_element) &&
		packChord(candidate.chord.key, modsMaskFromChord(candidate.chord), candidate.chord.trigger) ==
		packChord(chord.key, modsMaskFromChord(chord), chord.trigger);
}
bool ShortcutManager::devPrecedes(const DevShortcutView& left, const DevShortcutView& right) noexcept {
	return precedenceLess(left.scope, left.priority, left.order, right.scope, right.priority, right.order);
}
bool ShortcutManager::visitDevShortcuts(void* target, DevShortcutVisitor visitor) const {
	if (!storage_ || !stateHandle_ || !visitor) return false;
	const auto& current = state();
	for (const auto& [identity, registration] : current.registrationsById) {
		if (!registration || registration->tombstoned) continue;
		const auto packed = registration->packedChord;
		const auto modifiers = static_cast<uint8_t>((packed >> 10u) & 15u);
		if (!visitor(target, DevShortcutView{
			.chord = {.key = unpackKey(packed), .ctrl = bool(modifiers & kModCtrl),
				.shift = bool(modifiers & kModShift), .alt = bool(modifiers & kModAlt),
				.super = bool(modifiers & kModSuper),
				.trigger = static_cast<ShortcutTrigger>((packed >> 14u) & 3u)},
			.action = registration->action.id, .epoch = current.registrationEpoch,
			.order = registration->registrationOrder, .id = identity,
			.priority = registration->priority, .scope = registration->scope,
			.handling = registration->handling, .callback = bool(registration->callback)})) return false;
	}
	return true;
}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ShortcutManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!storage_ || stateHandle_ == 0u) return;
	try {
		const auto& current = state();
		devSystems::append_capacity_demand(sink,
										   FlowUi::detail::InitialCapacityTarget::shortcuts_chords,
										   current.chordBuckets, current.capacity_demand, window_);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::shortcuts_registrations,
			current.registrationsById, current.capacity_demand, window_);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::shortcuts_keys,
			current.registeredKeyRefCount, current.capacity_demand, window_);
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(current.chordBuckets);
		memory.addNodeContainer(current.registrationsById);
		memory.addNodeContainer(current.registeredKeyRefCount);
		for (const auto& [_, bucket] : current.chordBuckets) {
			if (bucket) memory.add(*bucket);
		}
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kShortcuts.id, memory, window_);
	} catch (...) {}
}
#endif

namespace manager_storage = detail::manager_storage;
namespace storage = detail::storage;

void ShortcutManager::init(storage::IStorageSystem& storageSystem, WindowId window) {
	if (storage_) throw FlowUiException(makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::ShortcutManagerInitialize));
	const storage::StringId name = storageSystem.intern("flowui.shortcut.root");
	const storage::ResourceKey key{storage::ResourceDomain::Input, name, window};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::ShortcutManagerState>(
		storageSystem, key, storage::ResourceKind::ShortcutRegistration, name);
	storage_ = &storageSystem;
	window_ = window;
	stateHandle_ = handle.packed();
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	state().capacity_demand.bind(storageSystem.memory_demand_control());
#endif
	detail::reserve_initial_capacity(state().chordBuckets,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::shortcuts_chords));
	detail::reserve_initial_capacity(
		state().registrationsById,
		storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::shortcuts_registrations));
	detail::reserve_initial_capacity(state().registeredKeyRefCount,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::shortcuts_keys));
}

Status ShortcutManager::installDefaultTextShortcuts(const DefaultTextShortcutConfig& config) {
	const auto registration_plan = defaults::text_shortcuts(config);
	if (!registration_plan)
		return unexpectedError(registration_plan.error());
	std::array<ShortcutId, defaults::TextShortcutDefaults::capacity> installed_ids{};
	size_t installed_count = 0;
	const auto rollback = [this, &installed_ids, &installed_count] {
		while (installed_count != 0) {
			unregisterShortcut(installed_ids[--installed_count]);
		}
	};
	try {
		for (const auto& binding :
			 std::span{registration_plan->bindings}.first(registration_plan->binding_count)) {
			const auto registration =
				registerShortcut(binding.chord, ShortcutScope::FocusedInput, config.priority,
								 [command = binding.command](ShortcutContext& context) {
									 return context.ui.inputFields().enqueueCommand(command);
								 });
			if (!registration) {
				rollback();
				return unexpectedError(registration.error());
			}
			installed_ids[installed_count++] = *registration;
		}
	} catch (...) {
		rollback();
		throw;
	}
	return {};
}

void ShortcutManager::destroy() noexcept {
	if (!storage_) return;
	try {
		const storage::StringId name = storage_->intern("flowui.shortcut.root");
		(void)storage_->removeManagerRecord(
			storage::ResourceKey{storage::ResourceDomain::Input, name, window_},
			storage::ResourceKind::ShortcutRegistration);
	} catch (...) {
	}
	storage_ = nullptr;
	window_ = InvalidWindowId;
	stateHandle_ = 0;
}

manager_storage::ShortcutManagerState& ShortcutManager::state() {
	auto* result = manager_storage::state<manager_storage::ShortcutManagerState>(
		storage_, storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ShortcutRegistration);
	if (!result) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ShortcutResolve));
	return *result;
}

const manager_storage::ShortcutManagerState& ShortcutManager::state() const {
	const auto* result = manager_storage::state<manager_storage::ShortcutManagerState>(
		storage_, storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ShortcutRegistration);
	if (!result) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ShortcutResolve));
	return *result;
}

Result<ShortcutId> ShortcutManager::registerShortcut(
	const ShortcutChord& chord,
	ShortcutScope scope,
	int32_t priority,
	ShortcutCallback callback) {
	if (!callback || chord.key < 0 || chord.key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) {
		return unexpectedError(makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
	}
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ShortcutRegister));
	auto& current = state();
	if (current.nextShortcutId == 0 || current.nextShortcutId > std::numeric_limits<ShortcutId>::max()) {
		return unexpectedError(makeError(ErrorCode::ShortcutIdSpaceExhausted, ErrorSite::ShortcutRegister));
	}

	manager_storage::ShortcutManagerState candidate = current;
	const ShortcutId id = static_cast<ShortcutId>(candidate.nextShortcutId);
	const uint32_t packed = packChord(chord.key, modsMaskFromChord(chord), chord.trigger);
	auto registration = std::make_shared<manager_storage::ShortcutRegistrationRecord>();
	registration->scope = scope;
	registration->priority = priority;
	registration->id = id;
	registration->registrationOrder = candidate.nextRegistrationOrder;
	registration->packedChord = packed;
	registration->callback = std::move(callback);
	manager_storage::ShortcutBucket bucket;
	if (const auto existing = candidate.chordBuckets.find(packed); existing != candidate.chordBuckets.end()) {
		bucket = *existing->second;
	}
	bucket.push_back(registration);
	std::stable_sort(bucket.begin(), bucket.end(), executableOrderLess);
	candidate.chordBuckets[packed] = std::make_shared<const manager_storage::ShortcutBucket>(std::move(bucket));
	candidate.registrationsById.emplace(id, std::move(registration));
	candidate.registeredKeyRefCount[chord.key] += 1u;
	++candidate.nextShortcutId;
	++candidate.nextRegistrationOrder;
	current = std::move(candidate);
	storage_->noteManagerMutation(window_);
	return id;
}

Result<ShortcutId> ShortcutManager::registerShortcut(
	const ShortcutChord& chord,
	ShortcutScope scope,
	int32_t priority,
	AppActionCall action,
	ShortcutHandling handling) {
	if (!action || chord.key < 0 || chord.key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) {
		return unexpectedError(makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
	}
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ShortcutRegister));
	auto& current = state();
	if (current.nextShortcutId == 0 || current.nextShortcutId > std::numeric_limits<ShortcutId>::max()) {
		return unexpectedError(makeError(ErrorCode::ShortcutIdSpaceExhausted, ErrorSite::ShortcutRegister));
	}

	manager_storage::ShortcutManagerState candidate = current;
	const ShortcutId id = static_cast<ShortcutId>(candidate.nextShortcutId);
	const uint32_t packed = packChord(chord.key, modsMaskFromChord(chord), chord.trigger);
	auto registration = std::make_shared<manager_storage::ShortcutRegistrationRecord>();
	registration->scope = scope;
	registration->priority = priority;
	registration->id = id;
	registration->registrationOrder = candidate.nextRegistrationOrder;
	registration->packedChord = packed;
	registration->action = action;
	registration->handling = handling;
	manager_storage::ShortcutBucket bucket;
	if (const auto existing = candidate.chordBuckets.find(packed); existing != candidate.chordBuckets.end()) {
		bucket = *existing->second;
	}
	bucket.push_back(registration);
	std::stable_sort(bucket.begin(), bucket.end(), executableOrderLess);
	candidate.chordBuckets[packed] = std::make_shared<const manager_storage::ShortcutBucket>(std::move(bucket));
	candidate.registrationsById.emplace(id, std::move(registration));
	candidate.registeredKeyRefCount[chord.key] += 1u;
	++candidate.nextShortcutId;
	++candidate.nextRegistrationOrder;
	current = std::move(candidate);
	storage_->noteManagerMutation(window_);
	return id;
}

bool ShortcutManager::unregisterShortcut(ShortcutId id) {
	auto& current = state();
	const auto registrationIt = current.registrationsById.find(id);
	if (registrationIt == current.registrationsById.end()) return false;
	const auto registration = registrationIt->second;
	registration->tombstoned = true;
	const uint32_t packed = registration->packedChord;
	const int key = unpackKey(packed);
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_registrations,
								current.registrationsById.size());
	current.registrationsById.erase(registrationIt);
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_chords,
								current.chordBuckets.size());
	if (const auto bucketIt = current.chordBuckets.find(packed); bucketIt != current.chordBuckets.end()) {
		manager_storage::ShortcutBucket bucket = *bucketIt->second;
		std::erase_if(bucket, [id](const auto& item) { return item->id == id; });
		if (bucket.empty()) current.chordBuckets.erase(bucketIt);
		else bucketIt->second = std::make_shared<const manager_storage::ShortcutBucket>(std::move(bucket));
	}
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_keys,
								current.registeredKeyRefCount.size());
	if (const auto keyIt = current.registeredKeyRefCount.find(key); keyIt != current.registeredKeyRefCount.end()) {
		if (keyIt->second <= 1u) current.registeredKeyRefCount.erase(keyIt);
		else --keyIt->second;
	}
	storage_->noteManagerMutation(window_);
	return true;
}

void ShortcutManager::clear() {
	auto& current = state();
	if (current.registrationsById.empty() && current.focusedElementId.id == 0u) return;
	for (auto& [_, registration] : current.registrationsById) registration->tombstoned = true;
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_chords,
								current.chordBuckets.size());
	current.chordBuckets.clear();
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_registrations,
								current.registrationsById.size());
	current.registrationsById.clear();
	FLOWUI_MEMORY_DEMAND_RECORD(current.capacity_demand,
								FlowUi::detail::InitialCapacityTarget::shortcuts_keys,
								current.registeredKeyRefCount.size());
	current.registeredKeyRefCount.clear();
	current.focusedElementId = {};
	++current.registrationEpoch;
	storage_->noteManagerMutation(window_);
}

void ShortcutManager::setFocusedElement(Clay_ElementId elementId) {
	auto& current = state();
	if (current.focusedElementId.id == elementId.id) return;
	current.focusedElementId = elementId;
	storage_->noteManagerMutation(window_);
}

void ShortcutManager::clearFocusedElement() { setFocusedElement({}); }

Clay_ElementId ShortcutManager::focusedElement() const { return state().focusedElementId; }

void ShortcutManager::beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput) {
	auto& current = state();
	if (current.registeredKeyRefCount.empty()) return;
	ShortcutContext context{ui, currentInput, previousInput, current.focusedElementId};
	const uint8_t currentMods = modsMaskFromInput(currentInput);
	const uint8_t previousMods = modsMaskFromInput(previousInput);
	std::array<int, FrameInput::kKeyboardKeyCount> keys{};
	size_t keyCount = 0;
	for (const auto& [key, _] : current.registeredKeyRefCount) keys[keyCount++] = key;

	for (size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
		const int key = keys[keyIndex];
		if (!current.registeredKeyRefCount.contains(key)) continue;
		const bool down = keyDown(currentInput, key);
		const bool wasDown = keyDown(previousInput, key);
		uint32_t packed = 0;
		if (down && !wasDown) packed = packChord(key, currentMods, ShortcutTrigger::Press);
		else if (!down && wasDown) packed = packChord(key, previousMods, ShortcutTrigger::Release);
		else if (down) packed = packChord(key, currentMods, ShortcutTrigger::Down);
		else continue;

		const auto bucketIt = current.chordBuckets.find(packed);
		if (bucketIt == current.chordBuckets.end()) continue;
		const manager_storage::PublishedShortcutBucket snapshot = bucketIt->second;
		for (const auto& executable : *snapshot) {
			if (!executable || executable->tombstoned ||
				!current.registrationsById.contains(executable->id) ||
				!scopeIsActive(*executable, context, ui)) continue;
			if (executable->callback) {
				if (executable->callback(context)) break;
				continue;
			}
			if (!executable->action) continue;
			const ActionInvocationStatus status = ui.actions().invoke(
				ActionCall{executable->action},
				ActionInvocationSource{
					.kind = ActionInvocationSourceKind::Shortcut,
					.window = ui.windowId(),
					.sourceId = executable->id,
				});
			// Consumption is authored explicitly and never inferred from an
			// arbitrary action result. Unavailable actions fall through.
			if (status == ActionInvocationStatus::Invoked &&
				executable->handling == ShortcutHandling::Consume) break;
		}
	}
}

} // namespace FlowUi
