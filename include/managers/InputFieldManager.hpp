#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ElementID.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "FlowUi/WindowId.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"
#include "managers/structs/InputStructs.hpp"

namespace FlowUi {

class UiManager;
namespace detail { struct InputFieldFrameOverrides; }
namespace detail::input_field { struct InputFieldKey; }
namespace detail::text { class TextLayoutService; }
namespace detail::storage { class IStorageSystem; }
namespace detail::manager_storage {
struct InputCaretState;
struct InputFieldState;
struct InputSelectionRange;
struct InputKeyRepeatState;
struct InputPointerDragState;
struct InputFieldManagerState;
struct FontFrameView;
}

/** @addtogroup flowui_input_field_manager
 * @{
 */

/**
 * @brief Manages text input fields, focus, caret state, and edit operations.
 *
 * InputFieldManager is the public entry point for custom FlowUi elements that
 * need editable text. The manager owns persistent field state, keyed by an
 * explicit strong Flow ID or ResourceKey, while the element remains responsible for drawing the
 * Clay containers and text node for that field.
 *
 * A field becomes part of the current frame when custom UI code calls
 * requestField(). The request updates the field configuration and Clay element
 * ids, then returns the current manager-owned text. The initial text is copied
 * only when a field id is first seen; later requests with the same id keep the
 * edited text until removeField() or clear() resets it.
 *
 * Focus and caret changes are requested separately through requestCaret(),
 * normally from an element interaction callback. FlowUi's frame lifecycle then
 * handles pointer hit testing, keyboard input, caret movement, selection
 * rendering, and text mutation before the frame is rendered.
 *
 * @code{.cpp}
 * struct MyTextInput {
 *     using Parameters = MyTextInputParameters;
 *     using InteractionContext = FlowUi::ElementInteractionContext<MyTextInput>;
 *     using BuildContext = FlowUi::ElementBuildContext<MyTextInput>;
 *
 *     struct Parts {
 *         static constexpr auto text = FlowUi::Part("text");
 *         static constexpr auto content = FlowUi::Part("content");
 *     };
 *
 *     static constexpr auto definitionId = FlowUi::DefinitionID("my-text-input");
 *
 *     static void onPressed(InteractionContext& context) {
 *         context.uiManager.inputFields().requestCaret(
 *             context.id,
 *             FlowUi::CaretRequestKind::SetPrimary);
 *     }
 *
 *     static void buildElement(BuildContext& context) {
 *         const Clay_ElementId rootId = context.clayID();
 *         const Clay_ElementId textId =
 *             context.uiManager.toClayEID(context.part(Parts::text));
 *         const Clay_ElementId contentId =
 *             context.uiManager.toClayEID(context.part(Parts::content));
 *
 *         const FlowUi::FieldQueryResult field =
 *             context.uiManager.inputFields().requestField(context.id, {
 *                 .initialText = context.params.initialText,
 *                 .config = FlowUi::FieldConfig{
 *                     .readOnly = false,
 *                     .allowNewline = false,
 *                 },
 *                 .textElementId = textId,
 *                 .contentElementId = contentId,
 *             });
 *
 *         Clay_TextElementConfig textConfig{};
 *         textConfig.fontId = context.params.fontId;
 *         textConfig.fontSize = 14;
 *
 *         CLAY(rootId, {}) {
 *             CLAY(contentId, {}) {
 *                 CLAY_TEXT(
 *                     context.uiManager.toClayString(
 *                         field.text.contiguous().value_or(std::string_view{})),
 *                     CLAY_TEXT_CONFIG(textConfig));
 *             };
 *         };
 *     }
 * };
 * @endcode
 *
 * @see @ref md_docs_2tutorials_2input__fields__and__shortcuts "Input Fields and Shortcuts"
 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
 */
class InputFieldManager {
public:
	/**
	 * @brief Register or update an input field for the current frame.
	 *
	 * requestField() marks the field as present for this frame, creates manager
	 * state when fieldId is new, updates the field config and Clay element ids,
	 * and returns the current text/focus snapshot for the field. Call it once
	 * per frame from the element that draws the field.
	 *
	 * If the currently focused field is not requested in a frame, FlowUi clears
	 * primary input focus for that field when the frame is finalized. Non-focused
	 * field state remains cached until removeField() or clear() is called.
	 *
	 * @param fieldId Strong element, global, part, or resource identity used to
	 * address the persistent field state.
	 * @param request First-use text, behavior configuration, and Clay element ids
	 * used for text measurement and pointer hit testing.
	 * @return Current manager-owned text plus focus and selection flags. Returns
	 * an empty result when fieldId is empty.
	 *
	 * @throws std::bad_alloc if internal field storage allocation fails.
	 *
	 * @code{.cpp}
	 * const FlowUi::FieldQueryResult field =
	 *     context.uiManager.inputFields().requestField(context.id, {
	 *         .initialText = "Search",
	 *         .config = FlowUi::FieldConfig{
	 *             .readOnly = false,
	 *             .allowNewline = false,
	 *             .maxBytes = 256,
	 *         },
	 *         .textElementId = context.clayID(FlowUi::LocalElementName("text")),
	 *         .contentElementId = context.clayID(FlowUi::LocalElementName("content")),
	 *     });
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2input__fields__and__shortcuts "Input Fields and Shortcuts"
	 */
	FieldQueryResult requestField(FlowElementID fieldId, const FieldRequest& request);
	FieldQueryResult requestField(GlobalFlowID fieldId, const FieldRequest& request);
	FieldQueryResult requestField(FlowElementPartID fieldId, const FieldRequest& request);
	FieldQueryResult requestField(ResourceKey key, const FieldRequest& request);

