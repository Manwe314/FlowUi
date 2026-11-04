#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>

// The repository's existing stb_image is public-domain/MIT licensed. Keep this
// decoder private so the offline format library never shares app decoder state.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include "stb_image.h"

namespace FlowUi::devSystems::agentic {
namespace {

[[nodiscard]] auto image_error(ErrorCode code) noexcept {
	return unexpectedError(makeError(code, ErrorSite::AgenticImage));
}

[[nodiscard]] bool valid_size(std::uint32_t width, std::uint32_t height,
							  std::uint64_t maximum_bytes) noexcept {
	return width && height && static_cast<std::uint64_t>(width) * height <= maximum_bytes / 3u &&
		   static_cast<std::uint64_t>(width) * height <=
			   std::numeric_limits<std::size_t>::max() / 3u;
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
	bytes.emplace_back(static_cast<std::uint8_t>(value >> 24u));
	bytes.emplace_back(static_cast<std::uint8_t>(value >> 16u));
	bytes.emplace_back(static_cast<std::uint8_t>(value >> 8u));
	bytes.emplace_back(static_cast<std::uint8_t>(value));
}

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
	std::uint32_t checksum = 0xffffffffu;
	for (const auto value : bytes) {
		checksum ^= value;
		for (unsigned bit = 0; bit < 8u; ++bit)
			checksum = (checksum >> 1u) ^ (0xedb88320u & (0u - (checksum & 1u)));
	}
	return ~checksum;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes) noexcept {
	return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
		   (static_cast<std::uint32_t>(bytes[1]) << 16u) |
		   (static_cast<std::uint32_t>(bytes[2]) << 8u) | bytes[3];
}

[[nodiscard]] bool valid_png_chunks(std::span<const std::uint8_t> bytes) noexcept {
	std::size_t cursor = 8u;
	bool first = true;
	bool image_data = false;
	while (cursor < bytes.size()) {
		if (bytes.size() - cursor < 12u)
			return false;
		const std::uint32_t length = read_u32(bytes.subspan(cursor, 4u));
		if (length > bytes.size() - cursor - 12u)
			return false;
		const auto chunk = bytes.subspan(cursor + 4u, static_cast<std::size_t>(length) + 4u);
		if (crc32(chunk) != read_u32(bytes.subspan(cursor + 8u + length, 4u)))
			return false;
		const bool header =
			chunk[0] == 'I' && chunk[1] == 'H' && chunk[2] == 'D' && chunk[3] == 'R';
		if (first != header || (header && length != 13u))
			return false;
		first = false;
		image_data = image_data ||
					 (chunk[0] == 'I' && chunk[1] == 'D' && chunk[2] == 'A' && chunk[3] == 'T');
		cursor += static_cast<std::size_t>(length) + 12u;
		if (chunk[0] == 'I' && chunk[1] == 'E' && chunk[2] == 'N' && chunk[3] == 'D')
			return !length && image_data && cursor == bytes.size();
	}
	return false;
}

void append_chunk(std::vector<std::uint8_t>& output, const std::array<char, 4>& type,
				  std::span<const std::uint8_t> payload) {
	append_u32(output, static_cast<std::uint32_t>(payload.size()));
	const std::size_t checksum_begin = output.size();
	for (const char character : type)
		output.emplace_back(static_cast<std::uint8_t>(character));
	output.insert(output.end(), payload.begin(), payload.end());
	append_u32(output, crc32(std::span(output).subspan(checksum_begin)));
}

[[nodiscard]] bool whitespace(std::uint8_t character) noexcept {
	return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

[[nodiscard]] std::array<std::uint8_t, 7> label_glyph(char character) noexcept {
	if (character >= 'a' && character <= 'z')
		character = static_cast<char>(character - 'a' + 'A');
	constexpr std::array<std::array<std::uint8_t, 7>, 36> glyphs{
		{{{14, 17, 19, 21, 25, 17, 14}}, {{4, 12, 4, 4, 4, 4, 14}},
		 {{14, 17, 1, 2, 4, 8, 31}},	 {{30, 1, 1, 14, 1, 1, 30}},
		 {{2, 6, 10, 18, 31, 2, 2}},	 {{31, 16, 16, 30, 1, 1, 30}},
		 {{14, 16, 16, 30, 17, 17, 14}}, {{31, 1, 2, 4, 8, 8, 8}},
		 {{14, 17, 17, 14, 17, 17, 14}}, {{14, 17, 17, 15, 1, 1, 14}},
		 {{14, 17, 17, 31, 17, 17, 17}}, {{30, 17, 17, 30, 17, 17, 30}},
		 {{14, 17, 16, 16, 16, 17, 14}}, {{30, 17, 17, 17, 17, 17, 30}},
		 {{31, 16, 16, 30, 16, 16, 31}}, {{31, 16, 16, 30, 16, 16, 16}},
		 {{14, 17, 16, 23, 17, 17, 15}}, {{17, 17, 17, 31, 17, 17, 17}},
		 {{14, 4, 4, 4, 4, 4, 14}},		 {{7, 2, 2, 2, 18, 18, 12}},
		 {{17, 18, 20, 24, 20, 18, 17}}, {{16, 16, 16, 16, 16, 16, 31}},
		 {{17, 27, 21, 21, 17, 17, 17}}, {{17, 25, 21, 19, 17, 17, 17}},
		 {{14, 17, 17, 17, 17, 17, 14}}, {{30, 17, 17, 30, 16, 16, 16}},
		 {{14, 17, 17, 17, 21, 18, 13}}, {{30, 17, 17, 30, 20, 18, 17}},
		 {{15, 16, 16, 14, 1, 1, 30}},	 {{31, 4, 4, 4, 4, 4, 4}},
		 {{17, 17, 17, 17, 17, 17, 14}}, {{17, 17, 17, 17, 17, 10, 4}},
		 {{17, 17, 17, 21, 21, 21, 10}}, {{17, 17, 10, 4, 10, 17, 17}},
		 {{17, 17, 10, 4, 4, 4, 4}},	 {{31, 1, 2, 4, 8, 16, 31}}}};
	if (character >= '0' && character <= '9')
		return glyphs[static_cast<std::size_t>(character - '0')];
	if (character >= 'A' && character <= 'Z')
		return glyphs[10u + static_cast<std::size_t>(character - 'A')];
	if (character == '_')
		return {0, 0, 0, 0, 0, 0, 31};
	if (character == '-')
		return {0, 0, 0, 31, 0, 0, 0};
	if (character == '.')
		return {0, 0, 0, 0, 0, 12, 12};
	if (character == '#')
		return {10, 10, 31, 10, 31, 10, 10};
	return {0, 0, 0, 0, 0, 0, 0};
}

void draw_label(AgenticCompletedImage& image, std::string_view label, std::size_t x, std::size_t y,
				std::uint32_t maximum_width) noexcept {
	for (std::size_t character_index = 0;
		 character_index < label.size() && character_index * 6u + 5u <= maximum_width;
		 ++character_index) {
		const auto glyph = label_glyph(label[character_index]);
		for (std::size_t row = 0; row < 7u; ++row)
			for (std::size_t column = 0; column < 5u; ++column)
				if ((glyph[row] & (1u << (4u - column))) != 0u) {
					const std::size_t offset =
						((y + row) * image.width + x + character_index * 6u + column) * 3u;
					image.rgb[offset] = 240u;
					image.rgb[offset + 1u] = 240u;
					image.rgb[offset + 2u] = 240u;
				}
	}
}

[[nodiscard]] bool ppm_integer(std::span<const std::uint8_t> bytes, std::size_t& cursor,
							   std::uint32_t& value) noexcept {
	while (cursor < bytes.size()) {
		if (whitespace(bytes[cursor])) {
			++cursor;
		} else if (bytes[cursor] == '#') {
			while (cursor < bytes.size() && bytes[cursor] != '\n')
				++cursor;
		} else {
			break;
		}
	}
	const char* first = reinterpret_cast<const char*>(bytes.data() + cursor);
	const char* end = reinterpret_cast<const char*>(bytes.data() + bytes.size());
	const auto parsed = std::from_chars(first, end, value);
	if (parsed.ec != std::errc{} || parsed.ptr == first)
		return false;
	cursor += static_cast<std::size_t>(parsed.ptr - first);
	return true;
}

} // namespace

std::string theme_revision_artifact_id(std::uint64_t theme_revision,
									   std::uint64_t schema_generation) {
	return "theme_" + std::to_string(theme_revision) + "_" + std::to_string(schema_generation);
}

Json image_metadata(const AgenticCompletedImage& image) {
	Json metadata = Json::make_object(
		{{"schema", "FlowUi.Agentic.Image"},
		 {"schema_version", "1.0"},
		 {"run_id", image.run_id},
		 {"capture_id", image.capture_id},
		 {"image_id", image.image_id},
		 {"app_tick", decimal(image.app_tick)},
		 {"window_id", decimal(image.window_id)},
		 {"window_frame", decimal(image.window_frame)},
		 {"frame_slot", Json::number(image.frame_slot)},
		 {"source", image.source},
		 {"viewport_key", image.viewport_key.empty() ? Json{} : Json(image.viewport_key)},
		 {"path", "visual/images/" + image.image_id + ".png"},
		 {"source_ppm_artifact_id", image.image_id + "_ppm"},
		 {"width", Json::number(image.width)},
		 {"height", Json::number(image.height)},
		 {"origin", "top-left"},
		 {"alpha", "discarded"},
		 {"encoding", image.encoding},
		 {"source_vk_format", Json::number(image.source_format)},
		 {"source_color_space", image.color_space},
		 {"theme_revision", decimal(image.theme_revision)},
		 {"schema_generation", decimal(image.schema_generation)},
		 {"source_image_revision", decimal(image.source_image_revision)},
		 {"recorded_monotonic_ns", decimal(image.recorded_monotonic_ns)},
		 {"completed_monotonic_ns", decimal(image.completed_monotonic_ns)},
		 {"copy_duration_ns", decimal(image.copy_duration_ns)},
		 {"crop",
		  Json::make_object({{"x", Json::number(image.crop_x)},
							 {"y", Json::number(image.crop_y)},
							 {"original_width", Json::number(image.original_width)},
							 {"original_height", Json::number(image.original_height)},
							 {"logical_to_physical_scale_x",
							  Json(static_cast<double>(image.logical_to_physical_scale_x))},
							 {"logical_to_physical_scale_y",
							  Json(static_cast<double>(image.logical_to_physical_scale_y))}})}});
	metadata.set("submission",
				 image.submission_available
					 ? Json::make_object({{"device_id", decimal(image.device_id)},
										  {"queue_id", decimal(image.queue_id)},
										  {"serial", decimal(image.submission_serial)}})
					 : Json{});
	metadata.set("tree",
				 image.tree_available
					 ? Json::make_object(
						   {{"artifact_id", image.tree_artifact_id.empty()
												? "tree_" + std::to_string(image.window_id) + "_" +
													  std::to_string(image.tree_generation)
												: image.tree_artifact_id},
							{"generation", decimal(image.tree_generation)}})
					 : Json{});
	metadata.set(
		"producer_frame",
		image.producer_available
			? Json::make_object({{"run_id", image.run_id},
								 {"window_id", decimal(image.window_id)},
								 {"app_tick", decimal(image.producer_app_tick)},
								 {"window_frame", decimal(image.producer_window_frame)},
								 {"submission_serial", decimal(image.producer_submission_serial)},
								 {"source_image_revision", decimal(image.source_image_revision)},
								 {"device_id", decimal(image.device_id)},
								 {"queue_id", decimal(image.queue_id)}})
			: Json{});
	metadata.set("theme",
				 image.theme_available && !image.theme_artifact_id.empty()
					 ? Json::make_object({{"artifact_id", image.theme_artifact_id},
										  {"revision", decimal(image.theme_revision)},
										  {"schema_generation", decimal(image.schema_generation)}})
					 : Json{});
	if (image.source == "viewport")
		metadata.set(
			"sampling_relationship",
			Json::make_object(
				{{"relationship", "viewport_sampled_by_window"},
				 {"method", "explicit_identity"},
				 {"resolution", image.producer_available ? "resolved" : "producer_unavailable"},
				 {"same_frame",
				  image.producer_available && image.producer_window_frame == image.window_frame},
				 {"consumer_app_tick", decimal(image.app_tick)},
				 {"consumer_window_frame", decimal(image.window_frame)},
				 {"consumer_submission_serial", decimal(image.submission_serial)}}));
	return metadata;
}

namespace {
[[nodiscard]] bool unsigned_field(const Json& object, std::string_view key,
								  std::uint64_t& output) noexcept {
	const auto* field = object.find(key);
	if (!field)
		return false;
	const auto decoded = parse_decimal(*field);
	if (!decoded)
		return false;
	output = *decoded;
	return true;
}
[[nodiscard]] bool dimension_field(const Json& object, std::string_view key,
								   std::uint32_t& output) noexcept {
	const auto* field = object.find(key);
	if (!field || field->kind != Json::Kind::number)
		return false;
	const auto value = field->uint64_or(UINT64_MAX);
	if (value > UINT32_MAX)
		return false;
	output = static_cast<std::uint32_t>(value);
	return true;
}
[[nodiscard]] bool same_unsigned(const Json& object, std::string_view key,
								 std::uint64_t expected) noexcept {
	std::uint64_t value = 0;
	return unsigned_field(object, key, value) && value == expected;
}
[[nodiscard]] const Json* clay_node_at(const Json& nodes, std::uint32_t node_index) noexcept {
	const Json* result = nullptr;
	for (const auto& node : nodes.array) {
		std::uint32_t index = 0;
		if (dimension_field(node, "index", index) && index == node_index) {
			if (result)
				return nullptr;
			result = &node;
		}
	}
	return result;
}
struct ImageLogicalBounds {
	double left = 0, top = 0, right = 0, bottom = 0;
};
[[nodiscard]] bool logical_bounds(const Json& node, ImageLogicalBounds& output) noexcept {
	const auto* bounds = node.find("bounds");
	if (!bounds || bounds->kind != Json::Kind::object)
		return false;
	const auto coordinate = [&](std::string_view key) {
		const auto* value = bounds->find(key);
		return value && value->kind == Json::Kind::number ? value->number_or(NAN) : NAN;
	};
	const double width = coordinate("width"), height = coordinate("height");
	output.left = coordinate("x");
	output.top = coordinate("y");
	output.right = output.left + width;
	output.bottom = output.top + height;
	return std::isfinite(output.left) && std::isfinite(output.top) && std::isfinite(output.right) &&
		   std::isfinite(output.bottom) && width > 0 && height > 0;
}
} // namespace

Status restore_image_metadata(AgenticCompletedImage& image, const Json& metadata) noexcept {
	try {
		if (metadata.kind != Json::Kind::object || !metadata.find("schema") ||
			metadata.find("schema")->string_or() != "FlowUi.Agentic.Image" ||
			!metadata.find("schema_version") ||
			metadata.find("schema_version")->string_or() != "1.0")
			return image_error(ErrorCode::AgenticIntegrityFailed);
		AgenticCompletedImage restored;
		const auto copy_string = [&](std::string_view key, std::string& destination,
									 std::size_t maximum) {
			const auto* value = metadata.find(key);
			if (!value || value->kind != Json::Kind::string || value->text.empty() ||
				value->text.size() > maximum)
				return false;
			destination = value->text;
			return true;
		};
		if (!copy_string("run_id", restored.run_id, 128u) || !valid_identifier(restored.run_id) ||
			!copy_string("capture_id", restored.capture_id, 128u) ||
			!valid_identifier(restored.capture_id) ||
			!copy_string("image_id", restored.image_id, 128u) ||
			!valid_identifier(restored.image_id) || !copy_string("source", restored.source, 32u) ||
			!copy_string("encoding", restored.encoding, 128u) ||
			!copy_string("source_color_space", restored.color_space, 256u) ||
			(restored.source != "window" && restored.source != "viewport") ||
			!metadata.find("origin") || metadata.find("origin")->string_or() != "top-left" ||
			!metadata.find("alpha") || metadata.find("alpha")->string_or() != "discarded")
			return image_error(ErrorCode::AgenticIntegrityFailed);
		if (!dimension_field(metadata, "width", restored.width) ||
			!dimension_field(metadata, "height", restored.height) ||
			restored.width != image.width || restored.height != image.height ||
			!dimension_field(metadata, "frame_slot", restored.frame_slot) ||
			!dimension_field(metadata, "source_vk_format", restored.source_format))
			return image_error(ErrorCode::AgenticIntegrityFailed);
		std::uint64_t window_id = 0;
		if (!unsigned_field(metadata, "window_id", window_id) || window_id > UINT32_MAX ||
			!unsigned_field(metadata, "app_tick", restored.app_tick) ||
			!unsigned_field(metadata, "window_frame", restored.window_frame) ||
			!unsigned_field(metadata, "theme_revision", restored.theme_revision) ||
			!unsigned_field(metadata, "schema_generation", restored.schema_generation) ||
			!unsigned_field(metadata, "source_image_revision", restored.source_image_revision) ||
			!unsigned_field(metadata, "recorded_monotonic_ns", restored.recorded_monotonic_ns) ||
			!unsigned_field(metadata, "completed_monotonic_ns", restored.completed_monotonic_ns) ||
			!unsigned_field(metadata, "copy_duration_ns", restored.copy_duration_ns) ||
			restored.completed_monotonic_ns < restored.recorded_monotonic_ns ||
			restored.copy_duration_ns >
				restored.completed_monotonic_ns - restored.recorded_monotonic_ns)
			return image_error(ErrorCode::AgenticIntegrityFailed);
		restored.window_id = static_cast<std::uint32_t>(window_id);
		if (const auto* viewport = metadata.find("viewport_key");
			viewport && viewport->kind != Json::Kind::null) {
			if (viewport->kind != Json::Kind::string || viewport->text.size() > 4096u)
				return image_error(ErrorCode::AgenticIntegrityFailed);
			restored.viewport_key = viewport->text;
		}
		const auto* crop = metadata.find("crop");
		if (!crop || !dimension_field(*crop, "x", restored.crop_x) ||
			!dimension_field(*crop, "y", restored.crop_y) ||
			!dimension_field(*crop, "original_width", restored.original_width) ||
			!dimension_field(*crop, "original_height", restored.original_height) ||
			static_cast<std::uint64_t>(restored.crop_x) + restored.width >
				restored.original_width ||
			static_cast<std::uint64_t>(restored.crop_y) + restored.height >
				restored.original_height)
			return image_error(ErrorCode::AgenticIntegrityFailed);
		const auto* scale_x = crop->find("logical_to_physical_scale_x");
		const auto* scale_y = crop->find("logical_to_physical_scale_y");
		const double scale_x_value =
			scale_x && scale_x->kind == Json::Kind::number ? scale_x->number_or(NAN) : NAN;
		const double scale_y_value =
			scale_y && scale_y->kind == Json::Kind::number ? scale_y->number_or(NAN) : NAN;
		if (!std::isfinite(scale_x_value) || !std::isfinite(scale_y_value) || scale_x_value <= 0 ||
			scale_y_value <= 0 || scale_x_value > std::numeric_limits<float>::max() ||
			scale_y_value > std::numeric_limits<float>::max())
			return image_error(ErrorCode::AgenticIntegrityFailed);
		restored.logical_to_physical_scale_x = static_cast<float>(scale_x_value);
		restored.logical_to_physical_scale_y = static_cast<float>(scale_y_value);
		if (const auto* submission = metadata.find("submission");
			submission && submission->kind != Json::Kind::null) {
			if (!unsigned_field(*submission, "device_id", restored.device_id) ||
				!unsigned_field(*submission, "queue_id", restored.queue_id) ||
				!unsigned_field(*submission, "serial", restored.submission_serial))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			restored.submission_available = true;
		}
		if (const auto* tree = metadata.find("tree"); tree && tree->kind != Json::Kind::null) {
			const auto* artifact = tree->find("artifact_id");
			if (!unsigned_field(*tree, "generation", restored.tree_generation) || !artifact ||
				artifact->kind != Json::Kind::string || !valid_identifier(artifact->text))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			restored.tree_artifact_id = artifact->text;
			restored.tree_available = true;
		}
		if (const auto* theme = metadata.find("theme"); theme && theme->kind != Json::Kind::null) {
			const auto* artifact = theme->find("artifact_id");
			if (!same_unsigned(*theme, "revision", restored.theme_revision) ||
				!same_unsigned(*theme, "schema_generation", restored.schema_generation) ||
				!artifact || artifact->kind != Json::Kind::string ||
				!valid_identifier(artifact->text))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			restored.theme_artifact_id = artifact->text;
			restored.theme_available = true;
		}
		if (const auto* producer = metadata.find("producer_frame");
			producer && producer->kind != Json::Kind::null) {
			if (!producer->find("run_id") ||
				producer->find("run_id")->string_or() != restored.run_id ||
				!same_unsigned(*producer, "window_id", restored.window_id) ||
				!same_unsigned(*producer, "device_id", restored.device_id) ||
				!same_unsigned(*producer, "queue_id", restored.queue_id) ||
				!same_unsigned(*producer, "source_image_revision",
							   restored.source_image_revision) ||
				!unsigned_field(*producer, "app_tick", restored.producer_app_tick) ||
				!unsigned_field(*producer, "window_frame", restored.producer_window_frame) ||
				!unsigned_field(*producer, "submission_serial",
								restored.producer_submission_serial))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			restored.producer_available = true;
		}
		if (image.rgb.size() != static_cast<std::uint64_t>(restored.width) * restored.height * 3u)
			return image_error(ErrorCode::AgenticIntegrityFailed);
		restored.rgb = std::move(image.rgb);
		image = std::move(restored);
		return {};
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<std::uint32_t> select_flow_node(const Json& tree, const Json& selector) noexcept {
	try {
		const auto* nodes = tree.find("flow_nodes");
		if (!nodes || nodes->kind != Json::Kind::array)
			return image_error(ErrorCode::AgenticUnavailable);
		Json normalized = selector;
		if (selector.kind == Json::Kind::string) {
			if (selector.text.starts_with("node:")) {
				const auto parsed = parse_decimal(Json(std::string_view(selector.text).substr(5u)));
				if (!parsed || *parsed > UINT32_MAX)
					return image_error(ErrorCode::AgenticInvalidRequest);
				normalized = Json::make_object({{"node_index", Json::number(*parsed)}});
			} else if (selector.text.starts_with("instance:")) {
				normalized = Json::make_object({{"instance_id", selector.text.substr(9u)}});
			} else
				normalized = Json::make_object({{"debug_name", selector}});
		}
		if (normalized.kind != Json::Kind::object || normalized.object.size() != 1u)
			return image_error(ErrorCode::AgenticInvalidRequest);
		const auto& key = normalized.object.front().first;
		const auto& value = normalized.object.front().second;
		std::uint32_t requested_index = 0;
		if (key == "node_index") {
			if (!dimension_field(normalized, "node_index", requested_index))
				return image_error(ErrorCode::AgenticInvalidRequest);
		} else if (key == "instance_id") {
			if (!parse_decimal(value))
				return image_error(ErrorCode::AgenticInvalidRequest);
		} else if (key != "debug_name" || value.kind != Json::Kind::string || value.text.empty() ||
				   value.text.size() > 4096u)
			return image_error(ErrorCode::AgenticInvalidRequest);
		std::optional<std::uint32_t> selected;
		for (const auto& node : nodes->array) {
			std::uint32_t index = 0;
			if (!dimension_field(node, "index", index))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			const auto* field = node.find(key == "node_index" ? "index" : key);
			const bool matches = key == "node_index" ? index == requested_index
													 : field && field->kind == Json::Kind::string &&
														   field->text == value.text;
			if (matches) {
				if (selected)
					return image_error(ErrorCode::AgenticInvalidScope);
				selected = index;
			}
		}
		if (!selected)
			return image_error(ErrorCode::AgenticUnavailable);
		return *selected;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<AgenticImageCrop> flow_node_image_crop(const AgenticCompletedImage& image, const Json& tree,
											  const Json& selector) noexcept {
	if (image.source != "window")
		return image_error(ErrorCode::AgenticUnavailable);
	if (!tree.find("schema") || tree.find("schema")->string_or() != "FlowUi.Agentic.Tree" ||
		!tree.find("schema_version") || tree.find("schema_version")->string_or() != "1.0")
		return image_error(ErrorCode::AgenticIntegrityFailed);
	const auto valid = validate_image(image);
	if (!valid)
		return unexpectedError(valid.error());
	if (!image.tree_available || !tree.find("run_id") ||
		tree.find("run_id")->string_or() != image.run_id ||
		!same_unsigned(tree, "window_id", image.window_id) ||
		!same_unsigned(tree, "window_frame", image.window_frame) ||
		!same_unsigned(tree, "app_tick", image.app_tick) ||
		!same_unsigned(tree, "generation", image.tree_generation))
		return image_error(ErrorCode::AgenticStaleGeneration);
	if (!tree.find("complete") || !tree.find("complete")->bool_or() ||
		(tree.find("truncated") && tree.find("truncated")->bool_or()))
		return image_error(ErrorCode::AgenticIncompleteCapture);
	const auto selected = select_flow_node(tree, selector);
	if (!selected)
		return unexpectedError(selected.error());
	const auto* nodes = tree.find("flow_nodes");
	const auto* node = clay_node_at(*nodes, *selected);
	std::uint32_t root_index = 0;
	const auto* clay_nodes = tree.find("clay_nodes");
	if (!node || !dimension_field(*node, "clay_root", root_index) || !clay_nodes ||
		clay_nodes->kind != Json::Kind::array)
		return image_error(ErrorCode::AgenticUnavailable);
	const auto* root = clay_node_at(*clay_nodes, root_index);
	ImageLogicalBounds geometry;
	if (!root || !logical_bounds(*root, geometry))
		return image_error(ErrorCode::AgenticIncompleteCapture);
	if (root->find("flags") && (root->find("flags")->uint64_or() & 32u))
		return image_error(ErrorCode::AgenticIncompleteCapture);
	const Json* clipped = root;
	std::array<std::uint32_t, 64> visited{};
	std::size_t visited_count = 0;
	while (clipped) {
		std::uint32_t clip_id = 0;
		if (!dimension_field(*clipped, "clip_clay_id", clip_id))
			return image_error(ErrorCode::AgenticIntegrityFailed);
		if (!clip_id)
			break;
		if (visited_count == visited.size() ||
			std::find(visited.begin(), visited.begin() + static_cast<std::ptrdiff_t>(visited_count),
					  clip_id) != visited.begin() + static_cast<std::ptrdiff_t>(visited_count))
			return image_error(ErrorCode::AgenticIncompleteCapture);
		visited[visited_count++] = clip_id;
		const Json* clip_node = nullptr;
		for (const auto& candidate : clay_nodes->array) {
			std::uint32_t candidate_id = 0;
			if (dimension_field(candidate, "clay_id", candidate_id) && candidate_id == clip_id) {
				if (clip_node)
					return image_error(ErrorCode::AgenticInvalidScope);
				clip_node = &candidate;
			}
		}
		ImageLogicalBounds clip_bounds;
		const auto* axes = clip_node ? clip_node->find("clip_axes") : nullptr;
		if (!clip_node || !logical_bounds(*clip_node, clip_bounds) || !axes ||
			!axes->find("horizontal") || !axes->find("vertical") ||
			axes->find("horizontal")->kind != Json::Kind::boolean ||
			axes->find("vertical")->kind != Json::Kind::boolean)
			return image_error(ErrorCode::AgenticIncompleteCapture);
		if (clip_node->find("flags") && (clip_node->find("flags")->uint64_or() & 32u))
			return image_error(ErrorCode::AgenticIncompleteCapture);
		if (axes->find("horizontal")->boolean) {
			geometry.left = std::max(geometry.left, clip_bounds.left);
			geometry.right = std::min(geometry.right, clip_bounds.right);
		}
		if (axes->find("vertical")->boolean) {
			geometry.top = std::max(geometry.top, clip_bounds.top);
			geometry.bottom = std::min(geometry.bottom, clip_bounds.bottom);
		}
		clipped = clip_node;
	}
	const double physical_left = geometry.left * image.logical_to_physical_scale_x;
	const double physical_right = geometry.right * image.logical_to_physical_scale_x;
	const double physical_top = geometry.top * image.logical_to_physical_scale_y;
	const double physical_bottom = geometry.bottom * image.logical_to_physical_scale_y;
	if (!std::isfinite(physical_left) || !std::isfinite(physical_right) ||
		!std::isfinite(physical_top) || !std::isfinite(physical_bottom))
		return image_error(ErrorCode::AgenticInvalidScope);
	const double left = std::max<double>(image.crop_x, std::floor(physical_left));
	const double top = std::max<double>(image.crop_y, std::floor(physical_top));
	const double right = std::min<double>(static_cast<std::uint64_t>(image.crop_x) + image.width,
										  std::ceil(physical_right));
	const double bottom = std::min<double>(static_cast<std::uint64_t>(image.crop_y) + image.height,
										   std::ceil(physical_bottom));
	if (right <= left || bottom <= top)
		return image_error(ErrorCode::AgenticInvalidScope);
	return AgenticImageCrop{static_cast<std::uint32_t>(left - image.crop_x),
							static_cast<std::uint32_t>(top - image.crop_y),
							static_cast<std::uint32_t>(right - left),
							static_cast<std::uint32_t>(bottom - top)};
}

Status validate_image(const AgenticCompletedImage& image, std::uint64_t maximum_bytes) noexcept {
	if (!valid_size(image.width, image.height, maximum_bytes))
		return image_error(ErrorCode::AgenticBudgetExhausted);
	if (image.rgb.size() != static_cast<std::uint64_t>(image.width) * image.height * 3u ||
		!std::isfinite(image.logical_to_physical_scale_x) ||
		!std::isfinite(image.logical_to_physical_scale_y) ||
		image.logical_to_physical_scale_x <= 0.0f || image.logical_to_physical_scale_y <= 0.0f)
		return image_error(ErrorCode::AgenticInvalidRequest);
	return {};
}

Result<std::vector<std::uint8_t>> encode_ppm(const AgenticCompletedImage& image,
											 std::uint64_t maximum_bytes) noexcept {
	const Status valid = validate_image(image, maximum_bytes);
	if (!valid)
		return unexpectedError(valid.error());
	try {
		const std::string header =
			"P6\n" + std::to_string(image.width) + " " + std::to_string(image.height) + "\n255\n";
		if (image.rgb.size() > maximum_bytes || header.size() > maximum_bytes - image.rgb.size())
			return image_error(ErrorCode::AgenticBudgetExhausted);
		std::vector<std::uint8_t> output;
		output.reserve(header.size() + image.rgb.size());
		output.insert(output.end(), header.begin(), header.end());
		output.insert(output.end(), image.rgb.begin(), image.rgb.end());
		return output;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<std::vector<std::uint8_t>> encode_png(const AgenticCompletedImage& image,
											 std::uint64_t maximum_bytes) noexcept {
	const Status valid = validate_image(image, maximum_bytes);
	if (!valid)
		return unexpectedError(valid.error());
	try {
		const std::uint64_t raw_size = image.rgb.size() + static_cast<std::uint64_t>(image.height);
		const std::uint64_t block_count = (raw_size + 65534u) / 65535u;
		const std::uint64_t encoded_size = 63u + raw_size + block_count * 5u;
		if (encoded_size > maximum_bytes ||
			encoded_size > std::numeric_limits<std::size_t>::max() ||
			raw_size + block_count * 5u + 6u > std::numeric_limits<std::uint32_t>::max())
			return image_error(ErrorCode::AgenticBudgetExhausted);
		std::vector<std::uint8_t> raw;
		raw.reserve(static_cast<std::size_t>(raw_size));
		const std::size_t row_bytes = static_cast<std::size_t>(image.width) * 3u;
		for (std::uint32_t row = 0; row < image.height; ++row) {
			raw.emplace_back(0u); // PNG filter: none.
			const auto first = image.rgb.begin() + static_cast<std::ptrdiff_t>(row * row_bytes);
			raw.insert(raw.end(), first, first + static_cast<std::ptrdiff_t>(row_bytes));
		}
		std::vector<std::uint8_t> compressed;
		compressed.reserve(static_cast<std::size_t>(raw_size + block_count * 5u + 6u));
		compressed.emplace_back(0x78u);
		compressed.emplace_back(0x01u); // zlib, 32 KiB window, stored DEFLATE blocks.
		std::uint32_t adler_low = 1u;
		std::uint32_t adler_high = 0u;
		for (std::size_t offset = 0; offset < raw.size();) {
			const auto block_size =
				static_cast<std::uint16_t>(std::min<std::size_t>(65535u, raw.size() - offset));
			compressed.emplace_back(offset + block_size == raw.size() ? 1u : 0u);
			compressed.emplace_back(static_cast<std::uint8_t>(block_size));
			compressed.emplace_back(static_cast<std::uint8_t>(block_size >> 8u));
			const auto inverted = static_cast<std::uint16_t>(~block_size);
			compressed.emplace_back(static_cast<std::uint8_t>(inverted));
			compressed.emplace_back(static_cast<std::uint8_t>(inverted >> 8u));
			for (std::size_t index = 0; index < block_size; ++index) {
				const std::uint8_t value = raw[offset + index];
				compressed.emplace_back(value);
				adler_low = (adler_low + value) % 65521u;
				adler_high = (adler_high + adler_low) % 65521u;
			}
			offset += block_size;
		}
		append_u32(compressed, (adler_high << 16u) | adler_low);
		std::vector<std::uint8_t> output{137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
		output.reserve(static_cast<std::size_t>(encoded_size));
		std::vector<std::uint8_t> header;
		header.reserve(13u);
		append_u32(header, image.width);
		append_u32(header, image.height);
		header.insert(header.end(), {8u, 2u, 0u, 0u, 0u});
		append_chunk(output, {'I', 'H', 'D', 'R'}, header);
		append_chunk(output, {'I', 'D', 'A', 'T'}, compressed);
		append_chunk(output, {'I', 'E', 'N', 'D'}, {});
		return output;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<AgenticCompletedImage> decode_image(std::span<const std::uint8_t> bytes,
										   std::uint64_t maximum_bytes) noexcept {
	try {
		if (bytes.empty() || bytes.size() > maximum_bytes ||
			bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return image_error(ErrorCode::AgenticBudgetExhausted);
		AgenticCompletedImage image;
		if (bytes.size() >= 3u && bytes[0] == 'P' && bytes[1] == '6' && whitespace(bytes[2])) {
			std::size_t cursor = 2u;
			std::uint32_t maximum = 0;
			if (!ppm_integer(bytes, cursor, image.width) ||
				!ppm_integer(bytes, cursor, image.height) || !ppm_integer(bytes, cursor, maximum) ||
				maximum != 255u || cursor >= bytes.size() || !whitespace(bytes[cursor]))
				return image_error(ErrorCode::AgenticInvalidRequest);
			if (bytes[cursor] == '\r' && cursor + 1u < bytes.size() && bytes[cursor + 1u] == '\n')
				++cursor;
			++cursor;
			if (!valid_size(image.width, image.height, maximum_bytes))
				return image_error(ErrorCode::AgenticBudgetExhausted);
			if (bytes.size() - cursor !=
				static_cast<std::uint64_t>(image.width) * image.height * 3u)
				return image_error(ErrorCode::AgenticInvalidRequest);
			image.rgb.assign(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end());
		} else {
			constexpr std::array<std::uint8_t, 8> signature{137u, 80u, 78u, 71u,
															13u,  10u, 26u, 10u};
			if (bytes.size() < signature.size() ||
				!std::equal(signature.begin(), signature.end(), bytes.begin()))
				return image_error(ErrorCode::AssetFormatUnsupported);
			if (!valid_png_chunks(bytes))
				return image_error(ErrorCode::AgenticIntegrityFailed);
			int width = 0;
			int height = 0;
			int channels = 0;
			if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width,
									   &height, &channels) ||
				width <= 0 || height <= 0 ||
				!valid_size(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
							maximum_bytes))
				return image_error(ErrorCode::AgenticBudgetExhausted);
			using DecodedPixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
			DecodedPixels pixels(stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
													   &width, &height, &channels, 3),
								 &stbi_image_free);
			if (!pixels)
				return image_error(ErrorCode::AgenticIntegrityFailed);
			image.width = static_cast<std::uint32_t>(width);
			image.height = static_cast<std::uint32_t>(height);
			image.rgb.assign(pixels.get(),
							 pixels.get() + static_cast<std::size_t>(width) * height * 3u);
		}
		image.original_width = image.width;
		image.original_height = image.height;
		return image;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<AgenticCompletedImage> crop_image(const AgenticCompletedImage& image,
										 const AgenticImageCrop& crop,
										 std::uint64_t maximum_bytes) noexcept {
	const Status valid = validate_image(image, maximum_bytes);
	if (!valid)
		return unexpectedError(valid.error());
	if (!crop.width || !crop.height || crop.x >= image.width || crop.y >= image.height ||
		crop.width > image.width - crop.x || crop.height > image.height - crop.y)
		return image_error(ErrorCode::AgenticInvalidScope);
	try {
		AgenticCompletedImage output = image;
		output.rgb.resize(static_cast<std::size_t>(crop.width) * crop.height * 3u);
		output.width = crop.width;
		output.height = crop.height;
		output.original_width = image.original_width ? image.original_width : image.width;
		output.original_height = image.original_height ? image.original_height : image.height;
		if (crop.x > std::numeric_limits<std::uint32_t>::max() - output.crop_x ||
			crop.y > std::numeric_limits<std::uint32_t>::max() - output.crop_y)
			return image_error(ErrorCode::AgenticInvalidScope);
		output.crop_x += crop.x;
		output.crop_y += crop.y;
		for (std::uint32_t row = 0; row < crop.height; ++row) {
			const std::size_t source =
				(static_cast<std::size_t>(crop.y + row) * image.width + crop.x) * 3u;
			const std::size_t destination = static_cast<std::size_t>(row) * crop.width * 3u;
			std::copy_n(image.rgb.begin() + static_cast<std::ptrdiff_t>(source),
						static_cast<std::size_t>(crop.width) * 3u,
						output.rgb.begin() + static_cast<std::ptrdiff_t>(destination));
		}
		return output;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<AgenticCompletedImage> make_contact_sheet(std::span<const AgenticCompletedImage> images,
												 std::uint32_t columns, std::uint32_t padding,
												 std::uint64_t maximum_bytes) noexcept {
	if (images.empty() || images.size() > 256u || !columns || columns > images.size() ||
		padding > 1024u)
		return image_error(ErrorCode::AgenticInvalidRequest);
	std::uint32_t cell_width = 0;
	std::uint32_t cell_height = 0;
	for (const auto& image : images) {
		const Status valid = validate_image(image, maximum_bytes);
		if (!valid)
			return unexpectedError(valid.error());
		const std::size_t label_characters =
			image.image_id.empty() ? 4u : std::min<std::size_t>(image.image_id.size(), 32u);
		cell_width =
			std::max({cell_width, image.width, static_cast<std::uint32_t>(label_characters * 6u)});
		cell_height = std::max(cell_height, image.height);
	}
	const std::uint64_t rows = (images.size() + columns - 1u) / columns;
	const std::uint64_t width =
		static_cast<std::uint64_t>(columns) * (static_cast<std::uint64_t>(cell_width) + padding) +
		padding;
	constexpr std::uint32_t label_height = 9u;
	const std::uint64_t height =
		rows * (static_cast<std::uint64_t>(cell_height) + label_height + padding) + padding;
	if (width > std::numeric_limits<std::uint32_t>::max() ||
		height > std::numeric_limits<std::uint32_t>::max() ||
		!valid_size(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
					maximum_bytes))
		return image_error(ErrorCode::AgenticBudgetExhausted);
	try {
		AgenticCompletedImage output;
		output.source = "contact_sheet";
		output.width = static_cast<std::uint32_t>(width);
		output.height = static_cast<std::uint32_t>(height);
		output.original_width = output.width;
		output.original_height = output.height;
		output.rgb.assign(static_cast<std::size_t>(width * height * 3u), 32u);
		for (std::size_t index = 0; index < images.size(); ++index) {
			const auto& image = images[index];
			const std::size_t x =
				padding + (index % columns) * (static_cast<std::uint64_t>(cell_width) + padding);
			const std::size_t label_y =
				padding + (index / columns) *
							  (static_cast<std::uint64_t>(cell_height) + label_height + padding);
			const std::size_t y = label_y + label_height;
			const std::string label =
				image.image_id.empty() ? "#" + std::to_string(index) : image.image_id;
			draw_label(output, label, x, label_y, cell_width);
			for (std::uint32_t row = 0; row < image.height; ++row)
				std::copy_n(image.rgb.begin() +
								static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) *
															image.width * 3u),
							static_cast<std::size_t>(image.width) * 3u,
							output.rgb.begin() +
								static_cast<std::ptrdiff_t>(((y + row) * output.width + x) * 3u));
		}
		return output;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

Result<AgenticImageDifference> compare_images(const AgenticCompletedImage& baseline,
											  const AgenticCompletedImage& candidate,
											  std::uint8_t tolerance,
											  std::uint64_t maximum_bytes) noexcept {
	const Status baseline_valid = validate_image(baseline, maximum_bytes);
	if (!baseline_valid)
		return unexpectedError(baseline_valid.error());
	const Status candidate_valid = validate_image(candidate, maximum_bytes);
	if (!candidate_valid)
		return unexpectedError(candidate_valid.error());
	if (baseline.width != candidate.width || baseline.height != candidate.height ||
		baseline.encoding != candidate.encoding || baseline.color_space != candidate.color_space)
		return image_error(ErrorCode::AgenticIncompatibleComparison);
	try {
		AgenticImageDifference difference;
		difference.heatmap = candidate;
		difference.heatmap.source = "difference_heatmap";
		std::uint64_t total = 0;
		for (std::size_t pixel = 0; pixel < baseline.rgb.size() / 3u; ++pixel) {
			bool changed = false;
			for (std::size_t channel = 0; channel < 3u; ++channel) {
				const auto delta = static_cast<std::uint8_t>(
					std::abs(static_cast<int>(baseline.rgb[pixel * 3u + channel]) -
							 static_cast<int>(candidate.rgb[pixel * 3u + channel])));
				difference.maximum_channel_difference =
					std::max(difference.maximum_channel_difference, delta);
				difference.heatmap.rgb[pixel * 3u + channel] = delta;
				total += delta;
				changed = changed || delta > tolerance;
			}
			if (changed)
				++difference.changed_pixels;
		}
		difference.mean_absolute_channel_difference =
			static_cast<double>(total) / baseline.rgb.size();
		difference.changed_pixel_fraction = static_cast<double>(difference.changed_pixels) /
											(static_cast<double>(baseline.width) * baseline.height);
		return difference;
	} catch (...) {
		return image_error(ErrorCode::AllocationFailed);
	}
}

} // namespace FlowUi::devSystems::agentic
