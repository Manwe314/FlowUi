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

void check_workbench_grid(float subtree_content_height) {
	std::vector<std::byte> memory(Clay_MinMemorySize());
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory.size(), memory.data()),
		{800, 600}, {});
	Clay_BeginLayout();

	// 1. DevWorkbenchArea (clipped parent column)
	Clay_ElementDeclaration area{};
	area.layout.sizing = {CLAY_SIZING_FIXED(800), CLAY_SIZING_FIXED(600)};
	area.clip = {.horizontal = true, .vertical = true};
	CLAY(CLAY_ID("workbench-area"), area) {
		// 2. DevInspectWorkbench (percentage bounded)
		Clay_ElementDeclaration workbench{};
		workbench.layout.sizing = {CLAY_SIZING_PERCENT(1.0f), CLAY_SIZING_PERCENT(1.0f)};
		workbench.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(CLAY_ID("workbench"), workbench) {
			// Header (fixed 40px)
			Clay_ElementDeclaration header{};
			header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40.0f)};
			CLAY(CLAY_ID("header"), header);

			// Subtitle (fixed 24px)
			Clay_ElementDeclaration subtitle{};
			subtitle.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24.0f)};
			CLAY(CLAY_ID("subtitle"), subtitle);

			// Content (padding = 8, gap = 8)
			constexpr uint16_t padding = 8u;
			constexpr uint16_t gap = 8u;
			Clay_ElementDeclaration content{};
			content.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
			content.layout.padding = Clay_Padding{padding, padding, padding, padding};
			content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			content.layout.childGap = gap;
			CLAY(CLAY_ID("content"), content) {
				// Rows (each 50% height)
				Clay_ElementDeclaration row{};
				row.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(0.5f)};
				row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				row.layout.childGap = gap;

				CLAY(CLAY_ID("top-row"), row) {
					Clay_ElementDeclaration card{};
					card.layout.sizing = {CLAY_SIZING_PERCENT(0.5f), CLAY_SIZING_PERCENT(1.0f)};
					card.clip = {.horizontal = true, .vertical = true};
					CLAY(CLAY_ID("card-preview"), card);
					CLAY(CLAY_ID("card-overview"), card);
				}

				CLAY(CLAY_ID("bottom-row"), row) {
					Clay_ElementDeclaration card{};
					card.layout.sizing = {CLAY_SIZING_PERCENT(0.5f), CLAY_SIZING_PERCENT(1.0f)};
					card.clip = {.horizontal = true, .vertical = true};
					CLAY(CLAY_ID("card-changes"), card);

					CLAY(CLAY_ID("card-subtree"), card) {
						// DevClaySubTree container
						Clay_ElementDeclaration subtree_container{};
						subtree_container.layout.sizing = {
							CLAY_SIZING_PERCENT(1.0f), CLAY_SIZING_PERCENT(1.0f)};
						subtree_container.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
						CLAY(CLAY_ID("subtree-container"), subtree_container) {
							// Summary bar
							Clay_ElementDeclaration summary_bar{};
							summary_bar.layout.sizing = {
								CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32.0f)};
							CLAY(CLAY_ID("summary-bar"), summary_bar);

							// Tree list scroll container
							Clay_ElementDeclaration tree_list{};
							tree_list.layout.sizing = {
								CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
							tree_list.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
							const auto scroll = Clay_GetScrollContainerData(CLAY_ID("tree-list"));
							tree_list.clip = {
								.vertical = true,
								.childOffset = scroll.found && scroll.scrollPosition
									? *scroll.scrollPosition : Clay_Vector2{},
							};
							CLAY(CLAY_ID("tree-list"), tree_list) {
								Clay_ElementDeclaration tree_content{};
								tree_content.layout.sizing = {
									CLAY_SIZING_GROW(0),
									CLAY_SIZING_FIXED(subtree_content_height)};
								CLAY(CLAY_ID("tree-content"), tree_content);
							}
						}
					}
				}
			}
		}
	}
	Clay_EndLayout(0.016f);

	require(Clay_GetElementData(CLAY_ID("workbench")).boundingBox.height == 600,
		"Workbench must remain strictly bounded to 600px");
	const float expected_row_height = (600.0f - 64.0f - 16.0f - 8.0f) * 0.5f; // 256px
	require(Clay_GetElementData(CLAY_ID("bottom-row")).boundingBox.height == expected_row_height,
		"Bottom row must remain strictly bounded to 50% of available height");
	require(Clay_GetElementData(CLAY_ID("card-subtree")).boundingBox.height == expected_row_height,
		"Subtree card must match the row height");

	const auto scroll_data = Clay_GetScrollContainerData(CLAY_ID("tree-list"));
	require(scroll_data.found, "Tree list scroll container must be registered");
	const float expected_scroll_container_height = expected_row_height - 32.0f; // 224px
	require(scroll_data.scrollContainerDimensions.height == expected_scroll_container_height,
		"Tree list scroll container height must equal card height minus summary bar");

	// Simulate mouse wheel over tree list
	const auto list_box = Clay_GetElementData(CLAY_ID("tree-list")).boundingBox;
	Clay_SetPointerState({list_box.x + 10, list_box.y + 10}, false);
	Clay_UpdateScrollContainers(false, {0, -5}, 0.016f);
	if (subtree_content_height > expected_scroll_container_height) {
		require(scroll_data.scrollPosition->y < 0,
			"Wheel must vertically scroll tree when content is larger than container");
	} else {
		require(scroll_data.scrollPosition->y == 0,
			"Wheel must not scroll tree when content fits");
	}
	Clay_SetCurrentContext(nullptr);
}

