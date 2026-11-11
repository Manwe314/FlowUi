#include "TestHarness.hpp"

#include "FlowUi/Private/inputService/InputFieldManagerState.hpp"
#include "FlowUi/Private/layoutService/ClayDevTreeBridge.hpp"
#include "FlowUi/Private/layoutService/PopupManagerState.hpp"
#include "FlowUi/Private/renderer/ViewportStorageController.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Private/utility/EnumFlags.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace {

using namespace FlowUi::detail::manager_storage;
using FlowUi::detail::EnumFlags;

enum class TestFlag : std::uint8_t {
	first = 1u << 0,
	last = 1u << 7,
};

enum class OtherFlag : std::uint8_t { first = 1u << 0 };

template <typename Flags, typename Flag>
concept AcceptsFlag = requires(Flags flags, Flag flag) {
	flags.has_flag(flag);
	flags.set_flag(flag);
	flags.clear_flag(flag);
};

static_assert(sizeof(EnumFlags<TestFlag>) == sizeof(std::uint8_t));
static_assert(alignof(EnumFlags<TestFlag>) == alignof(std::uint8_t));
static_assert(std::is_trivially_copyable_v<EnumFlags<TestFlag>>);
static_assert(AcceptsFlag<EnumFlags<TestFlag>, TestFlag>);
static_assert(!AcceptsFlag<EnumFlags<TestFlag>, OtherFlag>);
static_assert(!AcceptsFlag<EnumFlags<TestFlag>, bool>);
static_assert(noexcept(EnumFlags<TestFlag>{TestFlag::first, TestFlag::last}));
static_assert(noexcept(EnumFlags<TestFlag>{}.has_flag(TestFlag::first)));
static_assert(noexcept(EnumFlags<TestFlag>{}.set_flag(TestFlag::first)));
static_assert(noexcept(EnumFlags<TestFlag>{}.clear_flag(TestFlag::first)));

/** Exercise every byte and every bit, including preserving all unrelated bits. */
[[nodiscard]] constexpr bool exhaustive_byte_operations() noexcept {
	for (std::uint32_t initial_bits = 0; initial_bits < 256; ++initial_bits) {
		EnumFlags<TestFlag> initial{};
		for (std::uint32_t bit_index = 0; bit_index < 8; ++bit_index) {
			const auto flag = static_cast<TestFlag>(1u << bit_index);
			initial.set_flag(flag, (initial_bits & (1u << bit_index)) != 0);
		}
		for (std::uint32_t changed_bit = 0; changed_bit < 8; ++changed_bit) {
			const auto changed_flag = static_cast<TestFlag>(1u << changed_bit);
			auto enabled = initial;
			auto disabled = initial;
			enabled.set_flag(changed_flag);
			disabled.clear_flag(changed_flag);
			for (std::uint32_t checked_bit = 0; checked_bit < 8; ++checked_bit) {
				const auto checked_flag = static_cast<TestFlag>(1u << checked_bit);
				const bool original_value = (initial_bits & (1u << checked_bit)) != 0;
				if (initial.has_flag(checked_flag) != original_value ||
					enabled.has_flag(checked_flag) !=
						(checked_bit == changed_bit || original_value) ||
					disabled.has_flag(checked_flag) !=
						(checked_bit != changed_bit && original_value))
					return false;
			}
		}
	}
	return true;
}

static_assert(exhaustive_byte_operations());

