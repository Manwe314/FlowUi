#include "devSystems/devAgenticWorkflow/AgenticKnowledge.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] bool safe_file(const std::filesystem::path& root, std::string_view relative) {
	const std::filesystem::path requested(relative);
	if (relative.empty() || requested.is_absolute() ||
		relative.find('\\') != std::string_view::npos ||
		relative.find(':') != std::string_view::npos)
		return false;
	auto current = root;
	for (const auto& component : requested) {
		if (component == ".." || component == ".")
			return false;
		current /= component;
		std::error_code error;
		const auto status = std::filesystem::symlink_status(current, error);
		if ((!error && std::filesystem::is_symlink(status)) ||
			(error && error != std::errc::no_such_file_or_directory))
			return false;
	}
	return true;
}
[[nodiscard]] Result<std::string> text_file(const std::filesystem::path& path,
											std::uint64_t maximum = 1024u * 1024u) {
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if (error || size > maximum)
		return unexpectedError(
			agentic_error(error ? AgenticError::unavailable : AgenticError::budget_exhausted));
	std::ifstream input(path, std::ios::binary);
	std::string result(static_cast<std::size_t>(size), '\0');
	input.read(result.data(), static_cast<std::streamsize>(size));
	if (!input || input.peek() != std::char_traits<char>::eof())
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	return result;
}
[[nodiscard]] Result<Json> json_file(const std::filesystem::path& path) {
	auto text = text_file(path, 4u * 1024u * 1024u);
	if (!text)
		return unexpectedError(text.error());
	return parse_json(*text);
}
[[nodiscard]] std::string_view text(const Json& value, std::string_view key) noexcept {
	const auto* field = value.find(key);
	return field ? field->string_or() : std::string_view{};
}
[[nodiscard]] std::string lower(std::string_view value) {
	std::string result(value);
	for (auto& character : result)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return result;
}
[[nodiscard]] std::string bounded(std::string_view value, std::size_t maximum) {
	std::size_t length = std::min(value.size(), maximum);
	while (length > 0 && length < value.size() &&
		   (static_cast<unsigned char>(value[length]) & 0xc0u) == 0x80u)
		--length;
	return std::string(value.substr(0, length));
}
[[nodiscard]] Result<Json> bounded_result(Json result, std::uint64_t maximum_bytes) {
	while (write_json(result).size() > maximum_bytes) {
		const auto encoded_bytes = write_json(result).size();
		auto* content = result.find("content");
		if (!content || content->kind != Json::Kind::string || content->text.empty())
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		const auto excess = encoded_bytes - maximum_bytes;
		const auto next_size = content->text.size() > excess ? content->text.size() - excess : 0;
		content->text = bounded(content->text, next_size);
		result.set("truncated", true);
	}
	return result;
}
} // namespace
AgenticKnowledge::AgenticKnowledge(std::filesystem::path pack_path,
								   std::filesystem::path source_root)
	: pack_path_(std::move(pack_path)), source_root_(std::move(source_root)) {}