void check_clay_workbench_grid(float dump_content_height, float code_content_height) {
	std::vector<std::byte> memory(Clay_MinMemorySize());
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory.size(), memory.data()),
		{800, 600}, {});
	Clay_BeginLayout();

	// DevWorkbenchArea
	Clay_ElementDeclaration area{};
	area.layout.sizing = {CLAY_SIZING_FIXED(800), CLAY_SIZING_FIXED(600)};
	area.clip = {.horizontal = true, .vertical = true};
	CLAY(CLAY_ID("workbench-area"), area) {
		// DevClayWorkbench root
		Clay_ElementDeclaration workbench{};
		workbench.layout.sizing = {CLAY_SIZING_PERCENT(1.0f), CLAY_SIZING_PERCENT(1.0f)};
		workbench.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(CLAY_ID("clay-workbench"), workbench) {
			// Header (fixed 48px)
			Clay_ElementDeclaration header{};
			header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48.0f)};
			CLAY(CLAY_ID("clay-wb-header"), header);

			// Body (padding = 14, gap = 12)
			constexpr uint16_t padding = 14u;
			constexpr uint16_t gap = 12u;
			Clay_ElementDeclaration body{};
			body.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
			body.layout.padding = Clay_Padding{padding, padding, padding, padding};
			body.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			body.layout.childGap = gap;
			CLAY(CLAY_ID("clay-wb-body"), body) {
				// Row 1 (50% height)
				Clay_ElementDeclaration top_row{};
				top_row.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(0.5f)};
				top_row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				top_row.layout.childGap = gap;
				CLAY(CLAY_ID("top-row"), top_row) {
					// Top-Left: Preview (50% width)
					Clay_ElementDeclaration preview_card{};
					preview_card.layout.sizing = {CLAY_SIZING_PERCENT(0.5f), CLAY_SIZING_PERCENT(1.0f)};
					preview_card.clip = {.horizontal = true, .vertical = true};
					CLAY(CLAY_ID("preview-card"), preview_card);

					// Top-Right: Data Dump (50% width)
					Clay_ElementDeclaration dump_card{};
					dump_card.layout.sizing = {CLAY_SIZING_PERCENT(0.5f), CLAY_SIZING_PERCENT(1.0f)};
					dump_card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
					CLAY(CLAY_ID("dump-card"), dump_card) {
						Clay_ElementDeclaration dump_header{};
						dump_header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32.0f)};
						CLAY(CLAY_ID("dump-header"), dump_header);

						Clay_ElementDeclaration dump_scroll{};
						dump_scroll.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
						dump_scroll.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
						const auto scroll = Clay_GetScrollContainerData(CLAY_ID("data-dump-scroll"));
						dump_scroll.clip = {
							.vertical = true,
							.childOffset = scroll.found && scroll.scrollPosition
								? *scroll.scrollPosition : Clay_Vector2{},
						};
						CLAY(CLAY_ID("data-dump-scroll"), dump_scroll) {
							Clay_ElementDeclaration dump_content{};
							dump_content.layout.sizing = {
								CLAY_SIZING_GROW(0),
								CLAY_SIZING_FIXED(dump_content_height)};
							CLAY(CLAY_ID("dump-content"), dump_content);
						}
					}
				}

				// Row 2 (50% height, single spanning element)
				Clay_ElementDeclaration bottom_row{};
				bottom_row.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(0.5f)};
				bottom_row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				CLAY(CLAY_ID("bottom-row"), bottom_row) {
					// Code Generator
					Clay_ElementDeclaration code_gen_card{};
					code_gen_card.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(1.0f)};
					code_gen_card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
					CLAY(CLAY_ID("code-gen-card"), code_gen_card) {
						Clay_ElementDeclaration code_header{};
						code_header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32.0f)};
						code_header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
						CLAY(CLAY_ID("code-header"), code_header) {
							Clay_ElementDeclaration title{};
							title.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
							CLAY(CLAY_ID("code-title"), title);

							Clay_ElementDeclaration copy_btn{};
							copy_btn.layout.sizing = {CLAY_SIZING_FIXED(60.0f), CLAY_SIZING_FIXED(22.0f)};
							CLAY(CLAY_ID("copy-btn"), copy_btn);
						}

						Clay_ElementDeclaration code_scroll{};
						code_scroll.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
						code_scroll.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
						const auto code_scroll_data = Clay_GetScrollContainerData(CLAY_ID("code-scroll"));
						code_scroll.clip = {
							.horizontal = true,
							.vertical = true,
							.childOffset = code_scroll_data.found && code_scroll_data.scrollPosition
								? *code_scroll_data.scrollPosition : Clay_Vector2{},
						};
						CLAY(CLAY_ID("code-scroll"), code_scroll) {
							Clay_ElementDeclaration code_body{};
							code_body.layout.sizing = {
								CLAY_SIZING_GROW(0),
								CLAY_SIZING_FIXED(code_content_height)};
							CLAY(CLAY_ID("code-body"), code_body);
						}
					}
				}
			}
		}
	}
	Clay_EndLayout(0.016f);

	require(Clay_GetElementData(CLAY_ID("clay-workbench")).boundingBox.height == 600,
		"Clay workbench must remain strictly bounded to 600px");
	const float expected_row_height = (600.0f - 48.0f - 28.0f - 12.0f) * 0.5f; // 256px
	require(Clay_GetElementData(CLAY_ID("top-row")).boundingBox.height == expected_row_height,
		"Clay workbench top row must remain strictly bounded to 50% height");
	require(Clay_GetElementData(CLAY_ID("bottom-row")).boundingBox.height == expected_row_height,
		"Clay workbench bottom row must remain strictly bounded to 50% height");

	// Verify Data Dump scroll container
	const auto dump_scroll_data = Clay_GetScrollContainerData(CLAY_ID("data-dump-scroll"));
	require(dump_scroll_data.found, "Data dump scroll container must be registered");
	const float expected_dump_scroll_height = expected_row_height - 32.0f; // 224px
	require(dump_scroll_data.scrollContainerDimensions.height == expected_dump_scroll_height,
		"Data dump scroll container height must match card minus header");

	// Verify Code Generator scroll container and copy button
	const auto code_scroll_data = Clay_GetScrollContainerData(CLAY_ID("code-scroll"));
	require(code_scroll_data.found, "Code scroll container must be registered");
	require(Clay_GetElementData(CLAY_ID("copy-btn")).boundingBox.width == 60.0f,
		"Copy button must be rendered with correct dimensions");

	// Test scroll interaction on data dump
	const auto dump_box = Clay_GetElementData(CLAY_ID("data-dump-scroll")).boundingBox;
	Clay_SetPointerState({dump_box.x + 10, dump_box.y + 10}, false);
	Clay_UpdateScrollContainers(false, {0, -5}, 0.016f);
	if (dump_content_height > expected_dump_scroll_height) {
		require(dump_scroll_data.scrollPosition->y < 0,
			"Wheel must vertically scroll data dump when content is larger than container");
	} else {
		require(dump_scroll_data.scrollPosition->y == 0,
			"Wheel must not scroll data dump when content fits");
	}

	Clay_SetCurrentContext(nullptr);
}