void popup_defaults_and_committed_state_preserve_flags() {
	PopupManagerState state{};
	PopupRecord record{};
	FLOWUI_CHECK(record.flags.has_flag(PopupRecordFlag::dismiss_on_escape));
	for (const auto flag : {PopupRecordFlag::has_pointer_snapshot, PopupRecordFlag::measured,
							PopupRecordFlag::has_bounds, PopupRecordFlag::dismissed,
							PopupRecordFlag::dismissal_pending,
							PopupRecordFlag::visible_last_committed})
		FLOWUI_CHECK(!record.flags.has_flag(flag));
	for (const auto flag : {PopupManagerFlag::frame_active,
							PopupManagerFlag::capacity_warning_issued,
							PopupManagerFlag::raw_primary_pointer_down_last_frame,
							PopupManagerFlag::consume_primary_pointer_until_release,
							PopupManagerFlag::suppress_all_primary_pointer_this_frame})
		FLOWUI_CHECK(!state.flags.has_flag(flag));

	record.flags.set_flag(PopupRecordFlag::has_pointer_snapshot);
	record.flags.set_flag(PopupRecordFlag::visible_last_committed);
	state.committedRecords.emplace(17u, record);
	state.workingRecords = state.committedRecords;
	auto& working_record = state.workingRecords.at(17u);
	working_record.flags.set_flag(PopupRecordFlag::dismissal_pending);
	working_record.flags.clear_flag(PopupRecordFlag::visible_last_committed);
	FLOWUI_CHECK(
		!state.committedRecords.at(17u).flags.has_flag(PopupRecordFlag::dismissal_pending));
	state.committedRecords.swap(state.workingRecords);
	const auto& committed_record = state.committedRecords.at(17u);
	FLOWUI_CHECK(committed_record.flags.has_flag(PopupRecordFlag::dismissal_pending));
	FLOWUI_CHECK(committed_record.flags.has_flag(PopupRecordFlag::has_pointer_snapshot));
	FLOWUI_CHECK(committed_record.flags.has_flag(PopupRecordFlag::dismiss_on_escape));
	FLOWUI_CHECK(!committed_record.flags.has_flag(PopupRecordFlag::visible_last_committed));

	state.flags.set_flag(PopupManagerFlag::consume_primary_pointer_until_release);
	state.flags.set_flag(PopupManagerFlag::frame_active);
	state.flags.set_flag(PopupManagerFlag::suppress_all_primary_pointer_this_frame);
	state.flags.clear_flag(PopupManagerFlag::frame_active);
	state.flags.clear_flag(PopupManagerFlag::suppress_all_primary_pointer_this_frame);
	FLOWUI_CHECK(state.flags.has_flag(PopupManagerFlag::consume_primary_pointer_until_release));
}

void field_defaults_and_independent_transitions() {
	InputFieldState field{};
	FLOWUI_CHECK(field.flags.has_flag(InputFieldFlag::caret_reveal_pending));
	FLOWUI_CHECK(!field.flags.has_flag(InputFieldFlag::initialized));
	FLOWUI_CHECK(!field.flags.has_flag(InputFieldFlag::mode_change_rejected));
	field.flags.set_flag(InputFieldFlag::initialized);
	field.flags.set_flag(InputFieldFlag::mode_change_rejected);
	field.flags.clear_flag(InputFieldFlag::caret_reveal_pending);
	FLOWUI_CHECK(field.flags.has_flag(InputFieldFlag::initialized));
	FLOWUI_CHECK(field.flags.has_flag(InputFieldFlag::mode_change_rejected));
	field.flags.clear_flag(InputFieldFlag::mode_change_rejected);
	FLOWUI_CHECK(field.flags.has_flag(InputFieldFlag::initialized));
	FLOWUI_CHECK(!field.flags.has_flag(InputFieldFlag::caret_reveal_pending));

	InputFieldManagerState manager{};
	FLOWUI_CHECK(manager.flags.has_flag(InputFieldManagerFlag::caret_blink_reset_pending));
	FLOWUI_CHECK(manager.flags.has_flag(InputFieldManagerFlag::emit_carets_this_frame));
	FLOWUI_CHECK(!manager.flags.has_flag(InputFieldManagerFlag::dirty));
	manager.flags.set_flag(InputFieldManagerFlag::dirty);
	manager.flags.clear_flag(InputFieldManagerFlag::caret_blink_reset_pending);
	FLOWUI_CHECK(manager.flags.has_flag(InputFieldManagerFlag::emit_carets_this_frame));
	FLOWUI_CHECK(manager.flags.has_flag(InputFieldManagerFlag::dirty));
	manager.flags.clear_flag(InputFieldManagerFlag::dirty);
	FLOWUI_CHECK(manager.flags.has_flag(InputFieldManagerFlag::emit_carets_this_frame));
}

void viewport_frame_reset_preserves_capture_state() {
	ViewportRecord record{};
	FLOWUI_CHECK(!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame));
#if FLOW_UI_DEV_MODE
	for (const auto flag : {ViewportRecordFlag::capture_requested,
							ViewportRecordFlag::capture_failed,
							ViewportRecordFlag::capture_release_requested})
		FLOWUI_CHECK(!record.flags.has_flag(flag));
	record.flags.set_flag(ViewportRecordFlag::capture_requested);
	record.flags.set_flag(ViewportRecordFlag::capture_failed);
	record.flags.set_flag(ViewportRecordFlag::capture_release_requested);
