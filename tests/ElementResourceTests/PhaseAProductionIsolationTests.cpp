#include "TestHarness.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "managers/UiManager.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"

#if FLOW_UI_DEV_MODE
#error "PhaseAProductionIsolationTests must be compiled only with FLOW_UI_DEV_MODE=0."
#endif

// Phase E production source/isolation test. Its target defines:
//   FLOWUI_UI_MANAGER_STATE_SOURCE
//   FLOWUI_UI_MANAGER_SOURCE
//   FLOWUI_ELEMENT_BUILDER_SOURCE
//   FLOWUI_ELEMENT_BRIDGE_HEADER

namespace {

template <typename State>
concept HasFlowRootIdTrackerStorage = requires(State& state) {
	state.flowRootIdTracker;
};

static_assert(
	!HasFlowRootIdTrackerStorage<FlowUi::detail::manager_storage::UiManagerState>,
	"Production UiManagerState must not contain Flow-ID claim/collision storage.");

std::string readSource(const char* path) {
	std::ifstream input(path, std::ios::binary);
	FLOWUI_CHECK(input.good());
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string_view trimLeft(std::string_view line) {
	const std::size_t first = line.find_first_not_of(" \t");
	return first == std::string_view::npos ? std::string_view{} : line.substr(first);
}

bool everyTokenOccurrenceIsInsideDevGuard(std::string_view source, std::string_view token) {
	struct GuardFrame {
		bool devGuardedBefore = false;
		bool opensDevGuard = false;
	};

	GuardFrame guards[64]{};
	std::size_t depth = 0;
	bool devGuarded = false;
	bool sawToken = false;

	std::size_t lineStart = 0;
	while (lineStart <= source.size()) {
		const std::size_t lineEnd = source.find('\n', lineStart);
		const std::size_t count = lineEnd == std::string_view::npos
			? source.size() - lineStart
			: lineEnd - lineStart;
		const std::string_view line = source.substr(lineStart, count);
		const std::string_view trimmed = trimLeft(line);

		if (trimmed.starts_with("#if FLOW_UI_DEV_MODE")) {
			FLOWUI_CHECK(depth < 64);
			guards[depth++] = {.devGuardedBefore = devGuarded, .opensDevGuard = true};
			devGuarded = true;
		} else if (trimmed.starts_with("#if") || trimmed.starts_with("#ifdef") || trimmed.starts_with("#ifndef")) {
			FLOWUI_CHECK(depth < 64);
			guards[depth++] = {.devGuardedBefore = devGuarded, .opensDevGuard = false};
		} else if (trimmed.starts_with("#else")) {
			FLOWUI_CHECK(depth != 0);
			const GuardFrame& frame = guards[depth - 1];
			if (frame.opensDevGuard) devGuarded = frame.devGuardedBefore;
		} else if (trimmed.starts_with("#endif")) {
			FLOWUI_CHECK(depth != 0);
			devGuarded = guards[--depth].devGuardedBefore;
		} else if (line.find(token) != std::string_view::npos) {
			sawToken = true;
			if (!devGuarded) return false;
		}

		if (lineEnd == std::string_view::npos) break;
		lineStart = lineEnd + 1;
	}

	FLOWUI_CHECK(depth == 0);
	return sawToken;
}

std::size_t occurrenceCount(std::string_view source, std::string_view token) {
	std::size_t count = 0;
	std::size_t offset = 0;
	while ((offset = source.find(token, offset)) != std::string_view::npos) {
		++count;
		offset += token.size();
	}
	return count;
}

void testFlowIdCollisionImplementationIsDevGuarded() {
	const std::string uiState = readSource(FLOWUI_UI_MANAGER_STATE_SOURCE);
	const std::string uiManager = readSource(FLOWUI_UI_MANAGER_SOURCE);
	const std::string builder = readSource(FLOWUI_ELEMENT_BUILDER_SOURCE);
	const std::string bridge = readSource(FLOWUI_ELEMENT_BRIDGE_HEADER);

	// These marker names deliberately form the implementation boundary. Every
	// field, collector call, collision check, and bridge declaration carrying
	// this feature must be physically inside a FLOW_UI_DEV_MODE preprocessor
	// block, rather than relying on dead-code elimination in production.
	FLOWUI_CHECK(everyTokenOccurrenceIsInsideDevGuard(uiState, "flowRootIdTracker"));
	FLOWUI_CHECK(everyTokenOccurrenceIsInsideDevGuard(uiManager, "claimFlowRootForDev"));
	FLOWUI_CHECK(everyTokenOccurrenceIsInsideDevGuard(builder, "claimFlowRootForDev"));
	FLOWUI_CHECK(everyTokenOccurrenceIsInsideDevGuard(bridge, "claimFlowRootForDev"));
	// draw() and construct() share one typed invocation pipeline, so there is one
	// physically dev-gated root claim call for both terminal operations.
	FLOWUI_CHECK(occurrenceCount(builder, "detail::claimFlowRootForDev") == 1);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run(
		"Flow-ID collision storage and code are dev-guarded",
		testFlowIdCollisionImplementationIsDevGuarded);
	return runner.finish();
}
