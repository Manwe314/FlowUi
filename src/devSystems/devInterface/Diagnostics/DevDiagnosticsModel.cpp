#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsExport.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <tuple>
#include <utility>

namespace FlowUi::devSystems {
namespace {
using Row = DevDiagnosticsOccurrenceRow;
[[nodiscard]] bool in_window(uint64_t tick, uint64_t anchor, uint64_t width) noexcept {
	return tick <= anchor && anchor - tick < std::max(uint64_t{1}, width);
}
[[nodiscard]] bool mask_accepts(uint32_t mask, uint64_t value) noexcept {
	return mask == UINT32_MAX || (value < 32 && (mask & (uint32_t{1} << value)) != 0);
}
[[nodiscard]] std::string lower(std::string_view value) {
	std::string result(value);
	for (char& character : result)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return result;
}
void append_number(std::string& text, uint64_t value) {
	text += ' ';
	text += std::to_string(value);
	std::array<char, 16> buffer{};
	const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
	text += " 0x";
	text.append(buffer.data(), converted.ptr);
}
[[nodiscard]] bool search_matches(const DevDiagnosticsSnapshot& snapshot, const Row& row,
								  std::string_view query) {
	if (query.empty())
		return true;
	std::string text(row.error.descriptor().name);
	text += ' ';
	text += row.error.message();
	text += ' ';
	text += row.error.siteDescriptor().name;
	text += ' ';
	text += diagnostics_domain_name(row.error.domain());
	append_number(text, row.id);
	append_number(text, static_cast<uint64_t>(row.error.code));
	append_number(text, static_cast<uint64_t>(row.error.site));
	append_number(text, row.error.nativeCode);
	for (const auto& step : row.steps) {
		append_number(text, step.source_id);
		const auto source =
			std::ranges::find(snapshot.sources, step.source_id, &DevDiagnosticsSource::id);
		if (source != snapshot.sources.end()) {
			text += ' ';
			text += source->name;
			text += ' ';
			text += source->file;
			text += ' ';
			text += source->function;
		}
	}
	text = lower(text);
	const auto lowered_query = lower(query);
	size_t term_begin = 0;
	while (term_begin < lowered_query.size()) {
		while (term_begin < lowered_query.size() &&
			   std::isspace(static_cast<unsigned char>(lowered_query[term_begin])))
			++term_begin;
		if (term_begin == lowered_query.size())
			break;
		size_t term_end = term_begin;
		while (term_end < lowered_query.size() &&
			   !std::isspace(static_cast<unsigned char>(lowered_query[term_end])))
			++term_end;
		if (text.find(std::string_view(lowered_query).substr(term_begin, term_end - term_begin)) ==
			std::string::npos)
			return false;
		term_begin = term_end;
	}
	return true;
}
[[nodiscard]] bool accepts(const DevDiagnosticsState& state, const Row& row) {
	const auto& filters = state.filters;
	if (!mask_accepts(filters.domain_mask, static_cast<uint64_t>(row.error.domain())) ||
		!mask_accepts(filters.impact_mask, static_cast<uint64_t>(row.error.descriptor().impact)) ||
		!mask_accepts(filters.resolution_mask, static_cast<uint64_t>(row.resolution)) ||
		(filters.first_tick && row.first_app_tick < filters.first_tick) ||
		(filters.last_tick && row.first_app_tick > filters.last_tick))
		return false;
	const auto context_matches = [&](const DevDiagnosticsStepReference& step) {
		const auto window = step.context.frame.window;
		if ((filters.window_mode == 1 && window != filters.window) ||
			(filters.window_mode == 2 && window != InvalidWindowId))
			return false;
		if (filters.window_mode == 1 &&
			((filters.first_frame && step.context.frame.frameNumber < filters.first_frame) ||
			 (filters.last_frame && step.context.frame.frameNumber > filters.last_frame)))
			return false;
		if (filters.source_mode == 1 && step.source_id != filters.source)
			return false;
		if (filters.source_mode == 2 && step.source_id != 0 &&
			std::ranges::any_of(state.snapshot.sources,
								[&](const auto& source) { return source.id == step.source_id; }))
			return false;
		return true;
	};
	const bool context_filter = filters.window_mode || filters.source_mode;
	if (context_filter && !(row.steps.empty() ? filters.window_mode != 1 && filters.source_mode != 1
											  : std::ranges::any_of(row.steps, context_matches)))
		return false;
	return search_matches(state.snapshot, row, filters.search);
}
[[nodiscard]] auto observation_key(const Row& row, bool use_timestamps) noexcept {
	return std::tuple{use_timestamps ? row.first_timestamp_ns : row.first_app_tick, row.id};
}
[[nodiscard]] const Row* find_row(const DevDiagnosticsState& state, uint64_t id) noexcept {
	const auto found = std::ranges::find(state.snapshot.rows, id, &Row::id);
	return found == state.snapshot.rows.end() ? nullptr : &*found;
}
[[nodiscard]] bool group_less(const DevDiagnosticsGroup& left, const DevDiagnosticsGroup& right,
							  uint64_t order) {
	const auto left_domain = diagnostics_domain_name(left.error.domain());
	const auto right_domain = diagnostics_domain_name(right.error.domain());
	if (left_domain != right_domain)
		return left_domain < right_domain;
	if (order == 0 && left.error.descriptor().impact != right.error.descriptor().impact)
		return left.error.descriptor().impact > right.error.descriptor().impact;
	if (order == 2 && left.count != right.count)
		return left.count > right.count;
	if (order == 3 && left.error.descriptor().name != right.error.descriptor().name)
		return left.error.descriptor().name < right.error.descriptor().name;
	if (left.last_tick != right.last_tick)
		return left.last_tick > right.last_tick;
	return std::tie(left.key.code, left.key.site, left.key.nativeCode, left.key.sourceId) <
		   std::tie(right.key.code, right.key.site, right.key.nativeCode, right.key.sourceId);
}
void rebuild_history(DevDiagnosticsState& state) {
	state.history.clear();
	const auto width = std::max(uint64_t{1}, state.filters.histogram_ticks);
	const auto begin = state.anchor_tick >= width - 1 ? state.anchor_tick - width + 1 : 0;
	const auto extent = state.anchor_tick - begin;
	// Global loss counters cannot identify which tick intervals lost evidence.
	const bool incomplete = false;
	for (size_t bin_index = 0; bin_index < state.bins.size(); ++bin_index) {
		auto& bin = state.bins[bin_index];
		bin = {};
		bin.first_tick = begin + static_cast<uint64_t>((static_cast<long double>(extent) + 1) *
													   bin_index / state.bins.size());
		const auto next_offset = static_cast<uint64_t>((static_cast<long double>(extent) + 1) *
													   (bin_index + 1) / state.bins.size());
		bin.last_tick = bin_index + 1 == state.bins.size()	   ? state.anchor_tick
						: next_offset > bin.first_tick - begin ? begin + next_offset - 1
															   : bin.first_tick;
		bin.incomplete = incomplete;
	}
	const auto* group = selected_diagnostics_group(state);
	if (!group)
		return;
	state.history.reserve(group->occurrences.size());
	for (const auto occurrence_id : group->occurrences) {
		const auto* row = find_row(state, occurrence_id);
		if (!row)
			continue;
		DevDiagnosticsHistoryRow history{.id = row->id,
										 .first_tick = row->first_app_tick,
										 .resolution = row->resolution,
										 .evidence_available = !row->steps.empty()};
		if (!row->steps.empty()) {
			history.window = row->steps.front().context.frame.window;
			history.frame = row->steps.front().context.frame.frameNumber;
		}
		state.history.emplace_back(history);
		if (row->first_app_tick >= begin && row->first_app_tick <= state.anchor_tick) {
			const auto position = std::min(
				state.bins.size() - 1,
				static_cast<size_t>(static_cast<long double>(row->first_app_tick - begin) *
									state.bins.size() / (static_cast<long double>(extent) + 1)));
			++state.bins[position].count;
			state.bins[position].selected |= row->id == state.selected_id;
		}
	}
	const bool use_timestamps = std::ranges::all_of(
		state.snapshot.rows, [](const auto& row) { return row.first_timestamp_ns != 0; });
	std::ranges::sort(state.history, [&](const auto& left, const auto& right) {
		return observation_key(*find_row(state, left.id), use_timestamps) >
			   observation_key(*find_row(state, right.id), use_timestamps);
	});
}
void load_frozen_selection(DevDiagnosticsState& state) {
	const auto found =
		std::ranges::find(state.owned.occurrences, state.selected_id, &DevErrorOccurrence::id);
	if (found != state.owned.occurrences.end()) {
		state.selected = *found;
		state.selected_evicted = !find_row(state, state.selected_id);
	} else {
		state.selected.reset();
	}
}
void preserve_investigation(DevDiagnosticsInvestigation& destination,
							const DevDiagnosticsInvestigation& preserved) {
	const auto append_missing = [](auto& target, const auto& source, auto identity) {
		target.reserve(target.size() + source.size());
		for (const auto& entry : source)
			if (std::ranges::find(target, entry.*identity, identity) == target.end())
				target.emplace_back(entry);
	};
	append_missing(destination.snapshot.sources, preserved.snapshot.sources,
				   &DevDiagnosticsSource::id);
	append_missing(destination.snapshot.breadcrumb_descriptors,
				   preserved.snapshot.breadcrumb_descriptors,
				   &DevDiagnosticsBreadcrumbDescriptor::id);
	append_missing(destination.stacks, preserved.stacks, &DevErrorStackTrace::id);
	append_missing(destination.captures, preserved.captures, &DevErrorTriggeredCapture::id);
	destination.occurrences.reserve(destination.occurrences.size() + preserved.occurrences.size());
	destination.advice_text.reserve(destination.advice_text.size() + preserved.advice_text.size());
	for (const auto& occurrence : preserved.occurrences)
		destination.occurrences.emplace_back(occurrence);
	for (const auto& prose : preserved.advice_text)
		destination.advice_text.emplace_back(prose);
	if (!destination.fatal_capsule)
		destination.fatal_capsule = preserved.fatal_capsule;
	destination.bind_advice_text();
	destination.owned_bytes = diagnostics_owned_bytes(destination);
}
void refresh_selected(DevDiagnosticsState& state, DevErrorReporting& reporting) {
	if (!state.selected_id)
		return;
	if (state.frozen) {
		load_frozen_selection(state);
		return;
	}
	if (!find_row(state, state.selected_id)) {
		state.selected_evicted = true;
		return;
	}
	auto report = reporting.diagnostics_investigation(state.selected_id);
	if (!report) {
		state.message_is_error = true;
		state.message = "Could not read investigation: " + std::string(report.error().message());
		return;
	}
	const auto found =
		std::ranges::find(report->occurrences, state.selected_id, &DevErrorOccurrence::id);
	if (found == report->occurrences.end()) {
		state.selected_evicted = true;
		return;
	}
	state.owned = std::move(*report);
	state.selected =
		*std::ranges::find(state.owned.occurrences, state.selected_id, &DevErrorOccurrence::id);
	state.selected_evicted = false;
	if (state.message.starts_with("Could not read investigation:") ||
		state.message.starts_with("Could not refresh diagnostics:")) {
		state.message.clear();
		state.message_is_error = false;
	}
	const bool step_missing =
		state.focus_kind == 1 && !std::ranges::any_of(state.selected->steps, [&](const auto& step) {
			return step.sequence == state.focus_sequence;
		});
	const bool breadcrumb_missing =
		state.focus_kind == 2 &&
		!std::ranges::any_of(state.selected->breadcrumbs, [&](const auto& breadcrumb) {
			return breadcrumb.sequence == state.focus_sequence;
		});
	if (step_missing || breadcrumb_missing) {
		state.focus_kind = 0;
		state.focus_sequence = 0;
		state.message = "Focused evidence is no longer retained; showing the first retained step.";
		state.message_is_error = true;
	}
}
} // namespace

DevErrorGroupKey diagnostics_group_key(const Row& row, DevErrorGroupMode mode) noexcept {
	return {.code = row.error.code,
			.site = row.error.site,
			.nativeCode = mode == DevErrorGroupMode::Native ? row.error.nativeCode : 0,
			.sourceId = mode == DevErrorGroupMode::Source && !row.steps.empty()
							? row.steps.front().source_id
							: 0};
}
void rebuild_diagnostics_model(DevDiagnosticsState& state) {
	auto previous = std::move(state.groups);
	state.groups.clear();
	state.groups.reserve(state.snapshot.rows.size());
	state.group_identities.reserve(state.group_identities.size() + state.snapshot.rows.size());
	state.anchor_tick = state.snapshot.status.lastConsumedAppTick;
	const auto mode = static_cast<DevErrorGroupMode>(std::min(state.filters.grouping, uint64_t{2}));
	std::vector<DevErrorGroupKey> all_keys;
	all_keys.reserve(state.snapshot.rows.size());
	const bool use_timestamps = std::ranges::all_of(
		state.snapshot.rows, [](const auto& row) { return row.first_timestamp_ns != 0; });
	for (const auto& row : state.snapshot.rows) {
		const auto key = diagnostics_group_key(row, mode);
		if (std::ranges::find(all_keys, key) == all_keys.end())
			all_keys.emplace_back(key);
		if (!accepts(state, row) ||
			(state.filters.activity_view == 2 &&
			 !in_window(row.first_app_tick, state.anchor_tick, state.filters.recent_ticks)))
			continue;
		auto found = std::ranges::find(state.groups, key, &DevDiagnosticsGroup::key);
		if (found == state.groups.end()) {
			DevDiagnosticsGroup group{.error = row.error,
									  .key = key,
									  .first_tick = row.first_app_tick,
									  .last_tick = row.first_app_tick,
									  .first_timestamp_ns = row.first_timestamp_ns,
									  .last_timestamp_ns = row.first_timestamp_ns,
									  .latest = row.id,
									  .first = row.id,
									  .mode = mode};
			const auto old = std::ranges::find_if(state.group_identities, [&](const auto& value) {
				return value.key == key && value.mode == mode;
			});
			if (old != state.group_identities.end())
				group.identity = old->identity;
			else {
				group.identity = state.next_identity++;
				state.group_identities.emplace_back(
					DevDiagnosticsIdentity{key, group.identity, mode});
			}
			state.groups.emplace_back(std::move(group));
			found = state.groups.end() - 1;
		}
		found->occurrences.emplace_back(row.id);
		++found->count;
		found->first_tick = std::min(found->first_tick, row.first_app_tick);
		found->last_tick = std::max(found->last_tick, row.first_app_tick);
		if (row.first_timestamp_ns) {
			found->first_timestamp_ns =
				found->first_timestamp_ns
					? std::min(found->first_timestamp_ns, row.first_timestamp_ns)
					: row.first_timestamp_ns;
			found->last_timestamp_ns = std::max(found->last_timestamp_ns, row.first_timestamp_ns);
		}
		if (observation_key(row, use_timestamps) >
			observation_key(*find_row(state, found->latest), use_timestamps))
			found->latest = row.id;
		if (observation_key(row, use_timestamps) <
			observation_key(*find_row(state, found->first), use_timestamps))
			found->first = row.id;
		found->ongoing |=
			in_window(row.first_app_tick, state.anchor_tick, state.filters.ongoing_ticks);
		found->recent_count +=
			in_window(row.first_app_tick, state.anchor_tick, state.filters.ongoing_ticks);
		const auto resolution = static_cast<size_t>(row.resolution);
		if (resolution < found->outcomes.size())
			++found->outcomes[resolution];
	}
	state.total_groups = all_keys.size();
	std::erase_if(state.group_identities, [&](const auto& identity) {
		return !std::ranges::any_of(state.snapshot.rows, [&](const auto& row) {
			return diagnostics_group_key(row, identity.mode) == identity.key;
		});
	});
	if (state.filters.activity_view == 1)
		std::erase_if(state.groups, [](const auto& group) { return !group.ongoing; });
	for (auto& group : state.groups) {
		if (std::ranges::any_of(group.occurrences, [&](const auto id) {
				return find_row(state, id)->first_timestamp_ns == 0;
			}))
			group.first_timestamp_ns = group.last_timestamp_ns = 0;
		std::vector<uint64_t> windows, subjects;
		windows.reserve(group.count);
		subjects.reserve(group.count);
		for (const auto id : group.occurrences) {
			const auto* row = find_row(state, id);
			if (row->error.subject)
				subjects.emplace_back(row->error.subject);
			for (const auto& step : row->steps)
				if (step.context.frame.window)
					windows.emplace_back(step.context.frame.window);
		}
		std::ranges::sort(windows);
		std::ranges::sort(subjects);
		group.window_count =
			static_cast<uint64_t>(std::ranges::unique(windows).begin() - windows.begin());
		group.subject_count =
			static_cast<uint64_t>(std::ranges::unique(subjects).begin() - subjects.begin());
	}
	const bool resort = state.refresh_order || !(state.filters == state.applied_filters);
	if (resort)
		std::ranges::sort(state.groups, [&](const auto& left, const auto& right) {
			return group_less(left, right, state.filters.order);
		});
	else {
		std::ranges::stable_sort(state.groups, [&](const auto& left, const auto& right) {
			const auto left_domain = diagnostics_domain_name(left.error.domain()),
					   right_domain = diagnostics_domain_name(right.error.domain());
			if (left_domain != right_domain)
				return left_domain < right_domain;
			const auto position = [&](const auto& group) {
				const auto found =
					std::ranges::find(previous, group.identity, &DevDiagnosticsGroup::identity);
				return found - previous.begin();
			};
			return position(left) < position(right);
		});
	}
	state.order_changed =
		!std::ranges::is_sorted(state.groups, [&](const auto& left, const auto& right) {
			return group_less(left, right, state.filters.order);
		});
	state.selected_outside_filters =
		state.selected_id && !std::ranges::any_of(state.groups, [&](const auto& group) {
			return std::ranges::find(group.occurrences, state.selected_id) !=
					   group.occurrences.end() &&
				   group.mode == static_cast<DevErrorGroupMode>(state.selected_mode);
		});
	if (const auto* group = selected_diagnostics_group(state)) {
		const auto* latest = find_row(state, group->latest);
		const auto* pinned = find_row(state, state.selected_id);
		const auto pinned_tick = pinned			  ? pinned->first_app_tick
								 : state.selected ? state.selected->firstAppTick
												  : 0;
		const auto pinned_timestamp = pinned		   ? pinned->first_timestamp_ns
									  : state.selected ? state.selected->firstTimestampNs
													   : 0;
		state.newer_available =
			latest && (latest->first_timestamp_ns && pinned_timestamp
						   ? std::tuple{latest->first_timestamp_ns, latest->id} >
								 std::tuple{pinned_timestamp, state.selected_id}
						   : std::tuple{latest->first_app_tick, latest->id} >
								 std::tuple{pinned_tick, state.selected_id});
		if (state.follow_latest && group->latest != state.selected_id) {
			state.selected_id = group->latest;
			state.focus_kind = 0;
			state.focus_sequence = 0;
			state.focused_advice.reset();
		}
	} else
		state.newer_available = false;
	rebuild_history(state);
	state.applied_filters = state.filters;
	state.refresh_order = false;
	state.dirty = false;
	++state.model_revision;
}
void refresh_diagnostics_model(DevDiagnosticsState& state, DevErrorReporting& reporting,
							   uint64_t now_ns, bool force) {
	if (!state.frozen &&
		(force || !state.initialized || now_ns - state.last_refresh_ns >= 200'000'000)) {
		auto snapshot = reporting.diagnostics_snapshot();
		if (!snapshot) {
			state.message_is_error = true;
			state.message =
				"Could not refresh diagnostics: " + std::string(snapshot.error().message());
			return;
		}
		state.snapshot = std::move(*snapshot);
		state.last_refresh_ns = now_ns;
		state.refreshed_revision = state.snapshot.revision;
		state.initialized = true;
		if (!state.show_trigger)
			state.capture_level = static_cast<uint64_t>(state.snapshot.capture_config.level);
		rebuild_diagnostics_model(state);
		refresh_selected(state, reporting);
	} else if (state.dirty || !(state.filters == state.applied_filters) || state.refresh_order) {
		rebuild_diagnostics_model(state);
		refresh_selected(state, reporting);
	}
}
Status freeze_diagnostics_model(DevDiagnosticsState& state, DevErrorReporting& reporting,
								bool selected_only, uint64_t byte_budget) noexcept {
	try {
		if (selected_only && !state.selected_id)
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::None));
		auto owned = reporting.diagnostics_investigation(
			selected_only ? std::optional{state.selected_id} : std::nullopt, byte_budget);
		bool limited = !owned;
		if (!owned) {
			DevDiagnosticsInvestigation held{};
			auto latest = reporting.diagnostics_snapshot();
			held.snapshot = latest ? std::move(*latest) : state.snapshot;
			owned = std::move(held);
		}
		if (state.selected_id &&
			std::ranges::find(owned->occurrences, state.selected_id, &DevErrorOccurrence::id) ==
				owned->occurrences.end()) {
			auto preserved = select_diagnostics_investigation(state.owned, state.selected_id);
			if (preserved) {
				if (selected_only) {
					owned = std::move(*preserved);
					limited = false;
				} else {
					if (!limited && (preserved->owned_bytes > byte_budget ||
									 owned->owned_bytes > byte_budget - preserved->owned_bytes)) {
						DevDiagnosticsInvestigation held{};
						held.snapshot = std::move(owned->snapshot);
						owned = std::move(held);
						limited = true;
					}
					preserve_investigation(*owned, *preserved);
				}
			} else if (selected_only) {
				return unexpectedError(preserved.error());
			}
		}
		auto snapshot = owned->snapshot;
		state.selected.reset();
		state.owned = std::move(*owned);
		state.snapshot = std::move(snapshot);
		state.frozen = true;
		state.selected_only_freeze = selected_only;
		state.frozen_details_limited = limited;
		state.dirty = true;
		rebuild_diagnostics_model(state);
		load_frozen_selection(state);
		state.message = "Frozen at tick " + std::to_string(state.anchor_tick) +
						(selected_only ? " · Selected investigation only"
						 : limited ? " · List and available selected details held; other details "
									 "exceeded the capture budget"
								   : " · Retained diagnostics") +
						(state.selected_evicted ? " · Preserved viewed investigation" : "");
		state.message_is_error = false;
		return {};
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::None));
	}
}
void select_diagnostics_group(DevDiagnosticsState& state, const DevErrorGroupKey& key,
							  DevErrorGroupMode mode) {
	const auto found = std::ranges::find_if(
		state.groups, [&](const auto& group) { return group.key == key && group.mode == mode; });
	if (found == state.groups.end())
		return;
	state.selected_group = key;
	state.selected_mode = static_cast<uint64_t>(mode);
	uint64_t selected = found->latest;
	std::string_view query = state.filters.search;
	while (!query.empty() && std::isspace(static_cast<unsigned char>(query.front())))
		query.remove_prefix(1);
	while (!query.empty() && std::isspace(static_cast<unsigned char>(query.back())))
		query.remove_suffix(1);
	uint64_t exact = 0;
	const bool hex = query.starts_with("0x") || query.starts_with("0X");
	if (hex)
		query.remove_prefix(2);
	const auto parsed =
		std::from_chars(query.data(), query.data() + query.size(), exact, hex ? 16 : 10);
	if (parsed.ec == std::errc{} && parsed.ptr == query.data() + query.size() &&
		std::ranges::find(found->occurrences, exact) != found->occurrences.end())
		selected = exact;
	select_diagnostics_occurrence(state, selected);
}
void select_diagnostics_occurrence(DevDiagnosticsState& state, uint64_t id) {
	const auto* row = find_row(state, id);
	if (!row)
		return;
	if (state.selected_only_freeze && id != state.selected_id)
		return;
	state.selected_id = id;
	state.follow_latest = false;
	state.selected_evicted = false;
	state.focus_kind = 0;
	state.focus_sequence = 0;
	state.focused_advice.reset();
	state.selected_bin = UINT64_MAX;
	state.history_page = 0;
	state.selected_mode = std::min(state.filters.grouping, uint64_t{2});
	state.selected_group =
		diagnostics_group_key(*row, static_cast<DevErrorGroupMode>(state.selected_mode));
	state.dirty = true;
}
void focus_diagnostics_event(DevDiagnosticsState& state, uint64_t kind,
							 uint64_t sequence) noexcept {
	state.focus_kind = kind;
	state.focus_sequence = sequence;
	state.focused_advice.reset();
}
void reset_diagnostics_filters(DevDiagnosticsState& state) {
	state.filters = {};
	state.refresh_order = true;
	state.dirty = true;
}
const DevErrorOccurrence*
diagnostics_selected_occurrence(const DevDiagnosticsState& state) noexcept {
	return state.selected ? &*state.selected : nullptr;
}
const DevDiagnosticsGroup* selected_diagnostics_group(const DevDiagnosticsState& state) noexcept {
	if (!state.selected_group)
		return nullptr;
	const auto found = std::ranges::find_if(state.groups, [&](const auto& group) {
		return group.key == *state.selected_group &&
			   static_cast<uint64_t>(group.mode) == state.selected_mode;
	});
	return found == state.groups.end() ? nullptr : &*found;
}
const DevDiagnosticsSource* diagnostics_source(const DevDiagnosticsState& state,
											   uint64_t id) noexcept {
	const auto& sources = state.owned.snapshot.sources;
	const auto owned = std::ranges::find(sources, id, &DevDiagnosticsSource::id);
	if (owned != sources.end())
		return &*owned;
	const auto found = std::ranges::find(state.snapshot.sources, id, &DevDiagnosticsSource::id);
	return found == state.snapshot.sources.end() ? nullptr : &*found;
}
std::string_view diagnostics_breadcrumb_name(const DevDiagnosticsState& state,
											 uint64_t id) noexcept {
	const auto& descriptors = state.owned.snapshot.breadcrumb_descriptors;
	const auto found = std::ranges::find(descriptors, id, &DevDiagnosticsBreadcrumbDescriptor::id);
	return found == descriptors.end() ? std::string_view{"Descriptor unavailable"} : found->name;
}
const DevErrorStackTrace* diagnostics_stack(const DevDiagnosticsState& state,
											uint64_t id) noexcept {
	const auto found = std::ranges::find(state.owned.stacks, id, &DevErrorStackTrace::id);
	return found == state.owned.stacks.end() ? nullptr : &*found;
}
const DevErrorTriggeredCapture* diagnostics_capture(const DevDiagnosticsState& state,
													uint64_t id) noexcept {
	const auto found = std::ranges::find(state.owned.captures, id, &DevErrorTriggeredCapture::id);
	return found == state.owned.captures.end() ? nullptr : &*found;
}
void diagnostics_focus_advice(DevDiagnosticsState& state,
							  const DevErrorAdviceEvidenceRef& evidence) {
	using Kind = DevErrorAdviceEvidenceKind;
	state.focused_advice = evidence;
	switch (evidence.kind) {
	case Kind::Contract:
	case Kind::Resolution:
		state.inspector_scroll_to = 0;
		break;
	case Kind::Subject:
	case Kind::Auxiliary:
	case Kind::NativeCode:
		state.inspector_scroll_to = 3;
		break;
	case Kind::Snapshot:
		state.inspector_scroll_to = 4;
		break;
	case Kind::CaptureQuality:
		state.inspector_scroll_to = 7;
		break;
	default:
		break;
	}
	switch (evidence.kind) {
	case Kind::Snapshot:
		state.inspector_expanded[4] = true;
		state.focus_kind = 3;
		state.focus_sequence = evidence.primary;
		break;
	case Kind::Contract:
		state.inspector_expanded[0] = true;
		break;
	case Kind::Subject:
	case Kind::Auxiliary:
	case Kind::NativeCode:
		state.inspector_expanded[3] = true;
		break;
	case Kind::Resolution:
		state.inspector_expanded[0] = true;
		break;
	case Kind::CaptureQuality:
		state.inspector_expanded[7] = true;
		break;
	case Kind::Frame:
	case Kind::TimingZone:
		state.navigation_message = "Performance evidence reference " +
								   std::to_string(evidence.primary) + " / " +
								   std::to_string(evidence.secondary);
		break;
	case Kind::MemoryEvent:
	case Kind::MemoryCheckpoint:
		state.navigation_message = "Memory evidence reference " + std::to_string(evidence.primary) +
								   " / " + std::to_string(evidence.secondary);
		break;
	}
}
uint32_t diagnostics_filter_count(const DevDiagnosticsFilters& filters) noexcept {
	return (filters.domain_mask != UINT32_MAX) + (filters.impact_mask != UINT32_MAX) +
		   (filters.resolution_mask != UINT32_MAX) + (filters.window_mode != 0) +
		   (filters.source_mode != 0) + (filters.first_tick != 0 || filters.last_tick != 0) +
		   (filters.window_mode == 1 && (filters.first_frame != 0 || filters.last_frame != 0));
}
std::string diagnostics_domain_name(ErrorDomain domain) {
	constexpr std::array names{"Unclassified", "Core",	   "App",	  "Platform", "Window", "Frame",
							   "Vulkan",	   "Renderer", "Storage", "Resource", "Asset",	"Font",
							   "Image",		   "Icon",	   "Element", "Action",	  "Theme",	"Input",
							   "Popup",		   "Shortcut", "Viewport"};
	const auto index = static_cast<size_t>(domain);
	return index < names.size() ? names[index] : "Unclassified (" + std::to_string(index) + ")";
}
std::string diagnostics_impact_name(ErrorImpact impact) {
	constexpr std::array names{"None",			"Degraded",			 "OperationFailed",
							   "FrameCanceled", "WindowUnavailable", "AppUnavailable",
							   "Fatal"};
	const auto index = static_cast<size_t>(impact);
	return index < names.size() ? names[index] : "Unknown (" + std::to_string(index) + ")";
}
std::string diagnostics_resolution_name(ErrorResolution resolution) {
	constexpr std::array names{
		"No resolution recorded", "Rejected",	   "Ignored",		  "Retried",
		"UsedFallback",			  "Skipped",	   "GrewCapacity",	  "EvictedAndRetried",
		"CanceledOperation",	  "CanceledFrame", "RecreatedWindow", "ClosedWindow",
		"RecreatedApp",			  "Propagated",	   "Halted",		  "Terminated"};
	const auto index = static_cast<size_t>(resolution);
	return index < names.size() ? names[index] : "Unknown (" + std::to_string(index) + ")";
}
std::string_view diagnostics_collection_name(DevErrorOccurrenceState state) noexcept {
	switch (state) {
	case DevErrorOccurrenceState::Open:
		return "Collecting";
	case DevErrorOccurrenceState::PostWindow:
		return "Collecting follow-up";
	case DevErrorOccurrenceState::Closed:
		return "Collection closed";
	}
	return "Unknown collection state";
}

