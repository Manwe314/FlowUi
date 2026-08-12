#include "TestHarness.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"
#include "internal/FlowScopeStack.hpp"
#if FLOW_UI_DEV_MODE
#include "managers/UiManager.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"
#endif

namespace {

struct ButtonElementTag {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/id/button");
};

struct SliderElementTag {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/id/slider");
};

struct PanelElementTag {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/id/panel");
};

struct RawDefinitionTag {
	static constexpr uint64_t definitionId = 0x8d3725cb49e10af6ull;
};

inline constexpr ButtonElementTag kButton{};
inline constexpr SliderElementTag kSlider{};
inline constexpr PanelElementTag kPanel{};
inline constexpr RawDefinitionTag kRawDefinition{};

template <auto& Element>
concept SupportsGlobalIdentity = requires {
	FlowUi::Global<Element>("test-global");
};

template <typename ID>
concept HasPublicClayFold = requires(ID id) {
	FlowUi::FlowIDToClayID(id);
};

constexpr FlowUi::LocalElementName kSaveName = "save";
constexpr FlowUi::LocalElementName kCancelName = "cancel";
constexpr FlowUi::FlowElementPart kSliderPart = FlowUi::Part("slider");
constexpr FlowUi::GlobalFlowID kGlobalSave = FlowUi::Global<kButton>("save");
constexpr FlowUi::GlobalFlowID kLongGlobalSave = FlowUi::GlobalID<kButton>("save");
constexpr FlowUi::IndexedElementName kIndexedRow = FlowUi::Indexed("row", 17);
constexpr FlowUi::IndexedElementName kKeyedRow = FlowUi::Keyed("row", 17);
constexpr FlowUi::AutoElementName kAutoToken =
	FlowUi::AutoIDAt("tests/ElementIDContractTests.cpp", 123, 9);
constexpr FlowUi::AutoElementName kFirstCapturedCallsite = FlowUi::AutoID();
constexpr FlowUi::AutoElementName kSecondCapturedCallsite = FlowUi::AutoID();

constexpr FlowUi::AutoElementName captureAutomaticDefault(
	FlowUi::AutoElementName name = FlowUi::AutoID()) {
	return name;
}

constexpr FlowUi::AutoElementName kFirstDefaultCallsite = captureAutomaticDefault();
constexpr FlowUi::AutoElementName kSecondDefaultCallsite = captureAutomaticDefault();

constexpr FlowUi::FlowElementID kRoot{
	.value = 0x4f92e7a1b36c508dull,
#if FLOW_UI_DEV_MODE
	.debugName = "@root",
#endif
};

template <typename Element>
consteval FlowUi::FlowElementID resolveLocalForTest(
	FlowUi::FlowElementID parent,
	const Element&,
	FlowUi::LocalElementName name) {
	return FlowUi::detail::element_id::resolveLocal(
		parent,
		Element::definitionId,
		name.token
#if FLOW_UI_DEV_MODE
		, name.debugName
#endif
	);
}

constexpr FlowUi::FlowElementID kLocalSave = FlowUi::detail::element_id::resolveLocal(
	kRoot,
	ButtonElementTag::definitionId,
	kSaveName.token
#if FLOW_UI_DEV_MODE
	, kSaveName.debugName
#endif
);

constexpr FlowUi::FlowElementID kLocalCancel = FlowUi::detail::element_id::resolveLocal(
	kRoot,
	ButtonElementTag::definitionId,
	kCancelName.token
#if FLOW_UI_DEV_MODE
	, kCancelName.debugName
#endif
);

constexpr FlowUi::FlowElementID kIndexedElement = FlowUi::detail::element_id::resolveLocal(
	kRoot,
	ButtonElementTag::definitionId,
	kIndexedRow.token
#if FLOW_UI_DEV_MODE
	, kIndexedRow.debugName
#endif
);

constexpr FlowUi::FlowElementID kAutomaticElement =
	FlowUi::detail::element_id::resolveAutomatic(
		kRoot,
		ButtonElementTag::definitionId,
		kAutoToken.token
#if FLOW_UI_DEV_MODE
		, kAutoToken.fileName
#endif
		);
constexpr FlowUi::FlowElementID kAutomaticSlider =
	FlowUi::detail::element_id::resolveAutomatic(
		kRoot,
		SliderElementTag::definitionId,
		kAutoToken.token);

constexpr FlowUi::FlowElementPartID kBoundSliderPart =
	FlowUi::PartID(kButton, kLocalSave, kSliderPart);
constexpr FlowUi::FlowElementPartID kSecondButtonSliderPart =
	FlowUi::PartID(kButton, kLocalCancel, kSliderPart);
constexpr FlowUi::FlowElementPartID kPanelSliderPart =
	FlowUi::PartID(kPanel, kLocalSave, kSliderPart);
constexpr FlowUi::FlowElementID kTopLevelPanel =
	resolveLocalForTest(FlowUi::RootFlowScopeID, kPanel, "settings");
constexpr FlowUi::FlowElementID kAutomaticUnderPanel =
	FlowUi::detail::element_id::resolveAutomatic(
		kTopLevelPanel,
		ButtonElementTag::definitionId,
		kAutoToken.token);
constexpr FlowUi::FlowElementID kNestedSave =
	resolveLocalForTest(kTopLevelPanel, kButton, "save");
constexpr FlowUi::FlowElementID kTopLevelSave =
	resolveLocalForTest(FlowUi::RootFlowScopeID, kButton, "save");
constexpr FlowUi::FlowElementID kRecursivePanel =
	resolveLocalForTest(kTopLevelPanel, kPanel, "settings");

static_assert(!std::is_convertible_v<FlowUi::FlowDefinitionID, FlowUi::FlowElementID>);
static_assert(!std::is_convertible_v<FlowUi::FlowElementID, FlowUi::FlowDefinitionID>);
static_assert(!std::is_convertible_v<FlowUi::FlowElementID, FlowUi::GlobalFlowID>);
static_assert(!std::is_convertible_v<FlowUi::GlobalFlowID, FlowUi::FlowElementID>);
static_assert(!std::is_convertible_v<FlowUi::FlowElementPart, FlowUi::FlowElementPartID>);
static_assert(!std::is_convertible_v<FlowUi::FlowElementPartID, FlowUi::FlowElementID>);
static_assert(!std::is_convertible_v<FlowUi::FlowElementID, uint64_t>);
static_assert(!std::is_convertible_v<uint64_t, FlowUi::FlowElementID>);
static_assert(!std::is_convertible_v<FlowUi::FlowDefinitionID, uint64_t>);
static_assert(HasPublicClayFold<FlowUi::FlowElementID>);
static_assert(HasPublicClayFold<FlowUi::GlobalFlowID>);
static_assert(HasPublicClayFold<FlowUi::FlowElementPartID>);
static_assert(!HasPublicClayFold<uint64_t>);
static_assert(!std::is_convertible_v<
	FlowUi::FlowElementID,
	FlowUi::detail::element::ElementInstanceKey>);
static_assert(!std::is_convertible_v<
	FlowUi::detail::element::ElementInstanceKey,
	FlowUi::FlowElementID>);

static_assert(std::is_trivially_copyable_v<FlowUi::FlowDefinitionID>);
static_assert(std::is_trivially_copyable_v<FlowUi::FlowElementID>);
static_assert(std::is_trivially_copyable_v<FlowUi::GlobalFlowID>);
static_assert(std::is_trivially_copyable_v<FlowUi::FlowElementPart>);
static_assert(std::is_trivially_copyable_v<FlowUi::FlowElementPartID>);
static_assert(std::is_trivially_copyable_v<FlowUi::LocalElementName>);
static_assert(std::is_trivially_copyable_v<FlowUi::RuntimeElementName>);
static_assert(std::is_trivially_copyable_v<FlowUi::IndexedElementName>);
static_assert(std::is_trivially_copyable_v<FlowUi::AutoElementName>);
static_assert(std::is_trivially_copyable_v<
	FlowUi::detail::element::ElementInstanceKey>);
static_assert(sizeof(FlowUi::detail::element::ElementInstanceKey) == sizeof(uint64_t));

#if !FLOW_UI_DEV_MODE
static_assert(sizeof(FlowUi::FlowDefinitionID) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::FlowElementID) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::GlobalFlowID) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::FlowElementPart) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::FlowElementPartID) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::LocalElementName) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::RuntimeElementName) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::IndexedElementName) == sizeof(uint64_t));
static_assert(sizeof(FlowUi::AutoElementName) == sizeof(uint64_t));
#endif

