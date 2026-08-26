#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#include <clay.h>

#include "devSystems/devTooling/tree/DevTreeCapture.hpp"

namespace {

using namespace FlowUi;
using namespace FlowUi::devSystems::tooling;

void require(bool condition, std::string_view message) {
	if (condition) return;
	std::cerr << "DevTreeCapture test failed: " << message << '\n';
	std::exit(1);
}

Clay_ElementId clayId(FlowElementID flow) {
	return Clay_ElementId{
		.id = FlowIDToClayID(flow),
		.stringId = Clay_String{
			.isStaticallyAllocated = true,
			.length = static_cast<int32_t>(flow.debugName.size()),
			.chars = flow.debugName.data(),
		},
	};
}

void open(FlowElementID flow, Clay_ElementDeclaration declaration = {}) {
	Clay__OpenElementWithId(clayId(flow));
	Clay__ConfigureOpenElement(declaration);
}

} // namespace

int main() {
	std::vector<std::byte> clayMemory(Clay_MinMemorySize());
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemory.size(), clayMemory.data());
	Clay_Context* clay = Clay_Initialize(arena, {800.0f, 600.0f}, {});
	require(clay != nullptr, "Clay context should initialize");
	Clay_SetCurrentContext(clay);

	DevTreeCapture capture{};
	const FlowElementID parent{.value = 0x101u, .debugName = "parent"};
	const FlowElementID child{.value = 0x202u, .debugName = "child"};
	const FlowElementID popup{.value = 0x303u, .debugName = "popup"};

	Clay_BeginLayout();
	capture.beginFrame(7, 1, *clay, nullptr);
	const auto parentToken = capture.beginFlow({
		.definition = FlowDefinitionID{11}, .instance = parent, .definitionName = "Parent"});
	Clay_ElementDeclaration parentDeclaration{};
	parentDeclaration.userData = reinterpret_cast<void*>(uintptr_t{1});
	parentDeclaration.image.imageData = reinterpret_cast<void*>(uintptr_t{2});
	open(parent, parentDeclaration);

	const auto childToken = capture.beginFlow({
		.definition = FlowDefinitionID{12}, .instance = child, .definitionName = "Child"});
	open(child);
	Clay__CloseElement();
	capture.endFlow(childToken);

	const auto popupToken = capture.beginFlow({
		.definition = FlowDefinitionID{13}, .instance = popup, .definitionName = "Popup"});
	Clay_ElementDeclaration popupDeclaration{};
	popupDeclaration.floating.attachTo = CLAY_ATTACH_TO_PARENT;
	open(popup, popupDeclaration);
	Clay__CloseElement();
	capture.endFlow(popupToken);

	CLAY_TEXT(CLAY_STRING("raw parent text"), CLAY_TEXT_CONFIG({}));
	Clay__CloseElement();
	capture.endFlow(parentToken);
	capture.noteAuthoredClayEnd();
	(void)Clay_EndLayout(0.016f);
	capture.finishAfterLayout();

	const DevTreeSnapshot& snapshot = capture.current();
	require(snapshot.generation == 1, "first completed tree should publish generation one");
	require(snapshot.flow.nodes.size() == 3, "three Flow nodes should be captured");
	require(snapshot.flow.nodes[0].firstChild == 1, "parent should link its first Flow child");
	require(snapshot.flow.nodes[1].nextSibling == 2, "normal and floating children should be siblings");
	require(snapshot.flow.nodes[2].parent == 0, "floating popup remains a semantic Flow child");
	require(hasFlag(snapshot.flow.nodes[2].flags, DevFlowNodeFlag::FloatingClayRoot),
		"floating popup should correlate to a separate Clay root");
	require(snapshot.flow.nodes[0].clayRoot != InvalidClayNode, "parent exact Clay root should resolve");
	require(snapshot.flow.nodes[1].clayRoot != InvalidClayNode, "child exact Clay root should resolve");
	require(snapshot.flow.nodes[2].clayRoot != InvalidClayNode, "popup exact Clay root should resolve");
	require(snapshot.clay.roots.size() == 2, "Clay forest should contain main and floating roots");
	require(snapshot.flow.nodes[0].directClayCount >= 2,
		"Flow leaf/parent direct contribution should include raw Clay implementation nodes");
	const DevClayNode& capturedParent = snapshot.clay.nodes[snapshot.flow.nodes[0].clayRoot];
	require(capturedParent.pointerPresence.userData && capturedParent.declaration.userData == nullptr,
		"published declarations should retain pointer presence but not user pointers");
	require(capturedParent.pointerPresence.imageData && capturedParent.declaration.image.imageData == nullptr,
		"published declarations should not retain image pointers");

	Clay_BeginLayout();
	capture.beginFrame(7, 2, *clay, nullptr);
	const FlowElementID empty{.value = 0x404u, .debugName = "empty"};
	const auto emptyToken = capture.beginFlow({
		.definition = FlowDefinitionID{14}, .instance = empty, .definitionName = "Empty"});
	capture.endFlow(emptyToken);
	capture.noteAuthoredClayEnd();
	(void)Clay_EndLayout(0.016f);
	capture.finishAfterLayout();
	const DevTreeSnapshot& invalid = capture.current();
	require(invalid.generation == 2, "invalid contracts should still publish a traversable snapshot");
	require(invalid.flow.nodes.size() == 1, "empty Flow invocation should remain visible");
	require(hasFlag(invalid.flow.nodes[0].flags, DevFlowNodeFlag::MissingClayRoot),
		"Flow invocation with no exact Clay root must be invalid");

	Clay_BeginLayout();
	capture.beginFrame(7, 3, *clay, nullptr);
	const auto malformedParent = capture.beginFlow({
		.definition = FlowDefinitionID{11}, .instance = parent});
	open(parent);
	Clay__CloseElement();
	const auto misplacedChild = capture.beginFlow({
		.definition = FlowDefinitionID{12}, .instance = child});
	open(child);
	Clay__CloseElement();
	capture.endFlow(misplacedChild);
	const FlowElementID escaped{.value = 0x505u, .debugName = "escaped"};
	open(escaped); // Directly owned sibling outside parent's distinguished root.
	Clay__CloseElement();
	capture.endFlow(malformedParent);
	capture.noteAuthoredClayEnd();
	(void)Clay_EndLayout(0.016f);
	capture.finishAfterLayout();
	const DevTreeSnapshot& malformed = capture.current();
	require(hasFlag(malformed.flow.nodes[0].flags, DevFlowNodeFlag::EscapedClayEmission),
		"direct Clay siblings outside the exact Flow root must be rejected");
	require(hasFlag(malformed.flow.nodes[1].flags, DevFlowNodeFlag::ClayParentMismatch),
		"semantic Flow children must agree with non-floating Clay ancestry");

	Clay_BeginLayout();
	capture.beginFrame(7, 4, *clay, nullptr);
	const auto canceledToken = capture.beginFlow({
		.definition = FlowDefinitionID{11}, .instance = parent});
	(void)canceledToken;
	capture.cancelFrame();
	require(capture.current().generation == 3,
		"cancellation must retain the last successfully completed snapshot");

	return 0;
}
