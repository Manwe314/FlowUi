#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

#include "FlowUi/Public/BuildConfig.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <clay.h>

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"

namespace FlowUi {

class UiManager;
namespace detail { struct InputFieldFrameOverrides; }
namespace detail::platform {
class WindowPlatformServices;
}
namespace detail::input_field { struct InputFieldKey; }
namespace detail::text { class TextLayoutService; }
namespace detail::storage {
class StorageBinding;
}
namespace devSystems { class MemorySampleSink; }
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
 * @brief Window-owned editable text, caret state, and atomic edit operations.
 *
 * Authoring operations return direct values and propagate exceptions to the frame
 * boundary. Operational exceptions latch frame failure. False, empty snapshots
 * and EditResult rejection values are ordinary outcomes; frameStatus() remains
 * the checked query for a latched failure.
 * Obtain App-owned managers through the checked App getter and unwrap its
 * reference_wrapper with result->get().
 * Returned text, metadata, resource views, and pointers remain borrowed under
 * each operation's documented invalidation rules.
 */
class InputFieldManager {
public:
	/**
	 * @brief Read the latched failure for the current input-field frame.
	 *
	 * A successful later call does not clear an earlier frame failure. UiManager checks this status when completing the frame.
	 *
	 * @return Success means no failure was latched; an error means the surrounding frame must be canceled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.frameStatus();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status frameStatus() const noexcept {
		return frameError_ ? Status{unexpectedError(frameError_)} : Status{};
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only development memory accounting to the borrowed sink. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Register or update a field and borrow its current frame snapshot.
	 *
	 * Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.
	 *
	 * @return FieldQueryResult; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * FlowUi::FieldRequest request{};
	 * request.initialText = "Search";
	 * request.textElementId = textElementId;
	 * request.contentElementId = contentElementId;
	 * const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
	 * @endcode
	 */
	[[nodiscard]] FieldQueryResult requestField(FlowElementID fieldId, const FieldRequest& request);
	/**
	 * @brief Register or update a field and borrow its current frame snapshot.
	 *
	 * Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.
	 *
	 * @return FieldQueryResult; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * FlowUi::FieldRequest request{};
	 * request.initialText = "Search";
	 * request.textElementId = textElementId;
	 * request.contentElementId = contentElementId;
	 * const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
	 * @endcode
	 */
	[[nodiscard]] FieldQueryResult requestField(GlobalFlowID fieldId, const FieldRequest& request);
	/**
	 * @brief Register or update a field and borrow its current frame snapshot.
	 *
	 * Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.
	 *
	 * @return FieldQueryResult; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * FlowUi::FieldRequest request{};
	 * request.initialText = "Search";
	 * request.textElementId = textElementId;
	 * request.contentElementId = contentElementId;
	 * const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
	 * @endcode
	 */
	[[nodiscard]] FieldQueryResult requestField(FlowElementPartID fieldId,
												const FieldRequest& request);
	/**
	 * @brief Register or update a field and borrow its current frame snapshot.
	 *
	 * Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return FieldQueryResult; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "search"};
	 * FlowUi::FieldRequest request{};
	 * request.initialText = "Search";
	 * request.textElementId = textElementId;
	 * request.contentElementId = contentElementId;
	 * const FlowUi::FieldQueryResult field = manager.requestField(key, request);
	 * @endcode
	 */
	[[nodiscard]] FieldQueryResult requestField(ResourceKey key, const FieldRequest& request);

	/**
	 * @brief Associate a visible multiline Clay text node with its document range.
	 *
	 * Submit every emitted VisibleTextLine during the same frame as requestField(). FieldHandle::operator bool only tests its slot and does not validate the epoch.
	 *
	 * @return true when accepted; false means an expired handle, unknown field, invalid text ID/range, or range not matching a visible line.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::FieldTextSpanSubmission span{
	 *     .logicalRange = line.logicalRange,
	 *     .textElementId = textElementId,
	 *     .visualLineIndex = line.visualLineIndex,
	 * };
	 * const auto accepted = manager.submitTextSpan(field, span);
	 * @endcode
	 */
	[[nodiscard]] bool submitTextSpan(FieldHandle field, const FieldTextSpanSubmission& span);