Status navigate_diagnostics_performance(App& app, DevInterfaceState& interface_state) {
	auto& state = interface_state.diagnostics;
	if (!state.selected)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	const auto& correlation = state.selected->timing;
	if (correlation.state != DevErrorCorrelationState::Available || !correlation.capture_generation)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	auto capture = app.devMonitoring().timingReporting().read_capture();
	if (capture.generation != correlation.capture_generation)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	auto selection = interface_state.performance_selection;
	selection.selected_scope = {correlation.frame.window, DevPerformanceScopeKind::Window};
	selection.selector_mode = 0;
	selection.hardware_domain = 0;
	selection.category_mask = UINT32_MAX;
	auto snapshot = interface_elements::extract_timeline(capture, selection);
	const auto found = std::ranges::find_if(snapshot.blocks, [&](const auto& block) {
		return block.app_tick == correlation.appTick && block.frame == correlation.frame &&
			   block.invocation_id == correlation.containingInvocation &&
			   block.type_id == correlation.containingZone &&
			   block.details().entity.kind == correlation.entity.kind &&
			   block.details().entity.primaryId == correlation.entity.primaryId &&
			   block.details().entity.secondaryId == correlation.entity.secondaryId;
	});
	if (found == snapshot.blocks.end())
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	const auto index = static_cast<size_t>(found - snapshot.blocks.begin());
	auto& timeline = interface_state.performance_timeline;
	timeline.snapshot = std::move(snapshot);
	timeline.selection = selection;
	timeline.capture_generation = capture.generation;
	timeline.pending = {};
	timeline.cards.clear();
	timeline.inspected_sample = index;
	++timeline.snapshot_revision;
	timeline.origin_ns = timeline.snapshot.start_ns;
	timeline.visible_start_ns = timeline.snapshot.blocks[index].start_ns;
	timeline.visible_duration_ns =
		std::max(uint64_t{1}, timeline.snapshot.blocks[index].duration_ns);
	interface_elements::clamp_timeline_view(timeline);
	interface_state.performance_selection = selection;
	state.return_available = true;
	state.navigation_message = "Captured timing invocation selected";
	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Performance);
	return {};
}
Status navigate_diagnostics_memory(App& app, DevInterfaceState& interface_state) {
	auto& state = interface_state.diagnostics;
	if (!state.selected)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	const auto& correlation = state.selected->memory;
	auto& reporting = app.devMonitoring().memoryReporting();
	const auto status = reporting.status();
	if (correlation.state != DevErrorCorrelationState::Available ||
		status.history_storage_generation != correlation.history_generation)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	std::optional<RetainedMemoryEvent> event;
	std::optional<MemorySampleSegment> checkpoint;
	if (correlation.eventSequence) {
		std::vector<RetainedMemoryEvent> events;
		events.reserve(1);
		const auto read =
			reporting.append_event_range(correlation.history_generation,
										 correlation.eventSequence - 1, 0, UINT64_MAX, 1, events);
		const auto found =
			std::ranges::find(events, correlation.eventSequence, &RetainedMemoryEvent::sequence);
		if (found != events.end() && found->operation.source == correlation.source &&
			found->operation.operation == correlation.operation)
			event = *found;
		if (!event)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
		if (read.generation != correlation.history_generation)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	} else {
		uint64_t candidates = 0;
		reporting.visit_history([&](const auto& segment) {
			if (segment.value.source == correlation.source &&
				(static_cast<uint16_t>(segment.value.flags) &
				 static_cast<uint16_t>(MemorySampleFlag::TuningAggregate)) == 0 &&
				segment.beginTick <= correlation.checkpointTick &&
				segment.endTickExclusive > correlation.checkpointTick) {
				++candidates;
				checkpoint = segment;
			}
		});
		if (!checkpoint || candidates != 1)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	}
	if (reporting.status().history_storage_generation != correlation.history_generation)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	auto& memory = interface_state.memory_workbench;
	const auto timestamp = event ? event->operation.timestampNs : checkpoint->beginNs;
	if (!timestamp)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	interface_elements::DevMemoryWorkbenchState destination;
	destination.duration_ns = memory.duration_ns;
	const auto half_window = std::max(uint64_t{1}, destination.duration_ns / 2);
	destination.retained_begin_ns = timestamp > half_window ? timestamp - half_window : 0;
	destination.view_end_ns =
		timestamp > UINT64_MAX - half_window ? UINT64_MAX : timestamp + half_window;
	interface_elements::hold_memory_history(destination, reporting);
	if (!destination.held_data ||
		destination.held_data->live.status.history_storage_generation !=
			correlation.history_generation ||
		reporting.status().history_storage_generation != correlation.history_generation)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	if (checkpoint &&
		!std::ranges::any_of(destination.held_data->segments, [&](const auto& segment) {
			return segment.sequence == checkpoint->sequence &&
				   segment.value.source == checkpoint->value.source &&
				   segment.value.window == checkpoint->value.window;
		}))
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::AppAccessDevMonitoring));
	if (event && !std::ranges::any_of(destination.held_events, [&](const auto& candidate) {
			return candidate.sequence == event->sequence;
		}))
		destination.held_events.emplace_back(*event);
	DevMemorySelection selection;
	selection.window_scope = event ? event->operation.frame.window : checkpoint->value.window;
	interface_elements::resolve_memory_tracks(destination, selection, destination.held_data->live);
	const auto track = std::ranges::find_if(destination.tracks, [&](const auto& candidate) {
		return std::ranges::find(candidate.history_sources, correlation.source) !=
			   candidate.history_sources.end();
	});
	const auto focused_track = track == destination.tracks.end() ? 0 : track->identity;
	state.navigation_memory_event = event;
	state.navigation_memory_checkpoint = checkpoint;
	memory.analysis.dataset = memory.analysis.previous_dataset = 0;
	memory.held_data = std::move(destination.held_data);
	memory.held_events = std::move(destination.held_events);
	memory.message = std::move(destination.message);
	memory.tracks = std::move(destination.tracks);
	memory.scope = destination.scope;
	memory.window = destination.window;
	memory.focus = focused_track;
	memory.next_live_ns = memory.next_history_ns = 0;
	memory.range_begin_ns = memory.range_end_ns = 0;
	memory.fit_pending = false;
	memory.lane_count = 1;
	memory.lane_settings[0] = {};
	memory.lane_settings[0].selected_key = {correlation.source, selection.window_scope};
	memory.selected_ns = timestamp;
	memory.hovered_ns = timestamp;
	memory.following = false;
	memory.view_end_ns = destination.view_end_ns;
	memory.view_begin_ns = destination.retained_begin_ns;
	interface_state.memory_selection = selection;
	state.return_available = true;
	state.navigation_message =
		event ? "Captured memory operation selected" : "Captured memory checkpoint selected";
	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Memory);
	return {};
}