	/**
	 * @brief Associate a rendered visible line with its logical document range.
	 *
	 * Multiline controls call this once for every VisibleTextLine they emit.
	 * The frame-scoped handle prevents stale submissions from being accepted.
	 */
	bool submitTextSpan(FieldHandle field, const FieldTextSpanSubmission& span);

	/**
	 * @brief Request focus or caret changes for an input field.
	 *
	 * Caret requests are applied immediately to manager state. SetPrimary is the
	 * normal focus operation for a clicked input field. ClearAll removes focus
	 * from every field and ignores fieldId. Add creates another caret for the
	 * requested field and is intended for advanced multi-caret editing.
	 *
	 * Calling SetPrimary for a field id that has not been requested yet creates
	 * empty manager state for that id. The next requestField() call for the same
	 * id will attach config and Clay element ids to that state.
	 *
	 * @param fieldId Stable id of the field to focus or modify. Ignored when
	 * kind is CaretRequestKind::ClearAll.
	 * @param kind Caret operation to perform.
	 *
	 * @throws std::bad_alloc if the request creates a new field state and
	 * internal storage allocation fails.
	 *
	 * @code{.cpp}
	 * context.uiManager.inputFields().requestCaret(
	 *     context.id,
	 *     FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	void requestCaret(FlowElementID fieldId, CaretRequestKind kind);
	void requestCaret(GlobalFlowID fieldId, CaretRequestKind kind);
	void requestCaret(FlowElementPartID fieldId, CaretRequestKind kind);
	void requestCaret(ResourceKey key, CaretRequestKind kind);

	/**
	 * @brief Remove a managed field by id.
	 *
	 * This discards the field's stored text, config, Clay element ids, carets,
	 * and selection state. Use it when an external value should replace the
	 * current editable text, or when a dynamic field is permanently removed from
	 * the UI.
	 *
	 * If the removed field owns primary input focus, focus is cleared or moved
	 * to another field that still has carets.
	 *
	 * @param fieldId Stable id of the field state to remove.
	 * @retval true a field with fieldId existed and was removed.
	 * @retval false fieldId is empty or no matching field exists.
	 *
	 * @code{.cpp}
	 * if (externalValueChanged) {
	 *     (void)context.uiManager.inputFields().removeField(fieldId);
	 * }
	 * @endcode
	 */
	bool removeField(FlowElementID fieldId);
	bool removeField(GlobalFlowID fieldId);
	bool removeField(FlowElementPartID fieldId);
	bool removeField(ResourceKey key);

	/**
	 * @brief Replace the managed text for an existing input field.
	 *
	 * replaceText() updates a field without removing its config, Clay element
	 * ids, or frame presence state. By default, existing carets and selections
	 * are preserved and clamped to valid UTF-8 boundaries in the replacement
	 * text. Pass false to clear any active carets from the field after updating
	 * the text.
	 *
	 * @param fieldId Stable id of the field state to update.
	 * @param text Replacement text to store for the field.
	 * @param preserveCaret Whether to preserve and clamp existing caret state.
	 * Defaults to true.
	 * @retval true the field existed and its stored text changed.
	 * @retval false fieldId is empty, no matching field exists, or text already
	 * matches the stored field text.
	 *
	 * @throws std::bad_alloc if copying replacement text requires allocation
	 * and allocation fails.
	 *
	 * @code{.cpp}
	 * const bool changed = app.ui().inputFields().replaceText(
	 *     MasterNameField,
	 *     externalName,
	 *     false);
	 * @endcode
	 */
	bool replaceText(FlowElementID fieldId, std::string_view text, bool preserveCaret = true);
	bool replaceText(GlobalFlowID fieldId, std::string_view text, bool preserveCaret = true);
	bool replaceText(FlowElementPartID fieldId, std::string_view text, bool preserveCaret = true);
	bool replaceText(ResourceKey key, std::string_view text, bool preserveCaret = true);

