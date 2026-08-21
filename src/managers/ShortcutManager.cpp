#include "managers/ShortcutManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <array>

#include <GLFW/glfw3.h>

#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ShortcutManagerState.hpp"
#include "managers/ActionManager.hpp"
#include "managers/UiManager.hpp"

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

bool executableOrderLess(
	const FlowUi::detail::manager_storage::ShortcutRegistration& a,
	const FlowUi::detail::manager_storage::ShortcutRegistration& b) {
	if (a->scope != b->scope) return static_cast<uint8_t>(a->scope) < static_cast<uint8_t>(b->scope);
	if (a->priority != b->priority) return a->priority > b->priority;
	return a->registrationOrder < b->registrationOrder;
}

bool scopeIsActive(
	const FlowUi::detail::manager_storage::ShortcutRegistrationRecord& executable,
	const FlowUi::ShortcutContext& context,
	FlowUi::UiManager& ui) {
	switch (executable.scope) {
	case FlowUi::ShortcutScope::FocusedInput: return ui.inputFields().hasPrimaryFieldFocus();
	case FlowUi::ShortcutScope::FocusedElement: return context.focusedElementId.id != 0u;
	case FlowUi::ShortcutScope::Global: return true;
	default: return false;
	}
}

} // namespace

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ShortcutManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!storage_ || stateHandle_ == 0u) return;
	try {
		const auto& current = state();
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
	if (storage_) throw std::logic_error("ShortcutManager is already initialized.");
	const storage::StringId name = storageSystem.intern("flowui.shortcut.root");
	const storage::ResourceKey key{storage::ResourceDomain::Input, name, window};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::ShortcutManagerState>(
		storageSystem, key, storage::ResourceKind::ShortcutRegistration, name);
	storage_ = &storageSystem;
	window_ = window;
	stateHandle_ = handle.packed();
}

void ShortcutManager::installDefaultTextShortcuts(const DefaultTextShortcutConfig& config) {
	if (!config.enabled) return;
	PlatformShortcutStyle platform = config.platform;
	if (platform == PlatformShortcutStyle::Auto) {
#if defined(__APPLE__)
		platform = PlatformShortcutStyle::Command;
#else
		platform = PlatformShortcutStyle::Control;
#endif
	}
	const auto chord = [platform](int key, bool shift = false, bool wordModifier = false) {
		ShortcutChord result{.key = key, .shift = shift};
		if (wordModifier && platform == PlatformShortcutStyle::Command) result.alt = true;
		else if (platform == PlatformShortcutStyle::Command) result.super = true;
		else result.ctrl = true;
		return result;
	};
	const auto add = [this, &config](ShortcutChord value, TextCommand command) {
		(void)registerShortcut(
			value,
			ShortcutScope::FocusedInput,
			config.priority,
			[command](ShortcutContext& context) {
				return context.ui.inputFields().enqueueCommand(command);
			});
	};
	if (config.selectAll) add(chord(GLFW_KEY_A), TextCommand::SelectAll);
	if (config.clipboard) {
		add(chord(GLFW_KEY_C), TextCommand::Copy);
		add(chord(GLFW_KEY_X), TextCommand::Cut);
		add(chord(GLFW_KEY_V), TextCommand::Paste);
	}
	if (config.undoRedoRequests) {
		add(chord(GLFW_KEY_Z), TextCommand::RequestUndo);
		add(chord(GLFW_KEY_Z, true), TextCommand::RequestRedo);
		if (platform == PlatformShortcutStyle::Control) add(chord(GLFW_KEY_Y), TextCommand::RequestRedo);
	}
	if (config.wordNavigation) {
		add(chord(GLFW_KEY_LEFT, false, true), TextCommand::MoveWordLeft);
		add(chord(GLFW_KEY_RIGHT, false, true), TextCommand::MoveWordRight);
		add(chord(GLFW_KEY_LEFT, true, true), TextCommand::MoveWordLeft);
		add(chord(GLFW_KEY_RIGHT, true, true), TextCommand::MoveWordRight);
		add(chord(GLFW_KEY_BACKSPACE, false, true), TextCommand::DeleteWordBackward);
		add(chord(GLFW_KEY_DELETE, false, true), TextCommand::DeleteWordForward);
		if (platform == PlatformShortcutStyle::Control) {
			add(chord(GLFW_KEY_HOME), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_END), TextCommand::MoveDocumentEnd);
			add(chord(GLFW_KEY_HOME, true), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_END, true), TextCommand::MoveDocumentEnd);
		} else {
			add(chord(GLFW_KEY_LEFT), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_RIGHT), TextCommand::MoveDocumentEnd);
			add(chord(GLFW_KEY_LEFT, true), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_RIGHT, true), TextCommand::MoveDocumentEnd);
		}
	}
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
	if (!result) throw std::logic_error("ShortcutManager is not attached to a live window storage scope.");
	return *result;
}

const manager_storage::ShortcutManagerState& ShortcutManager::state() const {
	const auto* result = manager_storage::state<manager_storage::ShortcutManagerState>(
		storage_, storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ShortcutRegistration);
	if (!result) throw std::logic_error("ShortcutManager is not attached to a live window storage scope.");
	return *result;
}

ShortcutId ShortcutManager::registerShortcut(
	const ShortcutChord& chord,
	ShortcutScope scope,
	int32_t priority,
	ShortcutCallback callback) {
	if (!callback || chord.key < 0 || chord.key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) return 0u;
	auto& current = state();
	if (current.nextShortcutId == 0 || current.nextShortcutId > std::numeric_limits<ShortcutId>::max()) return 0u;

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

ShortcutId ShortcutManager::registerShortcut(
	const ShortcutChord& chord,
	ShortcutScope scope,
	int32_t priority,
	AppActionCall action,
	ShortcutHandling handling) {
	if (!action || chord.key < 0 || chord.key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) return 0u;
	auto& current = state();
	if (current.nextShortcutId == 0 || current.nextShortcutId > std::numeric_limits<ShortcutId>::max()) return 0u;

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
	current.registrationsById.erase(registrationIt);
	if (const auto bucketIt = current.chordBuckets.find(packed); bucketIt != current.chordBuckets.end()) {
		manager_storage::ShortcutBucket bucket = *bucketIt->second;
		std::erase_if(bucket, [id](const auto& item) { return item->id == id; });
		if (bucket.empty()) current.chordBuckets.erase(bucketIt);
		else bucketIt->second = std::make_shared<const manager_storage::ShortcutBucket>(std::move(bucket));
	}
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
	current.chordBuckets.clear();
	current.registrationsById.clear();
	current.registeredKeyRefCount.clear();
	current.focusedElementId = {};
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