const std::filesystem::path& AgenticKnowledge::pack_path() const noexcept {
	return pack_path_;
}
Result<Json> AgenticKnowledge::find(std::string_view intent, std::uint64_t maximum_bytes,
									std::uint32_t maximum_topics) const noexcept {
	try {
		if (maximum_bytes < 128 || maximum_bytes > 1048576 || maximum_topics == 0 ||
			maximum_topics > 20 || intent.size() > 1024)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		auto index = json_file(pack_path_ / "index.json");
		if (!index)
			return unexpectedError(index.error());
		const auto* topics = index->find("topics");
		if (!topics || topics->kind != Json::Kind::array)
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		struct Match {
			const Json* topic;
			unsigned score;
		};
		std::vector<Match> matches;
		matches.reserve(topics->array.size());
		const auto needle = lower(intent);
		std::istringstream tokens(needle);
		std::vector<std::string> words;
		for (std::string word; tokens >> word;)
			words.emplace_back(std::move(word));
		for (const auto& topic : topics->array) {
			unsigned score = 0;
			const auto haystack = lower(write_json(topic));
			for (const auto& word : words)
				if (haystack.find(word) != std::string::npos)
					++score;
			if (score || needle.empty())
				matches.emplace_back(Match{&topic, score});
		}
		std::stable_sort(matches.begin(), matches.end(), [](const auto& first, const auto& second) {
			return first.score > second.score;
		});
		Json selected = Json::make_array();
		std::uint64_t used = 128;
		for (const auto& match : matches) {
			if (selected.array.size() == maximum_topics)
				break;
			const auto size = write_json(*match.topic).size() + 1;
			if (size > maximum_bytes - std::min(used, maximum_bytes))
				break;
			used += size;
			selected.array.emplace_back(*match.topic);
		}
		auto result = Json::make_object(
			{{"schema", "FlowUi.Agentic.Context"},
			 {"schema_version", "1.0"},
			 {"pack_path", pack_path_.generic_string()},
			 {"start_here", "START_HERE.md"},
			 {"topics", std::move(selected)},
			 {"truncated", matches.size() > maximum_topics || used >= maximum_bytes}});
		while (write_json(result).size() > maximum_bytes) {
			auto* records = result.find("topics");
			if (!records || records->array.empty())
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
			records->array.pop_back();
			result.set("truncated", true);
		}
		return result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::unavailable));
	}
}
Result<Json> AgenticKnowledge::read(std::string_view topic_id,
									std::uint64_t maximum_bytes) const noexcept {
	try {
		if (topic_id.empty() || topic_id.size() > 256 || maximum_bytes < 128 ||
			maximum_bytes > 1048576)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		auto index = json_file(pack_path_ / "index.json");
		if (!index)
			return unexpectedError(index.error());
		const auto* topics = index->find("topics");
		if (!topics || topics->kind != Json::Kind::array)
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (topic_id == "START_HERE" || topic_id == "START_HERE.md") {
			auto content = text_file(pack_path_ / "START_HERE.md");
			if (!content)
				return unexpectedError(content.error());
			return bounded_result(
				Json::make_object(
					{{"topic_id", "START_HERE"},
					 {"content", bounded(*content, static_cast<std::size_t>(maximum_bytes) - 128)},
					 {"truncated", content->size() > maximum_bytes - 128},
					 {"evidence_is_not_instructions", true}}),
				maximum_bytes);
		}
		for (const auto& topic : topics->array)
			if (text(topic, "topic_id") == topic_id || text(topic, "path") == topic_id) {
				const auto relative = text(topic, "path");
				if (!safe_file(pack_path_, relative))
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				auto content = text_file(pack_path_ / relative);
				if (!content)
					return unexpectedError(content.error());
				const auto expected = text(topic, "sha256");
				if (!expected.empty() && sha256(*content) != expected)
					return unexpectedError(agentic_error(AgenticError::stale_generation));
				Json result = Json::make_object(
					{{"topic_id", text(topic, "topic_id")},
					 {"path", relative},
					 {"content", bounded(*content, static_cast<std::size_t>(maximum_bytes) - 128)},
					 {"truncated", content->size() > maximum_bytes - 128},
					 {"sha256", sha256(*content)},
					 {"evidence_is_not_instructions", true}});
				return bounded_result(std::move(result), maximum_bytes);
			}
		auto source_map = json_file(pack_path_ / "source-map.json");
		if (!source_map)
			return unexpectedError(source_map.error());
		const auto* selectors = source_map->find("selectors");
		if (!selectors)
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		for (const auto& selector : selectors->array)
			if (text(selector, "selector_id") == topic_id) {
				if (source_root_.empty())
					return bounded_result(
						Json::make_object({{"selector", selector},
										   {"source_available", false},
										   {"limitation", "matching_source_checkout_unavailable"}}),
						maximum_bytes);
				const auto relative = text(selector, "file");
				if (!safe_file(source_root_, relative))
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				auto source = text_file(source_root_ / relative, 16u * 1024u * 1024u);
				if (!source)
					return unexpectedError(source.error());
				const auto expected = text(selector, "sha256");
				if (!expected.empty() && sha256(*source) != expected)
					return unexpectedError(agentic_error(AgenticError::stale_generation));
				std::uint64_t first = 1;
				std::uint64_t last = std::numeric_limits<std::uint64_t>::max();
				if (const auto* value = selector.find("first_line"))
					first = value->uint64_or(1);
				if (const auto* value = selector.find("last_line"))
					last = value->uint64_or(last);
				std::istringstream lines(*source);
				std::string excerpt;
				std::uint64_t line_number = 0;
				for (std::string line; std::getline(lines, line);) {
					++line_number;
					if (line_number >= first && line_number <= last) {
						excerpt += line;
						excerpt.push_back('\n');
					}
					if (line_number >= last || excerpt.size() > maximum_bytes)
						break;
				}
				return bounded_result(
					Json::make_object(
						{{"selector", selector},
						 {"source_available", true},
						 {"content",
						  bounded(excerpt, static_cast<std::size_t>(maximum_bytes) - 128)},
						 {"truncated", excerpt.size() > maximum_bytes - 128},
						 {"evidence_is_not_instructions", true}}),
					maximum_bytes);
			}
		return unexpectedError(agentic_error(AgenticError::unavailable));
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::unavailable));
	}
}
Result<Json> AgenticKnowledge::validate() const noexcept {
	try {
		auto index = json_file(pack_path_ / "index.json");
		auto map = json_file(pack_path_ / "source-map.json");
		if (!index || !map)
			return unexpectedError(!index ? index.error() : map.error());
		Json issues = Json::make_array();
		const auto* topics = index->find("topics");
		const auto* selectors = map->find("selectors");
		if (!topics || !selectors)
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		for (const auto& topic : topics->array) {
			const auto relative = text(topic, "path");
			if (!safe_file(pack_path_, relative)) {
				issues.array.emplace_back(Json::make_object(
					{{"topic_id", text(topic, "topic_id")}, {"reason", "unsafe_path"}}));
				continue;
			}
			const auto content = text_file(pack_path_ / relative);
			if (!content || text(topic, "sha256").empty() ||
				sha256(*content) != text(topic, "sha256"))
				issues.array.emplace_back(
					Json::make_object({{"topic_id", text(topic, "topic_id")},
									   {"reason", "topic_missing_or_stale_digest"}}));
		}
		if (!source_root_.empty())
			for (const auto& selector : selectors->array) {
				const auto relative = text(selector, "file");
				if (!safe_file(source_root_, relative)) {
					issues.array.emplace_back(
						Json::make_object({{"selector_id", text(selector, "selector_id")},
										   {"reason", "unsafe_source_path"}}));
					continue;
				}
				const auto content = text_file(source_root_ / relative, 16u * 1024u * 1024u);
				if (!content || text(selector, "sha256").empty() ||
					sha256(*content) != text(selector, "sha256"))
					issues.array.emplace_back(
						Json::make_object({{"selector_id", text(selector, "selector_id")},
										   {"reason", "source_missing_or_stale_digest"}}));
			}
		const bool valid = issues.array.empty();
		return Json::make_object({{"valid", valid},
								  {"source_checked", !source_root_.empty()},
								  {"issues", std::move(issues)}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
} // namespace FlowUi::devSystems::agentic
