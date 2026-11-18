#include "../../TestHarness.hpp"
#include "FlowUi.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/IconManager.hpp"
#include "FlowUi/Public/managers/InputFieldManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
#include "RegisteredTextProvider.hpp"
#endif
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
#include "RegisteredFontProvider.hpp"
#endif
#if FLOWUI_CONFIG_CUSTOM_ICON_RASTER_PROVIDER
#include "RegisteredIconProvider.hpp"
#endif
#include <array>
#include <fstream>
#include <iostream>

int main(int argumentCount, char** arguments) {
	if (argumentCount != 1 &&
		(argumentCount != 3 || std::string_view(arguments[1]) != "--write-font"))
		return 2;
	using namespace FlowUi;
	AppConfig configuration;
	configuration.window.width = 480;
	configuration.window.height = 320;
	configuration.resources.use_build_directory = false;
	std::vector<std::byte> ownedFont;
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
	test::DistinctFontMsdfProvider provider;
	const std::array source{std::byte{1}, std::byte{2}, std::byte{3}};
	auto baked = test::requireValue(provider.bake(
		{.sourceBytes = source, .faceName = "Fixture", .pixelSize = 18, .pixelRange = 6}));
	ownedFont = test::requireValue(provider.encodeBaked(baked.asset));
	if (argumentCount == 3) {
		std::ofstream output(arguments[2], std::ios::binary);
		output.write(reinterpret_cast<const char*>(ownedFont.data()),
					 static_cast<std::streamsize>(ownedFont.size()));
		return output ? 0 : 3;
	}
	configuration.ui.defaultFontFamily.name = "Independent fixture";
	configuration.ui.defaultFontFamily.faces = {FontFaceCreateInfo{.baked_data = ownedFont}};
#else
	if (argumentCount == 3)
		return 2;
#endif
	{
		auto application = test::requireValue(makeApplication(configuration));
		auto& fonts = test::requireValue(application.fonts()).get();
		FLOWUI_CHECK(fonts.getFontById(0) && fonts.getFontById(0)->defaultVariant());
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
		FLOWUI_CHECK(fonts.getFontById(0)->defaultVariant()->lineHeight == 1.4f);
		auto failedFace = configuration.ui.defaultFontFamily;
		failedFace.name = "Rejected output";
		test::DistinctFontMsdfProvider::invalidOutput = true;
		FLOWUI_CHECK(!fonts.createFamily(failedFace));
		test::DistinctFontMsdfProvider::invalidOutput = false;
		FLOWUI_CHECK(!fonts.getFontById(1));
		auto validFace = configuration.ui.defaultFontFamily;
		validFace.name = "Accepted after rollback";
		FLOWUI_CHECK(fonts.createFamily(validFace));
		FLOWUI_CHECK(fonts.getFontById(1));
#endif
		auto& icons = test::requireValue(application.icons()).get();
		FLOWUI_CHECK(test::requireValue(icons.registerSvg(
			"provider.svg",
			R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="#1177dd"/></svg>)")));
		const auto texture = test::requireValue(icons.textureRef("provider.svg"));
		constexpr FlowElementID editorId{0xF103u};
		constexpr std::string_view initialEditorText = "Provider editor AV original";
		constexpr std::string_view editedEditorText = "Provider editor AV replacement";
		std::uint64_t initialEditorRevision = 0;
		for (unsigned frameIndex = 0; frameIndex < 4; ++frameIndex) {
			FLOWUI_CHECK(application.pollEvents());
			FLOWUI_CHECK(application.beginFrame());
			auto& ui = test::requireValue(application.ui()).get();
			auto& inputFields = ui.inputFields();
			const auto editorContentId = CLAY_ID("Provider editor content");
			const auto editorTextId = CLAY_ID("Provider editor text");
			const FieldRequest editorRequest{
				.initialText = initialEditorText,
				.layout = {.fontId = 0,
						   .fontSize = static_cast<std::uint16_t>(frameIndex < 2 ? 18 : 19),
						   .viewportWidth = 360,
						   .viewportHeight = 60},
				.textElementId = editorTextId,
				.contentElementId = editorContentId,
			};
#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
			const auto editorCallsBefore =
				test::DeterministicTextLayoutProvider::calls.load(std::memory_order_relaxed);
#endif
			if (frameIndex == 1)
				FLOWUI_CHECK(inputFields.replaceText(editorId, editedEditorText));
			const auto editorField = inputFields.requestField(editorId, editorRequest);
			const auto expectedEditorText = frameIndex == 0 ? initialEditorText : editedEditorText;
			FLOWUI_CHECK(test::requireValue(editorField.text.copy()) == expectedEditorText);
			FLOWUI_CHECK(editorField.visibleLines.size() == 1);
			const auto& editorLine = editorField.visibleLines.front();
			FLOWUI_CHECK(editorLine.text == expectedEditorText);
			if (frameIndex == 0)
				initialEditorRevision = editorField.revision;
			else
				FLOWUI_CHECK(editorField.revision > initialEditorRevision);
#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
			if (frameIndex < 3)
				FLOWUI_CHECK(test::DeterministicTextLayoutProvider::calls.load(
								 std::memory_order_relaxed) > editorCallsBefore);
#endif
			CLAY(CLAY_ID("Provider surface"),
				 {.layout = {
					  .sizing = {.width = CLAY_SIZING_FIXED(400), .height = CLAY_SIZING_FIXED(220)},
					  .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
				CLAY_TEXT(CLAY_STRING("Provider integration AV"),
						  CLAY_TEXT_CONFIG(
							  {.textColor = {255, 255, 255, 255}, .fontId = 0, .fontSize = 18}));
				CLAY(editorContentId, {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(360),
															 .height = CLAY_SIZING_FIXED(60)}}}) {
					CLAY(editorTextId, editorLine.declaration) {
						CLAY_TEXT(ui.toClayString(editorLine.text),
								  CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255},
													.fontId = 0,
													.fontSize = editorRequest.layout.fontSize}));
					}
				}
				CLAY(CLAY_ID("Provider icon"),
					 {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(64),
											.height = CLAY_SIZING_FIXED(32)}},
					  .image = {.imageData = ui.imageData(texture)}}) {}
			}
			FLOWUI_CHECK(inputFields.submitTextSpan(
				editorField.field, {.logicalRange = editorLine.logicalRange,
									.textElementId = editorTextId,
									.visualLineIndex = editorLine.visualLineIndex}));
			FLOWUI_CHECK(inputFields.frameStatus());
			FLOWUI_CHECK(application.endFrame());
			FLOWUI_CHECK(application.drawFrame());
		}
#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
		FLOWUI_CHECK(test::DeterministicTextLayoutProvider::calls > 0);
#endif
#if FLOWUI_CONFIG_CUSTOM_FONT_MSDF_PROVIDER
		FLOWUI_CHECK(test::DistinctFontMsdfProvider::loadCalls >= 2);
#endif
#if FLOWUI_CONFIG_CUSTOM_ICON_RASTER_PROVIDER
		FLOWUI_CHECK(test::DistinctIconRasterProvider::parseCalls > 0);
		FLOWUI_CHECK(test::DistinctIconRasterProvider::rasterCalls > 0);
#endif
	}
#if FLOWUI_CONFIG_CUSTOM_ICON_RASTER_PROVIDER
	FLOWUI_CHECK(test::DistinctIconRasterProvider::liveDocuments == 0);
#endif
	std::cout << "Selected text/font/icon providers performed actual App, Clay measurement, editor "
				 "creation, editing and remeasurement, atlas upload, rendering and cleanup.\n";
}
