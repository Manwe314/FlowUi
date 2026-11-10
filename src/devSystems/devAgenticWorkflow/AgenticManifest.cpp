#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAnalysis.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include "devSystems/devAgenticWorkflow/schemas/v1/AgenticSchemaData.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] const Json* local_schema_reference(const Json& root, std::string_view reference) {
	if (reference == "#")
		return &root;
	if (!reference.starts_with("#/"))
		return nullptr;
	reference.remove_prefix(2);
	const Json* current = &root;
	while (true) {
		const auto separator = reference.find('/');
		const auto encoded = reference.substr(0, separator);
		std::string key;
		key.reserve(encoded.size());
		for (std::size_t index = 0; index < encoded.size(); ++index) {
			if (encoded[index] == '~') {
				if (++index >= encoded.size() || (encoded[index] != '0' && encoded[index] != '1'))
					return nullptr;
				key.push_back(encoded[index] == '0' ? '~' : '/');
			} else
				key.push_back(encoded[index]);
		}
		if (current->kind == Json::Kind::object)
			current = current->find(key);
		else if (current->kind == Json::Kind::array) {
			std::size_t index = 0;
			const auto parsed = std::from_chars(key.data(), key.data() + key.size(), index);
			if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() ||
				index >= current->array.size())
				return nullptr;
			current = &current->array[index];
		} else
			return nullptr;
		if (!current || separator == std::string_view::npos)
			return current;
		reference.remove_prefix(separator + 1);
	}
}

[[nodiscard]] bool schema_type_matches(const Json& value, std::string_view type) noexcept {
	return (type == "object" && value.kind == Json::Kind::object) ||
		   (type == "array" && value.kind == Json::Kind::array) ||
		   (type == "string" && value.kind == Json::Kind::string) ||
		   (type == "number" && value.kind == Json::Kind::number) ||
		   (type == "boolean" && value.kind == Json::Kind::boolean) ||
		   (type == "null" && value.kind == Json::Kind::null) ||
		   (type == "integer" && value.kind == Json::Kind::number &&
			std::isfinite(value.number_or(NAN)) &&
			std::floor(value.number_or()) == value.number_or());
}

