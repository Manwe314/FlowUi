#include "managers/ShortcutManager.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <array>

#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ShortcutManagerState.hpp"
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

	// Registration is a cold mutation. Build a complete candidate root so an
	// allocation failure cannot partially publish indices or consume an id.
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

bool ShortcutManager::unregisterShortcut(ShortcutId id) {
	auto& current = state();
	const auto registrationIt = current.registrationsById.find(id);
	if (registrationIt == current.registrationsById.end()) return false;
	const auto registration = registrationIt->second;
	registration->tombstoned = true; // visible immediately to an active dispatch snapshot
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
	// IDs and registration order are intentionally monotonic for the complete
	// window lifetime; clear must not create ABA aliases with an old snapshot.
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
				!scopeIsActive(*executable, context, ui) || !executable->callback) continue;
			if (executable->callback(context)) break;
		}
	}
}

} // namespace FlowUi