	/**
	 * @brief Clear all managed input field state.
	 *
	 * clear() removes every field, clears primary focus, resets key repeat and
	 * pointer drag state, and drops any frame render overrides. It is a global
	 * reset for the input field system.
	 *
	 * @code{.cpp}
	 * app.ui().inputFields().clear();
	 * @endcode
	 */
	void clear();

	/**
	 * @brief Return whether any field owns primary input focus.
	 *
	 * This is useful when global shortcuts should be suppressed while the user is
	 * editing text. ShortcutManager uses the same state for
	 * ShortcutScope::FocusedInput.
	 *
	 *	@retval true a primary field id is set and that field has at least one caret.
	 *  @retval false no primary field id is set.
	 * 
	 *
	 * @code{.cpp}
	 * if (!app.ui().inputFields().hasPrimaryFieldFocus()) {
	 *     runGlobalShortcut();
	 * }
	 * @endcode
	 */
	bool hasPrimaryFieldFocus() const;
	bool canEditPrimaryField() const;

	/**
	 * @brief Return selected text from the primary input field.
	 *
	 * When the focused field has multiple selections, the earliest selected
	 * range is returned. If no primary field exists, the primary field is gone,
	 * or no selection is active, the returned view is empty.
	 *
	 * @return View of the selected text in manager-owned storage, or an empty
	 * view when there is no active selection. The view remains valid until the
	 * selected field text changes or the field is removed.
	 *
	 * @code{.cpp}
	 * const std::string selected(app.ui().inputFields().getSelectedText());
	 * if (!selected.empty()) {
	 *     app.ui().setClipboardText(selected);
	 * }
	 * @endcode
	 */
	std::string_view getSelectedText() const;

	/**
	 * @brief Insert UTF-8 text at the primary caret.
	 *
	 * The text is inserted into the focused field at each caret position. Active
	 * selections are replaced. The operation respects FieldConfig::readOnly and
	 * FieldConfig::maxBytes for the target field.
	 *
	 * This function is intended for paste commands, virtual keyboards, or other
	 * application-driven text insertion. Normal keyboard text input is collected
	 * by FlowUi's frame input pipeline and applied automatically.
	 *
	 * @param utf8Text UTF-8 text to insert.
	 * @retval true the focused field text or caret positions changed.
	 * @retval false utf8Text is empty, no field is focused, the field is read-only, the
	 * field has no caret, or the insertion was rejected by maxBytes.
	 *
	 * @throws std::bad_alloc if copying or inserting text requires allocation
	 * and allocation fails.
	 *
	 * @code{.cpp}
	 * const std::string clipboard = app.ui().clipboardText();
	 * (void)app.ui().inputFields().insertTextAtPrimaryCaret(clipboard);
	 * @endcode
	 */
	bool insertTextAtPrimaryCaret(std::string_view utf8Text);

	/** Queue a semantic command for the currently focused field. */
	bool enqueueCommand(TextCommand command, std::string_view payload = {});

