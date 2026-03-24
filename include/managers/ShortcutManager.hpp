#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "window/Inputs.hpp"

namespace FlowUi {

class UiManager;

enum class ShortcutScope : uint8_t {
	FocusedInput = 0,
	FocusedElement = 1,
	Global = 2,
};

enum class ShortcutTrigger : uint8_t {
	Press = 0,
	Release = 1,
	Down = 2,
};

struct ShortcutChord {
	int key = -1;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
	bool super = false;
	ShortcutTrigger trigger = ShortcutTrigger::Press;
};

struct ShortcutContext {
	UiManager& ui;
	const FrameInput& currentInput;
	const FrameInput& previousInput;
	Clay_ElementId focusedElementId{};
};

using ShortcutCallback = std::function<bool(ShortcutContext&)>;
using ShortcutId = uint32_t;

class ShortcutManager {
public:
	ShortcutId registerShortcut(
		const ShortcutChord& chord,
		ShortcutScope scope,
		int32_t priority,
		ShortcutCallback callback);
	bool unregisterShortcut(ShortcutId id);
	void clear();

	void setFocusedElement(Clay_ElementId elementId);
	void clearFocusedElement();
	Clay_ElementId focusedElement() const { return focusedElementId_; }

	void beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput);

private:
	struct ShortcutExecutable {
		ShortcutScope scope = ShortcutScope::Global;
		int32_t priority = 0;
		ShortcutId id = 0u;
		uint64_t registrationOrder = 0u;
		ShortcutCallback callback{};
	};

	using ShortcutBucket = std::vector<ShortcutExecutable>;

	static uint8_t modsMaskFromChord(const ShortcutChord& chord);
	static uint8_t modsMaskFromInput(const FrameInput& input);
	static bool keyDown(const FrameInput& input, int key);
	static uint32_t packChord(int key, uint8_t modsMask, ShortcutTrigger trigger);
	static int unpackKey(uint32_t packedChord);
	static bool executableOrderLess(const ShortcutExecutable& a, const ShortcutExecutable& b);

	bool dispatchPackedChord(ShortcutContext& context, UiManager& ui, uint32_t packedChord) const;
	bool scopeIsActive(const ShortcutExecutable& executable, const ShortcutContext& context, UiManager& ui) const;

	std::unordered_map<uint32_t, ShortcutBucket> chordBuckets_{};
	std::unordered_map<ShortcutId, uint32_t> shortcutIdToChord_{};
	std::unordered_map<int, uint32_t> registeredKeyRefCount_{};
	uint64_t nextShortcutId_ = 1u;
	uint64_t nextRegistrationOrder_ = 1u;
	Clay_ElementId focusedElementId_{};
};

} // namespace FlowUi
