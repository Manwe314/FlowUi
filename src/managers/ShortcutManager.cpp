#include "managers/ShortcutManager.hpp"

#include <algorithm>

#include "managers/UiManager.hpp"

namespace {

constexpr uint8_t kModCtrl = 1u << 0u;
constexpr uint8_t kModShift = 1u << 1u;
constexpr uint8_t kModAlt = 1u << 2u;
constexpr uint8_t kModSuper = 1u << 3u;

} // namespace

namespace FlowUi {

ShortcutId ShortcutManager::registerShortcut(
	const ShortcutChord& chord,
	ShortcutScope scope,
	int32_t priority,
	ShortcutCallback callback) {
	if (!callback) {
		return 0u;
	}
	if (chord.key < 0 || chord.key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) {
		return 0u;
	}

	const ShortcutId id = nextShortcutId_++;
	const uint32_t packedChord = packChord(chord.key, modsMaskFromChord(chord), chord.trigger);

	ShortcutExecutable executable{};
	executable.scope = scope;
	executable.priority = priority;
	executable.id = id;
	executable.registrationOrder = nextRegistrationOrder_++;
	executable.callback = std::move(callback);

	ShortcutBucket& bucket = chordBuckets_[packedChord];
	bucket.push_back(std::move(executable));
	std::stable_sort(bucket.begin(), bucket.end(), executableOrderLess);

	shortcutIdToChord_[id] = packedChord;
	registeredKeyRefCount_[chord.key] += 1u;
	return id;
}

bool ShortcutManager::unregisterShortcut(ShortcutId id) {
	const auto chordIt = shortcutIdToChord_.find(id);
	if (chordIt == shortcutIdToChord_.end()) {
		return false;
	}

	const uint32_t packedChord = chordIt->second;
	const auto bucketIt = chordBuckets_.find(packedChord);
	if (bucketIt == chordBuckets_.end()) {
		shortcutIdToChord_.erase(chordIt);
		return false;
	}

	ShortcutBucket& bucket = bucketIt->second;
	const size_t oldSize = bucket.size();
	bucket.erase(
		std::remove_if(
			bucket.begin(),
			bucket.end(),
			[id](const ShortcutExecutable& executable) {
				return executable.id == id;
			}),
		bucket.end());
	const bool removed = bucket.size() != oldSize;

	shortcutIdToChord_.erase(chordIt);
	if (removed) {
		const int key = unpackKey(packedChord);
		auto keyRefIt = registeredKeyRefCount_.find(key);
		if (keyRefIt != registeredKeyRefCount_.end()) {
			if (keyRefIt->second <= 1u) {
				registeredKeyRefCount_.erase(keyRefIt);
			} else {
				keyRefIt->second -= 1u;
			}
		}
	}
	if (bucket.empty()) {
		chordBuckets_.erase(bucketIt);
	}
	return removed;
}

void ShortcutManager::clear() {
	chordBuckets_.clear();
	shortcutIdToChord_.clear();
	registeredKeyRefCount_.clear();
	nextShortcutId_ = 1u;
	nextRegistrationOrder_ = 1u;
	focusedElementId_ = Clay_ElementId{};
}

void ShortcutManager::setFocusedElement(Clay_ElementId elementId) {
	focusedElementId_ = elementId;
}

void ShortcutManager::clearFocusedElement() {
	focusedElementId_ = Clay_ElementId{};
}

void ShortcutManager::beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput) {
	if (registeredKeyRefCount_.empty()) {
		return;
	}

	ShortcutContext context{
		.ui = ui,
		.currentInput = currentInput,
		.previousInput = previousInput,
		.focusedElementId = focusedElementId_,
	};

	const uint8_t currentModsMask = modsMaskFromInput(currentInput);
	const uint8_t previousModsMask = modsMaskFromInput(previousInput);

	// Snapshot keys so callbacks can safely add/remove shortcuts.
	std::vector<int> keysToProcess;
	keysToProcess.reserve(registeredKeyRefCount_.size());
	for (const auto& [key, _] : registeredKeyRefCount_) {
		keysToProcess.push_back(key);
	}

	for (int key : keysToProcess) {
		if (registeredKeyRefCount_.find(key) == registeredKeyRefCount_.end()) {
			continue;
		}

		const bool isDown = keyDown(currentInput, key);
		const bool wasDown = keyDown(previousInput, key);
		if (isDown && !wasDown) {
			(void)dispatchPackedChord(context, ui, packChord(key, currentModsMask, ShortcutTrigger::Press));
			continue;
		}
		if (!isDown && wasDown) {
			(void)dispatchPackedChord(context, ui, packChord(key, previousModsMask, ShortcutTrigger::Release));
			continue;
		}
		if (isDown) {
			(void)dispatchPackedChord(context, ui, packChord(key, currentModsMask, ShortcutTrigger::Down));
		}
	}
}