void service_diagnostics(App& app, DevInterfaceState& interface_state, bool visible) {
	auto& state = interface_state.diagnostics;
	auto& reporting = app.devMonitoring().errorReporting();
	const auto command = std::exchange(state.pending, DevDiagnosticsCommand::None);
	const auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
											   std::chrono::steady_clock::now().time_since_epoch())
											   .count());
	try {
		if (visible || command != DevDiagnosticsCommand::None) {
			state.live_capture_config = app.devMonitoring().errors().config();
			state.live_reporting_config = reporting.config();
		}
		if (command != DevDiagnosticsCommand::None)
			state.message_is_error = false;
		switch (command) {
		case DevDiagnosticsCommand::None:
			break;
		case DevDiagnosticsCommand::Refresh:
			state.refresh_order = true;
			state.dirty = true;
			break;
		case DevDiagnosticsCommand::ClearSelection:
			state.selected.reset();
			state.selected_id = 0;
			state.selected_group.reset();
			state.focused_advice.reset();
			state.focus_kind = state.focus_sequence = 0;
			state.follow_latest = state.selected_evicted = state.newer_available = false;
			state.dirty = true;
			state.message = "Selection cleared";
			break;
		case DevDiagnosticsCommand::ApplyCaptureLevel: {
			auto config = app.devMonitoring().errors().config();
			config.level = static_cast<DevErrorCaptureLevel>(
				std::min(state.capture_level, static_cast<uint64_t>(compiledDevErrorLevel())));
			app.devMonitoring().errors().setConfig(config);
			state.capture_level =
				static_cast<uint64_t>(app.devMonitoring().errors().config().level);
			state.message = "Effective capture level updated for future evidence";
			state.last_refresh_ns = 0;
			break;
		}
		case DevDiagnosticsCommand::ArmTrigger: {
			auto config = reporting.config();
			const auto duplicate = std::ranges::find_if(config.triggers, [&](const auto& trigger) {
				return trigger.code == state.trigger.code && trigger.site == state.trigger.site &&
					   trigger.matchNativeCode == state.trigger.matchNativeCode &&
					   (!trigger.matchNativeCode || trigger.nativeCode == state.trigger.nativeCode);
			});
			if (config.triggers.size() >= 64)
				state.message = "Trigger limit reached (64)";
			else if (duplicate != config.triggers.end())
				state.message = "An identical matching rule is already armed";
			else {
				config.triggers.emplace_back(state.trigger);
				reporting.setConfig(config);
				state.message =
					"Trigger armed for future admitted occurrences; first matching rule wins";
			}
			state.last_refresh_ns = 0;
			break;
		}
		case DevDiagnosticsCommand::RemoveTrigger:
		case DevDiagnosticsCommand::MoveTriggerUp:
		case DevDiagnosticsCommand::MoveTriggerDown: {
			auto config = reporting.config();
			const auto index = state.command_index;
			if (index < config.triggers.size()) {
				if (command == DevDiagnosticsCommand::RemoveTrigger)
					config.triggers.erase(config.triggers.begin() + static_cast<ptrdiff_t>(index));
				else if (command == DevDiagnosticsCommand::MoveTriggerUp && index > 0)
					std::swap(config.triggers[index], config.triggers[index - 1]);
				else if (command == DevDiagnosticsCommand::MoveTriggerDown &&
						 index + 1 < config.triggers.size())
					std::swap(config.triggers[index], config.triggers[index + 1]);
				reporting.setConfig(config);
				state.message = "Armed trigger order updated";
			}
			state.last_refresh_ns = 0;
			break;
		}
		case DevDiagnosticsCommand::Freeze:
		case DevDiagnosticsCommand::FreezeSelected: {
			const auto result = freeze_diagnostics_model(
				state, reporting, command == DevDiagnosticsCommand::FreezeSelected);
			if (!result) {
				state.message =
					"Could not freeze diagnostics: " + std::string(result.error().message());
				state.message_is_error = true;
			}
			break;
		}
		case DevDiagnosticsCommand::Resume:
			state.frozen = false;
			state.selected_only_freeze = false;
			state.frozen_details_limited = false;
			state.last_refresh_ns = 0;
			state.message = "Live view resumed";
			break;
		case DevDiagnosticsCommand::ExportSelected:
		case DevDiagnosticsCommand::ExportRetained: {
			const bool selected_only = command == DevDiagnosticsCommand::ExportSelected;
			if (!selected_only && (state.selected_only_freeze || state.frozen_details_limited)) {
				state.message = "This frozen dataset contains only the selected investigation; "
								"choose Selected investigation export.";
				state.message_is_error = true;
				break;
			}
			auto owned = state.frozen || (selected_only && state.selected_evicted)
							 ? selected_only ? select_diagnostics_investigation(state.owned,
																				state.selected_id)
											 : Result<DevDiagnosticsInvestigation>{state.owned}
							 : reporting.diagnostics_investigation(
								   selected_only ? std::optional{state.selected_id} : std::nullopt);
			if (!owned || owned->occurrences.empty()) {
				state.message = "No retained evidence available for export";
				break;
			}
			DevDiagnosticsExportMetadata metadata{};
			metadata.frozen = state.frozen;
			metadata.group_mode = static_cast<DevErrorGroupMode>(state.selected_mode);
			metadata.group_key = state.selected_group.value_or(DevErrorGroupKey{});
			metadata.filter_description =
				"search=" + state.filters.search +
				"; activity=" + std::to_string(state.filters.activity_view) +
				"; domain_mask=" + std::to_string(state.filters.domain_mask) +
				"; impact_mask=" + std::to_string(state.filters.impact_mask) +
				"; resolution_mask=" + std::to_string(state.filters.resolution_mask) +
				"; window_mode/id=" + std::to_string(state.filters.window_mode) + "/" +
				std::to_string(state.filters.window) +
				"; source_mode/id=" + std::to_string(state.filters.source_mode) + "/" +
				std::to_string(state.filters.source) +
				"; tick_range=" + std::to_string(state.filters.first_tick) + ".." +
				std::to_string(state.filters.last_tick) +
				"; frame_range=" + std::to_string(state.filters.first_frame) + ".." +
				std::to_string(state.filters.last_frame) +
				"; ongoing/recent/histogram=" + std::to_string(state.filters.ongoing_ticks) + "/" +
				std::to_string(state.filters.recent_ticks) + "/" +
				std::to_string(state.filters.histogram_ticks) + "; scope=" +
				(selected_only
					 ? "selected"
					 : "all retained; filters describe view, export contains retained set");
			const auto status = write_diagnostics_export(*owned, state.export_path, metadata);
			state.message = status ? "Exported " + state.export_path + ".json and .md"
								   : "Export failed: " + std::string(status.error().message());
			state.message_is_error = !status;
			break;
		}
		case DevDiagnosticsCommand::OpenPerformance: {
			const auto status = navigate_diagnostics_performance(app, interface_state);
			if (!status)
				state.navigation_message =
					"Evidence evicted or exact timing identity/generation unavailable";
			break;
		}
		case DevDiagnosticsCommand::OpenMemory: {
			const auto status = navigate_diagnostics_memory(app, interface_state);
			if (!status)
				state.navigation_message =
					"Evidence evicted or exact memory identity/generation unavailable";
			break;
		}
		case DevDiagnosticsCommand::Back:
			interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
			break;
		}
		const auto previous_revision = state.model_revision;
		if (visible || command != DevDiagnosticsCommand::None)
			refresh_diagnostics_model(state, reporting, now,
									  command != DevDiagnosticsCommand::None);
		if (command == DevDiagnosticsCommand::Refresh &&
			state.model_revision != previous_revision && !state.message_is_error) {
			++state.refresh_count;
			state.message = "Refreshed " +
							std::string(state.frozen ? "frozen view" : "live diagnostics") + " #" +
							std::to_string(state.refresh_count) + " · tick " +
							std::to_string(state.anchor_tick) + " · " +
							std::to_string(state.groups.size()) + " groups";
		}
		if (command != DevDiagnosticsCommand::None) {
			state.live_capture_config = app.devMonitoring().errors().config();
			state.live_reporting_config = reporting.config();
			state.capture_level = static_cast<uint64_t>(state.live_capture_config.level);
		}
		interface_state.activeErrorCount = state.snapshot.status.retainedOccurrences;
	} catch (...) {
		state.message_is_error = true;
		state.message = "Diagnostics action could not allocate its bounded view";
	}
}
} // namespace FlowUi::devSystems
#endif