static_assert(kSaveName);
static_assert(!SupportsGlobalIdentity<kRawDefinition>);
static_assert(kCancelName);
static_assert(kSliderPart);
static_assert(kGlobalSave);
static_assert(kIndexedRow);
static_assert(kAutoToken);
static_assert(kLocalSave);
static_assert(kIndexedElement);
static_assert(kAutomaticElement);
static_assert(kAutomaticElement != kAutomaticUnderPanel);
static_assert(kAutomaticElement != kAutomaticSlider);
static_assert(kBoundSliderPart);
static_assert(kBoundSliderPart != kSecondButtonSliderPart);
static_assert(kBoundSliderPart != kPanelSliderPart);
static_assert(FlowUi::PartID(kButton, kLocalSave, kSliderPart) == kBoundSliderPart);
static_assert(FlowUi::RootFlowScopeID);
static_assert(kGlobalSave == kLongGlobalSave);
static_assert(kIndexedRow.token == kKeyedRow.token);
static_assert(kFirstCapturedCallsite.token != kSecondCapturedCallsite.token);
static_assert(kFirstDefaultCallsite.token != kSecondDefaultCallsite.token);
static_assert(kLocalSave != kLocalCancel);
static_assert(kNestedSave != kTopLevelSave);
static_assert(kRecursivePanel != kTopLevelPanel);
static_assert(
	resolveLocalForTest(FlowUi::RootFlowScopeID, kPanel, "settings") ==
	kTopLevelPanel);
