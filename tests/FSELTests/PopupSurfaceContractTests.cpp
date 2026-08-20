#include "TestHarness.hpp"

#include <type_traits>

#include <FSEL/PopupSurface.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::PopupSurface>);
static_assert(FlowUi::ConstructibleFlowElement<FlowUi::FSEL::PopupSurface>);
static_assert(!FlowUi::DrawableFlowElement<FlowUi::FSEL::PopupSurface>);
static_assert(!FlowUi::HasState<FlowUi::FSEL::PopupSurface>);
static_assert(std::is_empty_v<FlowUi::FSEL::PopupSurface>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("popup surface defaults to a themed fit-sized parent popup", [] {
		const FlowUi::FSEL::PopupSurfaceParameters parameters{};
		FLOWUI_CHECK(parameters.popupRequest.anchor.kind ==
			FlowUi::PopupAnchorKind::Parent);
		FLOWUI_CHECK(parameters.popupRequest.firstFrame ==
			FlowUi::PopupFirstFramePolicy::DeferUntilMeasured);
		FLOWUI_CHECK(parameters.popupRequest.outsidePress ==
			FlowUi::PopupOutsidePressPolicy::DismissAndBlockAnchor);
		FLOWUI_CHECK(parameters.popupRequest.dismissOnEscape);
		FLOWUI_CHECK(!parameters.onDismissed);
		FLOWUI_CHECK(!parameters.padding.has_value());
		FLOWUI_CHECK(!parameters.childGap.has_value());
		FLOWUI_CHECK(!parameters.backgroundColor.has_value());
		FLOWUI_CHECK(!parameters.borderColor.has_value());
	});

	runner.run("popup behavior and visual overrides remain independent", [] {
		FlowUi::FSEL::PopupSurfaceParameters parameters{};
		parameters.popupRequest.anchor = FlowUi::PopupAnchor::pointerFollow();
		parameters.popupRequest.layer = FlowUi::PopupLayer::CriticalPopup;
		parameters.popupRequest.expectedSize = Clay_Dimensions{320.0f, 80.0f};
		parameters.padding = CLAY_PADDING_ALL(18);
		parameters.backgroundColor = FlowUi::Flow_Color("#112233ff");

		FLOWUI_CHECK(parameters.popupRequest.anchor.kind ==
			FlowUi::PopupAnchorKind::PointerFollow);
		FLOWUI_CHECK(parameters.popupRequest.layer ==
			FlowUi::PopupLayer::CriticalPopup);
		FLOWUI_CHECK(parameters.popupRequest.expectedSize->width == 320.0f);
		FLOWUI_CHECK(parameters.padding->left == 18);
		FLOWUI_CHECK(parameters.backgroundColor->r == 17.0f);
	});

	return runner.finish();
}
