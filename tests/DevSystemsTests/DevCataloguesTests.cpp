#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <string_view>

#include "devSystems/devTooling/catalogue/DevCatalogues.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"

namespace catalogue_test {

struct Parameters {
	float opacity = 1.0f;
	bool visible = true;
};

struct State {
	unsigned selections = 0;
};

struct Element {
	using Parameters = catalogue_test::Parameters;
	using State = catalogue_test::State;
	static constexpr FlowUi::FlowDefinitionID definitionId{0xc470001u};
	static constexpr std::string_view debugName = "Catalogue test element";
	static void buildElement(FlowUi::ElementBuildContext<Element>&) {}
};

FLOWUI_DEV_SCHEMA(
	Parameters,
	FLOWUI_DEV_FIELD(Parameters, opacity),
	FLOWUI_DEV_FIELD(Parameters, visible))

FLOWUI_DEV_SCHEMA(State, FLOWUI_DEV_FIELD(State, selections))

void verifyElementCatalogueCachingAndTreeCorrelation() {
	using namespace FlowUi;
	using namespace FlowUi::devMode;
	using namespace FlowUi::devSystems::tooling;

	DevSchemaRegistry schemas;
	schemas.ensureElement<Element>();
	assert(schemas.publishPendingAtSafePoint());

	DevCatalogues catalogues;
	catalogues.bindManagers(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &schemas);

	const auto initial = catalogues.queryElements(nullptr);
	assert(initial.size() == 1);
	assert(initial.front().definition == Element::definitionId);
	assert(initial.front().definitionName == Element::debugName);
	assert(initial.front().reflectedFieldCount == 2);
	assert(initial.front().activeInstanceCount == 0);
	assert(initial.front().isBakeable);

	const DevElementCatalogEntry* const stableAddress = initial.data();
	assert(catalogues.queryElements(nullptr).data() == stableAddress);

	DevTreeSnapshot tree{};
	tree.generation = 7;
	tree.flow.nodes.resize(3);
	tree.flow.nodes[0].definition = Element::definitionId;
	tree.flow.nodes[1].definition = Element::definitionId;
	tree.flow.nodes[2].definition = FlowDefinitionID{0xc470002u};
	const auto correlated = catalogues.queryElements(&tree);
	assert(correlated.size() == 1);
	assert(correlated.front().activeInstanceCount == 2);
	assert(catalogues.queryElements(&tree).data() == correlated.data());
	assert(catalogues.memoryFootprintBytes() >= sizeof(DevElementCatalogEntry));
}

void verifyUnboundCategoriesAreEmpty() {
	FlowUi::devMode::DevCatalogues catalogues;
	assert(catalogues.queryImages().empty());
	assert(catalogues.queryIcons().empty());
	assert(catalogues.queryAtlases().empty());
	assert(catalogues.queryFonts().empty());
	assert(catalogues.queryActions().empty());
	assert(catalogues.queryThemes().empty());
	assert(!catalogues.acquireInspectionLease().isValid());
}

} // namespace catalogue_test

int main() {
	catalogue_test::verifyElementCatalogueCachingAndTreeCorrelation();
	catalogue_test::verifyUnboundCategoriesAreEmpty();
	return 0;
}