static_assert(FlowUi::detail::element::toInstanceKey(kLocalSave).value == kLocalSave.value);
static_assert(FlowUi::detail::element::toInstanceKey(kBoundSliderPart).value == kBoundSliderPart.value);
static_assert(FlowUi::FlowIDToClayID(kLocalSave) != 0);
static_assert(FlowUi::FlowIDToClayID(FlowUi::InvalidFlowElementID) == 0);

// Golden values lock the initial serialized identity format. Any intentional
// algorithm/domain change must update these together with migration notes.
static_assert(ButtonElementTag::definitionId.value == 13231105510252788636ull);
static_assert(kLocalSave.value == 5979617354764644833ull);
static_assert(kIndexedElement.value == 11393374418967636827ull);
static_assert(kGlobalSave.value == 8117759928635473701ull);
static_assert(kAutoToken.token == 12402217401591132627ull);
static_assert(kAutomaticElement.value == 9889995015823979172ull);
static_assert(kSliderPart.token == 906657204996161764ull);
static_assert(kBoundSliderPart.value == 5562870610090380435ull);
static_assert(FlowUi::FlowIDToClayID(kLocalSave) == 1688294371u);

void testExplicitRuntimeNameHashing() {
	std::string runtimeStorage = "save";
	const FlowUi::RuntimeElementName runtimeName = FlowUi::RuntimeName(runtimeStorage);

	FLOWUI_CHECK(runtimeName.token == kSaveName.token);
	FLOWUI_CHECK(runtimeName.token != 0);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(runtimeName.debugName.data() == runtimeStorage.data());
	FLOWUI_CHECK(runtimeName.debugName == "save");
#endif

	FLOWUI_CHECK(!FlowUi::RuntimeName(""));
}

void testIndexedNamesAcceptRuntimeKeys() {
	uint64_t runtimeIndex = 31;
	const FlowUi::IndexedElementName first = FlowUi::Indexed("row", runtimeIndex);
	++runtimeIndex;
	const FlowUi::IndexedElementName second = FlowUi::Keyed("row", runtimeIndex);

	FLOWUI_CHECK(first);
	FLOWUI_CHECK(second);
	FLOWUI_CHECK(first.token != second.token);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(first.debugName == "row");
	FLOWUI_CHECK(first.index == 31);
	FLOWUI_CHECK(second.debugName == "row");
	FLOWUI_CHECK(second.index == 32);
#endif
}

void testLocalIndexedSequence() {
	auto ids = FlowUi::IndexedIDs("channel", 3);
	const FlowUi::IndexedElementName first = ids.next();
	const FlowUi::IndexedElementName second = ids.next();
	FLOWUI_CHECK(first == FlowUi::Indexed("channel", 3));
	FLOWUI_CHECK(second == FlowUi::Indexed("channel", 4));
	FLOWUI_CHECK(ids.nextIndex() == 5);
}

void testDomainSeparation() {
	FLOWUI_CHECK(ButtonElementTag::definitionId.value != kSaveName.token);
	FLOWUI_CHECK(kSaveName.token != kGlobalSave.value);
	FLOWUI_CHECK(kSliderPart.token != kSaveName.token);
	FLOWUI_CHECK(kBoundSliderPart.value != kLocalSave.value);
	FLOWUI_CHECK(FlowUi::Global<kSlider>("save").value != kGlobalSave.value);
}

void testFlowScopeStackLifecycle() {
	FlowUi::detail::manager_storage::FlowScopeStack scopes;
	FLOWUI_CHECK(scopes.current() == FlowUi::RootFlowScopeID);
	FLOWUI_CHECK(scopes.depth() == 0);

	scopes.beginFrame();
	FLOWUI_CHECK(scopes.current() == FlowUi::RootFlowScopeID);
	FLOWUI_CHECK(scopes.depth() == 1);

	const size_t rootDepth = scopes.push(kTopLevelPanel);
	FLOWUI_CHECK(rootDepth == 1);
	FLOWUI_CHECK(scopes.current() == kTopLevelPanel);
	const size_t panelDepth = scopes.push(kNestedSave);
	FLOWUI_CHECK(panelDepth == 2);
	FLOWUI_CHECK(scopes.current() == kNestedSave);

	scopes.restore(panelDepth);
	FLOWUI_CHECK(scopes.current() == kTopLevelPanel);
	scopes.restore(rootDepth);
	FLOWUI_CHECK(scopes.current() == FlowUi::RootFlowScopeID);

	FLOWUI_CHECK_THROWS(scopes.push(FlowUi::InvalidFlowElementID));
	FLOWUI_CHECK(scopes.current() == FlowUi::RootFlowScopeID);
	scopes.cancelFrame();
	FLOWUI_CHECK(scopes.depth() == 0);
	FLOWUI_CHECK(scopes.current() == FlowUi::RootFlowScopeID);
}

