#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

#define CLAY_IMPLEMENTATION
#include <clay.h>

namespace {
void require(bool condition, const char* message) {
	if (condition) return;
	std::cerr << message << '\n';
	std::exit(1);
}

void check_panel(float header_height, float content_height, bool bounded, int clip_count) {
	std::vector<std::byte> memory(Clay_MinMemorySize());
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory.size(), memory.data()),
		{320, 300}, {});
	Clay_BeginLayout();
	Clay_ElementDeclaration column{};
	column.layout.sizing = {CLAY_SIZING_FIXED(320), CLAY_SIZING_FIXED(300)};
	column.clip = {.horizontal = true, .vertical = true};
	CLAY(CLAY_ID("column"), column) {
		Clay_ElementDeclaration panel{};
		panel.layout.sizing = bounded
			? Clay_Sizing{CLAY_SIZING_PERCENT(1), CLAY_SIZING_PERCENT(1)}
			: Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
		panel.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(CLAY_ID("panel"), panel) {
			Clay_ElementDeclaration header{};
			header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(header_height)};
			CLAY(CLAY_ID("header"), header) {
				// Selector rows and preview controls register clips before the inspector.
				for (int clip_index = 0; clip_index < clip_count; ++clip_index) {
					Clay_ElementDeclaration clipped_control{};
					clipped_control.layout.sizing = {CLAY_SIZING_FIXED(1), CLAY_SIZING_FIXED(1)};
					clipped_control.clip.horizontal = true;
					clipped_control.clip.scrollInputDisabled = true;
					CLAY(CLAY_IDI("clipped-control", clip_index), clipped_control);
				}
			}
			Clay_ElementDeclaration scroll_region{};
			scroll_region.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
			scroll_region.clip.vertical = true;
			CLAY(CLAY_ID("scroll"), scroll_region) {
				Clay_ElementDeclaration content{};
				content.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(content_height)};
				CLAY(CLAY_ID("content"), content);
			}
		}
	}
	Clay_EndLayout(0.016f);
	require(Clay_GetElementData(CLAY_ID("panel")).boundingBox.height == 300,
		"Panel must remain bounded by the clipped column");
	const auto scroll = Clay_GetScrollContainerData(CLAY_ID("scroll"));
	require(scroll.found && scroll.scrollContainerDimensions.height == 300 - header_height,
		"Scroll region must fill the space below fixed headers");
	Clay_SetPointerState({100, header_height + 10}, false);
	Clay_UpdateScrollContainers(false, {0, -3}, 0.016f);
	require(content_height > 300 - header_height ? scroll.scrollPosition->y < 0
		: scroll.scrollPosition->y == 0, "Wheel must scroll only overflowing content");
	Clay_SetCurrentContext(nullptr);
}
} // namespace

int main(int argument_count, char**) {
	for (const float header_height : {126.0f, 150.0f}) {
		for (const float content_height : {50.0f, 900.0f}) {
			for (const int clip_count : {0, 150}) {
				check_panel(header_height, content_height, argument_count == 1, clip_count);
			}
		}
	}
}