[[nodiscard]] bool schema_matches(const Json& value, const Json& schema, unsigned depth = 0,
								  const Json* root_schema = nullptr) {
	if (depth > 64)
		return false;
	if (schema.kind == Json::Kind::boolean)
		return schema.boolean;
	if (schema.kind != Json::Kind::object)
		return false;
	if (!root_schema)
		root_schema = &schema;
	if (const auto* reference = schema.find("$ref")) {
		const auto* target = local_schema_reference(*root_schema, reference->string_or());
		if (!target || !schema_matches(value, *target, depth + 1, root_schema))
			return false;
	}
	for (const auto keyword : {"anyOf", "oneOf", "allOf"}) {
		if (const auto* alternatives = schema.find(keyword)) {
			if (alternatives->kind != Json::Kind::array || alternatives->array.empty())
				return false;
			std::size_t matched = 0;
			for (const auto& alternative : alternatives->array)
				if (schema_matches(value, alternative, depth + 1, root_schema))
					++matched;
			if ((keyword == std::string_view("anyOf") && matched == 0) ||
				(keyword == std::string_view("oneOf") && matched != 1) ||
				(keyword == std::string_view("allOf") && matched != alternatives->array.size()))
				return false;
		}
	}
	const auto* type = schema.find("type");
	if (type) {
		const bool matched =
			type->kind == Json::Kind::array
				? std::any_of(type->array.begin(), type->array.end(),
							  [&](const auto& candidate) {
								  return schema_type_matches(value, candidate.string_or());
							  })
				: schema_type_matches(value, type->string_or());
		if (!matched)
			return false;
	}
	if (const auto* constant = schema.find("const");
		constant && write_json(value) != write_json(*constant))
		return false;
	if (const auto* values = schema.find("enum")) {
		const auto encoded = write_json(value);
		if (std::none_of(values->array.begin(), values->array.end(),
						 [&](const auto& candidate) { return write_json(candidate) == encoded; }))
			return false;
	}
	if (const auto* inverse = schema.find("not");
		inverse && schema_matches(value, *inverse, depth + 1, root_schema))
		return false;
	if (value.kind == Json::Kind::string) {
		if (const auto* minimum = schema.find("minLength");
			minimum && value.text.size() < minimum->uint64_or())
			return false;
		if (const auto* maximum = schema.find("maxLength");
			maximum && value.text.size() > maximum->uint64_or())
			return false;
		if (const auto* pattern = schema.find("pattern")) {
			if (pattern->text == "^(0|[1-9][0-9]{0,19})$") {
				if (!parse_decimal(value))
					return false;
			} else {
				thread_local std::map<std::string, std::regex> patterns;
				auto found = patterns.find(pattern->text);
				if (found == patterns.end())
					found = patterns.emplace(pattern->text, std::regex(pattern->text)).first;
				if (!std::regex_search(value.text, found->second))
					return false;
			}
		}
		if (const auto* format = schema.find("format");
			format && format->string_or() == "date-time") {
			if (!std::regex_match(
					value.text,
					std::regex(
						R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?Z$)")))
				return false;
		}
	}
	if (value.kind == Json::Kind::number) {
		const auto numeric = value.number_or(NAN);
		if (!std::isfinite(numeric))
			return false;
		if (const auto* minimum = schema.find("minimum"); minimum && numeric < minimum->number_or())
			return false;
		if (const auto* minimum = schema.find("exclusiveMinimum");
			minimum && numeric <= minimum->number_or())
			return false;
		if (const auto* maximum = schema.find("maximum"); maximum && numeric > maximum->number_or())
			return false;
	}
	if (value.kind == Json::Kind::array) {
		if (const auto* maximum = schema.find("maxItems");
			maximum && value.array.size() > maximum->uint64_or())
			return false;
		if (const auto* minimum = schema.find("minItems");
			minimum && value.array.size() < minimum->uint64_or())
			return false;
		if (const auto* unique = schema.find("uniqueItems"); unique && unique->bool_or()) {
			std::set<std::string> observed;
			for (const auto& member : value.array)
				if (!observed.insert(write_json(member)).second)
					return false;
		}
		if (const auto* items = schema.find("items"))
			for (const auto& member : value.array)
				if (!schema_matches(member, *items, depth + 1, root_schema))
					return false;
	}
	if (value.kind == Json::Kind::object) {
		if (const auto* required = schema.find("required"))
			for (const auto& key : required->array)
				if (!value.find(key.string_or()))
					return false;
		const auto* properties = schema.find("properties");
		const auto* additional = schema.find("additionalProperties");
		for (const auto& member : value.object) {
			const auto* property = properties ? properties->find(member.first) : nullptr;
			if (property) {
				if (!schema_matches(member.second, *property, depth + 1, root_schema))
					return false;
			} else if (additional && additional->kind == Json::Kind::boolean &&
					   !additional->boolean)
				return false;
			else if (additional && additional->kind == Json::Kind::object &&
					 !schema_matches(member.second, *additional, depth + 1, root_schema))
				return false;
		}
	}
	return true;
}
[[nodiscard]] bool has_registered_schema(std::string_view schema_name) noexcept {
	return std::any_of(detail::schema_documents.begin(), detail::schema_documents.end(),
					   [&](const auto& schema) { return schema.first == schema_name; });
}
[[nodiscard]] bool valid_tree_references(const Json& tree) noexcept {
	const auto* flow_nodes = tree.find("flow_nodes");
	const auto* clay_nodes = tree.find("clay_nodes");
	if (!flow_nodes || flow_nodes->kind != Json::Kind::array)
		return false;
	const auto flow_count = flow_nodes->array.size();
	const auto clay_count = clay_nodes ? clay_nodes->array.size() : 0;
	const auto reference_valid = [](const Json* reference, std::size_t count) noexcept {
		return !reference || reference->kind == Json::Kind::null ||
			   reference->uint64_or(UINT64_MAX) < count;
	};
	for (const auto& forest : {flow_nodes, clay_nodes}) {
		if (!forest)
			continue;
		for (std::size_t index = 0; index < forest->array.size(); ++index) {
			const auto& node = forest->array[index];
			const auto* record_index = node.find("index");
			if (!record_index || record_index->uint64_or(UINT64_MAX) != index)
				return false;
			for (const auto key : {"parent", "first_child", "next_sibling"}) {
				const auto* reference = node.find(key);
				if (!reference_valid(reference, forest->array.size()) ||
					(reference && reference->kind != Json::Kind::null &&
					 reference->uint64_or() == index))
					return false;
			}
			const auto* subtree_end = node.find("subtree_end_exclusive");
			if (!subtree_end || subtree_end->uint64_or(UINT64_MAX) > forest->array.size())
				return false;
			if (const auto* complete = tree.find("complete");
				complete && complete->bool_or() && subtree_end->uint64_or() <= index)
				return false;
		}
	}
	for (const auto& node : flow_nodes->array) {
		if (!reference_valid(node.find("clay_root"), clay_count))
			return false;
		if (const auto* direct = node.find("direct_clay_nodes"))
			for (const auto& reference : direct->array)
				if (!reference_valid(&reference, clay_count))
					return false;
	}
	if (clay_nodes)
		for (const auto& node : clay_nodes->array)
			if (!reference_valid(node.find("flow_owner"), flow_count))
				return false;
	if (const auto* roots = tree.find("flow_roots"))
		for (const auto& reference : roots->array)
			if (!reference_valid(&reference, flow_count))
				return false;
	if (const auto* roots = tree.find("clay_roots"))
		for (const auto& root : roots->array)
			for (const auto key : {"node", "attachment_parent", "clip_node"})
				if (!reference_valid(root.find(key), clay_count))
					return false;
	return true;
}
[[nodiscard]] bool registered_schema_matches(const Json& document, std::string_view schema_name) {
	static const auto schemas = [] {
		std::vector<std::pair<std::string_view, Json>> result;
		result.reserve(detail::schema_documents.size());
		for (const auto& schema : detail::schema_documents) {
			auto parsed = parse_json(schema.second);
			if (parsed)
				result.emplace_back(schema.first, std::move(*parsed));
		}
		return result;
	}();
	for (const auto& schema : schemas)
		if (schema.first == schema_name)
			return schema_matches(document, schema.second);
	return true;
}

constexpr std::array<std::uint32_t, 64> round_constants{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
[[nodiscard]] bool safe_relative(std::string_view name) {
	if (name.empty() || name.size() > 1024 || name.find('\\') != std::string_view::npos ||
		name.find(':') != std::string_view::npos || name.find('\0') != std::string_view::npos)
		return false;
	const std::filesystem::path path(name);
	if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
		return false;
	for (const auto& component : path)
		if (component == "." || component == ".." || component.empty())
			return false;
	return true;
}
[[nodiscard]] bool confined(const std::filesystem::path& root,
							const std::filesystem::path& relative) {
	if (!safe_relative(relative.generic_string()))
		return false;
	auto current = root;
	for (const auto& component : relative) {
		current /= component;
		std::error_code error;
		const auto status = std::filesystem::symlink_status(current, error);
		if (!error && std::filesystem::is_symlink(status))
			return false;
		if (error && error != std::errc::no_such_file_or_directory)
			return false;
	}
	const auto canonical_root = std::filesystem::weakly_canonical(root);
	const auto canonical_target = std::filesystem::weakly_canonical(current);
	auto target_iterator = canonical_target.begin();
	for (const auto& component : canonical_root) {
		if (target_iterator == canonical_target.end() || *target_iterator != component)
			return false;
		++target_iterator;
	}
	return true;
}
[[nodiscard]] Result<std::string> read_text(const std::filesystem::path& path,
											std::uint64_t maximum_bytes) noexcept {
	try {
		std::error_code error;
		const auto size = std::filesystem::file_size(path, error);
		if (error || size > maximum_bytes ||
			size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
			return unexpectedError(
				agentic_error(error ? AgenticError::unavailable : AgenticError::budget_exhausted));
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		std::string result(static_cast<std::size_t>(size), '\0');
		stream.read(result.data(), static_cast<std::streamsize>(size));
		if (!stream || stream.peek() != std::char_traits<char>::eof())
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		return result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
[[nodiscard]] FlowUiError publication_io_error(int native_error = errno) noexcept {
	auto failure = agentic_error(AgenticError::publication_failed);
	failure.nativeCode = static_cast<std::uint32_t>(native_error);
	return failure;
}
[[nodiscard]] Status write_bytes(const std::filesystem::path& path,
								 std::span<const std::uint8_t> bytes) {
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
		return unexpectedError(publication_io_error());
	stream.write(reinterpret_cast<const char*>(bytes.data()),
				 static_cast<std::streamsize>(bytes.size()));
	stream.flush();
	if (!stream)
		return unexpectedError(publication_io_error());
	stream.close();
	if (stream.fail())
		return unexpectedError(publication_io_error());
	return {};
}
[[nodiscard]] Status write_text(const std::filesystem::path& path, std::string_view text) {
	return write_bytes(path, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}
[[nodiscard]] Status atomic_text(const std::filesystem::path& path, std::string_view text) {
	auto identifier = make_identifier("write");
	if (!identifier)
		return unexpectedError(identifier.error());
	const auto temporary = path.parent_path() / ("." + *identifier + ".pending");
	const auto written = write_text(temporary, text);
	if (!written)
		return written;
	std::error_code error;
#ifdef _WIN32
	if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		auto failure = agentic_error(AgenticError::publication_failed);
		failure.nativeCode = GetLastError();
		std::filesystem::remove(temporary, error);
		return unexpectedError(failure);
	}
#else
	std::filesystem::rename(temporary, path, error);
	if (error) {
		const auto failure = publication_io_error(error.value());
		std::filesystem::remove(temporary, error);
		return unexpectedError(failure);
	}
#endif
	return {};
}
struct StoreLock {
	std::filesystem::path path;
	bool acquired = false;
#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
	OVERLAPPED lock_region{};
#else
	int descriptor = -1;
#endif
	explicit StoreLock(const std::filesystem::path& root,
					   std::string_view lock_name = ".index.lock")
		: path(root / lock_name) {
#ifdef _WIN32
		handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
							 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
							 FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle != INVALID_HANDLE_VALUE)
			acquired = LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1,
								  0, &lock_region) != 0;
#else
		descriptor = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
		if (descriptor >= 0)
			acquired = ::flock(descriptor, LOCK_EX | LOCK_NB) == 0;
#endif
	}
	~StoreLock() {
#ifdef _WIN32
		if (handle != INVALID_HANDLE_VALUE) {
			if (acquired)
				UnlockFileEx(handle, 0, 1, 0, &lock_region);
			CloseHandle(handle);
		}
#else
		if (descriptor >= 0) {
			if (acquired)
				::flock(descriptor, LOCK_UN);
			::close(descriptor);
		}
#endif
	}
	StoreLock(const StoreLock&) = delete;
	StoreLock& operator=(const StoreLock&) = delete;
};

[[nodiscard]] const Json* nested(const Json& value, std::string_view first,
								 std::string_view second) noexcept {
	const auto* parent = value.find(first);
	return parent ? parent->find(second) : nullptr;
}
[[nodiscard]] std::string member_string(const Json& value, std::string_view key) {
	const auto* member = value.find(key);
	return member ? std::string(member->string_or()) : std::string{};
}
[[nodiscard]] bool collect_verification_capture_ids(const Json& value,
													std::set<std::string>& protected_ids) {
	if (value.kind == Json::Kind::object) {
		for (const auto& [field, member] : value.object) {
			if (field == "capture_id" || field == "capture") {
				if (member.kind != Json::Kind::string || !valid_identifier(member.string_or()))
					return false;
				protected_ids.emplace(member.string_or());
			} else if (!collect_verification_capture_ids(member, protected_ids))
				return false;
		}
	} else if (value.kind == Json::Kind::array)
		for (const auto& member : value.array)
			if (!collect_verification_capture_ids(member, protected_ids))
				return false;
	return true;
}
[[nodiscard]] Result<std::filesystem::path> find_run(const std::filesystem::path& root,
													 std::string_view run_id) {
	const auto runs = root / "runs";
	std::error_code error;
	if (!std::filesystem::is_directory(runs, error))
		return unexpectedError(agentic_error(AgenticError::unavailable));
	std::filesystem::path found;
	for (const auto& entry : std::filesystem::directory_iterator(runs)) {
		if (!entry.is_directory() || entry.is_symlink())
			continue;
		const auto provenance = read_text(entry.path() / "run.json", 4u * 1024u * 1024u);
		if (!provenance)
			continue;
		const auto parsed = parse_json(*provenance);
		if (!parsed || member_string(*parsed, "run_id") != run_id)
			continue;
		if (!found.empty())
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		found = entry.path();
	}
	if (found.empty())
		return unexpectedError(agentic_error(AgenticError::unavailable));
	return found;
}
[[nodiscard]] Result<Json> scan_captures(const std::filesystem::path& root, bool verify) {
	Json captures = Json::make_array();
	const auto runs = root / "runs";
	std::error_code error;
	if (!std::filesystem::exists(runs, error))
		return captures;
	for (const auto& run : std::filesystem::directory_iterator(runs)) {
		if (!run.is_directory() || run.is_symlink())
			continue;
		const auto directory = run.path() / "captures";
		if (!std::filesystem::is_directory(directory, error))
			continue;
		for (const auto& capture : std::filesystem::directory_iterator(directory)) {
			if (!capture.is_directory() || capture.is_symlink() ||
				capture.path().filename().string().starts_with('.'))
				continue;
			const auto manifest = read_manifest(capture.path() / "manifest.json", verify);
			if (!manifest)
				continue;
			captures.array.emplace_back(Json::make_object(
				{{"run_id", manifest->run_id},
				 {"capture_id", manifest->capture_id},
				 {"manifest_path",
				  std::filesystem::relative(manifest->manifest_path, root).generic_string()},
				 {"sha256", manifest->digest},
				 {"status", manifest->partial ? "partial" : "sealed"}}));
		}
	}
	std::sort(captures.array.begin(), captures.array.end(),
			  [](const Json& first, const Json& second) {
				  return member_string(first, "capture_id") < member_string(second, "capture_id");
			  });
	return captures;
}
[[nodiscard]] bool valid_digest(std::string_view digest) noexcept {
	if (digest.size() != 64)
		return false;
	for (const auto character : digest)
		if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
			return false;
	return true;
}
/* A false result requests authoritative repair; an error leaves this advisory cache deferred. */
[[nodiscard]] Result<bool> update_capture_index(const std::filesystem::path& root,
												const AgenticManifest& manifest) noexcept {
	try {
		constexpr std::uint64_t maximum_index_bytes = 16u * 1024u * 1024u;
		StoreLock lock(root);
		if (!lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		const auto index_path = root / "index.json";
		if (!confined(root, "index.json"))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		std::error_code error;
		if (!std::filesystem::exists(index_path, error))
			return false;
		if (error || !std::filesystem::is_regular_file(index_path, error))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		auto contents = read_text(index_path, maximum_index_bytes);
		if (!contents)
			return unexpectedError(contents.error());
		auto index = parse_json(*contents, maximum_index_bytes);
		if (!index)
			return false;
		auto* captures = index->find("captures");
		if (!captures || captures->kind != Json::Kind::array)
			return false;
		std::string_view previous_identity;
		for (const auto& capture : captures->array) {
			const auto* identity = capture.find("capture_id");
			if (!identity || !valid_identifier(identity->string_or()) ||
				!valid_identifier(member_string(capture, "run_id")) ||
				!safe_relative(member_string(capture, "manifest_path")) ||
				!valid_digest(member_string(capture, "sha256")))
				return false;
			const auto current_identity = identity->string_or();
			if (current_identity == manifest.capture_id)
				return unexpectedError(agentic_error(AgenticError::invalid_scope));
			if (!previous_identity.empty() && previous_identity >= current_identity)
				return false;
			previous_identity = current_identity;
		}
		if (!validate_agentic_document(*index, "FlowUi.Agentic.Index"))
			return false;
		const auto relative_manifest =
			manifest.manifest_path.lexically_relative(std::filesystem::absolute(root));
		if (!safe_relative(relative_manifest.generic_string()))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		captures->array.reserve(captures->array.size() + 1);
		const auto position =
			std::lower_bound(captures->array.begin(), captures->array.end(), manifest.capture_id,
							 [](const Json& capture, std::string_view identity) {
								 return capture.find("capture_id")->string_or() < identity;
							 });
		captures->array.emplace(
			position, Json::make_object({{"run_id", manifest.run_id},
										 {"capture_id", manifest.capture_id},
										 {"manifest_path", relative_manifest.generic_string()},
										 {"sha256", manifest.digest},
										 {"status", manifest.partial ? "partial" : "sealed"}}));
		const auto encoded = write_json(*index);
		if (encoded.size() > maximum_index_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		auto written = atomic_text(index_path, encoded);
		if (!written)
			return unexpectedError(written.error());
		return true;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
[[nodiscard]] bool inventory_has(const Json& document, std::string_view artifact_id,
								 std::string_view artifact_path = {}) {
	const auto* artifacts = document.find("artifacts");
	if (!artifacts)
		return false;
	for (const auto& artifact : artifacts->array) {
		if ((!artifact_id.empty() && member_string(artifact, "artifact_id") == artifact_id) ||
			(!artifact_path.empty() && member_string(artifact, "path") == artifact_path))
			return true;
	}
	return false;
}
[[nodiscard]] Status validate_owned_artifact(const AgenticArtifact& artifact,
											 std::string_view contents, const Json& manifest) {
	if (!has_registered_schema(artifact.schema))
		return {};
	auto validate_record = [&](const Json& record) -> Status {
		const auto valid = validate_agentic_document(record, artifact.schema);
		if (!valid)
			return valid;
		if (const auto* run = record.find("run_id");
			run && run->string_or() != member_string(manifest, "run_id"))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (const auto* capture = record.find("capture_id");
			capture && capture->string_or() != member_string(manifest, "capture_id"))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (artifact.schema == "FlowUi.Agentic.Correlation") {
			const auto resolution = member_string(record, "resolution");
			if (resolution == "resolved" || resolution == "candidate") {
				for (const auto key : {"source", "target"}) {
					const auto* selector = record.find(key);
					if (!selector ||
						!inventory_has(manifest, member_string(*selector, "artifact_id")))
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
				}
			}
		}
		if (artifact.schema == "FlowUi.Agentic.Image") {
			if (!inventory_has(manifest, member_string(record, "image_id"),
							   member_string(record, "path")) ||
				!inventory_has(manifest, member_string(record, "source_ppm_artifact_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			if (const auto* tree = record.find("tree");
				tree && tree->kind != Json::Kind::null &&
				!inventory_has(manifest, member_string(*tree, "artifact_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			if (const auto* theme = record.find("theme");
				theme && theme->kind != Json::Kind::null &&
				!inventory_has(manifest, member_string(*theme, "artifact_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		}
		return {};
	};
	if (artifact.media_type == "application/x-ndjson") {
		std::size_t begin = 0;
		while (begin < contents.size()) {
			auto end = contents.find('\n', begin);
			if (end == std::string_view::npos)
				end = contents.size();
			const auto line = contents.substr(begin, end - begin);
			begin = end + 1;
			if (line.empty())
				continue;
			auto record = parse_json(line);
			if (!record)
				return unexpectedError(record.error());
			const auto valid = validate_record(*record);
			if (!valid)
				return valid;
		}
		return {};
	}
	if (artifact.media_type != "application/json")
		return {};
	const auto document = parse_json(contents);
	if (!document)
		return unexpectedError(document.error());
	return validate_record(*document);
}
[[nodiscard]] Json artifact_json(const AgenticArtifact& artifact) {
	return Json::make_object({{"artifact_id", artifact.artifact_id},
							  {"path", artifact.path},
							  {"media_type", artifact.media_type},
							  {"schema", artifact.schema},
							  {"schema_version", artifact.schema_version},
							  {"integer_encoding", artifact.integer_encoding},
							  {"bytes", decimal(artifact.bytes)},
							  {"sha256", artifact.sha256}});
}
[[nodiscard]] Status add_json(AgenticCaptureSnapshot& snapshot, std::string id, std::string path,
							  std::string schema, Json value) {
	return add_text_artifact(snapshot, std::move(id), std::move(path), "application/json",
							 std::move(schema), write_json(value));
}
[[nodiscard]] Status add_standard_artifacts(AgenticCaptureSnapshot& snapshot) {
	const std::array<std::pair<std::string_view, SectionRequirement>, 4> requested{
		{{"performance", snapshot.request.performance},
		 {"runtime_diagnostics", snapshot.request.runtime_diagnostics},
		 {"memory", snapshot.request.memory},
		 {"visual", snapshot.request.visual}}};
	for (const auto& requirement : requested) {
		auto found =
			std::find_if(snapshot.coverage.begin(), snapshot.coverage.end(),
						 [&](const auto& section) { return section.first == requirement.first; });
		if (found == snapshot.coverage.end()) {
			SectionCoverage coverage;
			coverage.status = requirement.second == SectionRequirement::omit
								  ? SectionStatus::not_requested
								  : SectionStatus::failed;
			if (coverage.status == SectionStatus::failed)
				coverage.reasons.emplace_back("requested_section_has_no_owned_producer_snapshot");
			snapshot.coverage.emplace_back(std::string(requirement.first), std::move(coverage));
			found = std::prev(snapshot.coverage.end());
		}
		if (requirement.second == SectionRequirement::required &&
			found->second.status != SectionStatus::complete)
			snapshot.partial = true;
	}

	Status added;
	if (std::none_of(
			snapshot.artifacts.begin(), snapshot.artifacts.end(),
			[](const auto& artifact) { return artifact.artifact_id == "capture_request"; })) {
		added = add_json(snapshot, "capture_request", "request.json",
						 "FlowUi.Agentic.CaptureRequest", to_json(snapshot.request));
		if (!added)
			return added;
	}
	Json sections = Json::make_object();
	Json coverage = Json::make_object();
	bool complete = !snapshot.partial;
	for (const auto& section : snapshot.coverage) {
		sections.set(section.first, to_json(section.second));
		coverage.set(section.first, name(section.second.status));
		if (section.second.status == SectionStatus::partial ||
			section.second.status == SectionStatus::failed)
			complete = false;
	}
	Json limitations = Json::make_array();
	for (const auto& value : snapshot.limitations)
		limitations.array.emplace_back(value);
	added = add_json(snapshot, "capture_quality", "quality.json", "FlowUi.Agentic.Quality",
					 Json::make_object({{"schema", "FlowUi.Agentic.Quality"},
										{"schema_version", agentic_schema_version},
										{"profile", name(snapshot.request.profile)},
										{"sections", sections},
										{"limitations", limitations},
										{"complete", complete},
										{"publication_durability",
										 "atomic_visibility_without_power_loss_guarantee"}}));
	if (!added)
		return added;
	added = add_json(snapshot, "source_dictionary", "correlation/sources.json",
					 "FlowUi.Agentic.Sources", snapshot.sources);
	if (!added)
		return added;
	std::string edges;
	for (const auto& edge : snapshot.correlations.array) {
		edges += write_json(edge);
		edges.push_back('\n');
	}
	added =
		add_text_artifact(snapshot, "correlation_edges", "correlation/correlations.jsonl",
						  "application/x-ndjson", "FlowUi.Agentic.Correlation", std::move(edges));
	if (!added)
		return added;
	if (!snapshot.frames.empty() || snapshot.request.performance != SectionRequirement::omit) {
		std::string relations;
		relations.reserve(snapshot.frames.size() * 512u);
		std::string csv =
			"run_id,app_tick,window_id,window_frame,frame_slot,begin_ns,end_ns,submit_ns,present_"
			"ns,submission_serial,device_id,queue_id,cpu_duration_ns,gpu_duration_ns,gpu_available,"
			"presented,canceled,observer_activity,internal_window,interrupted\n";
		for (const auto& frame : snapshot.frames) {
			relations += write_json(to_json(frame));
			relations.push_back('\n');
			csv +=
				frame.key.run_id + ',' + std::to_string(frame.key.app_tick) + ',' +
				std::to_string(frame.key.window_id) + ',' + std::to_string(frame.key.window_frame) +
				',' + std::to_string(frame.frame_slot) + ',' + std::to_string(frame.begin_ns) +
				',' + std::to_string(frame.end_ns) + ',' + std::to_string(frame.submit_ns) + ',' +
				std::to_string(frame.present_ns) + ',' + std::to_string(frame.submission_serial) +
				',' + std::to_string(frame.device_id) + ',' + std::to_string(frame.queue_id) + ',' +
				std::to_string(frame.cpu_duration_ns) + ',' +
				(frame.gpu_duration_available ? std::to_string(frame.gpu_duration_ns) : "") + ',' +
				(frame.gpu_duration_available ? "1" : "0") + ',' + (frame.presented ? "1" : "0") +
				',' + (frame.canceled ? "1" : "0") + ',' +
				std::to_string(static_cast<std::uint32_t>(frame.observers)) + ',' +
				(frame.internal_window ? "1" : "0") + ',' + (frame.interrupted ? "1" : "0") + '\n';
		}
		added =
			add_text_artifact(snapshot, "frame_relations", "correlation/frames.jsonl",
							  "application/x-ndjson", "FlowUi.Agentic.Frame", std::move(relations));
		if (!added)
			return added;
		added =
			add_text_artifact(snapshot, "performance_frames", "performance/frames.csv", "text/csv",
							  "FlowUi.Agentic.FrameCSV", std::move(csv), "1.0", "decimal_text");
		if (!added)
			return added;
		auto analysis = analyze_performance(snapshot.frames);
		if (!analysis)
			return unexpectedError(analysis.error());
		added = add_json(snapshot, "performance_summary", "performance/summary.json",
						 "FlowUi.Agentic.Performance", std::move(*analysis));
		if (!added)
			return added;
	}
	if (!snapshot.actions.array.empty()) {
		std::string actions;
		for (const auto& action : snapshot.actions.array) {
			actions += write_json(action);
			actions.push_back('\n');
		}
		added =
			add_text_artifact(snapshot, "scenario_actions", "workload/actions.jsonl",
							  "application/x-ndjson", "FlowUi.Agentic.Action", std::move(actions));
		if (!added)
			return added;
	}
	if (!snapshot.images.empty()) {
		if (snapshot.images.size() > snapshot.request.limits.maximum_images)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		std::string metadata;
		for (const auto& image : snapshot.images) {
			if (image.run_id != snapshot.run_id || image.capture_id != snapshot.capture_id ||
				!valid_identifier(image.image_id))
				return unexpectedError(agentic_error(AgenticError::invalid_scope));
			auto ppm = encode_ppm(image, snapshot.request.limits.maximum_image_bytes);
			auto png = encode_png(image, snapshot.request.limits.maximum_image_bytes);
			if (!ppm || !png)
				return unexpectedError(!ppm ? ppm.error() : png.error());
			AgenticArtifactPayload original;
			original.bytes = std::move(*ppm);
			original.artifact_id = image.image_id + "_ppm";
			original.path = "visual/images/" + image.image_id + ".ppm";
			original.media_type = "image/x-portable-pixmap";
			original.schema = "FlowUi.Agentic.ImageRGB8";
			AgenticArtifactPayload derivative;
			derivative.bytes = std::move(*png);
			derivative.artifact_id = image.image_id;
			derivative.path = "visual/images/" + image.image_id + ".png";
			derivative.media_type = "image/png";
			derivative.schema = "FlowUi.Agentic.ImageRGB8";
			auto record = image_metadata(image);
			record.set("path", derivative.path);
			record.set("source_ppm_artifact_id", original.artifact_id);
			metadata += write_json(record);
			metadata.push_back('\n');
			snapshot.artifacts.emplace_back(std::move(original));
			snapshot.artifacts.emplace_back(std::move(derivative));
		}
		added =
			add_text_artifact(snapshot, "visual_frames", "visual/frames.jsonl",
							  "application/x-ndjson", "FlowUi.Agentic.Image", std::move(metadata));
		if (!added)
			return added;
	}
	std::string summary =
		"# Agentic capture " + snapshot.capture_id +
		"\n\nValidity: " + (complete ? "complete" : "partial") + ". Workload: " +
		(snapshot.request.scenario_id.empty() ? "unregistered observation"
											  : snapshot.request.scenario_id) +
		". Profile: " + std::string(name(snapshot.request.profile)) +
		".\n\nAcceptance: evaluate the declared objective against all required independent passes; "
		"capture publication alone is not a passing objective.\n\n";
	summary += "- [Quality and exclusions](quality.json)\n- [Resolved request](request.json)\n- "
			   "[Source dictionary](correlation/sources.json)\n";
	unsigned findings = 0;
	for (const auto& limitation : snapshot.limitations) {
		if (findings++ == 10)
			break;
		summary += "- Limitation: " + limitation + ".\n";
	}
	return add_text_artifact(snapshot, "capture_summary", "summary.md", "text/markdown",
							 "FlowUi.Agentic.Summary", std::move(summary));
}
} // namespace
std::string sha256(std::span<const std::uint8_t> bytes) {
	std::array<std::uint32_t, 8> state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
									   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	auto transform = [&](const std::array<std::uint8_t, 64>& block) {
		std::array<std::uint32_t, 64> schedule{};
		for (unsigned index = 0; index < 16; ++index)
			schedule[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24u) |
							  (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16u) |
							  (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8u) |
							  block[index * 4 + 3];
		for (unsigned index = 16; index < 64; ++index) {
			const auto first = std::rotr(schedule[index - 15], 7) ^
							   std::rotr(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3u);
			const auto second = std::rotr(schedule[index - 2], 17) ^
								std::rotr(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10u);
			schedule[index] = schedule[index - 16] + first + schedule[index - 7] + second;
		}
		auto working = state;
		for (unsigned index = 0; index < 64; ++index) {
			const auto upper =
				std::rotr(working[4], 6) ^ std::rotr(working[4], 11) ^ std::rotr(working[4], 25);
			const auto choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
			const auto first =
				working[7] + upper + choice + round_constants[index] + schedule[index];
			const auto lower =
				std::rotr(working[0], 2) ^ std::rotr(working[0], 13) ^ std::rotr(working[0], 22);
			const auto majority =
				(working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
			for (unsigned shift = 7; shift > 0; --shift)
				working[shift] = working[shift - 1];
			working[4] += first;
			working[0] = first + lower + majority;
		}
		for (unsigned index = 0; index < 8; ++index)
			state[index] += working[index];
	};
	std::array<std::uint8_t, 64> block{};
	std::size_t offset = 0;
	while (bytes.size() - offset >= 64) {
		std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), 64, block.begin());
		transform(block);
		offset += 64;
	}
	block.fill(0);
	const auto remainder = bytes.size() - offset;
	std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), remainder, block.begin());
	block[remainder] = 0x80u;
	if (remainder >= 56) {
		transform(block);
		block.fill(0);
	}
	const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8u;
	for (unsigned index = 0; index < 8; ++index)
		block[63 - index] = static_cast<std::uint8_t>(bit_length >> (index * 8u));
	transform(block);
	constexpr char hex[] = "0123456789abcdef";
	std::string output;
	output.reserve(64);
	for (const auto word : state)
		for (unsigned digit = 0; digit < 8; ++digit)
			output.push_back(hex[(word >> (28u - digit * 4u)) & 0xfu]);
	return output;
}
std::string sha256(std::string_view bytes) {
	return sha256({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
}
AgenticStore::AgenticStore(std::filesystem::path root, AgenticLimits limits)
	: root_(std::move(root)), limits_(limits) {}
const std::filesystem::path& AgenticStore::root() const noexcept {
	return root_;
}
Status AgenticStore::initialize() noexcept {
	try {
		if (root_.empty())
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		std::filesystem::create_directories(root_);
		StoreLock lock(root_);
		if (!lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		const auto path = root_ / "store.json";
		if (std::filesystem::exists(path)) {
			auto text = read_text(path, 65536);
			if (!text)
				return unexpectedError(text.error());
			auto json = parse_json(*text);
			if (!json || member_string(*json, "schema") != "FlowUi.Agentic.Store" ||
				member_string(*json, "schema_version") != "1.0" ||
				!valid_identifier(member_string(*json, "store_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		} else {
			auto identity = make_identifier("store");
			if (!identity)
				return unexpectedError(identity.error());
			const auto written = atomic_text(
				path, write_json(Json::make_object({{"schema", "FlowUi.Agentic.Store"},
													{"schema_version", "1.0"},
													{"layout_major", Json::number(1)},
													{"store_id", *identity},
													{"created_at_utc", utc_timestamp()}})));
			if (!written)
				return written;
		}
		std::filesystem::create_directories(root_ / "runs");
		std::filesystem::create_directories(root_ / "runtime" / "instances");
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<std::filesystem::path> AgenticStore::reserve_run(std::string_view run_id,
														const Json& provenance) noexcept {
	try {
		if (!valid_identifier(run_id) || provenance.kind != Json::Kind::object)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		auto initialized = initialize();
		if (!initialized)
			return unexpectedError(initialized.error());
		StoreLock lock(root_);
		if (!lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		if (find_run(root_, run_id))
			return unexpectedError(agentic_error(AgenticError::busy));
		std::string timestamp = utc_timestamp();
		timestamp.erase(std::remove_if(timestamp.begin(), timestamp.end(),
									   [](char character) {
										   return character == '-' || character == ':' ||
												  character == '.';
									   }),
						timestamp.end());
		const auto path = root_ / "runs" / (timestamp + "_" + std::string(run_id));
		if (!std::filesystem::create_directory(path))
			return unexpectedError(agentic_error(AgenticError::busy));
		Json run = provenance;
		run.set("schema", "FlowUi.Agentic.Run");
		run.set("schema_version", "1.0");
		run.set("run_id", std::string(run_id));
		run.set("created_at_utc", utc_timestamp());
		run.set("monotonic_anchor_ns", decimal(monotonic_nanoseconds()));
		auto written = write_text(path / "run.json", write_json(run));
		if (!written)
			return unexpectedError(written.error());
		std::filesystem::create_directory(path / "captures");
		std::filesystem::create_directory(path / "comparisons");
		return path;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<std::filesystem::path> AgenticStore::resolve_run(std::string_view run_id) const noexcept {
	try {
		if (!valid_identifier(run_id))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		return find_run(root_, run_id);
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::unavailable));
	}
}
Status validate_agentic_document(const Json& document, std::string_view expected_schema) noexcept {
	try {
		if (!registered_schema_matches(document, expected_schema))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		const bool inventory_scoped = expected_schema == "FlowUi.Agentic.Sources" ||
									  expected_schema == "FlowUi.Agentic.Scenario";
		if (!inventory_scoped && (document.kind != Json::Kind::object ||
								  member_string(document, "schema") != expected_schema ||
								  member_string(document, "schema_version") != "1.0"))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (expected_schema == "FlowUi.Agentic.Tree" && !valid_tree_references(document))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (expected_schema == "FlowUi.Agentic.Action")
			if (const auto* observed = document.find("observed_frame");
				observed && member_string(*observed, "run_id") != member_string(document, "run_id"))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		if (expected_schema == "FlowUi.Agentic.InputFrame" ||
			expected_schema == "FlowUi.Agentic.InteractionObservation") {
			if (const auto* observed = document.find("frame");
				observed && member_string(*observed, "run_id") != member_string(document, "run_id"))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		}
		if (expected_schema == "FlowUi.Agentic.InteractionResult") {
			if (const auto* observed = document.find("activation_frame");
				observed &&
				(member_string(*observed, "run_id") != member_string(document, "run_id") ||
				 member_string(*observed, "window_id") != member_string(document, "window_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		}
		if (expected_schema == "FlowUi.Agentic.InputFrame" ||
			expected_schema == "FlowUi.Agentic.InteractionPlan") {
			auto valid_input_slots = [](const Json& input) noexcept {
				for (const auto key : {"keys_down", "buttons_down"}) {
					const auto* slots = input.find(key);
					if (!slots)
						continue;
					std::uint64_t previous = 0;
					bool first = true;
					for (const auto& slot : slots->array) {
						const auto current =
							slot.uint64_or(std::numeric_limits<std::uint64_t>::max());
						if (!first && current <= previous)
							return false;
						first = false;
						previous = current;
					}
				}
				return true;
			};
			if (expected_schema == "FlowUi.Agentic.InputFrame") {
				for (const auto key : {"effective_input", "native_input"})
					if (const auto* input = document.find(key); input && !valid_input_slots(*input))
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
			} else {
				for (const auto key : {"frames", "spans"}) {
					if (const auto* rows = document.find(key))
						for (const auto& row : rows->array) {
							const auto* input = row.find("input");
							if (!valid_input_slots(input ? *input : row))
								return unexpectedError(
									agentic_error(AgenticError::integrity_failed));
						}
				}
			}
		}
		if (const auto* features = document.find("required_features")) {
			if (features->kind != Json::Kind::array)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			for (const auto& feature : features->array)
				if (feature.string_or() != "correlation_keys_v1" &&
					feature.string_or() != "section_coverage_v1")
					return unexpectedError(agentic_error(AgenticError::unavailable));
		}
		if (expected_schema == "FlowUi.Agentic.Capture") {
			if (!valid_identifier(member_string(document, "run_id")) ||
				!valid_identifier(member_string(document, "capture_id")) ||
				!valid_identifier(member_string(document, "job_id")))
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			const auto status = member_string(document, "status");
			if (status != "sealed" && status != "partial")
				return unexpectedError(agentic_error(AgenticError::incomplete));
			const auto* artifacts = document.find("artifacts");
			const auto* coverage = document.find("coverage");
			const auto* measurement = document.find("measurement");
			if (!artifacts || artifacts->kind != Json::Kind::array ||
				artifacts->array.size() > 100000 || !coverage ||
				coverage->kind != Json::Kind::object || !measurement ||
				measurement->kind != Json::Kind::object)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			for (const auto& member : coverage->object) {
				const auto value = member.second.string_or();
				if (value != "complete" && value != "partial" && value != "not_requested" &&
					value != "disabled" && value != "unsupported" && value != "failed")
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
			}
			const auto* first = measurement->find("first_app_tick");
			const auto* end = measurement->find("end_app_tick_exclusive");
			const auto* begin_ns = measurement->find("begin_monotonic_ns");
			const auto* end_ns = measurement->find("end_monotonic_ns");
			if (!first || !end || !begin_ns || !end_ns)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			const auto parsed_first = parse_decimal(*first);
			const auto parsed_end = parse_decimal(*end);
			const auto parsed_begin = parse_decimal(*begin_ns);
			const auto parsed_end_ns = parse_decimal(*end_ns);
			if (!parsed_first || !parsed_end || !parsed_begin || !parsed_end_ns ||
				*parsed_first > *parsed_end || *parsed_begin > *parsed_end_ns)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			std::set<std::string> identities;
			std::set<std::string> paths;
			for (const auto& artifact : artifacts->array) {
				const auto id = member_string(artifact, "artifact_id");
				const auto path = member_string(artifact, "path");
				const auto* bytes = artifact.find("bytes");
				if (!valid_identifier(id) || !safe_relative(path) ||
					(path == "manifest.json" || path == ".owner.json") ||
					!identities.insert(id).second || !paths.insert(path).second || !bytes ||
					!parse_decimal(*bytes) || !valid_digest(member_string(artifact, "sha256")) ||
					member_string(artifact, "media_type").empty() ||
					member_string(artifact, "schema").empty() ||
					member_string(artifact, "schema_version").empty())
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
			}
			if (const auto* entrypoints = document.find("entrypoints")) {
				if (entrypoints->kind != Json::Kind::object)
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				for (const auto& entry : entrypoints->object)
					if (!paths.contains(std::string(entry.second.string_or())))
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
			}
		} else if (expected_schema == "FlowUi.Agentic.CaptureRequest") {
			const auto request = capture_request_from_json(document);
			if (!request)
				return unexpectedError(request.error());
		} else if (expected_schema == "FlowUi.Agentic.Frame") {
			for (const auto key : {"app_tick", "window_id", "window_frame", "begin_ns", "end_ns"}) {
				const auto* value = document.find(key);
				if (!value || !parse_decimal(*value))
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
			}
		} else if (expected_schema == "FlowUi.Agentic.Correlation") {
			if (!document.find("source") || !document.find("target") ||
				member_string(document, "relationship").empty() ||
				member_string(document, "method").empty() ||
				member_string(document, "confidence").empty() ||
				member_string(document, "resolution").empty())
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
		}
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
Result<AgenticManifest> read_manifest(const std::filesystem::path& path, bool verify_hashes,
									  std::uint64_t maximum_artifact_bytes) noexcept {
	try {
		if (std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		const auto text = read_text(path, 16u * 1024u * 1024u);
		if (!text)
			return unexpectedError(text.error());
		auto document = parse_json(*text);
		if (!document)
			return unexpectedError(document.error());
		auto valid = validate_agentic_document(*document, "FlowUi.Agentic.Capture");
		if (!valid)
			return unexpectedError(valid.error());
		AgenticManifest output;
		output.manifest_path = std::filesystem::absolute(path);
		output.run_id = member_string(*document, "run_id");
		output.capture_id = member_string(*document, "capture_id");
		output.partial = member_string(*document, "status") == "partial";
		output.digest = sha256(*text);
		const auto* artifacts = document->find("artifacts");
		output.artifacts.reserve(artifacts->array.size());
		for (const auto& value : artifacts->array) {
			AgenticArtifact artifact;
			artifact.artifact_id = member_string(value, "artifact_id");
			artifact.path = member_string(value, "path");
			artifact.media_type = member_string(value, "media_type");
			artifact.schema = member_string(value, "schema");
			artifact.schema_version = member_string(value, "schema_version");
			artifact.integer_encoding = member_string(value, "integer_encoding");
			artifact.sha256 = member_string(value, "sha256");
			artifact.bytes = *parse_decimal(*value.find("bytes"));
			if (!confined(path.parent_path(), artifact.path))
				return unexpectedError(agentic_error(AgenticError::invalid_scope));
			std::error_code error;
			const auto size = std::filesystem::file_size(path.parent_path() / artifact.path, error);
			if (error || size != artifact.bytes || size > maximum_artifact_bytes)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			if (verify_hashes) {
				const auto contents =
					read_text(path.parent_path() / artifact.path, maximum_artifact_bytes);
				if (!contents || sha256(*contents) != artifact.sha256)
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				const auto schema_valid = validate_owned_artifact(artifact, *contents, *document);
				if (!schema_valid)
					return unexpectedError(schema_valid.error());
			}
			output.artifacts.emplace_back(std::move(artifact));
		}
		output.document = std::move(*document);
		return output;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
Result<AgenticManifest> AgenticStore::publish(AgenticCaptureSnapshot snapshot) noexcept {
	try {
		if (!valid_identifier(snapshot.run_id) || !valid_identifier(snapshot.capture_id) ||
			!valid_identifier(snapshot.job_id) ||
			snapshot.first_app_tick > snapshot.end_app_tick_exclusive ||
			snapshot.measurement_begin_ns > snapshot.measurement_end_ns)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		StoreLock publication_lock(root_, ".publication.lock");
		if (!publication_lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		const auto run = find_run(root_, snapshot.run_id);
		if (!run)
			return unexpectedError(run.error());
		const auto staging = *run / "captures" / ("." + snapshot.capture_id + ".pending");
		const auto final = *run / "captures" / snapshot.capture_id;
		if (std::filesystem::exists(final) || !std::filesystem::create_directory(staging))
			return unexpectedError(agentic_error(AgenticError::busy));
		auto store_text = read_text(root_ / "store.json", 65536);
		if (!store_text)
			return unexpectedError(store_text.error());
		auto store_document = parse_json(*store_text);
		if (!store_document || !valid_identifier(member_string(*store_document, "store_id")))
			return unexpectedError(agentic_error(AgenticError::integrity_failed));
		auto owner_written = write_text(
			staging / ".owner.json",
			write_json(Json::make_object({{"schema", "FlowUi.Agentic.StagingOwner"},
										  {"schema_version", "1.0"},
										  {"store_id", member_string(*store_document, "store_id")},
										  {"run_id", snapshot.run_id},
										  {"capture_id", snapshot.capture_id},
										  {"job_id", snapshot.job_id}})));
		if (!owner_written)
			return unexpectedError(owner_written.error());
		auto owned_before = snapshot_owned_bytes(snapshot);
		if (!owned_before || *owned_before > limits_.maximum_snapshot_bytes ||
			*owned_before > snapshot.request.limits.maximum_snapshot_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		auto prepared = add_standard_artifacts(snapshot);
		if (!prepared)
			return unexpectedError(prepared.error());
		auto owned_after = snapshot_owned_bytes(snapshot);
		if (!owned_after || *owned_after > limits_.maximum_snapshot_bytes ||
			*owned_after > snapshot.request.limits.maximum_snapshot_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		std::uint64_t total_bytes = 0;
		std::set<std::string> identities;
		std::set<std::string> paths;
		Json artifacts = Json::make_array();
		artifacts.array.reserve(snapshot.artifacts.size());
		for (const auto& artifact : snapshot.artifacts) {
			if (!valid_identifier(artifact.artifact_id) || !safe_relative(artifact.path) ||
				(artifact.path == "manifest.json" || artifact.path == ".owner.json") ||
				!identities.insert(artifact.artifact_id).second ||
				!paths.insert(artifact.path).second ||
				artifact.bytes.size() > limits_.maximum_artifact_bytes ||
				artifact.bytes.size() > snapshot.request.limits.maximum_artifact_bytes)
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			if (artifact.bytes.size() > limits_.maximum_snapshot_bytes -
											std::min(total_bytes, limits_.maximum_snapshot_bytes))
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
			total_bytes += artifact.bytes.size();
		}
		if (total_bytes > snapshot.request.limits.maximum_snapshot_bytes ||
			total_bytes > snapshot.request.limits.maximum_disk_bytes ||
			total_bytes > limits_.maximum_disk_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		std::error_code disk_error;
		const auto disk = std::filesystem::space(root_, disk_error);
		if (disk_error || disk.available < total_bytes + 65536u)
			return unexpectedError(agentic_error(AgenticError::publication_failed));
		std::uint64_t existing_bytes = 0;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root_ / "runs")) {
			if (entry.is_symlink())
				continue;
			if (entry.is_regular_file()) {
				const auto size = entry.file_size();
				if (size > limits_.maximum_disk_bytes -
							   std::min(existing_bytes, limits_.maximum_disk_bytes))
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				existing_bytes += size;
			}
		}
		if (total_bytes >
			limits_.maximum_disk_bytes - std::min(existing_bytes, limits_.maximum_disk_bytes))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		for (const auto& payload : snapshot.artifacts) {
			if (!confined(staging, payload.path))
				return unexpectedError(agentic_error(AgenticError::invalid_scope));
			const auto destination = staging / payload.path;
			std::filesystem::create_directories(destination.parent_path());
			auto written = write_bytes(destination, payload.bytes);
			if (!written)
				return unexpectedError(written.error());
			AgenticArtifact artifact;
			artifact.artifact_id = payload.artifact_id;
			artifact.path = payload.path;
			artifact.media_type = payload.media_type;
			artifact.schema = payload.schema;
			artifact.schema_version = payload.schema_version;
			artifact.integer_encoding = payload.integer_encoding;
			artifact.bytes = payload.bytes.size();
			artifact.sha256 = sha256(payload.bytes);
			artifacts.array.emplace_back(artifact_json(artifact));
		}
		Json coverage = Json::make_object();
		bool partial = snapshot.partial;
		for (const auto& section : snapshot.coverage) {
			coverage.set(section.first, name(section.second.status));
			if (section.second.status == SectionStatus::partial ||
				section.second.status == SectionStatus::failed)
				partial = true;
		}
		Json windows = Json::make_array();
		for (const auto window : snapshot.request.window_ids)
			windows.array.emplace_back(decimal(window));
		Json limitations = Json::make_array();
		for (const auto& value : snapshot.limitations)
			limitations.array.emplace_back(value);
		Json measurement = Json::make_object(
			{{"mode", name(snapshot.request.mode)},
			 {"profile", name(snapshot.request.profile)},
			 {"first_app_tick", decimal(snapshot.first_app_tick)},
			 {"end_app_tick_exclusive", decimal(snapshot.end_app_tick_exclusive)},
			 {"begin_monotonic_ns", decimal(snapshot.measurement_begin_ns)},
			 {"end_monotonic_ns", decimal(snapshot.measurement_end_ns)},
			 {"window_ids", std::move(windows)},
			 {"timing_capture_generation", decimal(snapshot.timing_capture_generation)},
			 {"scenario_id", snapshot.request.scenario_id},
			 {"scenario_revision", snapshot.request.scenario_revision},
			 {"scenario_digest", snapshot.request.scenario_digest},
			 {"fixture_id", snapshot.request.fixture_id},
			 {"seed", decimal(snapshot.request.seed)}});
		if (snapshot.request.objective)
			measurement.set("objective", to_json(*snapshot.request.objective));
		Json manifest = Json::make_object(
			{{"schema", "FlowUi.Agentic.Capture"},
			 {"schema_version", "1.0"},
			 {"store_layout_major", Json::number(1)},
			 {"run_id", snapshot.run_id},
			 {"capture_id", snapshot.capture_id},
			 {"job_id", snapshot.job_id},
			 {"status", partial ? "partial" : "sealed"},
			 {"requested_at_utc",
			  snapshot.requested_at_utc.empty() ? utc_timestamp() : snapshot.requested_at_utc},
			 {"published_at_utc", utc_timestamp()},
			 {"measurement", std::move(measurement)},
			 {"provenance", snapshot.provenance},
			 {"coverage", std::move(coverage)},
			 {"artifacts", std::move(artifacts)},
			 {"entrypoints", Json::make_object({{"summary", "summary.md"},
												{"quality", "quality.json"},
												{"sources", "correlation/sources.json"}})},
			 {"limitations", std::move(limitations)},
			 {"required_features",
			  Json::make_array({"correlation_keys_v1", "section_coverage_v1"})}});
		auto valid = validate_agentic_document(manifest, "FlowUi.Agentic.Capture");
		if (!valid)
			return unexpectedError(valid.error());
		auto written = write_text(staging / "manifest.json", write_json(manifest));
		if (!written)
			return unexpectedError(written.error());
		auto verified =
			read_manifest(staging / "manifest.json", true, limits_.maximum_artifact_bytes);
		if (!verified)
			return unexpectedError(verified.error());
		std::filesystem::rename(staging, final);
		std::error_code owner_cleanup_error;
		std::filesystem::remove(final / ".owner.json", owner_cleanup_error);
		auto published =
			read_manifest(final / "manifest.json", false, limits_.maximum_artifact_bytes);
		if (!published)
			return unexpectedError(published.error());
		const auto indexed = update_capture_index(root_, *published);
		if (!indexed || (!*indexed && !repair_index()))
			published->document.set("index_update", "deferred_repair_required");
		return published;
	} catch (const std::filesystem::filesystem_error& failure) {
		return unexpectedError(publication_io_error(failure.code().value()));
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<AgenticManifest> AgenticStore::resolve_capture(std::string_view capture_id_or_path,
													  bool verify_hashes) const noexcept {
	try {
		const std::filesystem::path candidate(capture_id_or_path);
		if (candidate.has_parent_path() || candidate.filename() == "manifest.json")
			return read_manifest(candidate, verify_hashes, limits_.maximum_artifact_bytes);
		if (!valid_identifier(capture_id_or_path))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		auto captures = scan_captures(root_, false);
		if (!captures)
			return unexpectedError(captures.error());
		std::filesystem::path selected;
		for (const auto& capture : captures->array)
			if (member_string(capture, "capture_id") == capture_id_or_path) {
				if (!selected.empty())
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				selected = root_ / member_string(capture, "manifest_path");
			}
		if (selected.empty())
			return unexpectedError(agentic_error(AgenticError::unavailable));
		return read_manifest(selected, verify_hashes, limits_.maximum_artifact_bytes);
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
Result<Json> AgenticStore::list_captures() const noexcept {
	try {
		return scan_captures(root_, false);
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::unavailable));
	}
}
Status AgenticStore::repair_index() noexcept {
	try {
		StoreLock lock(root_);
		if (!lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		auto captures = scan_captures(root_, false);
		if (!captures)
			return unexpectedError(captures.error());
		return atomic_text(root_ / "index.json",
						   write_json(Json::make_object({{"schema", "FlowUi.Agentic.Index"},
														 {"schema_version", "1.0"},
														 {"captures", std::move(*captures)}})));
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<Json> AgenticStore::prune(std::uint64_t older_than_seconds, bool dry_run) noexcept {
	try {
		StoreLock publication_lock(root_, ".publication.lock");
		if (!publication_lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		StoreLock lock(root_);
		if (!lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		auto captures = scan_captures(root_, true);
		if (!captures)
			return unexpectedError(captures.error());
		std::set<std::string> protected_ids;
		for (const auto& run : std::filesystem::directory_iterator(root_ / "runs")) {
			if (!run.is_directory() || run.is_symlink())
				continue;
			const auto reports = run.path() / "verification";
			if (std::filesystem::is_directory(reports)) {
				if (std::filesystem::is_symlink(reports))
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				for (const auto& entry : std::filesystem::directory_iterator(reports)) {
					if (entry.path().filename().string().starts_with('.') ||
						entry.path().extension() != ".json")
						continue;
					if (entry.is_symlink() || !entry.is_regular_file())
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
					auto text = read_text(entry.path(), 1024u * 1024u);
					if (!text)
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
					auto report = parse_json(*text, 1024u * 1024u);
					auto run_text = read_text(run.path() / "run.json", 4u * 1024u * 1024u);
					if (!run_text)
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
					auto run_document = parse_json(*run_text);
					if (!report ||
						!validate_agentic_document(*report, "FlowUi.Agentic.VerificationReport") ||
						member_string(*report, "job_id") != entry.path().stem().string() ||
						!run_document ||
						(report->find("run_id") && member_string(*report, "run_id") !=
													   member_string(*run_document, "run_id")) ||
						!collect_verification_capture_ids(*report, protected_ids))
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
				}
			}
			const auto comparisons = run.path() / "comparisons";
			if (!std::filesystem::is_directory(comparisons))
				continue;
			for (const auto& entry : std::filesystem::directory_iterator(comparisons)) {
				if (!entry.is_directory() || entry.is_symlink() ||
					entry.path().filename().string().starts_with('.'))
					continue;
				auto text = read_text(entry.path() / "comparison.json", 4u * 1024u * 1024u);
				if (!text)
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				auto json = parse_json(*text);
				if (!json || !valid_identifier(member_string(*json, "baseline_capture_id")) ||
					!valid_identifier(member_string(*json, "candidate_capture_id")))
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				protected_ids.insert(member_string(*json, "baseline_capture_id"));
				protected_ids.insert(member_string(*json, "candidate_capture_id"));
			}
		}
		Json selected = Json::make_array();
		const auto now = std::filesystem::file_time_type::clock::now();
		for (const auto& capture : captures->array) {
			const auto id = member_string(capture, "capture_id");
			const auto path = root_ / member_string(capture, "manifest_path");
			const auto manifest = read_manifest(path, false);
			if (!manifest)
				continue;
			const auto* pinned = manifest->document.find("pinned");
			const auto* verification_owner =
				nested(manifest->document, "provenance", "verification_job_id");
			if (protected_ids.contains(id) || (pinned && pinned->bool_or()) || verification_owner)
				continue;
			const auto age = std::chrono::duration_cast<std::chrono::seconds>(
								 now - std::filesystem::last_write_time(path))
								 .count();
			if (age < 0 || static_cast<std::uint64_t>(age) < older_than_seconds)
				continue;
			selected.array.emplace_back(
				Json::make_object({{"capture_id", id},
								   {"manifest_path", member_string(capture, "manifest_path")},
								   {"removed", !dry_run}}));
			if (!dry_run) {
				const auto relative = std::filesystem::relative(path.parent_path(), root_);
				if (!confined(root_, relative) ||
					path.parent_path().parent_path().filename() != "captures")
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				std::filesystem::remove_all(path.parent_path());
			}
		}
		Json staging_selected = Json::make_array();
		auto store_text = read_text(root_ / "store.json", 65536);
		if (!store_text)
			return unexpectedError(store_text.error());
		auto store_document = parse_json(*store_text);
		if (!store_document)
			return unexpectedError(store_document.error());
		for (const auto& run : std::filesystem::directory_iterator(root_ / "runs")) {
			if (!run.is_directory() || run.is_symlink())
				continue;
			const auto directory = run.path() / "captures";
			if (!std::filesystem::is_directory(directory))
				continue;
			for (const auto& entry : std::filesystem::directory_iterator(directory)) {
				if (staging_selected.array.size() >= 100)
					break;
				const auto filename = entry.path().filename().string();
				if (!entry.is_directory() || entry.is_symlink() || !filename.starts_with('.') ||
					!filename.ends_with(".pending"))
					continue;
				const auto owner_path = entry.path() / ".owner.json";
				if (std::filesystem::is_symlink(std::filesystem::symlink_status(owner_path)))
					continue;
				const auto owner_text = read_text(owner_path, 65536);
				if (!owner_text)
					continue;
				const auto owner = parse_json(*owner_text);
				if (!owner || member_string(*owner, "schema") != "FlowUi.Agentic.StagingOwner" ||
					member_string(*owner, "store_id") !=
						member_string(*store_document, "store_id") ||
					filename != "." + member_string(*owner, "capture_id") + ".pending")
					continue;
				const auto age = std::chrono::duration_cast<std::chrono::seconds>(
									 now - std::filesystem::last_write_time(owner_path))
									 .count();
				if (age < 0 || static_cast<std::uint64_t>(age) < older_than_seconds)
					continue;
				const auto relative = std::filesystem::relative(entry.path(), root_);
				if (!confined(root_, relative))
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				staging_selected.array.emplace_back(
					Json::make_object({{"capture_id", member_string(*owner, "capture_id")},
									   {"path", relative.generic_string()},
									   {"removed", !dry_run}}));
				if (!dry_run)
					std::filesystem::remove_all(entry.path());
			}
		}
		auto remaining = scan_captures(root_, false);
		if (!remaining)
			return unexpectedError(remaining.error());
		if (!dry_run) {
			auto indexed =
				atomic_text(root_ / "index.json",
							write_json(Json::make_object({{"schema", "FlowUi.Agentic.Index"},
														  {"schema_version", "1.0"},
														  {"captures", std::move(*remaining)}})));
			if (!indexed)
				return unexpectedError(indexed.error());
		}
		return Json::make_object({{"dry_run", dry_run},
								  {"selected", std::move(selected)},
								  {"staging", std::move(staging_selected)},
								  {"staging_limit", Json::number(100)}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<std::vector<std::uint8_t>> read_artifact(const AgenticManifest& manifest,
												std::string_view artifact_id,
												std::uint64_t maximum_bytes) noexcept {
	try {
		for (const auto& artifact : manifest.artifacts)
			if (artifact.artifact_id == artifact_id) {
				if (artifact.bytes > maximum_bytes)
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				if (!confined(manifest.manifest_path.parent_path(), artifact.path))
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				auto bytes =
					read_text(manifest.manifest_path.parent_path() / artifact.path, maximum_bytes);
				if (!bytes)
					return unexpectedError(bytes.error());
				if (bytes->size() != artifact.bytes || sha256(*bytes) != artifact.sha256)
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				return std::vector<std::uint8_t>(bytes->begin(), bytes->end());
			}
		return unexpectedError(agentic_error(AgenticError::unavailable));
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
Result<Json> read_artifact_json(const AgenticManifest& manifest, std::string_view artifact_id,
								std::uint64_t maximum_bytes) noexcept {
	const auto bytes = read_artifact(manifest, artifact_id, maximum_bytes);
	if (!bytes)
		return unexpectedError(bytes.error());
	return parse_json({reinterpret_cast<const char*>(bytes->data()), bytes->size()},
					  static_cast<std::size_t>(maximum_bytes));
}
namespace {
template <typename Visitor>
[[nodiscard]] Result<std::uint64_t> visit_csv(std::string_view text, std::uint64_t maximum_records,
											  Visitor&& visitor) {
	std::vector<std::string> headers;
	std::vector<std::string> fields;
	std::uint64_t record_count = 0;
	bool stopped = false;
	std::string field;
	bool quoted = false;
	bool after_quote = false;
	auto finish_row = [&]() -> bool {
		fields.emplace_back(std::move(field));
		field.clear();
		if (headers.empty()) {
			headers = std::move(fields);
			std::set<std::string> unique_headers;
			for (const auto& header : headers)
				if (!unique_headers.insert(header).second)
					return false;
			fields.clear();
			return headers.size() <= 4096;
		}
		if (fields.size() == 1 && fields.front().empty()) {
			fields.clear();
			return true;
		}
		if (fields.size() != headers.size() || record_count >= maximum_records)
			return false;
		Json row = Json::make_object();
		row.object.reserve(fields.size());
		for (std::size_t index = 0; index < fields.size(); ++index) {
			row.object.emplace_back(headers[index], Json(std::move(fields[index])));
		}
		stopped = !visitor(std::move(row), record_count++);
		fields.clear();
		return true;
	};
	for (std::size_t position = 0; position < text.size(); ++position) {
		const char character = text[position];
		if (quoted) {
			if (character == '"') {
				if (position + 1 < text.size() && text[position + 1] == '"') {
					field.push_back('"');
					++position;
				} else {
					quoted = false;
					after_quote = true;
				}
			} else
				field.push_back(character);
		} else if (character == '"') {
			if (!field.empty() || after_quote)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			quoted = true;
		} else if (character == ',') {
			fields.emplace_back(std::move(field));
			field.clear();
			after_quote = false;
		} else if (character == '\n' || character == '\r') {
			if (character == '\r' && position + 1 < text.size() && text[position + 1] == '\n')
				++position;
			if (!finish_row())
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
			after_quote = false;
			if (stopped)
				return record_count;
		} else {
			if (after_quote)
				return unexpectedError(agentic_error(AgenticError::integrity_failed));
			field.push_back(character);
		}
	}
	if (quoted)
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	if ((!field.empty() || !fields.empty() || after_quote) && !finish_row())
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	return record_count;
}
void normalize_native_integers(Json& value) {
	if (value.kind == Json::Kind::number && !value.text.empty() && value.text.front() != '-' &&
		value.text.find_first_of(".eE") == std::string::npos)
		value.kind = Json::Kind::string;
	for (auto& member : value.array)
		normalize_native_integers(member);
	for (auto& member : value.object)
		normalize_native_integers(member.second);
}
void normalize_runtime_occurrence(Json& value) {
	const auto* error = value.find("error");
	if (!error || error->kind != Json::Kind::object)
		return;
	const auto* impact = error->find("impact");
	const auto severity = impact ? impact->uint64_or() : 0;
	value.set("domain", "runtime");
	if (const auto* identity = value.find("id"))
		value.set("occurrence_id", *identity);
	value.set("severity", severity == 6	  ? "fatal"
						  : severity >= 2 ? "error"
						  : severity == 1 ? "warning"
										  : "info");
	value.set("severity_classification", "error_impact");
}
[[nodiscard]] bool matches_filter(const Json& record, const Json& filter) {
	const auto* interval_first = record.find("first_app_tick");
	const auto* interval_end = record.find("end_app_tick_exclusive");
	const bool interval_record = interval_first && interval_end;
	if (const auto* first = filter.find("first_app_tick"))
		if (const auto* end = filter.find("end_app_tick_exclusive");
			end && first->uint64_or() >= end->uint64_or())
			return false;
	for (const auto& member : filter.object) {
		if (interval_record &&
			(member.first == "first_app_tick" || member.first == "end_app_tick_exclusive" ||
			 member.first == "app_tick")) {
			const auto first = parse_decimal(*interval_first);
			const auto end = parse_decimal(*interval_end);
			const auto selected = parse_decimal(member.second);
			if (!first || !end || !selected || *first >= *end)
				return false;
			if ((member.first == "first_app_tick" && *end <= *selected) ||
				(member.first == "end_app_tick_exclusive" && *first >= *selected) ||
				(member.first == "app_tick" && (*selected < *first || *selected >= *end)))
				return false;
		} else if (member.first == "first_app_tick" || member.first == "end_app_tick_exclusive") {
			const auto* tick = record.find("app_tick");
			if (!tick)
				tick =
					record.find(member.first == "first_app_tick" ? "lastAppTick" : "firstAppTick");
			if (!tick)
				return false;
			if (member.first == "first_app_tick" && tick->uint64_or() < member.second.uint64_or())
				return false;
			if (member.first == "end_app_tick_exclusive" &&
				tick->uint64_or() >= member.second.uint64_or())
				return false;
		} else if (member.first == "record_sequence") {
			const auto* steps = record.find("steps");
			if (!steps || steps->kind != Json::Kind::array ||
				std::none_of(steps->array.begin(), steps->array.end(), [&](const auto& step) {
					const auto* sequence = step.find("sequence");
					return sequence && sequence->uint64_or() == member.second.uint64_or();
				}))
				return false;
		} else {
			const auto* value = record.find(member.first);
			if (!value)
				if (const auto* frame = record.find("frame"))
					value = frame->find(member.first);
			if (!value && member.first == "window_frame")
				value = record.find("frame_number");
			if (!value && member.first == "occurrence_id")
				value = record.find("id");
			if (!value || write_json(*value) != write_json(member.second))
				return false;
		}
	}
	return true;
}
} // namespace
Result<AgenticPage> query_artifact(const AgenticManifest& manifest, std::string_view artifact_id,
								   const Json& filter, std::uint32_t limit,
								   std::uint64_t maximum_bytes, std::string_view cursor) noexcept {
	try {
		if (limit == 0 || limit > 10000 || maximum_bytes < 128 ||
			maximum_bytes > 16u * 1024u * 1024u || filter.kind != Json::Kind::object)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const auto found =
			std::find_if(manifest.artifacts.begin(), manifest.artifacts.end(),
						 [&](const auto& artifact) { return artifact.artifact_id == artifact_id; });
		if (found == manifest.artifacts.end())
			return unexpectedError(agentic_error(AgenticError::unavailable));
		const auto projection = found->schema == "FlowUi.Agentic.Themes" ? "themes_fields_v1" : "";
		const auto hash =
			sha256(std::string(artifact_id) + write_json(filter) + "1.0" + projection);
		const auto prefix = manifest.digest + ":" + hash + ":";
		std::uint64_t offset = 0;
		if (!cursor.empty()) {
			if (!cursor.starts_with(prefix))
				return unexpectedError(agentic_error(AgenticError::stale_generation));
			const auto tail = cursor.substr(prefix.size());
			const auto parsed = std::from_chars(tail.data(), tail.data() + tail.size(), offset);
			if (parsed.ec != std::errc{} || parsed.ptr != tail.data() + tail.size())
				return unexpectedError(agentic_error(AgenticError::invalid_request));
		}
		const auto bytes = read_artifact(manifest, artifact_id, 256u * 1024u * 1024u);
		if (!bytes)
			return unexpectedError(bytes.error());
		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		AgenticPage page;
		page.manifest_digest = manifest.digest;
		std::uint64_t result_bytes = 2;
		std::uint64_t record_count = 0;
		bool stopped = false;
		Status failure;
		auto visitor = [&](Json record, std::uint64_t index) -> bool {
			record_count = index + 1;
			if (index < offset)
				return true;
			if (found->integer_encoding == "native_numbers" ||
				found->schema == "FlowUi.Diagnostics") {
				normalize_native_integers(record);
				if (found->schema == "FlowUi.Diagnostics")
					normalize_runtime_occurrence(record);
			}
			if (!matches_filter(record, filter))
				return true;
			const auto record_bytes = write_json(record).size() + 1;
			if (page.records.array.size() == limit ||
				record_bytes > maximum_bytes - std::min(result_bytes, maximum_bytes)) {
				if (page.records.array.empty())
					failure = unexpectedError(agentic_error(AgenticError::budget_exhausted));
				else {
					page.cursor = prefix + std::to_string(index);
					page.truncated = true;
				}
				stopped = true;
				return false;
			}
			result_bytes += record_bytes;
			page.records.array.emplace_back(std::move(record));
			return true;
		};
		if (found->media_type == "application/x-ndjson") {
			std::size_t begin = 0;
			while (begin < text.size() && !stopped) {
				auto end = text.find('\n', begin);
				if (end == std::string_view::npos)
					end = text.size();
				const auto line = text.substr(begin, end - begin);
				begin = end + 1;
				if (line.empty())
					continue;
				auto record = parse_json(line);
				if (!record)
					return unexpectedError(record.error());
				if (record_count >= 1000000)
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				visitor(std::move(*record), record_count);
			}
		} else if (found->media_type == "text/csv") {
			auto visited = visit_csv(text, 1000000, visitor);
			if (!visited)
				return unexpectedError(visited.error());
			record_count = *visited;
		} else if (found->media_type == "application/json") {
			auto parsed = parse_json(text);
			if (!parsed)
				return unexpectedError(parsed.error());
			if (found->schema == "FlowUi.Agentic.Themes") {
				if (!validate_agentic_document(*parsed, "FlowUi.Agentic.Themes"))
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				auto* variants = parsed->find("variants");
				if (!variants || variants->kind != Json::Kind::array)
					return unexpectedError(agentic_error(AgenticError::integrity_failed));
				for (auto& variant : variants->array) {
					auto* fields = variant.find("fields");
					if (!fields || fields->kind != Json::Kind::array)
						return unexpectedError(agentic_error(AgenticError::integrity_failed));
					for (auto& field : fields->array) {
						if (record_count >= 1000000)
							return unexpectedError(agentic_error(AgenticError::budget_exhausted));
						for (const auto key : {"run_id", "theme_revision", "schema_generation",
											   "schema_fingerprint", "collection_available"})
							if (const auto* value = parsed->find(key))
								field.set(key, *value);
						for (const auto& [source, destination] :
							 {std::pair{"variant", "variant"},
							  {"type_id", "variant_type_id"},
							  {"type_name", "variant_type_name"},
							  {"revision", "variant_revision"},
							  {"active", "active"},
							  {"status", "variant_status"}})
							if (const auto* value = variant.find(source))
								field.set(destination, *value);
						if (!visitor(std::move(field), record_count))
							break;
					}
					if (stopped)
						break;
				}
			} else {
				Json* records = parsed.operator->();
				if (parsed->kind != Json::Kind::array)
					for (const auto key : {"records", "diagnostics", "occurrences"})
						if (auto* selected = parsed->find(key);
							selected && selected->kind == Json::Kind::array) {
							records = selected;
							break;
						}
				if (records->kind == Json::Kind::array) {
					for (auto& record : records->array)
						if (!visitor(std::move(record), record_count))
							break;
				} else
					visitor(std::move(*parsed), 0);
			}
		} else
			return unexpectedError(agentic_error(AgenticError::unavailable));
		if (!failure)
			return unexpectedError(failure.error());
		if (offset > record_count)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		return page;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
namespace {
struct NativeTimingAggregate {
	Json selector;
	std::string zone_id;
	std::string window_id;
	std::string track;
	std::string device_id;
	std::string queue_id;
	std::uint64_t count = 0, observed_count = 0, excluded_count = 0, inclusive_ns = 0,
				  exclusive_ns = 0;
	std::uint64_t minimum_ns = UINT64_MAX, maximum_ns = 0, first_tick = UINT64_MAX, last_tick = 0;
	std::uint64_t uncalibrated = 0, truncated = 0;
	bool gpu = false, exclusive_complete = true;
};
struct NativeGpuSample {
	Json selector;
	std::string zone_id, window_id, device_id, queue_id;
	std::uint64_t index = 0, parent = UINT32_MAX, duration = 0, child_ns = 0, tick = 0;
	std::uint64_t flags = 0, depth = 0;
	bool valid = false, exclusive_complete = true;
};
[[nodiscard]] std::string bounded_native_text(std::string_view text, std::size_t maximum) {
	auto length = std::min(text.size(), maximum);
	while (length && length < text.size() &&
		   (static_cast<unsigned char>(text[length]) & 0xc0u) == 0x80u)
		--length;
	return std::string(text.substr(0, length));
}
[[nodiscard]] std::uint64_t native_unsigned(const Json& row, std::string_view key,
											std::uint64_t fallback = 0) noexcept {
	const auto* value = row.find(key);
	return value ? value->uint64_or(fallback) : fallback;
}
[[nodiscard]] Json native_locator(const Json& row, bool gpu) {
	Json selector = Json::make_object({{"artifact_id", "performance_timing"},
									   {"record_kind", gpu ? "gpu_sample" : "cpu_sample"}});
	for (const auto key : {"app_tick", "window_id", "frame_number", "record_index",
						   "capture_generation", "zone_type_id"})
		if (const auto* value = row.find(key))
			selector.set(key == std::string_view("frame_number") ? "window_frame" : key, *value);
	if (gpu)
		for (const auto key :
			 {"submission_serial", "device_identity", "queue_identity", "zone_index"}) {
			if (const auto* value = row.find(key))
				selector.set(std::string(key), *value);
		}
	else if (const auto* invocation = row.find("invocation_id"))
		selector.set("invocation_id", *invocation);
	return selector;
}
[[nodiscard]] Json native_source(const Json& descriptor) {
	const auto file = member_string(descriptor, "source_file");
	bool relative = !file.empty();
	const std::filesystem::path path(file);
	if (path.is_absolute() || file.find('\\') != std::string::npos ||
		file.find(':') != std::string::npos)
		relative = false;
	for (const auto& component : path)
		if (component == "..")
			relative = false;
	Json result = Json::make_object(
		{{"repository_id", "application_or_flowui"},
		 {"file", relative ? Json(bounded_native_text(file, 1024)) : Json{}},
		 {"symbol", bounded_native_text(member_string(descriptor, "source_function"), 2048)},
		 {"line", descriptor.find("source_line") ? *descriptor.find("source_line") : Json{}},
		 {"column", descriptor.find("source_column") ? *descriptor.find("source_column") : Json{}},
		 {"locator_available", relative},
		 {"source_available", false},
		 {"matching_checkout_verified", false}});
	result.set("limitation", relative ? "matching_source_checkout_not_verified"
									  : "matching_repository_relative_source_unavailable");
	return result;
}
} // namespace
Result<Json> analyze_timing_artifact(const AgenticManifest& manifest,
									 std::optional<std::uint64_t> window_id, std::uint32_t top,
									 bool include_source, std::uint64_t maximum_bytes) noexcept {
	try {
		if (top == 0 || top > 100 || maximum_bytes < 512 || maximum_bytes > 16u * 1024u * 1024u)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const auto artifact = std::find_if(
			manifest.artifacts.begin(), manifest.artifacts.end(),
			[](const auto& value) { return value.artifact_id == "performance_timing"; });
		if (artifact == manifest.artifacts.end())
			return unexpectedError(agentic_error(AgenticError::unavailable));
		if (artifact->schema != "FlowUi.PerformanceCaptureCSV" || artifact->schema_version != "1")
			return unexpectedError(agentic_error(AgenticError::incompatible));
		auto bytes = read_artifact(manifest, "performance_timing", 256u * 1024u * 1024u);
		if (!bytes)
			return unexpectedError(bytes.error());
		std::map<std::string, Json> descriptors;
		std::map<std::string, std::size_t> group_indices;
		std::vector<NativeTimingAggregate> groups;
		groups.reserve(1024);
		std::vector<NativeGpuSample> gpu_frame;
		gpu_frame.reserve(256);
		std::string gpu_frame_key;
		std::uint64_t cpu_exclusive_total = 0, gpu_exclusive_total = 0, excluded_cpu = 0,
					  excluded_gpu = 0;
		Status failure;
		bool gpu_hierarchy_complete = true;
		const auto sum = [&](std::uint64_t& total, std::uint64_t value) {
			if (value > UINT64_MAX - total) {
				failure = unexpectedError(agentic_error(AgenticError::budget_exhausted));
				return false;
			}
			total += value;
			return true;
		};
		const auto aggregate = [&](std::string zone, std::string window, std::string track,
								   std::string device, std::string queue, bool gpu, bool valid,
								   std::uint64_t duration, std::uint64_t exclusive,
								   std::uint64_t tick, std::uint64_t flags, bool exclusive_complete,
								   Json selector) {
			const auto key = std::string(gpu ? "gpu:" : "cpu:") + zone + ':' + window + ':' +
							 track + ':' + device + ':' + queue;
			auto present = group_indices.find(key);
			if (present == group_indices.end()) {
				if (groups.size() >= 16384) {
					failure = unexpectedError(agentic_error(AgenticError::budget_exhausted));
					return;
				}
				const auto index = groups.size();
				group_indices.emplace(key, index);
				NativeTimingAggregate group;
				group.zone_id = std::move(zone);
				group.window_id = std::move(window);
				group.track = std::move(track);
				group.device_id = std::move(device);
				group.queue_id = std::move(queue);
				group.gpu = gpu;
				group.selector = std::move(selector);
				groups.emplace_back(std::move(group));
				present = group_indices.find(key);
			}
			auto& group = groups[present->second];
			++group.observed_count;
			if (!valid) {
				++group.excluded_count;
				if (gpu)
					++excluded_gpu;
				else
					++excluded_cpu;
				return;
			}
			if (group.count == 0 && group.observed_count > 1)
				group.selector = std::move(selector);
			++group.count;
			if (!sum(group.inclusive_ns, duration) || !sum(group.exclusive_ns, exclusive))
				return;
			group.minimum_ns = std::min(group.minimum_ns, duration);
			group.maximum_ns = std::max(group.maximum_ns, duration);
			group.first_tick = std::min(group.first_tick, tick);
			group.last_tick = std::max(group.last_tick, tick);
			group.exclusive_complete = group.exclusive_complete && exclusive_complete;
			if (gpu && (flags & 2u))
				++group.uncalibrated;
			if ((gpu && (flags & 4u)) || (!gpu && (flags & 128u)))
				++group.truncated;
			if (gpu)
				sum(gpu_exclusive_total, exclusive);
			else
				sum(cpu_exclusive_total, exclusive);
		};
		const auto flush_gpu = [&] {
			std::map<std::uint64_t, std::size_t> indices;
			for (std::size_t index = 0; index < gpu_frame.size(); ++index)
				if (!indices.emplace(gpu_frame[index].index, index).second) {
					gpu_frame[index].exclusive_complete = false;
					gpu_hierarchy_complete = false;
				}
			for (auto& child : gpu_frame)
				if (child.valid && child.depth != 0) {
					const auto parent = indices.find(child.parent);
					if (parent != indices.end()) {
						auto& owner = gpu_frame[parent->second];
						if (!owner.valid || owner.depth + 1 != child.depth) {
							child.exclusive_complete = false;
							gpu_hierarchy_complete = false;
						}
						if (!sum(owner.child_ns, child.duration))
							return;
					} else {
						child.exclusive_complete = false;
						gpu_hierarchy_complete = false;
					}
				}
			for (auto& sample : gpu_frame) {
				const auto exclusive =
					sample.child_ns <= sample.duration ? sample.duration - sample.child_ns : 0;
				const bool complete = sample.exclusive_complete &&
									  sample.child_ns <= sample.duration && !(sample.flags & 4u);
				if (!complete)
					gpu_hierarchy_complete = false;
				aggregate(std::move(sample.zone_id), std::move(sample.window_id), {},
						  std::move(sample.device_id), std::move(sample.queue_id), true,
						  sample.valid, sample.duration, exclusive, sample.tick, sample.flags,
						  complete, std::move(sample.selector));
			}
			gpu_frame.clear();
			gpu_frame_key.clear();
		};
		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		auto visited = visit_csv(text, 1000000, [&](Json row, std::uint64_t) {
			if (const auto* version = row.find("schema_version");
				!version || version->string_or() != "1") {
				failure = unexpectedError(agentic_error(AgenticError::incompatible));
				return false;
			}
			const auto kind = member_string(row, "record_kind");
			if (kind == "zone_descriptor") {
				if (descriptors.size() >= 16384) {
					failure = unexpectedError(agentic_error(AgenticError::budget_exhausted));
					return false;
				}
				descriptors.emplace(member_string(row, "zone_type_id"), std::move(row));
				return true;
			}
			if (kind != "cpu_sample" && kind != "gpu_sample")
				return true;
			for (const auto key : {"zone_type_id", "app_tick", "window_id", "frame_number",
								   "capture_generation", "duration_ns", "quality_flags", "depth"}) {
				const auto* value = row.find(key);
				if (!value || !parse_decimal(*value)) {
					failure = unexpectedError(agentic_error(AgenticError::integrity_failed));
					return false;
				}
			}
			if (kind == "cpu_sample") {
				for (const auto key :
					 {"exclusive_ns", "track", "invocation_id", "parent_invocation_id"}) {
					const auto* value = row.find(key);
					if (!value || !parse_decimal(*value)) {
						failure = unexpectedError(agentic_error(AgenticError::integrity_failed));
						return false;
					}
				}
			} else {
				for (const auto key :
					 {"zone_index", "parent_zone_index", "submission_serial", "device_identity",
					  "queue_identity", "timestamp_valid_bits"}) {
					const auto* value = row.find(key);
					if (!value || !parse_decimal(*value)) {
						failure = unexpectedError(agentic_error(AgenticError::integrity_failed));
						return false;
					}
				}
			}
			if (const auto* measurement = manifest.document.find("measurement")) {
				const auto* generation = measurement->find("timing_capture_generation");
				if (generation && generation->uint64_or() != 0 &&
					generation->uint64_or() != native_unsigned(row, "capture_generation")) {
					failure = unexpectedError(agentic_error(AgenticError::stale_generation));
					return false;
				}
				const auto* first = measurement->find("first_app_tick");
				const auto* end = measurement->find("end_app_tick_exclusive");
				const auto tick = native_unsigned(row, "app_tick");
				if (first && end && (tick < first->uint64_or() || tick >= end->uint64_or())) {
					failure = unexpectedError(agentic_error(AgenticError::invalid_scope));
					return false;
				}
			}
			const auto window = native_unsigned(row, "window_id");
			if (window_id && window != 0 && window != *window_id)
				return true;
			const auto duration = native_unsigned(row, "duration_ns"),
					   tick = native_unsigned(row, "app_tick"),
					   flags = native_unsigned(row, "quality_flags");
			if (kind == "cpu_sample") {
				const auto exclusive = native_unsigned(row, "exclusive_ns");
				const bool valid = (flags & 1u) && !(flags & 126u) && exclusive <= duration;
				aggregate(member_string(row, "zone_type_id"), member_string(row, "window_id"),
						  member_string(row, "track"), {}, {}, false, valid, duration, exclusive,
						  tick, flags, !(flags & 128u), native_locator(row, false));
			} else {
				const auto key = member_string(row, "app_tick") + ':' +
								 member_string(row, "window_id") + ':' +
								 member_string(row, "frame_number") + ':' +
								 member_string(row, "device_identity") + ':' +
								 member_string(row, "queue_identity") + ':' +
								 member_string(row, "submission_serial");
				if (!gpu_frame_key.empty() && key != gpu_frame_key)
					flush_gpu();
				gpu_frame_key = key;
				if (gpu_frame.size() >= 65536) {
					failure = unexpectedError(agentic_error(AgenticError::budget_exhausted));
					return false;
				}
				NativeGpuSample sample;
				sample.zone_id = member_string(row, "zone_type_id");
				sample.window_id = member_string(row, "window_id");
				sample.device_id = member_string(row, "device_identity");
				sample.queue_id = member_string(row, "queue_identity");
				sample.index = native_unsigned(row, "zone_index", UINT32_MAX);
				sample.parent = native_unsigned(row, "parent_zone_index", UINT32_MAX);
				sample.depth = native_unsigned(row, "depth");
				sample.duration = duration;
				sample.tick = tick;
				sample.flags = flags;
				sample.valid = (flags & 1u) && native_unsigned(row, "timestamp_valid_bits") > 0;
				sample.selector = native_locator(row, true);
				gpu_frame.emplace_back(std::move(sample));
			}
			return bool(failure);
		});
		if (!visited)
			return unexpectedError(visited.error());
		if (!failure)
			return unexpectedError(failure.error());
		flush_gpu();
		if (!failure)
			return unexpectedError(failure.error());
		std::stable_sort(groups.begin(), groups.end(), [](const auto& first, const auto& second) {
			if (first.gpu != second.gpu)
				return first.gpu < second.gpu;
			if (first.exclusive_ns != second.exclusive_ns)
				return first.exclusive_ns > second.exclusive_ns;
			return first.zone_id < second.zone_id;
		});
		Json cpu = Json::make_array(), gpu = Json::make_array();
		Json roles = Json::make_object();
		std::uint64_t cpu_group_count = 0, gpu_group_count = 0;
		constexpr std::array<std::string_view, 5> role_names{"work", "wait", "gap", "gpu_work",
															 "developer_tool_work"};
		constexpr std::array<std::string_view, 11> category_names{
			"lifecycle",	"frame", "input", "element", "layout",		  "prepare",
			"renderer_cpu", "gpu",	 "wait",  "user",	 "developer_tool"};
		for (const auto& group : groups) {
			auto& list = group.gpu ? gpu : cpu;
			if (group.gpu)
				++gpu_group_count;
			else
				++cpu_group_count;
			const auto descriptor = descriptors.find(group.zone_id);
			const auto* metadata = descriptor == descriptors.end() ? nullptr : &descriptor->second;
			const auto role =
				metadata ? native_unsigned(*metadata, "zone_role", UINT64_MAX) : UINT64_MAX;
			const auto category =
				metadata ? native_unsigned(*metadata, "zone_category", UINT64_MAX) : UINT64_MAX;
			const auto role_name = role < role_names.size()
									   ? role_names[static_cast<std::size_t>(role)]
									   : std::string_view("unknown");
			if (!group.gpu) {
				auto* accumulated = roles.find(role_name);
				const auto previous = accumulated ? accumulated->uint64_or() : 0;
				if (group.exclusive_ns > UINT64_MAX - previous)
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				roles.set(std::string(role_name), decimal(previous + group.exclusive_ns));
			}
			if (list.array.size() >= top)
				continue;
			Json record = Json::make_object(
				{{"zone_type_id", group.zone_id},
				 {"name",
				  metadata ? Json(bounded_native_text(member_string(*metadata, "zone_name"), 512))
						   : Json("descriptor_unavailable")},
				 {"scope", group.window_id == "0" ? "shared_application" : "selected_window"},
				 {"window_id", group.window_id},
				 {"role", role_name},
				 {"category", category < category_names.size()
								  ? category_names[static_cast<std::size_t>(category)]
								  : std::string_view("unknown")},
				 {"count", decimal(group.count)},
				 {"observed_count", decimal(group.observed_count)},
				 {"excluded_count", decimal(group.excluded_count)},
				 {"inclusive_total_ns", decimal(group.inclusive_ns)},
				 {"observed_exclusive_total_ns", decimal(group.exclusive_ns)},
				 {"exclusive_complete", group.exclusive_complete},
				 {"minimum_inclusive_ns", group.count ? decimal(group.minimum_ns) : Json{}},
				 {"maximum_inclusive_ns", group.count ? decimal(group.maximum_ns) : Json{}},
				 {"mean_inclusive_ns",
				  group.count ? Json(static_cast<double>(group.inclusive_ns) / group.count)
							  : Json{}},
				 {"mean_observed_exclusive_ns",
				  group.count ? Json(static_cast<double>(group.exclusive_ns) / group.count)
							  : Json{}},
				 {"first_app_tick", group.count ? decimal(group.first_tick) : Json{}},
				 {"last_app_tick", group.count ? decimal(group.last_tick) : Json{}},
				 {"detail_truncated_samples", decimal(group.truncated)},
				 {"evidence", group.selector}});
			if (group.gpu) {
				record.set("device_id", group.device_id);
				record.set("queue_id", group.queue_id);
				record.set("uncalibrated_samples", decimal(group.uncalibrated));
			} else
				record.set("track", group.track);
			if (include_source)
				record.set("source", metadata ? native_source(*metadata)
											  : Json::make_object(
													{{"source_available", false},
													 {"limitation", "descriptor_unavailable"}}));
			list.array.emplace_back(std::move(record));
		}
		Json result = Json::make_object(
			{{"schema", "FlowUi.Agentic.TimingContributors"},
			 {"schema_version", "1.0"},
			 {"algorithm_version", "native_observed_exclusive_1"},
			 {"capture_id", manifest.capture_id},
			 {"manifest_sha256", manifest.digest},
			 {"artifact_id", "performance_timing"},
			 {"ranking", "observed_exclusive_total_ns_descending_per_cpu_or_gpu_domain"},
			 {"cpu", std::move(cpu)},
			 {"gpu", std::move(gpu)},
			 {"cpu_group_count", decimal(cpu_group_count)},
			 {"gpu_group_count", decimal(gpu_group_count)},
			 {"cpu_observed_exclusive_total_ns", decimal(cpu_exclusive_total)},
			 {"gpu_observed_exclusive_total_ns",
			  gpu_hierarchy_complete ? decimal(gpu_exclusive_total) : Json{}},
			 {"gpu_exclusive_complete", gpu_hierarchy_complete},
			 {"cpu_roles_exclusive_ns", std::move(roles)},
			 {"excluded_cpu_samples", decimal(excluded_cpu)},
			 {"excluded_gpu_samples", decimal(excluded_gpu)},
			 {"truncated", cpu_group_count > top || gpu_group_count > top},
			 {"limitations",
			  Json::make_array({"retained_scope_costs_do_not_establish_causality",
								"exclusive_durations_use_retained_direct_children",
								"CPU_threads_and_GPU_queues_can_overlap; totals_are_not_wall_time",
								"GPU_duration_does_not_imply_CPU_timeline_calibration",
								"zone_type_ids_are_build_scoped; "
								"cross_revision_matching_requires_semantics_and_sources"})}});
		while (write_json(result).size() > maximum_bytes) {
			auto* cpu_rows = result.find("cpu");
			auto* gpu_rows = result.find("gpu");
			if (cpu_rows->array.empty() && gpu_rows->array.empty())
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
			if (cpu_rows->array.size() >= gpu_rows->array.size() && !cpu_rows->array.empty())
				cpu_rows->array.pop_back();
			else
				gpu_rows->array.pop_back();
			result.set("truncated", true);
		}
		return result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::integrity_failed));
	}
}
Result<std::filesystem::path> publish_comparison(const AgenticManifest& baseline,
												 const AgenticManifest& candidate,
												 const Json& comparison) noexcept {
	try {
		const auto captures = candidate.manifest_path.parent_path().parent_path();
		if (captures.filename() != "captures")
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		StoreLock publication_lock(captures.parent_path().parent_path().parent_path(),
								   ".publication.lock");
		if (!publication_lock.acquired)
			return unexpectedError(agentic_error(AgenticError::busy));
		const auto parent = captures.parent_path() / "comparisons";
		std::filesystem::create_directories(parent);
		auto identifier = make_identifier("comparison");
		if (!identifier)
			return unexpectedError(identifier.error());
		const auto staging = parent / ("." + *identifier + ".pending");
		const auto final = parent / *identifier;
		if (!std::filesystem::create_directory(staging))
			return unexpectedError(agentic_error(AgenticError::busy));
		Json result = comparison;
		result.set("baseline_capture_id", baseline.capture_id);
		result.set("candidate_capture_id", candidate.capture_id);
		result.set("baseline_manifest_sha256", baseline.digest);
		result.set("candidate_manifest_sha256", candidate.digest);
		const auto text = write_json(result);
		auto written = write_text(staging / "comparison.json", text);
		if (!written)
			return unexpectedError(written.error());
		const auto summary = "# Agentic comparison\n\nBaseline: " + baseline.capture_id +
							 ". Candidate: " + candidate.capture_id +
							 ".\n\n[Full comparison](comparison.json)\n";
		written = write_text(staging / "summary.md", summary);
		if (!written)
			return unexpectedError(written.error());
		auto manifest = Json::make_object(
			{{"schema", "FlowUi.Agentic.ComparisonManifest"},
			 {"schema_version", "1.0"},
			 {"comparison_id", *identifier},
			 {"baseline_manifest_sha256", baseline.digest},
			 {"candidate_manifest_sha256", candidate.digest},
			 {"artifacts",
			  Json::make_array({Json::make_object({{"path", "comparison.json"},
												   {"sha256", sha256(text)},
												   {"bytes", decimal(text.size())}}),
								Json::make_object({{"path", "summary.md"},
												   {"sha256", sha256(summary)},
												   {"bytes", decimal(summary.size())}})})}});
		written = write_text(staging / "manifest.json", write_json(manifest));
		if (!written)
			return unexpectedError(written.error());
		std::filesystem::rename(staging, final);
		return final / "manifest.json";
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
} // namespace FlowUi::devSystems::agentic
