#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityTargets.hpp"

namespace FlowUi::detail {
/* Evidence belongs to each selection so partial updates preserve earlier provenance. */
struct MemoryCapacityEvidence {
	std::string kind{"recording"}, source_name{};
	uint64_t dataset_id = 0, begin_ns = 0, end_ns = 0;
	uint64_t begin_tick = 0, end_tick_exclusive = 0;
	uint32_t sample_every_ticks = 1;
	bool complete = true, acknowledge_incomplete = false;
};
/* An initial hint in the registered target's units, never a runtime maximum. */
struct MemoryCapacitySelection {
	MemoryCapacityEvidence evidence{};
	std::string key{}, policy{"observed_maximum"};
	uint64_t value = 0, observed_maximum = 0, headroom = 0;
	double percentile = 100;
};
/* Optional v1 memory payload inside a v2 flowchanges envelope. */
struct MemoryCapacityManifest {
	std::vector<MemoryCapacitySelection> targets{};
	std::optional<float> growth_factor{};
	std::optional<bool> allow_runtime_growth{};
	bool enabled = true;
};
/* Strict decimal strings prevent JSON floating point loss for 64-bit capacities. */
[[nodiscard]] inline bool parse_capacity_unsigned(std::string_view value,
												  uint64_t& output) noexcept {
	if (value.empty() || value.find_first_not_of("0123456789") != std::string_view::npos)
		return false;
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), output);
	return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
/* Validate both active and disabled payloads before saving or compiling. */
[[nodiscard]] inline bool validate_memory_capacity_manifest(const MemoryCapacityManifest& section,
															std::string& error) {
	if (section.growth_factor &&
		(!std::isfinite(*section.growth_factor) || *section.growth_factor <= 1)) {
		error = "memoryCapacity.growthFactor must be finite and greater than one";
		return false;
	}
	for (size_t index = 0; index < section.targets.size(); ++index) {
		const auto& selection = section.targets[index];
		const auto* definition = find_initial_capacity(memory_capacity_target_id(selection.key));
		const auto fail = [&](std::string_view reason) {
			error = "memoryCapacity.targets[" + std::to_string(index) + "]." + selection.key +
					": " + std::string(reason);
			return false;
		};
		if (!definition || definition->key != selection.key)
			return fail("unknown or observation-only target");
		if (!valid_initial_capacity(*definition, selection.value))
			return fail("invalid range or alignment");
		for (size_t previous = 0; previous < index; ++previous)
			if (section.targets[previous].key == selection.key)
				return fail("duplicate target");
		if (selection.evidence.kind != "recording" && selection.evidence.kind != "live" &&
			selection.evidence.kind != "maximum")
			return fail("measured evidence required; synthetic previews cannot be exported");
		if (selection.evidence.sample_every_ticks == 0 ||
			selection.evidence.end_tick_exclusive < selection.evidence.begin_tick ||
			selection.evidence.end_ns < selection.evidence.begin_ns)
			return fail("invalid sampling interval or capture range");
		if (!selection.evidence.complete && !selection.evidence.acknowledge_incomplete)
			return fail("incomplete evidence requires acknowledgement");
		for (const char character : selection.evidence.source_name)
			if (static_cast<unsigned char>(character) < 32 && character != '\n' &&
				character != '\r' && character != '\t' && character != '\b' && character != '\f')
				return fail("unsupported control character in source name");
		if (selection.policy != "observed_maximum" && selection.policy != "percentile" &&
			selection.policy != "manual")
			return fail("unknown selection policy");
		if (selection.policy == "percentile" &&
			(selection.evidence.kind == "maximum" || !std::isfinite(selection.percentile) ||
			 selection.percentile < 0 || selection.percentile > 100))
			return fail("percentile requires a retained distribution and a finite percentile");
		if (selection.policy == "observed_maximum") {
			if (selection.headroom > UINT64_MAX - selection.observed_maximum)
				return fail("headroom overflow");
			const auto effective = selection.value ? selection.value : definition->default_value;
			if (effective < selection.observed_maximum + selection.headroom)
				return fail("initial value does not cover observed maximum plus headroom");
		}
	}
	return true;
}

