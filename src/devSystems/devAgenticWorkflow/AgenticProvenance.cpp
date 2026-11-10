#include "devSystems/devAgenticWorkflow/AgenticProvenance.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticAdapters.hpp"
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/Resources.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <set>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif
#else
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif
#endif

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] std::string_view string_field(const Json& value, std::string_view key) noexcept {
	const auto* field = value.find(key);
	return field ? field->string_or() : std::string_view{};
}
void limitation(AgenticCaptureSnapshot& snapshot, std::string_view reason) {
	if (std::none_of(snapshot.limitations.begin(), snapshot.limitations.end(),
					 [&](const auto& value) { return value == reason; }))
		snapshot.limitations.emplace_back(reason);
}
[[nodiscard]] bool known(const Json* value) noexcept {
	return value && value->kind != Json::Kind::null &&
		   !(value->kind == Json::Kind::string &&
			 (value->text.empty() || value->text == "unknown")) &&
		   !(value->kind == Json::Kind::object && value->object.empty());
}
void erase_keys(Json& value, std::span<const std::string_view> excluded) {
	if (value.kind == Json::Kind::object) {
		std::erase_if(value.object, [&](const auto& member) {
			return std::find(excluded.begin(), excluded.end(), member.first) != excluded.end();
		});
		for (auto& member : value.object)
			erase_keys(member.second, excluded);
	}
	for (auto& member : value.array)
		erase_keys(member, excluded);
}
[[nodiscard]] bool contains_unavailable(const Json& value) noexcept {
	if (const auto* available = value.find("available");
		available && available->kind == Json::Kind::boolean && !available->boolean)
		return true;
	if (const auto* collection = value.find("collection_available");
		collection && !collection->bool_or())
		return true;
	if (string_field(value, "status") == "schema_unavailable")
		return true;
	for (const auto& member : value.array)
		if (contains_unavailable(member))
			return true;
	for (const auto& member : value.object)
		if (contains_unavailable(member.second))
			return true;
	return false;
}
[[nodiscard]] Json selected_fields(const Json& input,
								   std::initializer_list<std::string_view> fields) {
	Json result = Json::make_object();
	for (const auto field : fields)
		if (const auto* value = input.find(field))
			result.set(std::string(field), *value);
	return result;
}
[[nodiscard]] Result<std::string> hash_file(const std::filesystem::path& path,
											std::uint64_t maximum_bytes,
											std::uint64_t& total_bytes) {
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if (error || size > maximum_bytes || total_bytes > maximum_bytes - size ||
		size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
		return unexpectedError(
			agentic_error(error ? AgenticError::unavailable : AgenticError::budget_exhausted));
	const auto before = std::filesystem::last_write_time(path, error);
	if (error)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
	if (!input || input.peek() != std::char_traits<char>::eof() ||
		std::filesystem::last_write_time(path, error) != before || error)
		return unexpectedError(agentic_error(AgenticError::stale_generation));
	total_bytes += size;
	return sha256(bytes);
}
[[nodiscard]] std::filesystem::path process_executable() {
#if defined(_WIN32)
	std::wstring path(32768, L'\0');
	const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
	if (length == 0 || length >= path.size())
		return {};
	path.resize(length);
	return std::filesystem::path(path);
#elif defined(__linux__)
	return std::filesystem::path("/proc/self/exe");
#elif defined(__APPLE__)
	std::uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	if (size == 0 || size > 32768)
		return {};
	std::vector<char> path(size);
	if (_NSGetExecutablePath(path.data(), &size) != 0)
		return {};
	return std::filesystem::path(path.data());
#else
	return {};
#endif
}
[[nodiscard]] std::string cpu_model() {
#if defined(__linux__)
	std::ifstream input("/proc/cpuinfo");
	std::uint64_t bytes = 0;
	for (std::string line; std::getline(input, line) && bytes < 65536;) {
		bytes += line.size();
		const auto separator = line.find(':');
		if (separator == std::string::npos)
			continue;
		const auto key = line.substr(0, separator);
		if (key.find("model name") == std::string::npos &&
			key.find("Hardware") == std::string::npos)
			continue;
		auto value = line.substr(separator + 1, 1024);
		const auto first = value.find_first_not_of(" \t");
		if (first != std::string::npos)
			return value.substr(first);
	}
#elif defined(__APPLE__)
	std::array<char, 256> value{};
	std::size_t size = value.size();
	if (sysctlbyname("machdep.cpu.brand_string", value.data(), &size, nullptr, 0) == 0 && size > 0)
		return std::string(value.data());
	size = value.size();
	if (sysctlbyname("hw.model", value.data(), &size, nullptr, 0) == 0 && size > 0)
		return std::string(value.data());
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	std::array<int, 4> words{};
	__cpuid(words.data(), static_cast<int>(0x80000000u));
	if (static_cast<unsigned>(words[0]) >= 0x80000004u) {
		std::array<char, 49> brand{};
		for (unsigned index = 0; index < 3; ++index) {
			__cpuid(words.data(), static_cast<int>(0x80000002u + index));
			std::copy_n(reinterpret_cast<const char*>(words.data()), 16, brand.data() + index * 16);
		}
		return std::string(brand.data());
	}
#endif
	return {};
}
[[nodiscard]] Json host_system() {
	Json result = Json::make_object(
		{{"logical_processor_count", Json::number(std::thread::hardware_concurrency())},
		 {"cpu_model", cpu_model()}});
#if defined(_WIN32)
	SYSTEM_INFO system{};
	GetNativeSystemInfo(&system);
	result.set("os", "Windows");
	result.set("processor_architecture", Json::number(system.wProcessorArchitecture));
	result.set("os_release", "unknown");
#else
	struct utsname system{};
	if (::uname(&system) == 0) {
		result.set("os", system.sysname);
		result.set("os_release", system.release);
		result.set("architecture", system.machine);
	}
#endif
	return result;
}
[[nodiscard]] Json runtime_catalogue(const Json& catalogue) {
	Json result = Json::make_object();
	for (const auto group : {"images", "icons", "fonts"}) {
		Json records = Json::make_array();
		if (const auto* values = catalogue.find(group))
			for (const auto& value : values->array) {
				if (const auto* key = value.find("key")) {
					const auto* domain = key->find("domain");
					if (domain && domain->uint64_or() ==
									  static_cast<std::uint8_t>(ResourceDomain::Development))
						continue;
				}
				if (std::string_view(group) == "images")
					records.array.emplace_back(
						selected_fields(value, {"key", "width", "height", "source_vk_format",
												"anonymous", "status"}));
				else if (std::string_view(group) == "icons")
					records.array.emplace_back(
						selected_fields(value, {"key", "name", "width", "height", "vector_svg"}));
				else
					records.array.emplace_back(selected_fields(
						value, {"family_name", "face_name", "weight", "italic", "em_size_points",
								"ascent", "descent", "line_gap"}));
			}
		std::sort(records.array.begin(), records.array.end(),
				  [](const auto& first, const auto& second) {
					  return write_json(first) < write_json(second);
				  });
		result.set(group, std::move(records));
	}
	return result;
}
} // namespace
Status complete_provenance(AgenticCaptureSnapshot& snapshot, App& application,
						   DevMonitoringAndReporting& monitoring, DevTooling& tooling,
						   const ResourceConfig& resources) noexcept {
	try {
		auto& provenance = snapshot.provenance;
		const auto byte_budget = std::min<std::uint64_t>(
			snapshot.request.limits.maximum_snapshot_bytes / 2, 128u * 1024u * 1024u);
		if (byte_budget < 65536)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		provenance.set("provenance_algorithm", "runtime_inputs_v1");
		provenance.set("provenance_prepared_monotonic_ns", decimal(monotonic_nanoseconds()));
		Json profile =
			provenance.find("build_profile") ? *provenance.find("build_profile") : Json{};
		constexpr std::array excluded_build{std::string_view("flowui_revision"),
											std::string_view("application_revision"),
											std::string_view("binary_digest")};
		erase_keys(profile, excluded_build);
		Json application_profile = provenance.find("application_build_profile")
									   ? *provenance.find("application_build_profile")
									   : Json{};
		constexpr std::array excluded_application{
			std::string_view("revision"), std::string_view("source_revision"),
			std::string_view("binary_digest"), std::string_view("application_revision")};
		erase_keys(application_profile, excluded_application);
		if (known(&application_profile))
			provenance.set("application_build_profile_digest",
						   sha256(write_json(application_profile)));
		else {
			provenance.set("application_build_profile_digest", "unknown");
			limitation(snapshot, "application_build_profile_not_declared");
		}
		if (known(&profile) && known(&application_profile))
			provenance.set("build_profile_digest",
						   sha256(write_json(Json::make_object(
							   {{"library", profile}, {"application", application_profile}}))));
		else {
			provenance.set("build_profile_digest", "unknown");
			limitation(snapshot, "complete_build_profile_unavailable");
		}
		provenance.set("build_profile_scope",
					   "FlowUi_library_and_explicitly_declared_application_compile_configuration");
		Json hardware =
			provenance.find("hardware") ? *provenance.find("hardware") : Json::make_object();
		hardware.set("host", host_system());
		hardware.set("render_backend", "Vulkan");
		const auto* host = hardware.find("host");
		const bool hardware_known = hardware.find("device_id") && hardware.find("vendor_id") &&
									hardware.find("driver_version") && host &&
									!string_field(*host, "cpu_model").empty() &&
									!string_field(*host, "os").empty();
		provenance.set("hardware", hardware);
		if (hardware_known)
			provenance.set("hardware_digest", sha256(write_json(hardware)));
		else {
			provenance.set("hardware_digest", "unknown");
			limitation(snapshot, "complete_hardware_identity_unavailable");
		}
		Json windows = Json::make_array();
		bool settings_known = true;
		const auto registered = application.devWindowSnapshot();
		for (const auto window_id : snapshot.request.window_ids) {
			if (window_id > UINT32_MAX ||
				!application.hasWindow(static_cast<WindowId>(window_id))) {
				settings_known = false;
				continue;
			}
			const auto logical = application.windowSize(static_cast<WindowId>(window_id));
			const auto physical = application.framebufferSize(static_cast<WindowId>(window_id));
			auto window = Json::make_object(
				{{"window_id", decimal(window_id)},
				 {"logical_width",
				  Json::number(static_cast<std::uint64_t>(std::max(0, logical.first)))},
				 {"logical_height",
				  Json::number(static_cast<std::uint64_t>(std::max(0, logical.second)))},
				 {"physical_width",
				  Json::number(static_cast<std::uint64_t>(std::max(0, physical.first)))},
				 {"physical_height",
				  Json::number(static_cast<std::uint64_t>(std::max(0, physical.second)))}});
			if (logical.first <= 0 || logical.second <= 0 || physical.first <= 0 ||
				physical.second <= 0)
				settings_known = false;
			else {
				window.set("physical_pixels_per_logical_x",
						   Json(static_cast<double>(physical.first) / logical.first));
				window.set("physical_pixels_per_logical_y",
						   Json(static_cast<double>(physical.second) / logical.second));
			}
			const auto info =
				std::find_if(registered.begin(), registered.end(),
							 [&](const auto& value) { return value.id == window_id; });
			if (info != registered.end()) {
				window.set("frame_slots", Json::number(info->framesInFlight));
				window.set("present_mode", Json::number(info->actual_present_mode));
				window.set("layout_dpi", Json(static_cast<double>(info->layout_dpi)));
				window.set("font_scale", Json(static_cast<double>(info->font_scale)));
			} else
				settings_known = false;
			windows.array.emplace_back(std::move(window));
		}
		AgenticCaptureSnapshot tooling_snapshot;
		tooling_snapshot.run_id = snapshot.run_id;
		tooling_snapshot.request.limits = snapshot.request.limits;
		tooling_snapshot.request.limits.maximum_snapshot_bytes =
			std::min<std::uint64_t>(byte_budget, 16u * 1024u * 1024u);
		tooling_snapshot.request.limits.maximum_artifact_bytes =
			tooling_snapshot.request.limits.maximum_snapshot_bytes;
		tooling_snapshot.request.limits.maximum_records =
			std::min<std::uint64_t>(snapshot.request.limits.maximum_records, 10000);
		const auto tooling_copied =
			materialize_tooling(tooling_snapshot, tooling, application.themes());
		Json themes;
		Json catalogue;
		if (tooling_copied)
			for (const auto& artifact : tooling_snapshot.artifacts) {
				const auto parsed = parse_json(
					{reinterpret_cast<const char*>(artifact.bytes.data()), artifact.bytes.size()},
					16u * 1024u * 1024u);
				if (!parsed)
					continue;
				if (artifact.artifact_id == "visual_themes")
					themes = *parsed;
				else if (artifact.artifact_id == "visual_catalogue")
					catalogue = *parsed;
			}
		if (!known(&themes) || !known(&catalogue) || contains_unavailable(themes)) {
			settings_known = false;
			limitation(snapshot, "tooling_provenance_unavailable_or_over_budget");
		}
		constexpr std::array excluded_themes{std::string_view("run_id"),
											 std::string_view("theme_revision"),
											 std::string_view("schema_generation"),
											 std::string_view("schema_fingerprint"),
											 std::string_view("revision"),
											 std::string_view("source")};
		erase_keys(themes, excluded_themes);
		Json active_themes = Json::make_array();
		if (const auto* variants = themes.find("variants"))
			for (const auto& variant : variants->array)
				if (const auto* active = variant.find("active");
					active && active->bool_or() && active_themes.array.size() < 128)
					active_themes.array.emplace_back(
						selected_fields(variant, {"type_id", "type_name", "variant"}));
		Json settings = Json::make_object(
			{{"selected_windows", std::move(windows)},
			 {"theme_values_digest",
			  known(&themes) ? Json(sha256(write_json(themes))) : Json("unknown")},
			 {"active_themes", std::move(active_themes)}});
		if (const auto* declared = provenance.find("application_settings"))
			settings.set("application_settings", *declared);
		else
			limitation(snapshot, "application_settings_covered_only_by_declared_fixture");
		provenance.set("settings_scope",
					   "selected_native_geometry_present_mode_point_scale_and_reflected_theme_"
					   "values; application_settings_only_if_declared");
		provenance.set("settings", settings);
		provenance.set("settings_digest", settings_known && !snapshot.request.window_ids.empty()
											  ? Json(sha256(write_json(settings)))
											  : Json("unknown"));
		if (!settings_known)
			limitation(snapshot, "selected_window_or_theme_settings_unavailable");
		const auto instrumentation = Json::make_object(
			{{"cpu_timing_level",
			  Json::number(static_cast<std::uint64_t>(monitoring.timing().config().cpuLevel))},
			 {"error_level",
			  Json::number(static_cast<std::uint64_t>(monitoring.errors().config().level))},
			 {"memory_level",
			  Json::number(static_cast<std::uint64_t>(monitoring.memory().config().level))},
			 {"optional_inspection",
			  snapshot.request.profile != CaptureProfile::performance_verify}});
		provenance.set("measurement_instrumentation", instrumentation);
		Json instrumentation_identity = Json::make_object({{"measurement", instrumentation}});
		if (const auto* flags = provenance.find("capability_flags"))
			instrumentation_identity.set("compiled", *flags);
		if (const auto* build = provenance.find("build_profile"))
			if (const auto* validation = build->find("validation"))
				instrumentation_identity.set("validation", *validation);
		provenance.set("instrumentation_digest",
					   known(provenance.find("capability_flags"))
						   ? Json(sha256(write_json(instrumentation_identity)))
						   : Json("unknown"));
		std::uint64_t hashed_bytes = 0;
		Json files = Json::make_array();
		bool pack_complete = true;
		std::set<std::filesystem::path> hashed_paths;
		std::uint64_t missing_custom_sources = 0;
		const auto add_file = [&](std::string label, const std::filesystem::path& path,
								  bool required) {
			std::error_code error;
			const auto identity = std::filesystem::weakly_canonical(path, error);
			if (error || path.empty()) {
				if (required)
					pack_complete = false;
				return;
			}
			if (!hashed_paths.insert(identity).second)
				return;
			if (files.array.size() >= 128) {
				if (required)
					pack_complete = false;
				++missing_custom_sources;
				return;
			}
			auto digest = hash_file(path, byte_budget, hashed_bytes);
			if (!digest) {
				if (required)
					pack_complete = false;
				else
					++missing_custom_sources;
				return;
			}
			files.array.emplace_back(
				Json::make_object({{"resource", std::move(label)},
								   {"sha256", *digest},
								   {"bytes", decimal(std::filesystem::file_size(path))}}));
		};
		for (const auto name :
			 {"flowui_ui_textured.vert", "flowui_ui_textured.frag", "flowui_ui_solid.vert",
			  "flowui_ui_solid.frag", "flowui_ui_msdf.vert", "flowui_ui_msdf.frag"}) {
			const auto resource_name = "shaders/" + std::string(name) + ".spv";
			auto path = locate_resource(resource_name, resources);
			if (path)
				add_file(resource_name, *path, true);
			else
				pack_complete = false;
		}
		auto default_font = locate_resource("fonts/Inter.arfont", resources);
		if (default_font)
			add_file("fonts/Inter.arfont", *default_font, true);
		else
			pack_complete = false;
		for (const auto& entry : tooling.catalogues().queryFonts())
			if (!entry.sourcePath.empty())
				add_file("font:" + std::string(entry.familyName) + ":" +
							 std::string(entry.faceName),
						 path_from_utf8(entry.sourcePath), false);
		for (const auto& entry : tooling.catalogues().queryImages())
			if (entry.key.domain != ResourceDomain::Development) {
				std::error_code error;
				if (!entry.isAnonymous && std::filesystem::is_regular_file(
											  path_from_utf8(entry.sourcePathOrDebugName), error))
					add_file("image:" + std::string(entry.key.name),
							 path_from_utf8(entry.sourcePathOrDebugName), false);
				else
					++missing_custom_sources;
			}
		std::sort(files.array.begin(), files.array.end(),
				  [](const auto& first, const auto& second) {
					  return write_json(first) < write_json(second);
				  });
		Json resource_identity = Json::make_object(
			{{"files", std::move(files)},
			 {"runtime_catalogue_digest",
			  known(&catalogue) ? Json(sha256(write_json(runtime_catalogue(catalogue))))
								: Json("unknown")},
			 {"fixture",
			  Json::make_object({{"scenario_id", snapshot.request.scenario_id},
								 {"scenario_revision", snapshot.request.scenario_revision},
								 {"scenario_digest", snapshot.request.scenario_digest},
								 {"fixture_id", snapshot.request.fixture_id},
								 {"seed", decimal(snapshot.request.seed)}})}});
		provenance.set("resource_inputs", resource_identity);
		provenance.set(
			"resource_scope",
			"library_pack_and_catalogued_files_at_preparation; "
			"runtime_descriptors_and_application_declared_fixture; GPU_bytes_not_read_back");
		provenance.set("unhashed_custom_resource_count", decimal(missing_custom_sources));
		if (!pack_complete)
			limitation(snapshot, "library_resource_pack_incomplete_or_over_budget");
		if (missing_custom_sources)
			limitation(
				snapshot,
				"custom_resource_content_covered_only_by_declared_fixture_and_runtime_descriptors");
		const bool fixture_known =
			!snapshot.request.scenario_digest.empty() && !snapshot.request.fixture_id.empty();
		provenance.set("resource_digest", pack_complete && known(&catalogue) &&
												  (missing_custom_sources == 0 || fixture_known)
											  ? Json(sha256(write_json(resource_identity)))
											  : Json("unknown"));
		const auto executable = process_executable();
		std::uint64_t binary_bytes = 0;
		auto binary =
			executable.empty()
				? Result<std::string>(unexpectedError(agentic_error(AgenticError::unavailable)))
				: hash_file(executable, byte_budget, binary_bytes);
		provenance.set("binary_digest", binary ? Json(*binary) : Json("unknown"));
		provenance.set("binary_digest_algorithm", "sha256_exact_process_executable_bytes");
		if (!binary)
			limitation(snapshot, "binary_hash_unavailable_or_over_budget");
		if (!known(provenance.find("application_revision"))) {
			provenance.set("application_revision", "unknown");
			limitation(snapshot, "application_source_revision_not_declared");
		}
		if (!known(provenance.find("application_dirty_digest")))
			provenance.set("application_dirty_digest", "unknown");
		if (write_json(provenance).size() >
			std::min<std::uint64_t>(snapshot.request.limits.maximum_snapshot_bytes / 4,
									1024u * 1024u))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		return {};
	} catch (const FlowUiException& error) {
		return unexpectedError(error.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}
} // namespace FlowUi::devSystems::agentic
#endif
