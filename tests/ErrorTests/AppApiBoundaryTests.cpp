#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/AppElementWindows.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/Color.hpp"

#include <functional>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(noexcept(FlowUi::makeApplication(std::declval<const FlowUi::AppConfig&>())));
static_assert(noexcept(std::declval<FlowUi::App&>().fonts()));
static_assert(noexcept(std::declval<FlowUi::App&>().ui()));
static_assert(noexcept(std::declval<FlowUi::App&>().beginFrame()));
static_assert(noexcept(std::declval<FlowUi::App&>().endFrame()));
static_assert(noexcept(std::declval<FlowUi::App&>().drawFrame()));
static_assert(noexcept(std::declval<FlowUi::App&>().clipboardText()));
static_assert(std::is_same_v<decltype(FlowUi::makeApplication(std::declval<const FlowUi::AppConfig&>())),
                             FlowUi::Result<FlowUi::App>>);
static_assert(std::is_same_v<decltype(std::declval<FlowUi::App&>().fonts()),
                             FlowUi::Result<std::reference_wrapper<FlowUi::FontManager>>>);

namespace {
[[nodiscard]] bool sameError(FlowUi::FlowUiError observed, FlowUi::FlowUiError expected) noexcept {
	return observed.code == expected.code && observed.site == expected.site &&
		observed.subject == expected.subject && observed.auxiliary == expected.auxiliary &&
		observed.nativeCode == expected.nativeCode;
}

struct CopyFailureCallback {
	FlowUi::FlowUiError error;
	explicit CopyFailureCallback(FlowUi::FlowUiError failure) noexcept : error(failure) {}
	CopyFailureCallback(const CopyFailureCallback& other) : error(other.error) {
		throw FlowUi::FlowUiException(error);
	}
	void operator()(FlowUi::UiManager&, FlowUi::WindowId) const noexcept {}
};

struct CopyFailureParameters {
	FlowUi::FlowUiError error{};
	CopyFailureParameters() noexcept = default;
	explicit CopyFailureParameters(FlowUi::FlowUiError failure) noexcept : error(failure) {}
	CopyFailureParameters(const CopyFailureParameters& other) : error(other.error) {
		throw FlowUi::FlowUiException(error);
	}
};

struct CopyFailureElement {
	using Parameters = CopyFailureParameters;
	using BuildContext = FlowUi::ElementBuildContext<CopyFailureElement>;
	static constexpr auto definitionId = FlowUi::DefinitionID("test.api-boundary.copy-failure");
	static void buildElement(BuildContext& context) noexcept {
		context.uiManager.recordFrameError(context.params.error);
	}
};
static_assert(FlowUi::FlowElement<CopyFailureElement>);

struct CopyFailureConfigurator {
	FlowUi::FlowUiError error;
	explicit CopyFailureConfigurator(FlowUi::FlowUiError failure) noexcept : error(failure) {}
	CopyFailureConfigurator(const CopyFailureConfigurator& other) : error(other.error) {
		throw FlowUi::FlowUiException(error);
	}
	FlowUi::Status operator()(FlowUi::ElementBuilder<CopyFailureElement>& builder, FlowUi::WindowId) const {
		builder.draw();
		return {};
	}
};

int liveFrameBoundaries() {
	FlowUi::AppConfig config{};
	config.window.width = 240;
	config.window.height = 160;
	config.window.title = "FlowUi API error boundary verification";
	config.vk.enableValidation = false;
	auto application = FlowUi::makeApplication(config);
	if (!application) {
		switch (application.error().code) {
		case FlowUi::ErrorCode::PlatformInitializationFailed:
		case FlowUi::ErrorCode::VulkanDeviceUnavailable:
		case FlowUi::ErrorCode::VulkanVersionUnsupported:
		case FlowUi::ErrorCode::VulkanExtensionMissing:
		case FlowUi::ErrorCode::VulkanFeatureMissing:
		case FlowUi::ErrorCode::WindowPresentationUnsupported:
			return 77;
		default:
			return 10;
		}
	}
	const auto original = FlowUi::makeError(FlowUi::ErrorCode::AssetNotFound,
		FlowUi::ErrorSite::ResourceLocate, 43, 17, 29);
	const FlowUi::UiBuildCallback flowFailure = [original](FlowUi::UiManager&, FlowUi::WindowId) {
		throw FlowUi::FlowUiException(original);
	};
	const auto failed = application->runFrame(flowFailure);
	if (failed || !sameError(failed.error(), original))
		return 11;
	const FlowUi::UiBuildCallback foreignFailure = [](FlowUi::UiManager&, FlowUi::WindowId) {
		throw 42;
	};
	const auto foreign = application->runFrame(foreignFailure);
	if (foreign || foreign.error().code != FlowUi::ErrorCode::ForeignExceptionObserved)
		return 12;
	const FlowUi::UiBuildCallback allocationFailure = [](FlowUi::UiManager&, FlowUi::WindowId) {
		throw std::bad_alloc{};
	};
	const auto allocation = application->runFrame(allocationFailure);
	if (allocation || allocation.error().code != FlowUi::ErrorCode::AllocationFailed)
		return 13;
	const FlowUi::UiBuildCallback latchedFailure = [original](FlowUi::UiManager& ui, FlowUi::WindowId) {
		ui.recordFrameError(original);
	};
	const auto latched = application->runFrame(latchedFailure);
	if (latched || !sameError(latched.error(), original))
		return 14;
	const auto latchedBeforeException = application->runFrame([original](FlowUi::UiManager& ui, FlowUi::WindowId) {
		ui.recordFrameError(original);
		throw std::bad_alloc{};
	});
	if (latchedBeforeException || !sameError(latchedBeforeException.error(), original))
		return 17;
	const auto publicAdapter = application->runFrame([original](FlowUi::UiManager&, FlowUi::WindowId) {
		(void)FlowUi::ValueOrThrow(FlowUi::Result<int>{FlowUi::unexpectedError(original)});
	});
	if (publicAdapter || !sameError(publicAdapter.error(), original))
		return 18;
	const auto returned = application->runFrame([original](FlowUi::UiManager&, FlowUi::WindowId) -> FlowUi::Status {
		return FlowUi::unexpectedError(original);
	});
	if (returned || !sameError(returned.error(), original))
		return 16;
	return application->runFrame([](FlowUi::UiManager&, FlowUi::WindowId) {}) ? 0 : 15;
}
} // namespace

