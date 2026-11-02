#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/Theme.hpp"
#include "TestHarness.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorModel.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include <array>
#include <limits>
#include <optional>

using namespace FlowUi;
using namespace FlowUi::devSystems::interface_elements::theme_editor;
namespace {
struct ComparisonValues {
	std::vector<std::string> labels{"one", "two"};
	std::optional<float> radius{8};
	std::array<float, 2> position{1, 2};
	Clay_Color color{12, 34, 56, 255};
	float amount = 0;
	int* unsupported = nullptr;
};
FLOWUI_DEV_SCHEMA(ComparisonValues, FLOWUI_DEV_FIELD(ComparisonValues, labels),
				  FLOWUI_DEV_FIELD(ComparisonValues, radius),
				  FLOWUI_DEV_FIELD(ComparisonValues, position),
				  FLOWUI_DEV_FIELD(ComparisonValues, color),
				  FLOWUI_DEV_FIELD(ComparisonValues, amount),
				  FLOWUI_DEV_FIELD(ComparisonValues, unsupported))
void test_semantic_comparisons() {
	devMode::DevSchemaRegistry registry;
	registry.ensureStruct<ComparisonValues>();
	FLOWUI_CHECK(registry.publishPendingAtSafePoint());
	const auto schema = registry.view();
	const auto* type = schema->findType(detail::typeHash<ComparisonValues>());
	FLOWUI_CHECK(type);
	const auto owner = devMode::DevTypeIndex{static_cast<uint32_t>(type - schema->types.data())};
	ComparisonValues left{}, right{};
	const auto compare_field = [&](std::string_view name) {
		for (const auto& field : schema->fieldsOf(owner))
			if (schema->string(field.name) == name) {
				const auto* operations = schema->fieldOperations[field.operations];
				return compare_values(*schema, field.valueType, operations->constAddress(&left),
									  operations->constAddress(&right));
			}
		return ValueComparison::Unavailable;
	};
	FLOWUI_CHECK(compare_field("labels") == ValueComparison::Equal);
	right.labels.back() = "different";
	FLOWUI_CHECK(compare_field("labels") == ValueComparison::Different);
	right.labels = left.labels;
	right.labels.emplace_back("third");
	FLOWUI_CHECK(compare_field("labels") == ValueComparison::Different);
	FLOWUI_CHECK(compare_field("radius") == ValueComparison::Equal);
	right.radius.reset();
	FLOWUI_CHECK(compare_field("radius") == ValueComparison::Different);
	left.radius.reset();
	FLOWUI_CHECK(compare_field("radius") == ValueComparison::Equal);
	right.position[1] = 3;
	FLOWUI_CHECK(compare_field("position") == ValueComparison::Different);
	FLOWUI_CHECK(compare_field("color") == ValueComparison::Equal);
	right.color.a = 128;
	FLOWUI_CHECK(compare_field("color") == ValueComparison::Different);
	FLOWUI_CHECK(compare_field("amount") == ValueComparison::Equal);
	right.amount = -0.0f;
	FLOWUI_CHECK(compare_field("amount") == ValueComparison::Different);
	left.amount = -0.0f;
	FLOWUI_CHECK(compare_field("amount") == ValueComparison::Equal);
	right.amount = std::numeric_limits<float>::quiet_NaN();
	FLOWUI_CHECK(compare_field("amount") == ValueComparison::Unavailable);
	right.amount = std::numeric_limits<float>::infinity();
	FLOWUI_CHECK(compare_field("amount") == ValueComparison::Unavailable);
	FLOWUI_CHECK(compare_field("unsupported") == ValueComparison::Unavailable);
}
void test_fsel_compound_comparison() {
	devMode::DevSchemaRegistry registry;
	registry.ensureTheme<FSEL::FSELTheme>();
	FLOWUI_CHECK(registry.publishPendingAtSafePoint());
	const auto schema = registry.view();
	const auto root = schema->findTheme(detail::typeHash<FSEL::FSELTheme>())->themeType;
	FSEL::FSELTheme left{}, right{};
	FLOWUI_CHECK(compare_values(*schema, root, &left, &right) == ValueComparison::Equal);
	right.buttonTheme.idle.backgroundColor.r += 1;
	FLOWUI_CHECK(compare_values(*schema, root, &left, &right) == ValueComparison::Different);
	right = left;
	right.textInputTheme.overlayStyle.caretBlinkPeriodSeconds = 2;
	FLOWUI_CHECK(compare_values(*schema, root, &left, &right) == ValueComparison::Different);
	FLOWUI_CHECK(compare_values(*schema, root, nullptr, &right) == ValueComparison::Unavailable);
}
} // namespace
int main() {
	FlowUi::test::Runner runner;
	runner.run("theme values use semantic comparisons with explicit unavailable results",
			   test_semantic_comparisons);
	runner.run("FSEL comparison walks compound fields", test_fsel_compound_comparison);
	return runner.finish();
}
