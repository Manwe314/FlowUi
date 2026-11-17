#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ElementManager.hpp"
#include "FlowUi/Public/managers/InputFieldManager.hpp"
#include "FlowUi/Public/managers/PopupManager.hpp"
#include "FlowUi/Public/managers/ShortcutManager.hpp"

#include <cassert>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
using namespace FlowUi;

static_assert(!noexcept(std::declval<AppActions&>().invoke(AppActionCall{})));
static_assert(!noexcept(std::declval<AppActions&>().invokeFor<int>(AppActionCall{})));
static_assert(!noexcept(std::declval<ActionManager&>().invoke(ActionCall{})));
static_assert(noexcept(std::declval<PopupManager&>().request(FlowElementID{}, PopupRequest{})));
static_assert(!noexcept(std::declval<PopupManager&>().dismiss(FlowElementID{})));
static_assert(noexcept(std::declval<ShortcutManager&>().unregisterShortcut(0)));
static_assert(!noexcept(std::declval<InputFieldManager&>().requestField(FlowElementID{}, FieldRequest{})));
static_assert(!noexcept(std::declval<InputFieldManager&>().replaceText(FlowElementID{}, std::string_view{})));
static_assert(!noexcept(std::declval<UiManager&>().toClayString(std::string_view{})));
static_assert(std::same_as<decltype(std::declval<ActionManager&>().invoke(ActionCall{})), ActionInvocationStatus>);
static_assert(std::same_as<decltype(std::declval<InputFieldManager&>().requestField(FlowElementID{}, FieldRequest{})), FieldQueryResult>);
static_assert(std::same_as<decltype(std::declval<InputFieldManager&>().requestCaret(FlowElementID{}, CaretRequestKind::ClearAll)), void>);
static_assert(std::same_as<decltype(std::declval<InputFieldManager&>().applyEdits(FlowElementID{}, std::span<const TextReplacement>{})), EditResult>);
static_assert(std::same_as<decltype(std::declval<PopupManager&>().consumeDismissed(FlowElementID{})), bool>);

template <typename Operation>
[[nodiscard]] FlowUiError expectLibraryFailure(Operation&& operation, ErrorCode code, ErrorSite site) {
	FlowUiError observed{};
	bool caughtLibraryFailure = false;
	try {
		std::forward<Operation>(operation)();
	} catch (const FlowUiException& exception) {
		observed = exception.error();
		caughtLibraryFailure = true;
	}
	assert(caughtLibraryFailure);
	assert(observed.code == code);
	assert(observed.site == site);
	return observed;
}

void verifyDetachedManagerErrors() {
	PopupManager popups;
	const auto popup = popups.request(FlowElementID{1}, PopupRequest{});
	assert(!popup);
	assert(popup.error().code == ErrorCode::ObjectNotInitialized);
	assert(popup.error().site == ErrorSite::PopupRequest);
	const auto dismissal = expectLibraryFailure([&] { popups.dismiss(FlowElementID{1}); },
		ErrorCode::ObjectNotInitialized, ErrorSite::PopupRequest);
	assert(dismissal.subject == popup.error().subject);
	const auto popupFrame = popups.frameStatus();
	assert(!popupFrame && popupFrame.error().code == dismissal.code);
	(void)expectLibraryFailure([&] { (void)popups.consumeDismissed(FlowElementID{1}); },
		ErrorCode::ObjectNotInitialized, ErrorSite::PopupRequest);

	ShortcutManager shortcuts;
	const auto removal = shortcuts.unregisterShortcut(1);
	assert(!removal);
	assert(removal.error().code == ErrorCode::ObjectNotInitialized);
	assert(removal.error().site == ErrorSite::ShortcutResolve);

	InputFieldManager inputFields;
	const auto field = expectLibraryFailure([&] { (void)inputFields.requestField(FlowElementID{1}, FieldRequest{}); },
		ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame);
	const auto replacement = expectLibraryFailure([&] { (void)inputFields.replaceText(FlowElementID{1}, "replacement"); },
		ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame);
	const auto inputFrame = inputFields.frameStatus();
	assert(!inputFrame && inputFrame.error().code == field.code && inputFrame.error().site == field.site);
	assert(replacement.code == field.code);
	(void)expectLibraryFailure([&] { (void)inputFields.applyEdits(FlowElementID{1}, std::span<const TextReplacement>{}); },
		ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame);
	assert(!inputFields.hasPrimaryFieldFocus());

	ActionManager actions;
	const auto invocation = actions.invoke(ActionCall{});
	assert(invocation == ActionInvocationStatus::Empty);
	const auto appInvocation = actions.appActions().invoke(AppActionCall{});
	assert(appInvocation == ActionInvocationStatus::Empty);
}
void verifyActionCallbackFailures() {
	ActionManager actions;
	const FlowUiError expected = makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, 42, 99, 7);
	constexpr auto preserveError = UiAction("tests.boundaries.preserve_error", [](const FlowUiError& error) {
		throw FlowUiException(error);
	});
	bool caughtOriginal = false;
	try {
		(void)actions.invoke(actions.uiActions().make(preserveError, expected));
	} catch (const FlowUiException& exception) {
		const auto& preserved = exception.error();
		caughtOriginal = preserved.code == expected.code && preserved.site == expected.site &&
			preserved.subject == expected.subject && preserved.auxiliary == expected.auxiliary &&
			preserved.nativeCode == expected.nativeCode;
	}
	assert(caughtOriginal);

	constexpr auto allocationFailure = UiAction("tests.boundaries.allocation", [] {
		throw std::bad_alloc{};
	});
	caughtOriginal = false;
	try {
		(void)actions.invoke(actions.uiActions().make(allocationFailure));
	} catch (const std::bad_alloc&) {
		caughtOriginal = true;
	}
	assert(caughtOriginal);

	constexpr auto foreignFailure = UiAction("tests.boundaries.foreign", [] {
		throw std::runtime_error("callback failure");
	});
	caughtOriginal = false;
	try {
		(void)actions.invoke(actions.uiActions().make(foreignFailure));
	} catch (const std::runtime_error& exception) {
		caughtOriginal = std::string_view(exception.what()) == "callback failure";
	}
	assert(caughtOriginal);

	// A user exception carrying a fatal-looking FlowUi code is not a proven library invariant failure.
	const auto userError = makeError(ErrorCode::ActionPublicationConflict, ErrorSite::ActionPublish);
	const auto contained = detail::catchResult<Status>(ErrorSite::ActionInvoke, [&]() -> Status {
		throw FlowUiException(userError);
	});
	assert(!contained && contained.error().code == userError.code);

}

} // namespace

int main() {
	verifyDetachedManagerErrors();
	verifyActionCallbackFailures();
}
