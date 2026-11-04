#include "FlowUi/Flow.hpp"
#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionTargets.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace tooling = FlowUi::devSystems::tooling;

void test_targets() {
#if FLOW_UI_DEV_CAPTURE_CLAY
	tooling::DevTreeSnapshot tree;
	tree.window = 3;
	tree.app_tick = 47;
	tree.frameNumber = 123;
	tree.generation = 9;
	tree.stats.complete = true;
	tree.strings = {'r', 'a', 'w', 'c', 'l', 'i', 'p', 'r', 'a', 'w'};
	tooling::DevClayNode raw;
	raw.clayId = 10;
	raw.idString = {0, 3};
	raw.bounds = {-10, 5, 100, 50};
	raw.clipClayId = 20;
	raw.rootIndex = 0;
	tooling::DevClayNode clip;
	clip.clayId = 20;
	clip.idString = {3, 4};
	clip.bounds = {20, 10, 30, 25};
	clip.declaration.clip = {.horizontal = true, .vertical = true};
	tree.clay.nodes = {raw, clip};
	tree.clay.roots.emplace_back(tooling::DevClayRoot{.node = 0, .zIndex = 4, .paintOrder = 7});
	AgenticInteractionTargetRegistry registry;
	AgenticInteractionTarget selector;
	selector.kind = AgenticTargetKind::clay_name;
	selector.clay_name = "raw";
	selector.window_id = 3;
	const auto resolved = registry.resolve(tree, selector, 2);
	FLOWUI_CHECK(resolved && resolved->clay_id == 10 && resolved->visible_region.x == 20);
	FLOWUI_CHECK(resolved->layout_point.x == 35 && resolved->layout_point.y == 22.5);
	FLOWUI_CHECK(resolved->window_point.x == 70 && resolved->window_point.y == 45);
	FLOWUI_CHECK(resolved->tree_generation == 9 && resolved->window_frame == 123 &&
				 resolved->z_index == 4 && resolved->paint_order == 7);
	FLOWUI_CHECK(resolved->hit_test_supported && !resolved->hit_test_verified);
	InteractionSnapshot actual;
	actual.hoveredElementIds = {10, 20, 99};
	actual.pressedElementIds = {10};
	auto observation = registry.observe(*resolved, actual);
	FLOWUI_CHECK(observation && observation->find("target_hovered")->bool_or() &&
				 observation->find("target_pressed")->bool_or());
	FLOWUI_CHECK(observation->find("other_hovered_clay_ids")->array.size() == 2);
	FLOWUI_CHECK(!observation->find("exclusive_routing_proven")->bool_or());
	FLOWUI_CHECK(!registry.observe(*resolved, actual, 2));
	selector.tree_generation = 8;
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	selector.tree_generation.reset();
	selector.layout_revision = 124;
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	selector.layout_revision.reset();
	selector.window_id = 4;
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	selector.window_id = 3;
	tree.clay.nodes.emplace_back(raw);
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.clay.nodes.pop_back();
	tree.clay.nodes[1].clipClayId = 20;
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.clay.nodes[1].clipClayId = 0;
	const auto viewport_clipped =
		registry.resolve(tree, selector, 2, AgenticInteractionRegion{0, 0, 25, 20});
	FLOWUI_CHECK(viewport_clipped && viewport_clipped->visible_region.width == 5 &&
				 viewport_clipped->visible_region.height == 10 &&
				 viewport_clipped->window_point.x == 45);
	FLOWUI_CHECK(!registry.resolve(tree, selector, 2, AgenticInteractionRegion{100, 100, 25, 20}));
	tree.clay.nodes[1].bounds = {200, 100, 30, 25};
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.clay.nodes[1] = clip;
	tree.clay.nodes[0].bounds.width = std::numeric_limits<float>::quiet_NaN();
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.clay.nodes[0] = raw;
	tooling::DevFlowNode flow;
	flow.instance.value = 9007199254741007ull;
	flow.clayRoot = 0;
	flow.debugName = {0, 3};
	tree.flow.nodes.emplace_back(flow);
	selector.kind = AgenticTargetKind::flow_instance;
	selector.flow_instance_id = flow.instance.value;
	const auto flow_target = registry.resolve(tree, selector);
	FLOWUI_CHECK(flow_target && flow_target->flow_instance_id == flow.instance.value);
	tree.flow.nodes.emplace_back(flow);
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.flow.nodes.pop_back();
	tree.stats.truncated = true;
	FLOWUI_CHECK(!registry.resolve(tree, selector));
	tree.stats.truncated = false;
	selector.kind = AgenticTargetKind::coordinates;
	selector.point = {40, 10};
	const auto point = registry.resolve(tree, selector, 2);
	FLOWUI_CHECK(point && point->layout_point.x == 20 && point->window_point.x == 40);
#endif
}
void test_custom_targets_and_export_validation() {
	AgenticInteractionTargetRegistry targets(1);
	AgenticCustomInteractionTarget custom;
	custom.target_id = "canvas.hit-region";
	custom.window_id = 4;
	custom.revision = 7;
	custom.bounds = {-5, -10, 40, 30};
	custom.clips = {{0, 0, 50, 50}};
	custom.app_tick = 2;
	custom.window_frame = 12;
	custom.expires_after_window_frame = 15;
	custom.observe = [](const AgenticResolvedInteractionTarget& target)
		-> Result<AgenticCustomInteractionObservation> {
		AgenticCustomInteractionObservation observed;
		observed.hovered = target.layout_point.x == 17.5;
		observed.outcome = Json::make_object({{"canvas", "actual_custom_hit_test"}});
		return observed;
	};
	FLOWUI_CHECK(targets.register_target(custom));
	FLOWUI_CHECK(!targets.register_target(custom));
	tooling::DevTreeSnapshot tree;
	tree.window = 4;
	tree.frameNumber = 13;
	AgenticInteractionTarget selector;
	selector.window_id = 4;
	selector.kind = AgenticTargetKind::custom_region;
	selector.custom_id = custom.target_id;
	selector.custom_revision = 7;
	auto resolved = targets.resolve(tree, selector, 1.5f);
	FLOWUI_CHECK(resolved && resolved->custom_revision == 7 && resolved->layout_point.x == 17.5 &&
				 resolved->window_point.x == 26.25);
	FLOWUI_CHECK(resolved->app_tick == 2 && resolved->window_frame == 12);
	InteractionSnapshot actual;
	auto observed = targets.observe(*resolved, actual);
	FLOWUI_CHECK(observed && observed->find("target_hovered")->bool_or());
	custom.revision = 8;
	FLOWUI_CHECK(targets.register_target(custom));
	FLOWUI_CHECK(!targets.observe(*resolved, actual));
	FLOWUI_CHECK(!targets.resolve(tree, selector));
	selector.custom_revision = 8;
	FLOWUI_CHECK(targets.resolve(tree, selector));
	tree.frameNumber = 16;
	FLOWUI_CHECK(!targets.resolve(tree, selector));
	auto second = custom;
	second.target_id = "another";
	FLOWUI_CHECK(!targets.register_target(second));
	FLOWUI_CHECK(!targets.unregister_target(custom.target_id, 7));
	FLOWUI_CHECK(targets.unregister_target(custom.target_id, 8));
	AgenticActionRegistry actions(1);
	AgenticActionDeclaration declaration;
	declaration.action_id = "explicit.action";
	FLOWUI_CHECK(!actions.register_bound_action(declaration, {}));
	AgenticActionAdapter adapter;
	FLOWUI_CHECK(!actions.register_action(declaration, adapter));
	adapter.decode_arguments = [](const Json& arguments) -> Result<Json> { return arguments; };
	adapter.invoke = [](App&, const AgenticActionInvocation&,
						const Json&) -> Result<AgenticActionReceipt> {
		AgenticActionReceipt receipt;
		receipt.completed = true;
		receipt.completion_succeeded = true;
		return receipt;
	};
	declaration.asynchronous = true;
	FLOWUI_CHECK(!actions.register_action(declaration, adapter));
	declaration.asynchronous = false;
	FLOWUI_CHECK(actions.register_action(declaration, adapter));
	FLOWUI_CHECK(!actions.register_action(declaration, adapter));
	declaration.revision = 2;
	FLOWUI_CHECK(actions.register_action(declaration, adapter));
	declaration.action_id = "overflow";
	FLOWUI_CHECK(!actions.register_action(declaration, adapter));
	FLOWUI_CHECK(!actions.unregister_action("explicit.action", 1));
	FLOWUI_CHECK(actions.unregister_action("explicit.action", 2));
}
#if !defined(FLOWUI_AGENTIC_TARGET_PURE_ONLY)
void test_native_actions() {
	std::uint32_t side_effects = 0, polls = 0;
	AppConfig config;
	config.window.title = "Agentic exported action contracts";
	config.window.width = 160;
	config.window.height = 120;
	config.vk.enableValidation = false;
	config.vk.enableDebugUtils = false;
	config.errors.observer.mode = ErrorReportingMode::SinkOnly;
	config.agentic_workflow.enabled = false;
	auto application = makeApplication(config);
	AgenticActionRegistry actions;
	constexpr auto increment_id = AppAction("test.agentic.export.increment");
	const auto bound = application.actions().appActions().bind(
		increment_id, [](std::uint32_t* count) { ++*count; }, &side_effects);
	FLOWUI_CHECK(bound);
	AgenticActionInvocation invocation;
	invocation.action_id = "increment";
	invocation.revision = 1;
	invocation.plan_id = "plan";
	invocation.step_id = "step";
	invocation.request_id = "request";
	invocation.invocation_sequence = 9007199254741003ull;
	invocation.window_id = application.mainWindowId();
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	FLOWUI_CHECK(side_effects == 0);
	AgenticActionDeclaration declaration;
	declaration.action_id = "increment";
	FLOWUI_CHECK(actions.register_bound_action(declaration, *bound));
	auto receipt = actions.invoke(application, invocation);
	FLOWUI_CHECK(receipt && receipt->dispatched && receipt->completed &&
				 receipt->completion_succeeded && side_effects == 1);
	const auto debug = application.actions().debugInfo(ActionCall(*bound));
	FLOWUI_CHECK(debug && debug->lastSource.kind == ActionInvocationSourceKind::Agentic &&
				 debug->lastSource.sourceId == invocation.invocation_sequence);
	FLOWUI_CHECK(AgenticActionRegistry::describe(*receipt).find("plan_id")->string_or() == "plan");
	invocation.arguments.set("unaccepted", true);
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	FLOWUI_CHECK(side_effects == 1);
	invocation.arguments = Json::make_object();
	invocation.revision = 2;
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	invocation.revision = 1;
	FLOWUI_CHECK(
		application.actions().appActions().setAvailability(increment_id, {.enabled = false}));
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	FLOWUI_CHECK(side_effects == 1);
	FLOWUI_CHECK(
		application.actions().appActions().setAvailability(increment_id, {.enabled = true}));
	AgenticActionAdapter adapter;
	adapter.decode_arguments = [](const Json& arguments) -> Result<Json> {
		const auto* amount = arguments.find("amount");
		if (arguments.object.size() != 1 || !amount || amount->kind != Json::Kind::number ||
			amount->number_or() != 7)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		return *amount;
	};
	adapter.available = [](const App&) -> Result<bool> { return true; };
	adapter.invoke = [&side_effects](App&, const AgenticActionInvocation&,
									 const Json& amount) -> Result<AgenticActionReceipt> {
		side_effects += static_cast<std::uint32_t>(amount.uint64_or());
		AgenticActionReceipt receipt;
		receipt.completion_token = "pending_work_7";
		return receipt;
	};
	adapter.poll_completion =
		[&polls](App&, const AgenticActionReceipt& previous) -> Result<AgenticActionReceipt> {
		auto result = previous;
		++polls;
		result.completed = polls >= 2;
		result.completion_succeeded = result.completed;
		if (result.completed)
			result.result = Json::make_object({{"saved", true}});
		return result;
	};
	declaration.action_id = "typed.save";
	declaration.asynchronous = true;
	FLOWUI_CHECK(actions.register_action(declaration, adapter));
	invocation.action_id = "typed.save";
	invocation.arguments = Json::make_object({{"amount", "7"}});
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	FLOWUI_CHECK(side_effects == 1);
	invocation.arguments = Json::make_object({{"amount", Json::number(7)}});
	auto pending = actions.invoke(application, invocation);
	FLOWUI_CHECK(pending && pending->dispatched && !pending->completed && side_effects == 8);
	pending = actions.poll(application, *pending);
	FLOWUI_CHECK(pending && !pending->completed && side_effects == 8);
	pending = actions.poll(application, *pending);
	FLOWUI_CHECK(pending && pending->completed && pending->completion_succeeded &&
				 side_effects == 8);
	auto timed = actions.invoke(application, invocation);
	FLOWUI_CHECK(timed);
	timed->invocation.deadline_monotonic_ns = 1;
	timed = actions.poll(application, *timed);
	FLOWUI_CHECK(timed && timed->completed && !timed->completion_succeeded &&
				 timed->reason == "application_completion_timeout");
	FLOWUI_CHECK(actions.cancel(application, *pending));
	constexpr auto throw_id = AppAction("test.agentic.export.throw");
	const auto throwing = application.actions().appActions().bind(
		throw_id, []() { throw std::runtime_error("app failure"); });
	FLOWUI_CHECK(throwing);
	declaration.action_id = "throws";
	declaration.asynchronous = false;
	FLOWUI_CHECK(actions.register_bound_action(declaration, *throwing));
	invocation.action_id = "throws";
	invocation.arguments = Json::make_object();
	auto failed = actions.invoke(application, invocation);
	FLOWUI_CHECK(failed && failed->dispatched && failed->completed &&
				 !failed->completion_succeeded && failed->reason == "application_callback_failed");
	FLOWUI_CHECK(application.actions().appActions().unbind(increment_id));
	invocation.action_id = "increment";
	FLOWUI_CHECK(!actions.invoke(application, invocation));
	auto exported = actions.list(application);
	FLOWUI_CHECK(exported && exported->find("actions")->array.size() == 3);
}
#endif
} // namespace
int main(int argc, char** argv) {
	FlowUi::test::Runner runner;
#if !defined(FLOWUI_AGENTIC_TARGET_PURE_ONLY)
	if (argc == 2 && std::string_view(argv[1]) == "native")
		runner.run(
			"explicit exported dispatch, typed arguments and asynchronous business completion",
			test_native_actions);
	else
#endif
	{
		runner.run(
			"raw Clay and optional Flow targets, clips, revisions, DPI and observed overlaps",
			test_targets);
		runner.run("owned custom target adapters and explicit export declaration validation",
				   test_custom_targets_and_export_validation);
	}
	return runner.finish();
}
