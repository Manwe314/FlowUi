#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"

#include <array>

namespace {
using namespace FlowUi::devSystems::agentic;

AgenticCompletedImage sample_image() {
	AgenticCompletedImage image;
	image.width = 3u;
	image.height = 2u;
	image.rgb = {255u, 0u,	0u,	 0u,  255u, 0u,	  0u,	0u,	  255u,
				 12u,  34u, 56u, 78u, 90u,	123u, 210u, 220u, 230u};
	image.run_id = "run_image";
	image.capture_id = "cap_image";
	image.window_id = 2u;
	image.window_frame = 9007199254740999u;
	image.original_width = image.width;
	image.original_height = image.height;
	image.logical_to_physical_scale_x = 2.0f;
	image.logical_to_physical_scale_y = 2.0f;
	return image;
}

void test_metadata_and_semantic_crops() {
	auto image = sample_image();
	image.image_id = "image_fixture";
	image.width = 100u;
	image.height = 80u;
	image.original_width = 100u;
	image.original_height = 80u;
	image.rgb.resize(100u * 80u * 3u, 100u);
	image.app_tick = 9007199254741001u;
	image.tree_generation = 93u;
	image.tree_available = true;
	image.tree_artifact_id = "tree_exact_fixture";
	image.logical_to_physical_scale_y = 1.5f;
	image.color_space = "srgb_nonlinear";
	image.source_format = 44u;
	image.device_id = 3u;
	image.queue_id = 8u;
	image.submission_serial = UINT64_MAX - 5u;
	image.submission_available = true;
	image.theme_revision = 99u;
	image.schema_generation = 7u;
	image.theme_artifact_id = "theme_99_7";
	image.theme_available = true;
	const auto png = encode_png(image);
	FLOWUI_CHECK(png);
	auto decoded = decode_image(*png);
	FLOWUI_CHECK(decoded);
	const auto metadata = image_metadata(image);
	FLOWUI_CHECK(restore_image_metadata(*decoded, metadata));
	FLOWUI_CHECK(decoded->window_frame == image.window_frame &&
				 decoded->app_tick == image.app_tick);
	FLOWUI_CHECK(decoded->submission_serial == UINT64_MAX - 5u);
	FLOWUI_CHECK(decoded->color_space == image.color_space &&
				 decoded->tree_artifact_id == image.tree_artifact_id);
	FLOWUI_CHECK(decoded->theme_available && decoded->theme_artifact_id == image.theme_artifact_id);
	FLOWUI_CHECK(decoded->logical_to_physical_scale_y == 1.5f);
	auto wrong_dimensions = metadata;
	wrong_dimensions.set("width", Json::number(101u));
	FLOWUI_CHECK(!restore_image_metadata(*decoded, wrong_dimensions));
	FLOWUI_CHECK(decoded->width == 100u && decoded->rgb == image.rgb);
	auto lossy_identifier = metadata;
	lossy_identifier.set("window_frame", Json::number(image.window_frame));
	FLOWUI_CHECK(!restore_image_metadata(*decoded, lossy_identifier));
	Json tree = Json::make_object(
		{{"schema", "FlowUi.Agentic.Tree"},
		 {"schema_version", "1.0"},
		 {"run_id", image.run_id},
		 {"window_id", decimal(image.window_id)},
		 {"window_frame", decimal(image.window_frame)},
		 {"app_tick", decimal(image.app_tick)},
		 {"generation", decimal(image.tree_generation)},
		 {"complete", true},
		 {"truncated", false},
		 {"flow_nodes",
		  Json::make_array({Json::make_object({{"index", Json::number(0u)},
											   {"instance_id", decimal(9007199254741013u)},
											   {"debug_name", "users_root"},
											   {"clay_root", Json::number(0u)}}),
							Json::make_object({{"index", Json::number(1u)},
											   {"instance_id", decimal(42u)},
											   {"debug_name", "row"},
											   {"clay_root", Json::number(1u)}})})},
		 {"clay_nodes",
		  Json::make_array(
			  {Json::make_object({{"index", Json::number(0u)},
								  {"clay_id", Json::number(10u)},
								  {"clip_clay_id", Json::number(20u)},
								  {"flags", Json::number(0u)},
								  {"bounds", Json::make_object({{"x", Json(-5.0)},
																{"y", Json(10.0)},
																{"width", Json(50.0)},
																{"height", Json(30.0)}})}}),
			   Json::make_object(
				   {{"index", Json::number(1u)},
					{"clay_id", Json::number(20u)},
					{"clip_clay_id", Json::number(0u)},
					{"flags", Json::number(0u)},
					{"clip_axes", Json::make_object({{"horizontal", true}, {"vertical", true}})},
					{"bounds", Json::make_object({{"x", Json(10.0)},
												  {"y", Json(0.0)},
												  {"width", Json(20.0)},
												  {"height", Json(40.0)}})}})})}});
	const auto rectangle = flow_node_image_crop(image, tree, Json("users_root"));
	FLOWUI_CHECK(rectangle && rectangle->x == 20u && rectangle->y == 15u &&
				 rectangle->width == 40u && rectangle->height == 45u);
	FLOWUI_CHECK(select_flow_node(tree, Json("instance:9007199254741013")).value() == 0u);
	const auto existing_crop = crop_image(image, {10u, 5u, 80u, 60u});
	FLOWUI_CHECK(existing_crop);
	const auto nested = flow_node_image_crop(*existing_crop, tree,
											 Json::make_object({{"node_index", Json::number(0u)}}));
	FLOWUI_CHECK(nested && nested->x == 10u && nested->y == 10u && nested->width == 40u &&
				 nested->height == 45u);
	tree.find("flow_nodes")->array[1].set("debug_name", "users_root");
	FLOWUI_CHECK(!select_flow_node(tree, Json("users_root")));
	FLOWUI_CHECK(select_flow_node(tree, Json("node:0")));
	tree.set("window_frame", decimal(image.window_frame + 1u));
	FLOWUI_CHECK(!flow_node_image_crop(image, tree, Json("node:0")));
	tree.set("window_frame", decimal(image.window_frame));
	image.source = "viewport";
	FLOWUI_CHECK(!flow_node_image_crop(image, tree, Json("node:0")));
	image.source = "window";
	tree.find("clay_nodes")->array[1].set("clip_clay_id", Json::number(20u));
	FLOWUI_CHECK(!flow_node_image_crop(image, tree, Json("node:0")));
}

void test_codecs_and_budgets() {
	const auto image = sample_image();
	const auto ppm = encode_ppm(image);
	FLOWUI_CHECK(ppm);
	const auto decoded_ppm = decode_image(*ppm);
	FLOWUI_CHECK(decoded_ppm && decoded_ppm->rgb == image.rgb);
	const auto png = encode_png(image);
	FLOWUI_CHECK(png);
	const auto decoded_png = decode_image(*png);
	FLOWUI_CHECK(decoded_png && decoded_png->rgb == image.rgb);
	FLOWUI_CHECK(decoded_png->width == 3u && decoded_png->height == 2u);
	auto damaged_png = *png;
	damaged_png[damaged_png.size() / 2u] ^= 1u;
	FLOWUI_CHECK(!decode_image(damaged_png));
	auto large = image;
	large.width = 256u;
	large.height = 256u;
	large.rgb.resize(256u * 256u * 3u, 125u);
	const auto large_png = encode_png(large);
	FLOWUI_CHECK(large_png);
	const auto large_decoded = decode_image(*large_png);
	FLOWUI_CHECK(large_decoded && large_decoded->rgb == large.rgb);
	FLOWUI_CHECK(!encode_png(image, 18u));
	FLOWUI_CHECK(!decode_image(*png, 8u));
	const std::array<std::uint8_t, 20> malformed{};
	FLOWUI_CHECK(!decode_image(malformed));
	auto invalid = image;
	invalid.rgb.pop_back();
	FLOWUI_CHECK(!validate_image(invalid));
}

void test_crops_differences_and_contact_sheet() {
	const auto image = sample_image();
	const auto cropped = crop_image(image, {1u, 1u, 2u, 1u});
	FLOWUI_CHECK(cropped &&
				 cropped->rgb == std::vector<std::uint8_t>({78u, 90u, 123u, 210u, 220u, 230u}));
	FLOWUI_CHECK(cropped->window_frame == image.window_frame && cropped->run_id == image.run_id);
	FLOWUI_CHECK(cropped->crop_x == 1u && cropped->crop_y == 1u);
	FLOWUI_CHECK(cropped->original_width == 3u && cropped->original_height == 2u);
	FLOWUI_CHECK(cropped->logical_to_physical_scale_x == 2.0f);
	FLOWUI_CHECK(!crop_image(image, {2u, 1u, 2u, 1u}));
	auto changed = image;
	changed.rgb[0] = 240u;
	const auto difference = compare_images(image, changed);
	FLOWUI_CHECK(difference && difference->changed_pixels == 1u);
	FLOWUI_CHECK(difference->maximum_channel_difference == 15u);
	FLOWUI_CHECK(difference->heatmap.rgb[0] == 15u);
	const auto tolerated = compare_images(image, changed, 15u);
	FLOWUI_CHECK(tolerated && tolerated->changed_pixels == 0u);
	changed.color_space = "linear";
	FLOWUI_CHECK(!compare_images(image, changed));
	const std::array images{image, image};
	const auto sheet = make_contact_sheet(images, 2u, 1u);
	FLOWUI_CHECK(sheet && sheet->width == 51u && sheet->height == 13u);
	FLOWUI_CHECK(encode_png(*sheet));
	FLOWUI_CHECK(!make_contact_sheet(images, 2u, 1u, 9u));
}
} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("bounded PPM/PNG codecs", test_codecs_and_budgets);
	runner.run("exact image crops, comparisons and contact sheets",
			   test_crops_differences_and_contact_sheet);
	runner.run("lossless image metadata, semantic node crops, DPI and clipping",
			   test_metadata_and_semantic_crops);
	return runner.finish();
}