/* Shared runtime/generator decoder; JSON adapters expose Kind, text, boolean, array and find. */
template <class Json>
[[nodiscard]] bool decode_memory_capacity_manifest(const Json& value,
												   MemoryCapacityManifest& output,
												   std::string& error) {
	using Kind = typename Json::Kind;
	error.clear();
	const auto fail = [&](std::string_view message) {
		if (error.empty())
			error = "memoryCapacity: " + std::string(message);
		return false;
	};
	const auto known_members = [&](const Json& object,
								   std::initializer_list<std::string_view> allowed,
								   std::string_view location) {
		for (const auto& member : object.object) {
			if (std::find(allowed.begin(), allowed.end(), member.first) == allowed.end()) {
				error = "memoryCapacity: unknown v1 " + std::string(location) + " member '" +
						member.first + "'; migrate the payload before rewriting";
				return false;
			}
		}
		return true;
	};
	const auto* version = value.find("version");
	const auto* enabled = value.find("enabled");
	const auto* targets = value.find("targets");
	if (value.kind != Kind::Object || !version || version->kind != Kind::Number ||
		version->text != "1" || !enabled || enabled->kind != Kind::Boolean || !targets ||
		targets->kind != Kind::Array)
		return fail("expected version 1, boolean enabled and targets array");
	if (!known_members(
			value,
			{"version", "enabled", "targets", "growthFactor", "allowRuntimeGrowth", "evidence"},
			"section"))
		return false;
	const auto known_evidence_members = [&](const Json& evidence) {
		return known_members(evidence,
							 {"kind", "sourceName", "datasetId", "beginNs", "endNs", "beginTick",
							  "endTickExclusive", "sampleEveryTicks", "complete",
							  "acknowledgeIncomplete"},
							 "evidence");
	};
	if (const auto* evidence = value.find("evidence");
		evidence && (evidence->kind != Kind::Object || !known_evidence_members(*evidence)))
		return fail("invalid legacy evidence");
	MemoryCapacityManifest decoded{};
	decoded.enabled = enabled->boolean;
	const auto unsigned_member = [&](const Json& object, std::string_view name,
									 uint64_t& destination, bool required) {
		const auto* member = object.find(name);
		return member ? member->kind == Kind::String &&
							parse_capacity_unsigned(member->text, destination)
					  : !required;
	};
	const auto decode_evidence = [&](const Json* evidence, MemoryCapacityEvidence& destination) {
		if (!evidence || evidence->kind != Kind::Object)
			return false;
		if (!known_evidence_members(*evidence))
			return false;
		const auto* kind = evidence->find("kind");
		const auto* complete = evidence->find("complete");
		const auto* cadence = evidence->find("sampleEveryTicks");
		uint64_t ticks = 0;
		if (!kind || kind->kind != Kind::String || !complete || complete->kind != Kind::Boolean ||
			!cadence || cadence->kind != Kind::Number ||
			!parse_capacity_unsigned(cadence->text, ticks) || ticks == 0 || ticks > UINT32_MAX)
			return false;
		destination.kind = kind->text;
		destination.complete = complete->boolean;
		destination.sample_every_ticks = static_cast<uint32_t>(ticks);
		if (const auto* acknowledgement = evidence->find("acknowledgeIncomplete")) {
			if (acknowledgement->kind != Kind::Boolean)
				return false;
			destination.acknowledge_incomplete = acknowledgement->boolean;
		}
		if (const auto* source = evidence->find("sourceName")) {
			if (source->kind != Kind::String)
				return false;
			destination.source_name = source->text;
		}
		if (!unsigned_member(*evidence, "datasetId", destination.dataset_id, false) ||
			!unsigned_member(*evidence, "beginNs", destination.begin_ns, false) ||
			!unsigned_member(*evidence, "endNs", destination.end_ns, false))
			return false;
		return unsigned_member(*evidence, "beginTick", destination.begin_tick, false) &&
			   unsigned_member(*evidence, "endTickExclusive", destination.end_tick_exclusive,
							   false);
	};
	for (const auto& item : targets->array) {
		MemoryCapacitySelection selection{};
		const auto* key = item.find("key");
		const auto* policy = item.find("policy");
		if (item.kind != Kind::Object || !key || key->kind != Kind::String || !policy ||
			policy->kind != Kind::String)
			return fail("target requires key and policy strings");
		if (!known_members(item,
						   {"key", "unit", "value", "policy", "observedMaximum", "headroom",
							"percentile", "evidence"},
						   "target"))
			return false;
		selection.key = key->text;
		selection.policy = policy->text;
		if (!unsigned_member(item, "value", selection.value, true) ||
			!unsigned_member(item, "observedMaximum", selection.observed_maximum, true) ||
			!unsigned_member(item, "headroom", selection.headroom, true))
			return fail("target values must be unsigned decimal strings");
		const auto* evidence = item.find("evidence");
		if (!evidence)
			evidence = value.find("evidence");
		if (!decode_evidence(evidence, selection.evidence))
			return fail("invalid target evidence");
		if (const auto* unit = item.find("unit")) {
			const auto* definition =
				find_initial_capacity(memory_capacity_target_id(selection.key));
			if (!definition || unit->kind != Kind::String ||
				unit->text !=
					(definition->unit == InitialCapacityUnit::Bytes ? "bytes" : "entries"))
				return fail("target unit mismatch");
		}
		if (const auto* percentile = item.find("percentile")) {
			if (percentile->kind != Kind::Number)
				return fail("percentile must be numeric");
			const auto parsed = std::from_chars(percentile->text.data(),
												percentile->text.data() + percentile->text.size(),
												selection.percentile);
			if (parsed.ec != std::errc{} ||
				parsed.ptr != percentile->text.data() + percentile->text.size())
				return fail("invalid percentile");
		}
		decoded.targets.emplace_back(std::move(selection));
	}
	if (const auto* factor = value.find("growthFactor")) {
		float number = 0;
		if (factor->kind != Kind::Number)
			return fail("growthFactor must be numeric");
		const auto parsed =
			std::from_chars(factor->text.data(), factor->text.data() + factor->text.size(), number);
		if (parsed.ec != std::errc{} || parsed.ptr != factor->text.data() + factor->text.size())
			return fail("invalid growthFactor");
		decoded.growth_factor = number;
	}
	if (const auto* growth = value.find("allowRuntimeGrowth")) {
		if (growth->kind != Kind::Boolean)
			return fail("allowRuntimeGrowth must be boolean");
		decoded.allow_runtime_growth = growth->boolean;
	}
	if (!validate_memory_capacity_manifest(decoded, error))
		return false;
	output = std::move(decoded);
	return true;
}
/* Quote source metadata; selection tokens are constrained by manifest validation. */
[[nodiscard]] inline std::string quote_capacity_json(std::string_view value) {
	std::string output = "\"";
	for (const char character : value) {
		switch (character) {
		case '"':
			output += "\\\"";
			break;
		case '\\':
			output += "\\\\";
			break;
		case '\n':
			output += "\\n";
			break;
		case '\r':
			output += "\\r";
			break;
		case '\t':
			output += "\\t";
			break;
		case '\b':
			output += "\\b";
			break;
		case '\f':
			output += "\\f";
			break;
		default:
			output += character;
			break;
		}
	}
	return output + '"';
}
/** Deterministic JSON; an indentation offset requests two-space pretty formatting. */
[[nodiscard]] inline std::string
encode_memory_capacity_manifest(const MemoryCapacityManifest& section,
								std::optional<size_t> indentation = std::nullopt) {
	std::string output = "{";
	const auto append_line = [&](size_t depth) {
		if (indentation) {
			output += '\n';
			output.append(*indentation + depth * 2, ' ');
		}
	};
	const auto append_member = [&](size_t depth, std::string_view name, std::string_view value,
								   bool first = false) {
		if (!first)
			output += ',';
		append_line(depth);
		output += quote_capacity_json(name);
		output += indentation ? ": " : ":";
		output += value;
	};
	append_member(1, "version", "1", true);
	append_member(1, "enabled", section.enabled ? "true" : "false");
	append_member(1, "targets", "[");
	auto selections = section.targets;
	std::sort(selections.begin(), selections.end(),
			  [](const auto& left, const auto& right) { return left.key < right.key; });
	for (size_t index = 0; index < selections.size(); ++index) {
		const auto& selection = selections[index];
		const auto& evidence = selection.evidence;
		if (index)
			output += ',';
		append_line(2);
		output += '{';
		const auto* definition = find_initial_capacity(memory_capacity_target_id(selection.key));
		append_member(3, "key", quote_capacity_json(selection.key), true);
		append_member(3, "unit",
					  definition->unit == InitialCapacityUnit::Bytes ? "\"bytes\"" : "\"entries\"");
		append_member(3, "value", quote_capacity_json(std::to_string(selection.value)));
		append_member(3, "policy", quote_capacity_json(selection.policy));
		append_member(3, "observedMaximum",
					  quote_capacity_json(std::to_string(selection.observed_maximum)));
		append_member(3, "headroom", quote_capacity_json(std::to_string(selection.headroom)));
		append_member(3, "percentile", std::to_string(selection.percentile));
		append_member(3, "evidence", "{");
		append_member(4, "kind", quote_capacity_json(evidence.kind), true);
		append_member(4, "complete", evidence.complete ? "true" : "false");
		append_member(4, "acknowledgeIncomplete",
					  evidence.acknowledge_incomplete ? "true" : "false");
		append_member(4, "sampleEveryTicks", std::to_string(evidence.sample_every_ticks));
		append_member(4, "sourceName", quote_capacity_json(evidence.source_name));
		append_member(4, "datasetId", quote_capacity_json(std::to_string(evidence.dataset_id)));
		append_member(4, "beginNs", quote_capacity_json(std::to_string(evidence.begin_ns)));
		append_member(4, "endNs", quote_capacity_json(std::to_string(evidence.end_ns)));
		append_member(4, "beginTick", quote_capacity_json(std::to_string(evidence.begin_tick)));
		append_member(4, "endTickExclusive",
					  quote_capacity_json(std::to_string(evidence.end_tick_exclusive)));
		append_line(3);
		output += '}';
		append_line(2);
		output += '}';
	}
	if (!selections.empty())
		append_line(1);
	output += ']';
	if (section.growth_factor) {
		char number[64]{};
		const auto written = std::to_chars(number, number + sizeof(number), *section.growth_factor);
		append_member(1, "growthFactor", std::string_view(number, written.ptr));
	}
	if (section.allow_runtime_growth)
		append_member(1, "allowRuntimeGrowth", *section.allow_runtime_growth ? "true" : "false");
	append_line(0);
	return output + '}';
}
} // namespace FlowUi::detail