	/** Apply a validated replacement batch atomically to an existing field. */
	EditResult applyEdits(
		FlowElementID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	EditResult applyEdits(
		GlobalFlowID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	EditResult applyEdits(
		FlowElementPartID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	EditResult applyEdits(
		ResourceKey key,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);

private:
	friend class UiManager;

	using CaretState = detail::manager_storage::InputCaretState;
	using FieldState = detail::manager_storage::InputFieldState;
	using SelectionRange = detail::manager_storage::InputSelectionRange;
	using KeyRepeatState = detail::manager_storage::InputKeyRepeatState;
	using PointerDragState = detail::manager_storage::InputPointerDragState;

	void init(
		detail::storage::IStorageSystem& storage,
		WindowId window,
		const InputManagerConfig& config,
		float pointsToPixelsScale,
		detail::text::TextLayoutService& textLayoutService);
	void destroy() noexcept;
	void setConfig(const InputManagerConfig& config);
	void setFontFrameView(
		const detail::manager_storage::FontFrameView& fontView,
		float pointsToPixelsScale);
	void beginFrame(const FrameInput& currentInput, const FrameInput& previousInput);
	void setClipboardAccess(
		std::function<void(std::string_view)> setClipboardText,
		std::function<std::string()> getClipboardText);
	Clay_RenderCommandArray endFrame(const Clay_RenderCommandArray& renderCommands);
	const detail::InputFieldFrameOverrides& frameOverrides() const;
	[[nodiscard]] detail::input_field::InputFieldKey normalizeFieldKey(ResourceKey key) const;
	FieldQueryResult requestFieldByKey(
		detail::input_field::InputFieldKey fieldId,
		const FieldRequest& request);
	void requestCaretByKey(
		detail::input_field::InputFieldKey fieldId,
		CaretRequestKind kind);
	bool removeFieldByKey(detail::input_field::InputFieldKey fieldId);
	bool replaceTextByKey(
		detail::input_field::InputFieldKey fieldId,
		std::string_view text,
		bool preserveCaret);

	void applyCapturedEdits(detail::input_field::InputFieldKey fieldId, FieldState& field);
	void applyPendingCommands(detail::input_field::InputFieldKey fieldId, FieldState& field);
	EditResult applyEditsByKey(
		detail::input_field::InputFieldKey fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin,
		bool requireFocus,
		bool enforceReadOnly);
	EditResult commitEdits(
		FieldState& field,
		std::span<const TextReplacement> edits,
		EditOrigin origin,
		bool enforceReadOnly);
	void refreshTransactionViews(FieldState& field);
	void moveCaretsByWord(FieldState& field, int direction, bool selecting);
	void applyWordDelete(FieldState& field, bool backspace);
	void markCaretBlinkReset();
	void updateCaretBlinkVisibility(
		bool hasAnyActiveCaret,
		const InputFieldOverlayStyle* style = nullptr);
	bool shouldTriggerActionWithRepeat(int key, KeyRepeatState& state);
	void applyTextInsertion(FieldState& field, std::string_view utf8Text, EditOrigin origin = EditOrigin::TypedInput);
	void applyDelete(FieldState& field, bool backspace, EditOrigin origin = EditOrigin::Delete);
	void moveCaretsHorizontal(FieldState& field, int direction, bool selecting);
	void moveCaretsVertical(FieldState& field, int direction, bool selecting);
	void moveCaretsToLineBoundary(FieldState& field, bool toEnd, bool selecting);
	void clampCaretsToText(FieldState& field) const;
	void revealPrimaryCaret(FieldState& field);
	void materializeVisibleLines(FieldState& field);
	float fieldLineHeight(const FieldState& field) const;
	std::vector<TextRange> visualRangesForHardLine(FieldState& field, size_t hardLineIndex);
	float caretXInRange(const FieldState& field, TextRange range, size_t byteOffset) const;

	static size_t clampUtf8Boundary(std::string_view text, size_t offset);
	static size_t nextUtf8Codepoint(std::string_view text, size_t offset);
	static size_t prevUtf8Codepoint(std::string_view text, size_t offset);
	static bool isUtf8ContinuationByte(char c);
	static bool keyPressedThisFrame(const FrameInput& currentInput, const FrameInput& previousInput, int key);
	static bool keyDown(const FrameInput& input, int key);
	static bool caretHasSelection(const CaretState& caret);
	static size_t caretSelectionStart(const CaretState& caret);
	static size_t caretSelectionEnd(const CaretState& caret);
	static std::string encodeTextInput(const FrameInput& input, bool allowNewline);
	static bool elementIdIsValid(const Clay_ElementId& id);

	float measureTextSlice(const Clay_StringSlice& text, const Clay_TextRenderData& textData) const;
	static Clay_StringSlice subSlice(const Clay_StringSlice& source, int start, int length);
	static bool findSliceOffsetFromCursor(
		std::string_view fullText,
		std::string_view needle,
		size_t cursor,
		size_t& outOffset);
	std::vector<SelectionRange> mergedSelectionRanges(const FieldState& field) const;

private:
	detail::storage::IStorageSystem* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	detail::manager_storage::InputFieldManagerState* state_ = nullptr;
	detail::text::TextLayoutService* textLayoutService_ = nullptr;
};

/** @} */

} // namespace FlowUi