void check_clay_inspector_panel(float content_height) {
	std::vector<std::byte> memory(Clay_MinMemorySize());
	Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(memory.size(), memory.data()),
		{320, 500}, {});
	Clay_BeginLayout();

	// Inspector column
	Clay_ElementDeclaration column{};
	column.layout.sizing = {CLAY_SIZING_FIXED(320), CLAY_SIZING_FIXED(500)};
	column.clip = {.horizontal = true, .vertical = true};
	CLAY(CLAY_ID("inspector-column"), column) {
		// DevClayInspector root
		Clay_ElementDeclaration inspector{};
		inspector.layout.sizing = {CLAY_SIZING_PERCENT(1.0f), CLAY_SIZING_PERCENT(1.0f)};
		inspector.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		CLAY(CLAY_ID("clay-inspector"), inspector) {
			// Header
			Clay_ElementDeclaration header{};
			header.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36.0f)};
			CLAY(CLAY_ID("inspector-header"), header);

			// Content column
			Clay_ElementDeclaration content{};
			content.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
			content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			content.layout.childGap = 12u;
			CLAY(CLAY_ID("inspector-content"), content) {
				// Identity card
				Clay_ElementDeclaration identity_card{};
				identity_card.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)};
				CLAY(CLAY_ID("identity-card"), identity_card) {
					Clay_ElementDeclaration id_item{};
					id_item.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(20.0f)};
					CLAY(CLAY_ID("id-item"), id_item);
				}

				// Owner card
				Clay_ElementDeclaration owner_card{};
				owner_card.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)};
				CLAY(CLAY_ID("owner-card"), owner_card) {
					Clay_ElementDeclaration owner_item{};
					owner_item.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(content_height)};
					CLAY(CLAY_ID("owner-item"), owner_item);
				}
			}
		}
	}
	Clay_EndLayout(0.016f);

	require(Clay_GetElementData(CLAY_ID("clay-inspector")).boundingBox.width == 320,
		"Clay inspector must remain bounded to 320px width");
	require(Clay_GetElementData(CLAY_ID("inspector-header")).boundingBox.height == 36,
		"Clay inspector header must be fixed at 36px height");
	require(Clay_GetElementData(CLAY_ID("identity-card")).boundingBox.width == 320,
		"Identity card must span the full inspector width");
	require(Clay_GetElementData(CLAY_ID("owner-card")).boundingBox.width == 320,
		"Owner card must span the full inspector width");

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
	for (const float content_height : {100.0f, 600.0f, 1500.0f}) {
		check_workbench_grid(content_height);
	}
	for (const float dump_height : {100.0f, 400.0f, 1200.0f}) {
		for (const float code_height : {80.0f, 500.0f}) {
			check_clay_workbench_grid(dump_height, code_height);
		}
	}
	for (const float owner_height : {20.0f, 60.0f, 120.0f}) {
		check_clay_inspector_panel(owner_height);
	}
}
