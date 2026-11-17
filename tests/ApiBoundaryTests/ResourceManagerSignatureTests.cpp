#include "../TestHarness.hpp"

#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/ViewPortManager.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

namespace {

using namespace FlowUi;

static_assert(noexcept(std::declval<const FontManager&>().getAtlasResource()));
static_assert(std::same_as<decltype(std::declval<const FontManager&>().getAtlasResource()),
	Result<std::reference_wrapper<const Font::AtlasArrayResource>>>);
static_assert(noexcept(Font::FontVariantData::kerningKey(65, 66)));
static_assert(noexcept(std::declval<const Font::FontVariantData&>().kerningAdvance(65, 66)));
static_assert(noexcept(std::declval<const Font::FontFaceData&>().defaultVariant()));

void verifyDetachedFontAtlas() {
	FontManager fonts;
	const auto atlas = fonts.getAtlasResource();
	FLOWUI_CHECK(!atlas);
	FLOWUI_CHECK(atlas.error().code == ErrorCode::ObjectNotInitialized);
	FLOWUI_CHECK(atlas.error().site == ErrorSite::FontLookupFamily);
}

void verifyBorrowedFontValues() {
	Font::FontVariantData variant;
	FLOWUI_CHECK(variant.kerningAdvance(65, 66) == 0);
	variant.kerningPairs.emplace(Font::FontVariantData::kerningKey(65, 66), -0.25f);
	FLOWUI_CHECK(variant.kerningAdvance(65, 66) == -0.25f);
	Font::FontFaceData face;
	FLOWUI_CHECK(face.defaultVariant() == nullptr);
	face.variants.emplace_back(std::move(variant));
	face.defaultVariantIndex = 0;
	FLOWUI_CHECK(face.defaultVariant() == std::addressof(face.variants.front()));
	face.defaultVariantIndex = 1;
	FLOWUI_CHECK(face.defaultVariant() == nullptr);
}

#if FLOWUI_PUBLIC_VULKAN_INTEROP
static_assert(!noexcept(std::declval<ViewPort&>().clearRenderCallback()));
static_assert(!noexcept(std::declval<ViewPort&>().setClearColor(0, 0, 0, 1)));
static_assert(!noexcept(std::declval<ViewPort&>().setClearEveryFrame(true)));
static_assert(!noexcept(std::declval<ViewPort&>().setRenderCallback(
	std::declval<const ViewPort::RenderCallback&>())));
static_assert(noexcept(std::declval<const ViewPort&>().getSize()));
static_assert(noexcept(std::declval<const ViewPort&>().clearEveryFrame()));
static_assert(noexcept(std::declval<const ViewPortManager&>().getVulkanInterop()));

struct CallableCopyFailure {
	int marker = 73;
};

struct ThrowingCallable {
	int* invocationCount;
	bool* failCopy;

	ThrowingCallable(int& invocations, bool& failure) noexcept
		: invocationCount(&invocations), failCopy(&failure) {}

	ThrowingCallable(const ThrowingCallable& source)
		: invocationCount(source.invocationCount), failCopy(source.failCopy) {
		if (*failCopy) throw CallableCopyFailure{};
	}

	void operator()(const ViewPortRenderContext&) const { ++*invocationCount; }
};

void verifyViewportConfigurationChannels() {
	ViewPort viewport;
	const auto checkDetached = [](Status status) {
		FLOWUI_CHECK(!status);
		FLOWUI_CHECK(status.error().code == ErrorCode::ViewportDetached);
		FLOWUI_CHECK(status.error().site == ErrorSite::ViewportConfigure);
	};
	checkDetached(viewport.clearRenderCallback());
	checkDetached(viewport.setClearColor(1, 0, 0, 1));
	checkDetached(viewport.setClearEveryFrame(false));
	FLOWUI_CHECK(viewport.clearEveryFrame());
	FLOWUI_CHECK(!viewport.hasRenderCallback());
	FLOWUI_CHECK(viewport.clearColor() == (std::array<float, 4>{0, 0, 0, 0}));

	int invocations = 0;
	bool failCopy = false;
	ThrowingCallable callable(invocations, failCopy);
	ViewPort::RenderCallback adapted(callable);
	failCopy = true;
	// A checked detached facade does not need to copy an already adapted callable.
	checkDetached(viewport.setRenderCallback(adapted));
	bool originalPayloadObserved = false;
	try {
		(void)viewport.setRenderCallback(callable);
	} catch (const CallableCopyFailure& failure) {
		originalPayloadObserved = failure.marker == 73;
	}
	FLOWUI_CHECK(originalPayloadObserved);
	FLOWUI_CHECK(invocations == 0);
	FLOWUI_CHECK(!viewport.hasRenderCallback());

	const auto nullPayload = viewport.setRenderCallback(std::shared_ptr<int>{},
		[](const ViewPortRenderContext&, int& value) { ++value; });
	FLOWUI_CHECK(!nullPayload);
	FLOWUI_CHECK(nullPayload.error().code == ErrorCode::ViewportConfigurationInvalid);

	ViewPortManager viewports;
	const auto interop = viewports.getVulkanInterop();
	FLOWUI_CHECK(!interop);
	FLOWUI_CHECK(interop.error().code == ErrorCode::ObjectNotInitialized);
	FLOWUI_CHECK(interop.error().site == ErrorSite::ViewportLookup);
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("detached font atlas keeps its checked R contract", verifyDetachedFontAtlas);
	runner.run("borrowed font values preserve ordinary absence outcomes", verifyBorrowedFontValues);
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	runner.run("viewport hybrid configuration preserves checked errors and original exceptions",
		verifyViewportConfigurationChannels);
#endif
	return runner.finish();
}