	/**
	 * @brief Apply a focus or caret request to persistent field state.
	 *
	 * SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.
	 *
	 * @post The request has completed. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	void requestCaret(FlowElementID fieldId, CaretRequestKind kind);
	/**
	 * @brief Apply a focus or caret request to persistent field state.
	 *
	 * SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.
	 *
	 * @post The request has completed. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	void requestCaret(GlobalFlowID fieldId, CaretRequestKind kind);
	/**
	 * @brief Apply a focus or caret request to persistent field state.
	 *
	 * SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.
	 *
	 * @post The request has completed. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	void requestCaret(FlowElementPartID fieldId, CaretRequestKind kind);
	/**
	 * @brief Apply a focus or caret request to persistent field state.
	 *
	 * SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @post The request has completed. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "search"};
	 * manager.requestCaret(key, FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	void requestCaret(ResourceKey key, CaretRequestKind kind);

	/**
	 * @brief Remove a field's text, configuration, carets, and selection.
	 *
	 * Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.
	 *
	 * @return true if a field was removed or false for an empty strong ID/missing field.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto removed = manager.removeField(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool removeField(FlowElementID fieldId);
	/**
	 * @brief Remove a field's text, configuration, carets, and selection.
	 *
	 * Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.
	 *
	 * @return true if a field was removed or false for an empty strong ID/missing field.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto removed = manager.removeField(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool removeField(GlobalFlowID fieldId);
	/**
	 * @brief Remove a field's text, configuration, carets, and selection.
	 *
	 * Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.
	 *
	 * @return true if a field was removed or false for an empty strong ID/missing field.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto removed = manager.removeField(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool removeField(FlowElementPartID fieldId);
	/**
	 * @brief Remove a field's text, configuration, carets, and selection.
	 *
	 * Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return true if a field was removed or false for an empty strong ID/missing field.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "search"};
	 * const auto removed = manager.removeField(key);
	 * @endcode
	 */
	[[nodiscard]] bool removeField(ResourceKey key);

	/**
	 * @brief Replace an existing field's text while retaining its configuration.
	 *
	 * preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.
	 *
	 * @return true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto changed = manager.replaceText(fieldId, "Updated text", true);
	 * @endcode
	 */
	[[nodiscard]] bool replaceText(FlowElementID fieldId, std::string_view text, bool preserveCaret = true);
	/**
	 * @brief Replace an existing field's text while retaining its configuration.
	 *
	 * preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.
	 *
	 * @return true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto changed = manager.replaceText(fieldId, "Updated text", true);
	 * @endcode
	 */
	[[nodiscard]] bool replaceText(GlobalFlowID fieldId, std::string_view text, bool preserveCaret = true);
	/**
	 * @brief Replace an existing field's text while retaining its configuration.
	 *
	 * preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.
	 *
	 * @return true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto changed = manager.replaceText(fieldId, "Updated text", true);
	 * @endcode
	 */
	[[nodiscard]] bool replaceText(FlowElementPartID fieldId, std::string_view text, bool preserveCaret = true);
	/**
	 * @brief Replace an existing field's text while retaining its configuration.
	 *
	 * preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "search"};
	 * const auto changed = manager.replaceText(key, "Updated text", true);
	 * @endcode
	 */
	[[nodiscard]] bool replaceText(ResourceKey key, std::string_view text, bool preserveCaret = true);

	/**
	 * @brief Clear every field and reset focus, repeat, drag, and frame overrides.
	 *
	 * All previously borrowed field views, spans, text, and handles must be discarded.
	 *
	 * @post The reset has completed.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.clear();
	 * @endcode
	 */
	void clear();

	/**
	 * @brief Test whether a primary field has at least one caret.
	 *
	 * This is a direct noexcept query; it does not report initialization errors.
	 *
	 * @return Returns false if detached, no primary field exists, or its caret list is empty.
	 */
	[[nodiscard]] bool hasPrimaryFieldFocus() const noexcept;
	/**
	 * @brief Test whether the primary field exists, has a caret, and is writable.
	 *
	 * This is a direct noexcept query.
	 *
	 * @return Returns false for detached, unfocused, missing, caret-free, or read-only fields.
	 */
	[[nodiscard]] bool canEditPrimaryField() const noexcept;

	/**
	 * @brief Borrow the earliest selected range in the primary field.
	 *
	 * Single-line selections borrow field storage; chunked selections borrow scratch storage reused by a later getSelectedText() call. Copy to std::string immediately to retain it across another query, text mutation, removal, or frame/lifetime change.
	 *
	 * @return an empty view if there is no primary field or active selection. Detached state and copy/allocation failures throw.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const std::string selectedText{manager.getSelectedText()};
	 * @endcode
	 */
	[[nodiscard]] std::string_view getSelectedText() const;