uint8_t ShortcutManager::modsMaskFromChord(const ShortcutChord& chord) {
	uint8_t mask = 0u;
	if (chord.ctrl) {
		mask |= kModCtrl;
	}
	if (chord.shift) {
		mask |= kModShift;
	}
	if (chord.alt) {
		mask |= kModAlt;
	}
	if (chord.super) {
		mask |= kModSuper;
	}
	return mask;
}

uint8_t ShortcutManager::modsMaskFromInput(const FrameInput& input) {
	uint8_t mask = 0u;
	if (input.ctrl) {
		mask |= kModCtrl;
	}
	if (input.shift) {
		mask |= kModShift;
	}
	if (input.alt) {
		mask |= kModAlt;
	}
	if (input.super) {
		mask |= kModSuper;
	}
	return mask;
}

bool ShortcutManager::keyDown(const FrameInput& input, int key) {
	if (key < 0 || key >= static_cast<int>(FrameInput::kKeyboardKeyCount)) {
		return false;
	}
	return input.keyDown[static_cast<size_t>(key)];
}

uint32_t ShortcutManager::packChord(int key, uint8_t modsMask, ShortcutTrigger trigger) {
	const uint32_t keyBits = static_cast<uint32_t>(std::max(0, key)) & 0x3FFu;
	const uint32_t modBits = static_cast<uint32_t>(modsMask & 0x0Fu) << 10u;
	const uint32_t triggerBits = (static_cast<uint32_t>(trigger) & 0x03u) << 14u;
	return keyBits | modBits | triggerBits;
}

int ShortcutManager::unpackKey(uint32_t packedChord) {
	return static_cast<int>(packedChord & 0x3FFu);
}

bool ShortcutManager::executableOrderLess(const ShortcutExecutable& a, const ShortcutExecutable& b) {
	if (a.scope != b.scope) {
		return static_cast<uint8_t>(a.scope) < static_cast<uint8_t>(b.scope);
	}
	if (a.priority != b.priority) {
		return a.priority > b.priority;
	}
	return a.registrationOrder < b.registrationOrder;
}

bool ShortcutManager::dispatchPackedChord(ShortcutContext& context, UiManager& ui, uint32_t packedChord) const {
	const auto it = chordBuckets_.find(packedChord);
	if (it == chordBuckets_.end()) {
		return false;
	}

	// Snapshot bucket so callback-side registrations don't invalidate iteration.
	const ShortcutBucket bucketSnapshot = it->second;
	for (const ShortcutExecutable& executable : bucketSnapshot) {
		if (shortcutIdToChord_.find(executable.id) == shortcutIdToChord_.end()) {
			continue;
		}
		if (!scopeIsActive(executable, context, ui)) {
			continue;
		}
		if (!executable.callback) {
			continue;
		}
		if (executable.callback(context)) {
			return true;
		}
	}
	return false;
}

bool ShortcutManager::scopeIsActive(
	const ShortcutExecutable& executable,
	const ShortcutContext& context,
	UiManager& ui) const {
	switch (executable.scope) {
	case ShortcutScope::FocusedInput:
		return ui.inputFields().hasPrimaryFieldFocus();
	case ShortcutScope::FocusedElement:
		return context.focusedElementId.id != 0u;
	case ShortcutScope::Global:
		return true;
	default:
		return false;
	}
}

} // namespace FlowUi