int main(int argumentCount, char** arguments) {
	if (argumentCount == 2 && std::string_view(arguments[1]) == "--live")
		return liveFrameBoundaries();
	FlowUi::App app;
	const auto fonts = app.fonts();
	const auto ui = app.ui(FlowUi::InvalidWindowId);
	const auto closed = app.shouldClose();
	const auto clipboard = app.clipboardText();
	const auto title = app.setWindowTitle("unavailable");
	const auto frame = app.beginFrame();
	const auto managed = app.dispatchManagedWindows();
	if (fonts || ui || closed || clipboard || title || frame || managed)
		return 1;
	if (fonts.error().code != FlowUi::ErrorCode::AppUnavailable ||
		ui.error().code != FlowUi::ErrorCode::AppUnavailable ||
		closed.error().code != FlowUi::ErrorCode::AppUnavailable ||
		clipboard.error().code != FlowUi::ErrorCode::AppUnavailable ||
		title.error().code != FlowUi::ErrorCode::AppUnavailable ||
		frame.error().code != FlowUi::ErrorCode::AppUnavailable ||
		managed.error().code != FlowUi::ErrorCode::AppUnavailable)
		return 2;
	bool invoked = false;
	const FlowUi::UiBuildCallback callback = [&](FlowUi::UiManager&, FlowUi::WindowId) { invoked = true; };
	const auto built = app.runFrame(callback);
	if (built || invoked || built.error().code != FlowUi::ErrorCode::AppUnavailable)
		return 3;
	const auto copyError = FlowUi::makeError(FlowUi::ErrorCode::AssetNotFound,
		FlowUi::ErrorSite::ResourceLocate, 91, 37, 23);
	const CopyFailureCallback copyFailure(copyError);
	const auto copied = app.runFrame(copyFailure);
	if (copied || !sameError(copied.error(), copyError))
		return 6;
	const CopyFailureParameters copiedParameters(copyError);
	const auto elementWindow = app.createWindow(FlowUi::WindowConfigOverrides{}, CopyFailureElement{}, copiedParameters);
	if (elementWindow || !sameError(elementWindow.error(), copyError))
		return 7;
	const CopyFailureConfigurator configurator(copyError);
	const auto configuredWindow = app.createWindow(FlowUi::WindowConfigOverrides{}, CopyFailureElement{}, configurator);
	if (configuredWindow || !sameError(configuredWindow.error(), copyError))
		return 8;
	const auto invalidColor = FlowUi::Flow_Color(std::string_view("#invalid"));
	if (invalidColor || invalidColor.error().code != FlowUi::ErrorCode::ColorInvalid)
		return 4;
	const auto validColor = FlowUi::Flow_Color(std::string_view("#102030ff"));
	if (!validColor || validColor->r != 16 || validColor->g != 32 || validColor->b != 48 || validColor->a != 255)
		return 5;
	return 0;
}