	/**
	 * @brief Insert UTF-8 text at every caret in the primary field, replacing selections.
	 *
	 * The input is consumed synchronously. FieldConfig readOnly/maxBytes rules apply; normal keyboard text already comes through the frame pipeline.
	 *
	 * @return true when text/caret state changed; false for empty text, absent focus/carets, read-only fields, unchanged text/carets, or rejected UTF-8/newline/size policies.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto changed = manager.insertTextAtPrimaryCaret("Pasted text");
	 * @endcode
	 */
	[[nodiscard]] bool insertTextAtPrimaryCaret(std::string_view utf8Text);

	/**
	 * @brief Queue a semantic text command for the field focused now.
	 *
	 * payload is copied. The queue captures field identity and selection-extension state; commands execute during field processing, not necessarily at submission.
	 *
	 * @return true when queued or false when no primary field owns focus.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::TextCommand command = FlowUi::TextCommand::SelectAll;
	 * const auto queued = manager.enqueueCommand(command);
	 * @endcode
	 */
	[[nodiscard]] bool enqueueCommand(TextCommand command, std::string_view payload = {});

	/**
	 * @brief Validate and atomically apply a replacement batch to an existing field.
	 *
	 * Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.
	 *
	 * @return EditResult: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome and leaves text unchanged. Operational failures propagate to the frame boundary.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const std::array edits{FlowUi::TextReplacement{
	 *     .oldRange = {0, 0},
	 *     .insertedText = "Hello",
	 * }};
	 * const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
	 * @endcode
	 */
	[[nodiscard]] EditResult applyEdits(
		FlowElementID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	/**
	 * @brief Validate and atomically apply a replacement batch to an existing field.
	 *
	 * Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.
	 *
	 * @return EditResult: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome and leaves text unchanged. Operational failures propagate to the frame boundary.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const std::array edits{FlowUi::TextReplacement{
	 *     .oldRange = {0, 0},
	 *     .insertedText = "Hello",
	 * }};
	 * const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
	 * @endcode
	 */
	[[nodiscard]] EditResult applyEdits(
		GlobalFlowID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	/**
	 * @brief Validate and atomically apply a replacement batch to an existing field.
	 *
	 * Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.
	 *
	 * @return EditResult: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome and leaves text unchanged. Operational failures propagate to the frame boundary.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const std::array edits{FlowUi::TextReplacement{
	 *     .oldRange = {0, 0},
	 *     .insertedText = "Hello",
	 * }};
	 * const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
	 * @endcode
	 */
	[[nodiscard]] EditResult applyEdits(
		FlowElementPartID fieldId,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);
	/**
	 * @brief Validate and atomically apply a replacement batch to an existing field.
	 *
	 * Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return EditResult: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome and leaves text unchanged. Operational failures propagate to the frame boundary.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "search"};
	 * const std::array edits{FlowUi::TextReplacement{
	 *     .oldRange = {0, 0},
	 *     .insertedText = "Hello",
	 * }};
	 * const auto edit = manager.applyEdits(key, edits, FlowUi::EditOrigin::Programmatic);
	 * @endcode
	 */
	[[nodiscard]] EditResult applyEdits(
		ResourceKey key,
		std::span<const TextReplacement> edits,
		EditOrigin origin = EditOrigin::Programmatic);

private:
	mutable FlowUiError frameError_{};
	friend class UiManager;

	using CaretState = detail::manager_storage::InputCaretState;
	using FieldState = detail::manager_storage::InputFieldState;
	using SelectionRange = detail::manager_storage::InputSelectionRange;
	using KeyRepeatState = detail::manager_storage::InputKeyRepeatState;
	using PointerDragState = detail::manager_storage::InputPointerDragState;

	void init(detail::storage::StorageBinding& storage, WindowId window,
			  const InputManagerConfig& config, float pointsToPixelsScale,
			  detail::text::TextLayoutService& textLayoutService);
	void destroy() noexcept;
	void setConfig(const InputManagerConfig& config);
	void setFontFrameView(
		const detail::manager_storage::FontFrameView& fontView,
		float pointsToPixelsScale);
	void beginFrame(
		const FrameInput& currentInput,
		const FrameInput& previousInput,
		uint32_t suppressedPrimaryPressClayId = 0);
	/** Bind the borrowed per-window clipboard bridge while manager storage exists. */
	void setPlatformServices(detail::platform::WindowPlatformServices* services) noexcept;
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
	static std::string encodeTextInput(const FrameInput& input, TextFieldMode mode);
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
	detail::storage::StorageBinding* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	detail::manager_storage::InputFieldManagerState* state_ = nullptr;
	detail::text::TextLayoutService* textLayoutService_ = nullptr;
};

/** @} */

} // namespace FlowUi