#endif
	record.flags.set_flag(ViewportRecordFlag::referenced_this_frame);
	record.flags.clear_flag(ViewportRecordFlag::referenced_this_frame);
#if FLOW_UI_DEV_MODE
	for (const auto flag : {ViewportRecordFlag::capture_requested,
							ViewportRecordFlag::capture_failed,
							ViewportRecordFlag::capture_release_requested})
		FLOWUI_CHECK(record.flags.has_flag(flag));
#endif
	FLOWUI_CHECK(!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame));
}

void context_requested_and_enabled_flags_remain_distinct() {
	VulkanContext context{};
	FLOWUI_CHECK(context.flags.has_flag(VulkanContext::Flag::dev_gpu_timing_requested));
	FLOWUI_CHECK(context.flags.has_flag(VulkanContext::Flag::dev_gpu_memory_requested));
	for (const auto flag : {VulkanContext::Flag::surface_maintenance_enabled,
							VulkanContext::Flag::memory_budget_enabled,
							VulkanContext::Flag::synchronization2_enabled,
							VulkanContext::Flag::calibrated_timestamps_enabled})
		FLOWUI_CHECK(!context.flags.has_flag(flag));
	context.flags.set_flag(VulkanContext::Flag::memory_budget_enabled);
	context.flags.clear_flag(VulkanContext::Flag::dev_gpu_memory_requested);
	FLOWUI_CHECK(context.flags.has_flag(VulkanContext::Flag::memory_budget_enabled));
	FLOWUI_CHECK(context.flags.has_flag(VulkanContext::Flag::dev_gpu_timing_requested));
}

#if FLOW_UI_DEV_MODE
void snapshot_estimate_defaults_are_preserved() {
	FlowUi::detail::storage::StorageMemorySnapshot snapshot{};
	using Flag = FlowUi::detail::storage::StorageMemorySnapshot::Flag;
	FLOWUI_CHECK(snapshot.flags.has_flag(Flag::resource_metadata_estimated));
	FLOWUI_CHECK(!snapshot.flags.has_flag(Flag::includes_resource_kinds));
	FLOWUI_CHECK(!snapshot.flags.has_flag(Flag::includes_windows));
	FLOWUI_CHECK(!snapshot.flags.has_flag(Flag::includes_individual_resources));
	snapshot.flags.set_flag(Flag::includes_windows);
	snapshot.flags.clear_flag(Flag::includes_windows);
	FLOWUI_CHECK(snapshot.flags.has_flag(Flag::resource_metadata_estimated));
}
#endif

#if FLOW_UI_DEV_MODE && FLOW_UI_DEV_CAPTURE_CLAY
void clay_element_flags_survive_copy() {
	FlowUi::detail::ClayDevElementView view{};
	using Flag = FlowUi::detail::ClayDevElementFlag;
	FLOWUI_CHECK(!view.flags.has_flag(Flag::bounds_available));
	FLOWUI_CHECK(!view.flags.has_flag(Flag::is_text));
	FLOWUI_CHECK(!view.flags.has_flag(Flag::exiting));
	view.flags.set_flag(Flag::bounds_available);
	view.flags.set_flag(Flag::is_text);
	view.flags.set_flag(Flag::exiting);
	auto copy = view;
	copy.flags.clear_flag(Flag::bounds_available);
	FLOWUI_CHECK(copy.flags.has_flag(Flag::is_text));
	FLOWUI_CHECK(copy.flags.has_flag(Flag::exiting));
	FLOWUI_CHECK(view.flags.has_flag(Flag::bounds_available));
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("exhaustive byte flag set/clear/copy",
			   [] { FLOWUI_CHECK(exhaustive_byte_operations()); });
	runner.run("popup defaults, copy, commit and pointer suppression",
			   popup_defaults_and_committed_state_preserve_flags);
	runner.run("field initialization, reveal, migration and blink flags",
			   field_defaults_and_independent_transitions);
	runner.run("viewport frame reset preserves capture flags",
			   viewport_frame_reset_preserves_capture_state);
	runner.run("Vulkan requested and enabled defaults",
			   context_requested_and_enabled_flags_remain_distinct);
#if FLOW_UI_DEV_MODE
	runner.run("storage snapshot estimated metadata default",
			   snapshot_estimate_defaults_are_preserved);
#endif
#if FLOW_UI_DEV_MODE && FLOW_UI_DEV_CAPTURE_CLAY
	runner.run("Clay element flag copy", clay_element_flags_survive_copy);
#endif
	return runner.finish();
}
