#include <FlowUi/Public/elementAuthoring/ElementConcepts.hpp>

#ifndef FLOWUI_DIAGNOSTIC_CASE
#define FLOWUI_DIAGNOSTIC_CASE 0
#endif

struct IncompletePayload;
struct NonDefaultPayload {
	NonDefaultPayload() = delete;
};
struct ThrowingDestructorPayload {
	~ThrowingDestructorPayload() noexcept(false);
};
struct UnsupportedResourcePayload {
	explicit UnsupportedResourcePayload(int);
};

struct DiagnosticElement {
	using BuildContext = FlowUi::ElementBuildContext<DiagnosticElement>;
	using InteractionContext = FlowUi::ElementInteractionContext<DiagnosticElement>;
#if FLOWUI_DIAGNOSTIC_CASE == 1
#elif FLOWUI_DIAGNOSTIC_CASE == 43
	static constexpr FlowUi::FlowDefinitionID definitionId{};
#elif FLOWUI_DIAGNOSTIC_CASE == 27
	static constexpr int definitionId = 1;
#elif FLOWUI_DIAGNOSTIC_CASE == 28
	static inline FlowUi::FlowDefinitionID definitionId = FlowUi::DefinitionID("tests/diagnostics/runtime-id");
#else
	static constexpr auto definitionId = FlowUi::DefinitionID("tests/diagnostics/element");
#endif

#if FLOWUI_DIAGNOSTIC_CASE == 42
	using Parameters = void;
	using State = void;
	using Resources = void;
	using Parts = void;
#elif FLOWUI_DIAGNOSTIC_CASE == 0
	using Parameters = int;
	using State = int;
	struct Resources {
		explicit Resources(FlowUi::App&);
	};
	struct Parts {};
#elif FLOWUI_DIAGNOSTIC_CASE == 2
	using Parameters = void;
#elif FLOWUI_DIAGNOSTIC_CASE == 29
	using Parameters = int&;
#elif FLOWUI_DIAGNOSTIC_CASE == 3
	using Parameters = IncompletePayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 5
	using Parameters = NonDefaultPayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 6
	using State = void;
#elif FLOWUI_DIAGNOSTIC_CASE == 30
	using State = int&;
#elif FLOWUI_DIAGNOSTIC_CASE == 7
	using State = IncompletePayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 4
	using State = NonDefaultPayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 8
	using State = ThrowingDestructorPayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 9
	using Resources = void;
#elif FLOWUI_DIAGNOSTIC_CASE == 31
	using Resources = int&;
#elif FLOWUI_DIAGNOSTIC_CASE == 10
	using Resources = IncompletePayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 13
	using Resources = UnsupportedResourcePayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 14
	using Resources = ThrowingDestructorPayload;
#elif FLOWUI_DIAGNOSTIC_CASE == 15
	using Parts = void;
#elif FLOWUI_DIAGNOSTIC_CASE == 32
	using Parts = int;
#elif FLOWUI_DIAGNOSTIC_CASE == 16
	using Parts = IncompletePayload;
#endif

#if FLOWUI_DIAGNOSTIC_CASE == 0
	static constexpr auto statePolicy = FlowUi::ElementStatePolicy::transient();
#elif FLOWUI_DIAGNOSTIC_CASE == 17
	static inline auto statePolicy = FlowUi::ElementStatePolicy::transient();
#elif FLOWUI_DIAGNOSTIC_CASE == 33
	static constexpr int statePolicy = 1;
#endif
#if FLOWUI_DIAGNOSTIC_CASE == 18
	static inline const char* debugName = "Diagnostic Fixture";
#elif FLOWUI_DIAGNOSTIC_CASE == 34
	static constexpr int debugName = 1;
#else
	static constexpr std::string_view debugName = "Diagnostic Fixture";
#endif
#if FLOWUI_DIAGNOSTIC_CASE == 0
	static constexpr bool isDevInternal = false;
#elif FLOWUI_DIAGNOSTIC_CASE == 19
	static inline bool isDevInternal = false;
#elif FLOWUI_DIAGNOSTIC_CASE == 35
	static constexpr std::string_view isDevInternal = "false";
#endif

#if FLOWUI_DIAGNOSTIC_CASE == 11
	static int buildElement(BuildContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 36
	static void buildElement(InteractionContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 37
	void buildElement(BuildContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 41
	static void buildElement(int);
	static void buildElement(float);
#elif FLOWUI_DIAGNOSTIC_CASE != 26
	static void buildElement(BuildContext&);
#endif
#if FLOWUI_DIAGNOSTIC_CASE == 0
	static Clay_ElementDeclaration constructElement(BuildContext&);
	static void onHovered(InteractionContext&);
	static void onHovered(int);
	static void onPressed(InteractionContext&);
	static void onHeld(InteractionContext&);
	static void onReleased(InteractionContext&);
	static void runLogic(InteractionContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 12
	static int constructElement(BuildContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 38
	static Clay_ElementDeclaration constructElement(InteractionContext&);
#endif
#if FLOWUI_DIAGNOSTIC_CASE == 20
	static void onHovered(BuildContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 21
	static int onPressed(InteractionContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 22
	void onHeld(InteractionContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 23
	static void onReleased(BuildContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 24
	static int runLogic(InteractionContext&);
#elif FLOWUI_DIAGNOSTIC_CASE == 25
	int instance_data;
#elif FLOWUI_DIAGNOSTIC_CASE == 39
	DiagnosticElement();
#elif FLOWUI_DIAGNOSTIC_CASE == 40
	DiagnosticElement(const DiagnosticElement&);
#endif
};

#if FLOWUI_DIAGNOSTIC_CASE == 0
inline char mutable_debug_characters[] = "Mutable characters";
struct MutableDebugCharactersElement {
	using BuildContext = FlowUi::ElementBuildContext<MutableDebugCharactersElement>;
	static constexpr auto definitionId = FlowUi::DefinitionID("tests/diagnostics/debug-characters");
	static constexpr std::string_view debugName{
		mutable_debug_characters, sizeof(mutable_debug_characters) - 1};
	static void buildElement(BuildContext&);
};
static_assert(FlowUi::FlowElement<MutableDebugCharactersElement>);
static_assert(FlowUi::validate_flow_element<MutableDebugCharactersElement>());
#endif

#if FLOWUI_DIAGNOSTIC_CASE == 0
static_assert(FlowUi::FlowElement<DiagnosticElement>);
static_assert(FlowUi::validate_flow_element<DiagnosticElement>());
static_assert(FlowUi::validate_flow_element<const DiagnosticElement&>());
static_assert(noexcept(FlowUi::validate_flow_element<DiagnosticElement>()));
#else
static_assert(!FlowUi::FlowElement<DiagnosticElement>);
#ifndef FLOWUI_QUERY_ONLY
static_assert(FlowUi::validate_flow_element<DiagnosticElement>());
#endif
#endif