#if FLOW_UI_DEV_MODE
void testClayBridgeCollisionTracker() {
	FlowUi::detail::manager_storage::ClayBridgeIdTrackerForDev tracker{};
	tracker.beginFrame();

	const FlowUi::detail::manager_storage::FlowRootClaimSourceForDev firstSource{
		.definitionId = ButtonElementTag::definitionId,
		.debugPath = "tests/id/first",
	};
	const FlowUi::detail::manager_storage::FlowRootClaimSourceForDev secondSource{
		.definitionId = SliderElementTag::definitionId,
		.debugPath = "tests/id/second",
	};
	const FlowUi::detail::element::ElementInstanceKey first{.value = 11};
	const FlowUi::detail::element::ElementInstanceKey second{.value = 29};

	FLOWUI_CHECK(tracker.claim(first, 73, firstSource) == nullptr);
	FLOWUI_CHECK(tracker.claim(first, 73, firstSource) == nullptr);
	const auto* collision = tracker.claim(second, 73, secondSource);
	FLOWUI_CHECK(collision != nullptr);
	FLOWUI_CHECK(collision->clayId == 73);
	FLOWUI_CHECK(collision->first.instanceId == first);
	FLOWUI_CHECK(collision->duplicate.instanceId == second);
	FLOWUI_CHECK(collision->first.source.debugPath == "tests/id/first");
	FLOWUI_CHECK(collision->duplicate.source.debugPath == "tests/id/second");
	FLOWUI_CHECK(tracker.collisionCount() == 1);
	FLOWUI_CHECK(tracker.claim(second, 73, secondSource) == nullptr);
	FLOWUI_CHECK(tracker.collisionCount() == 1);

	tracker.beginFrame();
	FLOWUI_CHECK(tracker.collisionCount() == 0);
}

void testAutomaticAndGlobalDuplicateClaims() {
	FlowUi::detail::manager_storage::FlowRootIdTrackerForDev tracker{};
	tracker.beginFrame();
	const FlowUi::detail::manager_storage::FlowRootClaimSourceForDev automatic{
		.definitionId = ButtonElementTag::definitionId,
		.debugPath = "@auto/test.cpp:8:2",
		.fileName = "test.cpp",
		.line = 8,
		.column = 2,
		.automaticIdentity = true,
	};
	const auto automaticId = FlowUi::detail::element::toInstanceKey(kAutomaticElement);
	FLOWUI_CHECK(tracker.claim(automaticId, automatic) == nullptr);
	const auto* automaticCollision = tracker.claim(automaticId, automatic);
	FLOWUI_CHECK(automaticCollision != nullptr);
	FLOWUI_CHECK(automaticCollision->first.automaticIdentity);
	FLOWUI_CHECK(automaticCollision->duplicate.automaticIdentity);

	tracker.beginFrame();
	const FlowUi::detail::manager_storage::FlowRootClaimSourceForDev global{
		.definitionId = ButtonElementTag::definitionId,
		.debugPath = "@global/save",
	};
	const auto globalId = FlowUi::detail::element::toInstanceKey(kGlobalSave);
	FLOWUI_CHECK(tracker.claim(globalId, global) == nullptr);
	FLOWUI_CHECK(tracker.claim(globalId, global) != nullptr);
	tracker.beginFrame();
	FLOWUI_CHECK(tracker.claim(globalId, global) == nullptr);
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("runtime names are explicit and hash their contents", testExplicitRuntimeNameHashing);
	runner.run("indexed names accept runtime numeric keys", testIndexedNamesAcceptRuntimeKeys);
	runner.run("local indexed sequences increment from their own starting position", testLocalIndexedSequence);
	runner.run("identity domains remain separated", testDomainSeparation);
	runner.run("Flow scope stack restores nesting and cancellation", testFlowScopeStackLifecycle);
#if FLOW_UI_DEV_MODE
	runner.run("Clay bridge detects injected 64-to-32 collisions", testClayBridgeCollisionTracker);
	runner.run("automatic and global duplicate claims are frame-local", testAutomaticAndGlobalDuplicateClaims);
#endif
	return runner.finish();
}
