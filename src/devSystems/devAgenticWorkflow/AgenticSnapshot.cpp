#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"
#include <algorithm>
#include <limits>

namespace FlowUi::devSystems::agentic {
Json to_json(const AgenticFrameRecord& frame) {
	auto output = Json::make_object(
		{{"schema", "FlowUi.Agentic.Frame"},
		 {"schema_version", agentic_schema_version},
		 {"run_id", frame.key.run_id},
		 {"app_tick", decimal(frame.key.app_tick)},
		 {"window_id", decimal(frame.key.window_id)},
		 {"window_frame", decimal(frame.key.window_frame)},
		 {"frame_slot", Json::number(frame.frame_slot)},
		 {"begin_ns", decimal(frame.begin_ns)},
		 {"end_ns", decimal(frame.end_ns)},
		 {"cpu_duration_ns", decimal(frame.cpu_duration_ns)},
		 {"width", Json::number(frame.width)},
		 {"height", Json::number(frame.height)},
		 {"observer_activity", Json::number(static_cast<std::uint32_t>(frame.observers))},
		 {"submitted", frame.submitted},
		 {"presented", frame.presented},
		 {"canceled", frame.canceled},
		 {"internal_window", frame.internal_window},
		 {"interrupted", frame.interrupted},
		 {"tree_generation", decimal(frame.tree_generation)}});
	if (frame.submitted) {
		output.set("submit_ns", decimal(frame.submit_ns));
		output.set("submission", Json::make_object({{"device_id", decimal(frame.device_id)},
													{"queue_id", decimal(frame.queue_id)},
													{"serial", decimal(frame.submission_serial)}}));
	}
	if (frame.presented)
		output.set("present_ns", decimal(frame.present_ns));
	if (frame.gpu_duration_available)
		output.set("gpu_duration_ns", decimal(frame.gpu_duration_ns));
	return output;
}
namespace {
[[nodiscard]] bool add_bytes(std::uint64_t& total, std::uint64_t bytes) noexcept {
	if (bytes > std::numeric_limits<std::uint64_t>::max() - total)
		return false;
	total += bytes;
	return true;
}
[[nodiscard]] bool json_bytes(const Json& value, std::uint64_t& total) noexcept {
	if (!add_bytes(total, value.text.capacity()) ||
		!add_bytes(total, value.array.capacity() * sizeof(Json)) ||
		!add_bytes(total, value.object.capacity() * sizeof(std::pair<std::string, Json>)))
		return false;
	for (const auto& member : value.array)
		if (!json_bytes(member, total))
			return false;
	for (const auto& member : value.object)
		if (!add_bytes(total, member.first.capacity()) || !json_bytes(member.second, total))
			return false;
	return true;
}
} // namespace
Result<std::uint64_t> snapshot_owned_bytes(const AgenticCaptureSnapshot& snapshot) noexcept {
	std::uint64_t total = sizeof(AgenticCaptureSnapshot);
	bool valid = add_bytes(total, snapshot.frames.capacity() * sizeof(AgenticFrameRecord)) &&
				 add_bytes(total, snapshot.images.capacity() * sizeof(AgenticCompletedImage)) &&
				 add_bytes(total, snapshot.artifacts.capacity() * sizeof(AgenticArtifactPayload));
	for (const auto& frame : snapshot.frames)
		valid = valid && add_bytes(total, frame.key.run_id.capacity());
	for (const auto& image : snapshot.images)
		valid =
			valid && add_bytes(total, image.rgb.capacity()) &&
			add_bytes(total, image.run_id.capacity() + image.capture_id.capacity() +
								 image.image_id.capacity() + image.source.capacity() +
								 image.viewport_key.capacity() + image.color_space.capacity() +
								 image.encoding.capacity() + image.theme_artifact_id.capacity());
	for (const auto& artifact : snapshot.artifacts)
		valid = valid && add_bytes(total, artifact.bytes.capacity()) &&
				add_bytes(total, artifact.artifact_id.capacity() + artifact.path.capacity() +
									 artifact.media_type.capacity() + artifact.schema.capacity() +
									 artifact.schema_version.capacity() +
									 artifact.integer_encoding.capacity());
	valid = valid && json_bytes(snapshot.provenance, total) &&
			json_bytes(snapshot.sources, total) && json_bytes(snapshot.correlations, total) &&
			json_bytes(snapshot.actions, total);
	valid =
		valid && add_bytes(total, snapshot.request.window_ids.capacity() * sizeof(std::uint64_t)) &&
		add_bytes(total,
				  snapshot.coverage.capacity() * sizeof(std::pair<std::string, SectionCoverage>));
	for (const auto& section : snapshot.coverage) {
		valid = valid &&
				add_bytes(total, section.first.capacity() +
									 section.second.covered_ranges.capacity() * sizeof(TickRange) +
									 section.second.gaps.capacity() * sizeof(TickRange) +
									 section.second.reasons.capacity() * sizeof(std::string));
		for (const auto& reason : section.second.reasons)
			valid = valid && add_bytes(total, reason.capacity());
	}
	for (const auto& limitation : snapshot.limitations)
		valid = valid && add_bytes(total, limitation.capacity());
	if (!valid)
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	return total;
}
Status add_text_artifact(AgenticCaptureSnapshot& snapshot, std::string artifact_id,
						 std::string path, std::string media_type, std::string schema,
						 std::string text, std::string schema_version,
						 std::string integer_encoding) noexcept {
	try {
		auto owned = snapshot_owned_bytes(snapshot);
		if (!owned)
			return unexpectedError(owned.error());
		std::uint64_t total_bytes = *owned;
		for (const auto& artifact : snapshot.artifacts) {
			if (artifact.artifact_id == artifact_id || artifact.path == path)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		if (text.size() > snapshot.request.limits.maximum_artifact_bytes ||
			total_bytes > snapshot.request.limits.maximum_snapshot_bytes ||
			text.size() > snapshot.request.limits.maximum_snapshot_bytes - total_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		AgenticArtifactPayload artifact;
		artifact.artifact_id = std::move(artifact_id);
		artifact.path = std::move(path);
		artifact.media_type = std::move(media_type);
		artifact.schema = std::move(schema);
		artifact.schema_version = std::move(schema_version);
		artifact.integer_encoding = std::move(integer_encoding);
		artifact.bytes.assign(text.begin(), text.end());
		snapshot.artifacts.emplace_back(std::move(artifact));
		return {};
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}
} // namespace FlowUi::devSystems::agentic
