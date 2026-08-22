#include "TestHarness.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include "Ui/Vk_UiRenderer.hpp"
#include "internal/InputQueue.hpp"

namespace {

using FlowUi::detail::InputFieldFrameOverrides;
using FlowUi::detail::InputFieldRectOverride;
using FlowUi::detail::buildUiInstancesDirect;
using FlowUi::detail::growUiInstanceCapacity;
using FlowUi::detail::measureUiConversionCapacity;

Clay_RenderCommandArray commandArray(std::span<Clay_RenderCommand> commands) {
	return Clay_RenderCommandArray{
		.capacity = static_cast<int32_t>(commands.size()),
		.length = static_cast<int32_t>(commands.size()),
		.internalArray = commands.data(),
	};
}

void testBoundedDirectConversion() {
	std::array<Clay_RenderCommand, 4> commands{};
	commands[0].commandType = CLAY_RENDER_COMMAND_TYPE_RECTANGLE;
	commands[0].boundingBox = Clay_BoundingBox{10.0f, 20.0f, 30.0f, 40.0f};
	commands[0].renderData.rectangle.backgroundColor = Clay_Color{255.0f, 0.0f, 0.0f, 255.0f};
	commands[1].commandType = CLAY_RENDER_COMMAND_TYPE_SCISSOR_START;
	commands[1].boundingBox = Clay_BoundingBox{5.0f, 6.0f, 70.0f, 80.0f};
	commands[2].commandType = CLAY_RENDER_COMMAND_TYPE_BORDER;
	commands[2].boundingBox = Clay_BoundingBox{12.0f, 22.0f, 32.0f, 42.0f};
	commands[2].renderData.border.color = Clay_Color{0.0f, 255.0f, 0.0f, 255.0f};
	commands[3].commandType = CLAY_RENDER_COMMAND_TYPE_SCISSOR_END;

	InputFieldFrameOverrides overrides{};
	overrides.rects.push_back(InputFieldRectOverride{
		.insertBeforeCommandIndex = 0,
		.boundingBox = Clay_BoundingBox{1.0f, 2.0f, 3.0f, 4.0f},
		.color = Clay_Color{0.0f, 0.0f, 255.0f, 255.0f},
	});
	Clay_RenderCommandArray clayCommands = commandArray(commands);
	const auto capacity = measureUiConversionCapacity(clayCommands, overrides);
	FLOWUI_CHECK(capacity.instances == 3);
	FLOWUI_CHECK(capacity.runs == 3);
	FLOWUI_CHECK(capacity.scissorDepth == 5);

	std::array<UiInstance, 3> instances{};
	std::array<UiRun, 3> runs{};
	std::array<RectF, 5> scissors{};
	const auto built = buildUiInstancesDirect(
		clayCommands, overrides, VkExtent2D{200, 100}, nullptr, 1.0f, 2.0f, 3.0f,
		instances, runs, scissors);
	FLOWUI_CHECK(built.instanceCount == 3);
	FLOWUI_CHECK(built.runCount == 2);
	FLOWUI_CHECK(runs[0].type == UiType::Solid);
	FLOWUI_CHECK(runs[0].firstInstance == 0);
	FLOWUI_CHECK(runs[0].instanceCount == 2);
	FLOWUI_CHECK(runs[1].firstInstance == 2);
	FLOWUI_CHECK(runs[1].instanceCount == 1);
	FLOWUI_CHECK(runs[1].scissor.x == 10.0f);
	FLOWUI_CHECK(runs[1].scissor.y == 18.0f);
	FLOWUI_CHECK(instances[1].x == 20.0f);
	FLOWUI_CHECK(instances[1].y == 60.0f);

	std::array<UiInstance, 2> tooSmall{};
	FLOWUI_CHECK_THROWS(buildUiInstancesDirect(
		clayCommands, overrides, VkExtent2D{200, 100}, nullptr, 1.0f, 1.0f, 1.0f,
		tooSmall, runs, scissors));
}

void testTextFallbackAndEmptyImage() {
	Clay_RenderCommand text{};
	text.commandType = CLAY_RENDER_COMMAND_TYPE_TEXT;
	text.boundingBox = Clay_BoundingBox{2.0f, 3.0f, 20.0f, 10.0f};
	constexpr char bytes[] = "abc";
	text.renderData.text.stringContents = Clay_StringSlice{3, bytes};
	text.renderData.text.textColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
	std::array textCommands{text};
	Clay_RenderCommandArray textArray = commandArray(textCommands);
	InputFieldFrameOverrides overrides{};
	const auto textCapacity = measureUiConversionCapacity(textArray, overrides);
	std::array<UiInstance, 3> instances{};
	std::array<UiRun, 1> runs{};
	std::array<RectF, 2> scissors{};
	const auto textBuilt = buildUiInstancesDirect(
		textArray, overrides, VkExtent2D{64, 64}, nullptr, 1.0f, 1.0f, 1.0f,
		instances, runs, scissors);
	FLOWUI_CHECK(textCapacity.instances == 3);
	FLOWUI_CHECK(textBuilt.instanceCount == 1);
	FLOWUI_CHECK(textBuilt.runCount == 1);
	FLOWUI_CHECK(textBuilt.textGlyphCount == 1);
	FLOWUI_CHECK(instances[0].type == static_cast<uint32_t>(UiType::Msdf));

	Clay_RenderCommand image{};
	image.commandType = CLAY_RENDER_COMMAND_TYPE_IMAGE;
	image.boundingBox = Clay_BoundingBox{0.0f, 0.0f, 0.0f, 10.0f};
	std::array imageCommands{image};
	Clay_RenderCommandArray imageArray = commandArray(imageCommands);
	const auto imageBuilt = buildUiInstancesDirect(
		imageArray, overrides, VkExtent2D{64, 64}, nullptr, 1.0f, 1.0f, 1.0f,
		instances, runs, scissors);
	FLOWUI_CHECK(imageBuilt.instanceCount == 0);
	FLOWUI_CHECK(imageBuilt.runCount == 0);
	FLOWUI_CHECK(imageBuilt.imageCommandCount == 1);
}

void testCapacityGrowthAndInvalidInput() {
	constexpr uint64_t oneMiB = 1024ull * 1024ull;
	FLOWUI_CHECK(growUiInstanceCapacity(0, 1, oneMiB) == oneMiB);
	FLOWUI_CHECK(growUiInstanceCapacity(oneMiB, oneMiB, oneMiB) == oneMiB);
	FLOWUI_CHECK(growUiInstanceCapacity(oneMiB, oneMiB + 1, oneMiB) == oneMiB + oneMiB / 2);
	FLOWUI_CHECK(growUiInstanceCapacity(37, 0, oneMiB) == 37);
	FLOWUI_CHECK_THROWS(growUiInstanceCapacity(
		std::numeric_limits<uint64_t>::max() - 1u,
		std::numeric_limits<uint64_t>::max(),
		oneMiB));

	InputFieldFrameOverrides overrides{};
	Clay_RenderCommandArray invalid{.capacity = 0, .length = -1, .internalArray = nullptr};
	FLOWUI_CHECK_THROWS(measureUiConversionCapacity(invalid, overrides));

	Clay_RenderCommand overlay{};
	overlay.commandType = CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START;
	std::array overlayCommands{overlay};
	Clay_RenderCommandArray overlayArray = commandArray(overlayCommands);
	const auto overlayCapacity = measureUiConversionCapacity(overlayArray, overrides);
	FLOWUI_CHECK(overlayCapacity.instances == 0);
	FLOWUI_CHECK(overlayCapacity.runs == 0);
	std::array<RectF, 2> scissors{};
	const auto overlayBuilt = buildUiInstancesDirect(
		overlayArray, overrides, VkExtent2D{64, 64}, nullptr, 1.0f, 1.0f, 1.0f,
		std::span<UiInstance>{}, std::span<UiRun>{}, scissors);
	FLOWUI_CHECK(overlayBuilt.instanceCount == 0);
	FLOWUI_CHECK(overlayBuilt.runCount == 0);
}

void testLogicalTextureBindingConversion() {
	static_assert(std::is_trivially_copyable_v<FlowUi::TextureRef>);
	FlowUi::TextureRef texture{};
	texture.handle = FlowUi::TextureHandle{1u, 7u};
	texture.sourceWidth = 16;
	texture.sourceHeight = 16;
	Clay_RenderCommand image{};
	image.commandType = CLAY_RENDER_COMMAND_TYPE_IMAGE;
	image.boundingBox = Clay_BoundingBox{1.0f, 2.0f, 16.0f, 16.0f};
	image.renderData.image.imageData = &texture;
	std::array commands{image};
	InputFieldFrameOverrides overrides{};
	std::array<UiInstance, 1> instances{};
	std::array<UiRun, 1> runs{};
	std::array<RectF, 1> scissors{};
	std::array<FlowUi::detail::storage::BindingHotRecord, 2> bindings{};
	bindings[1] = FlowUi::detail::storage::BindingHotRecord{
		.textureGeneration = 7u,
		.descriptorIndex = 23u,
	};
	const auto built = buildUiInstancesDirect(
		commandArray(commands), overrides, VkExtent2D{64, 64}, nullptr,
		1.0f, 1.0f, 1.0f, instances, runs, scissors, bindings);
	FLOWUI_CHECK(built.instanceCount == 1u);
	FLOWUI_CHECK(instances[0].texIndex == 23u);

	bindings[1].textureGeneration = 8u;
	const auto stale = buildUiInstancesDirect(
		commandArray(commands), overrides, VkExtent2D{64, 64}, nullptr,
		1.0f, 1.0f, 1.0f, instances, runs, scissors, bindings);
	FLOWUI_CHECK(stale.instanceCount == 1u);
	FLOWUI_CHECK(instances[0].texIndex == 0u);

	texture.handle = {};
	texture.skipIfUnavailable = true;
	const auto skipped = buildUiInstancesDirect(
		commandArray(commands), overrides, VkExtent2D{64, 64}, nullptr,
		1.0f, 1.0f, 1.0f, instances, runs, scissors, bindings);
	FLOWUI_CHECK(skipped.instanceCount == 0u);
	FLOWUI_CHECK(skipped.imageCommandCount == 1u);
}

void testBoundedTextInputPolicies() {
	FlowUi::detail::InputQueue dropNewest(2u, FlowUi::InputQueueOverflowPolicy::DropNewest);
	dropNewest.pushChar(U'a');
	dropNewest.pushChar(U'b');
	dropNewest.pushChar(U'c');
	const FlowUi::FrameInput newestFrame = dropNewest.drain(0.01);
	FLOWUI_CHECK(newestFrame.text == std::vector<char32_t>({U'a', U'b'}));
	FLOWUI_CHECK(newestFrame.droppedTextInputCount == 1u);

	FlowUi::detail::InputQueue dropOldest(2u, FlowUi::InputQueueOverflowPolicy::DropOldest);
	dropOldest.pushChar(U'a');
	dropOldest.pushChar(U'b');
	dropOldest.pushChar(U'c');
	const FlowUi::FrameInput oldestFrame = dropOldest.drain(0.01);
	FLOWUI_CHECK(oldestFrame.text == std::vector<char32_t>({U'b', U'c'}));
	FLOWUI_CHECK(oldestFrame.droppedTextInputCount == 1u);

	dropOldest.pushChar(U'd');
	const FlowUi::FrameInput reusedFrame = dropOldest.drain(0.01);
	FLOWUI_CHECK(reusedFrame.text == std::vector<char32_t>({U'd'}));
	FLOWUI_CHECK(reusedFrame.droppedTextInputCount == 0u);
}

void testRendererHasNoVmaOwnership() {
	std::ifstream source(FLOWUI_UI_RENDERER_SOURCE, std::ios::binary);
	FLOWUI_CHECK(source.good());
	const std::string contents(
		(std::istreambuf_iterator<char>(source)),
		std::istreambuf_iterator<char>());
	FLOWUI_CHECK(contents.find("vmaCreate") == std::string::npos);
	FLOWUI_CHECK(contents.find("vmaDestroy") == std::string::npos);
	FLOWUI_CHECK(contents.find("VmaAllocation") == std::string::npos);
	FLOWUI_CHECK(contents.find("reserveTextureSlots") == std::string::npos);
	FLOWUI_CHECK(contents.find("uiTextureSlotInfos") == std::string::npos);
	FLOWUI_CHECK(contents.find("vkDeviceWaitIdle") == std::string::npos);

	for (const char* path : {
		FLOWUI_APP_SOURCE,
		FLOWUI_IMAGE_MANAGER_SOURCE,
		FLOWUI_ICON_MANAGER_SOURCE,
		FLOWUI_VIEWPORT_MANAGER_SOURCE,
	}) {
		std::ifstream managerSource(path, std::ios::binary);
		FLOWUI_CHECK(managerSource.good());
		const std::string managerContents(
			(std::istreambuf_iterator<char>(managerSource)),
			std::istreambuf_iterator<char>());
		FLOWUI_CHECK(managerContents.find("UiTextureRegistry") == std::string::npos);
		FLOWUI_CHECK(managerContents.find("slotId") == std::string::npos);
	}
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("bounded direct renderer conversion", testBoundedDirectConversion);
	runner.run("text fallback and empty image conversion", testTextFallbackAndEmptyImage);
	runner.run("instance capacity growth and invalid input", testCapacityGrowthAndInvalidInput);
	runner.run("logical texture binding direct conversion", testLogicalTextureBindingConversion);
	runner.run("bounded text input overflow policies", testBoundedTextInputPolicies);
	runner.run("renderer has no direct VMA ownership", testRendererHasNoVmaOwnership);
	return runner.finish();
}
