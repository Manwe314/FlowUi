#include <cstdlib>
#include <iostream>
#include <vector>

#include <clay.h>

#include "devSystems/devTooling/tree/DevTreeCapture.hpp"

int main() {
	using namespace FlowUi;
	using namespace FlowUi::devSystems::tooling;
	std::vector<std::byte> memory(Clay_MinMemorySize());
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(memory.size(), memory.data());
	Clay_Context* clay = Clay_Initialize(arena, {640.0f, 480.0f}, {});
	if (!clay) return 1;
	Clay_SetCurrentContext(clay);
	Clay_BeginLayout();

	DevTreeCapture capture{};
	capture.beginFrame(9, 1, *clay, nullptr);
	const FlowElementID element{.value = 0x919u, .debugName = "flow-only"};
	const auto token = capture.beginFlow({
		.definition = FlowDefinitionID{91},
		.instance = element,
		.definitionName = "FlowOnly",
	});
	capture.endFlow(token);
	(void)Clay_EndLayout(0.016f);
	capture.finishAfterLayout();

	const DevTreeSnapshot& snapshot = capture.current();
	if (snapshot.generation != 1 || snapshot.flow.nodes.size() != 1 ||
		snapshot.string(snapshot.flow.nodes[0].debugName) != "flow-only" ||
		!snapshot.stats.complete) {
		std::cerr << "Flow-only DevTreeCapture did not publish the completed Flow forest\n";
		return 1;
	}
	return 0;
}
