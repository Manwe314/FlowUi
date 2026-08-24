add_executable(flowui_storage_types_tests
    StorageTests/StorageTypesTests.cpp
)

target_compile_features(flowui_storage_types_tests PRIVATE cxx_std_23)
target_include_directories(flowui_storage_types_tests
    PRIVATE
        ${FLOWUI_GENERATED_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME flowui.storage.types COMMAND flowui_storage_types_tests)
set_tests_properties(flowui.storage.types PROPERTIES
    LABELS "flowui;storage;unit"
    TIMEOUT 30
)

add_executable(flowui_text_layout_service_tests
	TextTests/TextLayoutServiceTests.cpp
)
target_compile_features(flowui_text_layout_service_tests PRIVATE cxx_std_23)
target_include_directories(flowui_text_layout_service_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_text_layout_service_tests PRIVATE
	FLOWUI_UI_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/UiManager.cpp"
	FLOWUI_INPUT_FIELD_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/InputFieldManager.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/Ui/Vk_UiRenderer.cpp"
)
target_link_libraries(flowui_text_layout_service_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.text.layout_service COMMAND flowui_text_layout_service_tests)
set_tests_properties(flowui.text.layout_service PROPERTIES
	LABELS "flowui;text;unit"
	TIMEOUT 30
)

add_executable(flowui_text_storage_viewport_tests
	TextTests/TextStorageViewportTests.cpp
)
target_compile_features(flowui_text_storage_viewport_tests PRIVATE cxx_std_23)
target_include_directories(flowui_text_storage_viewport_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_text_storage_viewport_tests PRIVATE
	FLOWUI_INPUT_FIELD_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/InputFieldManager.cpp"
	FLOWUI_INPUT_FIELD_STATE_SOURCE="${PROJECT_SOURCE_DIR}/include/internal/ManagerStorage/InputFieldManagerState.hpp"
)
target_link_libraries(flowui_text_storage_viewport_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.text.storage_viewport COMMAND flowui_text_storage_viewport_tests)
set_tests_properties(flowui.text.storage_viewport PROPERTIES
	LABELS "flowui;text;input;unit"
	TIMEOUT 30
)

add_executable(flowui_action_value_contract_tests
	ActionTests/ActionValueContractTests.cpp
)
target_compile_features(flowui_action_value_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_action_value_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_action_value_contract_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.actions.value_contracts COMMAND flowui_action_value_contract_tests)
set_tests_properties(flowui.actions.value_contracts PROPERTIES
	LABELS "flowui;actions;unit"
	TIMEOUT 30
)

add_executable(flowui_app_action_manager_tests
	ActionTests/AppActionManagerTests.cpp
	HeadlessVulkanFixture.cpp
)
target_compile_features(flowui_app_action_manager_tests PRIVATE cxx_std_23)
target_include_directories(flowui_app_action_manager_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
	target_compile_definitions(flowui_app_action_manager_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_app_action_manager_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
add_test(NAME flowui.actions.app_manager COMMAND flowui_app_action_manager_tests)
set_tests_properties(flowui.actions.app_manager PROPERTIES
	LABELS "flowui;actions;storage;integration"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_library(flowui_action_dev_compile_contract OBJECT
	ActionTests/ActionValueContractTests.cpp
	${PROJECT_SOURCE_DIR}/src/managers/ActionManager.cpp
)
target_compile_features(flowui_action_dev_compile_contract PRIVATE cxx_std_23)
target_compile_definitions(flowui_action_dev_compile_contract PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_action_dev_compile_contract PRIVATE
	${FLOWUI_GENERATED_INCLUDE_DIR}
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/external
	${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(flowui_ui_renderer_conversion_tests
    RendererTests/UiRendererConversionTests.cpp
)
target_compile_features(flowui_ui_renderer_conversion_tests PRIVATE cxx_std_23)
target_include_directories(flowui_ui_renderer_conversion_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_ui_renderer_conversion_tests
	PRIVATE
		FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/Ui/Vk_UiRenderer.cpp"
		FLOWUI_APP_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi.cpp"
		FLOWUI_IMAGE_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/ImageManager.cpp"
		FLOWUI_ICON_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/IconManager.cpp"
		FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/ViewPortManager.cpp"
)
target_link_libraries(flowui_ui_renderer_conversion_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.ui.renderer_conversion COMMAND flowui_ui_renderer_conversion_tests)
set_tests_properties(flowui.ui.renderer_conversion PROPERTIES
    LABELS "flowui;ui;renderer;unit"
    TIMEOUT 30
)

add_executable(flowui_phase4_api_tests ApiTests/Phase4ApiTests.cpp)
target_compile_features(flowui_phase4_api_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase4_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_phase4_api_tests PRIVATE
	FLOWUI_APP_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi.cpp"
	FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/ViewPortManager.cpp"
	FLOWUI_SWAPCHAIN_SOURCE="${PROJECT_SOURCE_DIR}/src/Vulkan/Vk_Swapchain.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/Ui/Vk_UiRenderer.cpp"
)
target_link_libraries(flowui_phase4_api_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.phase4.api COMMAND flowui_phase4_api_tests)
set_tests_properties(flowui.phase4.api PROPERTIES LABELS "flowui;phase4;unit" TIMEOUT 30)

add_executable(flowui_phase5_manager_storage_tests ApiTests/Phase5ManagerStorageTests.cpp)
target_compile_features(flowui_phase5_manager_storage_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase5_manager_storage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_phase5_manager_storage_tests PRIVATE
	FLOWUI_IMAGE_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/ImageManager.cpp"
	FLOWUI_ICON_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/IconManager.cpp"
	FLOWUI_FONT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/FontManager.cpp"
	FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/ViewPortManager.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/Ui/Vk_UiRenderer.cpp"
	FLOWUI_STORAGE_INTERFACE_SOURCE="${PROJECT_SOURCE_DIR}/include/internal/StorageSystem/IStorageSystem.hpp"
	FLOWUI_STORAGE_TYPES_SOURCE="${PROJECT_SOURCE_DIR}/include/internal/StorageSystem/StorageTypes.hpp"
)
target_link_libraries(flowui_phase5_manager_storage_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.phase5.manager_storage COMMAND flowui_phase5_manager_storage_tests)
set_tests_properties(flowui.phase5.manager_storage PROPERTIES
	LABELS "flowui;phase5;managers;storage;unit"
	TIMEOUT 30
)

add_executable(flowui_element_definition_registration_tests
	ElementResourceTests/PhaseDDefinitionRegistrationTests.cpp
)
target_compile_features(flowui_element_definition_registration_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_definition_registration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_definition_registration_tests PRIVATE FlowUi::FlowUi)
add_test(
	NAME flowui.elements.definition_registration
	COMMAND flowui_element_definition_registration_tests
)
set_tests_properties(flowui.elements.definition_registration PROPERTIES
	LABELS "flowui;elements;unit"
	TIMEOUT 30
)

add_executable(flowui_element_concept_contract_tests
	ElementResourceTests/ElementConceptContractTests.cpp
)
target_compile_features(flowui_element_concept_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_concept_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_concept_contract_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.elements.concept_contracts COMMAND flowui_element_concept_contract_tests)
set_tests_properties(flowui.elements.concept_contracts PROPERTIES
	LABELS "flowui;elements;unit"
	TIMEOUT 30
)

if(FLOWUI_COMPILE_FSELI_ENABLED)
	add_executable(flowui_fsel_button_contract_tests
		FSELTests/ButtonContractTests.cpp
	)
	target_compile_features(flowui_fsel_button_contract_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_fsel_button_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_button_contract_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.fsel.button_contracts COMMAND flowui_fsel_button_contract_tests)
	set_tests_properties(flowui.fsel.button_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_selectable_surface_contract_tests
		FSELTests/SelectableSurfaceContractTests.cpp
	)
	target_compile_features(flowui_fsel_selectable_surface_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_selectable_surface_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_selectable_surface_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.selectable_surface_contracts
		COMMAND flowui_fsel_selectable_surface_contract_tests
	)
	set_tests_properties(flowui.fsel.selectable_surface_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_radio_choice_contract_tests
		FSELTests/RadioChoiceContractTests.cpp
	)
	target_compile_features(flowui_fsel_radio_choice_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_radio_choice_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_radio_choice_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.radio_choice_contracts
		COMMAND flowui_fsel_radio_choice_contract_tests
	)
	set_tests_properties(flowui.fsel.radio_choice_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_boolean_control_contract_tests
		FSELTests/BooleanControlContractTests.cpp
	)
	target_compile_features(flowui_fsel_boolean_control_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_boolean_control_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_boolean_control_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.boolean_control_contracts
		COMMAND flowui_fsel_boolean_control_contract_tests
	)
	set_tests_properties(flowui.fsel.boolean_control_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_slider_contract_tests
		FSELTests/SliderContractTests.cpp
	)
	target_compile_features(flowui_fsel_slider_contract_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_fsel_slider_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_slider_contract_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.fsel.slider_contracts COMMAND flowui_fsel_slider_contract_tests)
	set_tests_properties(flowui.fsel.slider_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_progress_bar_contract_tests
		FSELTests/ProgressBarContractTests.cpp
	)
	target_compile_features(flowui_fsel_progress_bar_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_progress_bar_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_progress_bar_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.progress_bar_contracts
		COMMAND flowui_fsel_progress_bar_contract_tests
	)
	set_tests_properties(flowui.fsel.progress_bar_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_label_contract_tests
		FSELTests/LabelContractTests.cpp
	)
	target_compile_features(flowui_fsel_label_contract_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_fsel_label_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_label_contract_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.fsel.label_contracts COMMAND flowui_fsel_label_contract_tests)
	set_tests_properties(flowui.fsel.label_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_library(flowui_fsel_label_dev_compile_contract OBJECT
		FSELTests/LabelContractTests.cpp
	)
	target_compile_features(flowui_fsel_label_dev_compile_contract
		PRIVATE cxx_std_23
	)
	target_compile_definitions(flowui_fsel_label_dev_compile_contract
		PRIVATE
			FLOW_UI_DEV_MODE=1
			COMPILE_FSELI=1
	)
	target_include_directories(flowui_fsel_label_dev_compile_contract
		PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR}
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/external
			${CMAKE_CURRENT_SOURCE_DIR}
	)

	add_executable(flowui_fsel_popup_surface_contract_tests
		FSELTests/PopupSurfaceContractTests.cpp
	)
	target_compile_features(flowui_fsel_popup_surface_contract_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_fsel_popup_surface_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_popup_surface_contract_tests PRIVATE FlowUi::FlowUi)
	add_test(
		NAME flowui.fsel.popup_surface_contracts
		COMMAND flowui_fsel_popup_surface_contract_tests
	)
	set_tests_properties(flowui.fsel.popup_surface_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_library(flowui_fsel_popup_surface_dev_compile_contract OBJECT
		FSELTests/PopupSurfaceContractTests.cpp
	)

	add_executable(flowui_fsel_combo_box_contract_tests
		FSELTests/ComboBoxContractTests.cpp
	)
	target_compile_features(flowui_fsel_combo_box_contract_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_fsel_combo_box_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_combo_box_contract_tests PRIVATE FlowUi::FlowUi)
	add_test(
		NAME flowui.fsel.combo_box_contracts
		COMMAND flowui_fsel_combo_box_contract_tests
	)
	set_tests_properties(flowui.fsel.combo_box_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_library(flowui_fsel_combo_box_dev_compile_contract OBJECT
		FSELTests/ComboBoxContractTests.cpp
	)
	target_compile_features(flowui_fsel_combo_box_dev_compile_contract
		PRIVATE cxx_std_23
	)
	target_compile_definitions(flowui_fsel_combo_box_dev_compile_contract
		PRIVATE
			FLOW_UI_DEV_MODE=1
			COMPILE_FSELI=1
	)
	target_include_directories(flowui_fsel_combo_box_dev_compile_contract
		PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR}
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/external
			${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_compile_features(flowui_fsel_popup_surface_dev_compile_contract
		PRIVATE cxx_std_23
	)
	target_compile_definitions(flowui_fsel_popup_surface_dev_compile_contract
		PRIVATE
			FLOW_UI_DEV_MODE=1
			COMPILE_FSELI=1
	)
	target_include_directories(flowui_fsel_popup_surface_dev_compile_contract
		PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR}
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/external
			${CMAKE_CURRENT_SOURCE_DIR}
	)

	add_executable(flowui_fsel_text_field_contract_tests
		FSELTests/TextFieldContractTests.cpp
	)
	target_compile_features(flowui_fsel_text_field_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_text_field_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_text_field_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.text_field_contracts
		COMMAND flowui_fsel_text_field_contract_tests
	)
	set_tests_properties(flowui.fsel.text_field_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_executable(flowui_fsel_number_input_contract_tests
		FSELTests/NumberInputContractTests.cpp
	)
	target_compile_features(flowui_fsel_number_input_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_number_input_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_number_input_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.number_input_contracts
		COMMAND flowui_fsel_number_input_contract_tests
	)
	set_tests_properties(flowui.fsel.number_input_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_library(flowui_fsel_number_input_dev_compile_contract OBJECT
		FSELTests/NumberInputContractTests.cpp
	)
	target_compile_features(flowui_fsel_number_input_dev_compile_contract
		PRIVATE cxx_std_23
	)
	target_compile_definitions(flowui_fsel_number_input_dev_compile_contract
		PRIVATE
			FLOW_UI_DEV_MODE=1
			COMPILE_FSELI=1
	)
	target_include_directories(flowui_fsel_number_input_dev_compile_contract
		PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR}
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/external
			${CMAKE_CURRENT_SOURCE_DIR}
	)

	add_executable(flowui_fsel_drag_value_contract_tests
		FSELTests/DragValueContractTests.cpp
	)
	target_compile_features(flowui_fsel_drag_value_contract_tests
		PRIVATE cxx_std_23
	)
	target_include_directories(flowui_fsel_drag_value_contract_tests
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	)
	target_link_libraries(flowui_fsel_drag_value_contract_tests
		PRIVATE FlowUi::FlowUi
	)
	add_test(
		NAME flowui.fsel.drag_value_contracts
		COMMAND flowui_fsel_drag_value_contract_tests
	)
	set_tests_properties(flowui.fsel.drag_value_contracts PROPERTIES
		LABELS "flowui;fsel;elements;unit"
		TIMEOUT 30
	)

	add_library(flowui_fsel_drag_value_dev_compile_contract OBJECT
		FSELTests/DragValueContractTests.cpp
	)
	target_compile_features(flowui_fsel_drag_value_dev_compile_contract
		PRIVATE cxx_std_23
	)
	target_compile_definitions(flowui_fsel_drag_value_dev_compile_contract
		PRIVATE
			FLOW_UI_DEV_MODE=1
			COMPILE_FSELI=1
	)
	target_include_directories(flowui_fsel_drag_value_dev_compile_contract
		PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR}
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/external
			${CMAKE_CURRENT_SOURCE_DIR}
	)

	# Interactive showcase: built with the tests, but deliberately not registered
	# with CTest because it runs until its window is closed.
	add_executable(flowui_fsel_demo demo/main.cpp)
	target_compile_features(flowui_fsel_demo PRIVATE cxx_std_23)
	target_link_libraries(flowui_fsel_demo PRIVATE FlowUi::FlowUi)
endif()

# Compile the typed builder's dev-only attachment points without pulling in the
# intentionally legacy developer element catalog or linking against a dev ABI.
add_library(flowui_element_builder_dev_compile_contract OBJECT
	ElementResourceTests/ElementConceptContractTests.cpp
)
target_compile_features(flowui_element_builder_dev_compile_contract PRIVATE cxx_std_23)
target_compile_definitions(flowui_element_builder_dev_compile_contract PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_element_builder_dev_compile_contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_builder_dev_compile_contract PRIVATE FlowUi::FlowUi)

# Compile UiManager's internal dev registry/capture/reporting path while the
# deliberately frozen legacy dev element UI remains excluded.
add_library(flowui_internal_dev_runtime_compile_contract OBJECT
	${PROJECT_SOURCE_DIR}/src/managers/UiManager.cpp
)
target_compile_features(flowui_internal_dev_runtime_compile_contract PRIVATE cxx_std_23)
target_compile_definitions(flowui_internal_dev_runtime_compile_contract PRIVATE
	FLOW_UI_DEV_MODE=1
	FLOWUI_SKIP_LEGACY_DEV_ELEMENTS=1
)
target_include_directories(flowui_internal_dev_runtime_compile_contract
	PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
)

add_executable(flowui_element_scope_lifecycle_tests
	ElementResourceTests/ElementScopeLifecycleTests.cpp
)
target_compile_features(flowui_element_scope_lifecycle_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_scope_lifecycle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_scope_lifecycle_tests PRIVATE FlowUi::FlowUi)
add_test(
	NAME flowui.elements.scope_lifecycle
	COMMAND flowui_element_scope_lifecycle_tests
)
set_tests_properties(flowui.elements.scope_lifecycle PROPERTIES
	LABELS "flowui;elements;integration;scope"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_executable(flowui_element_id_contract_tests
	ElementResourceTests/ElementIDContractTests.cpp
)
target_compile_features(flowui_element_id_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_id_contract_tests
	PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
		${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME flowui.elements.id_contracts COMMAND flowui_element_id_contract_tests)
set_tests_properties(flowui.elements.id_contracts PROPERTIES
	LABELS "flowui;elements;unit"
	TIMEOUT 30
)

# This target validates dev-only ID metadata and collision bookkeeping without
# compiling the intentionally legacy developer element catalog.
add_executable(flowui_element_id_dev_contract_tests
	ElementResourceTests/ElementIDContractTests.cpp
)
target_compile_features(flowui_element_id_dev_contract_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_element_id_dev_contract_tests PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_element_id_dev_contract_tests
	PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
		${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME flowui.elements.id_dev_contracts COMMAND flowui_element_id_dev_contract_tests)
set_tests_properties(flowui.elements.id_dev_contracts PROPERTIES
	LABELS "flowui;elements;dev;unit"
	TIMEOUT 30
)

# Exercise the internal developer registry/runtime and diagnostics without
# compiling the intentionally legacy dev element catalog or JSON persistence.
add_executable(flowui_dev_runtime_id_contract_tests
	ElementResourceTests/DevRuntimeIDContractTests.cpp
	${PROJECT_SOURCE_DIR}/src/devMode/devRuntime.cpp
)
target_compile_features(flowui_dev_runtime_id_contract_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_runtime_id_contract_tests PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_dev_runtime_id_contract_tests
	PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
		${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME flowui.elements.dev_runtime_id_contracts COMMAND flowui_dev_runtime_id_contract_tests)
set_tests_properties(flowui.elements.dev_runtime_id_contracts PROPERTIES
	LABELS "flowui;elements;dev;unit"
	TIMEOUT 30
)

add_executable(flowui_dev_id_reporting_contract_tests
	ElementResourceTests/PhaseADevFlowIdTests.cpp
	ElementResourceTests/DevFlowIdContractTestDriver.cpp
)
target_compile_features(flowui_dev_id_reporting_contract_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_id_reporting_contract_tests PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_dev_id_reporting_contract_tests
	PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
		${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME flowui.elements.dev_id_reporting_contracts COMMAND flowui_dev_id_reporting_contract_tests)
set_tests_properties(flowui.elements.dev_id_reporting_contracts PROPERTIES
	LABELS "flowui;elements;dev;unit"
	TIMEOUT 30
)

add_executable(flowui_element_interaction_id_contract_tests
	ElementResourceTests/InteractionIDContractTests.cpp
)
target_compile_features(flowui_element_interaction_id_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_interaction_id_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_interaction_id_contract_tests PRIVATE FlowUi::FlowUi)
add_test(
	NAME flowui.elements.interaction_id_contracts
	COMMAND flowui_element_interaction_id_contract_tests
)
set_tests_properties(flowui.elements.interaction_id_contracts PROPERTIES
	LABELS "flowui;elements;unit"
	TIMEOUT 30
)

add_executable(flowui_input_field_id_contract_tests
	ElementResourceTests/InputFieldIDContractTests.cpp
)
target_compile_features(flowui_input_field_id_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_input_field_id_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_input_field_id_contract_tests PRIVATE FlowUi::FlowUi)
add_test(
	NAME flowui.input_fields.id_contracts
	COMMAND flowui_input_field_id_contract_tests
)
set_tests_properties(flowui.input_fields.id_contracts PROPERTIES
	LABELS "flowui;input-fields;elements;unit"
	TIMEOUT 30
)

add_executable(flowui_element_state_api_tests
	ElementResourceTests/PhaseFStateApiTests.cpp
)
target_compile_features(flowui_element_state_api_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_state_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_state_api_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.elements.state_api COMMAND flowui_element_state_api_tests)
set_tests_properties(flowui.elements.state_api PROPERTIES
	LABELS "flowui;elements;unit"
	TIMEOUT 30
)

add_executable(flowui_element_state_gc_tests
	ElementResourceTests/PhaseGStateGcTests.cpp
	HeadlessVulkanFixture.cpp
)
target_compile_features(flowui_element_state_gc_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_state_gc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
	target_compile_definitions(flowui_element_state_gc_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_element_state_gc_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
add_test(NAME flowui.elements.state_gc COMMAND flowui_element_state_gc_tests)
set_tests_properties(flowui.elements.state_gc PROPERTIES
	LABELS "flowui;elements;storage;unit"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_executable(flowui_element_resources_tests
	ElementResourceTests/PhaseHResourcesTests.cpp
	HeadlessVulkanFixture.cpp
)
target_compile_features(flowui_element_resources_tests PRIVATE cxx_std_23)
target_include_directories(flowui_element_resources_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
	target_compile_definitions(flowui_element_resources_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_element_resources_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
add_test(NAME flowui.elements.resources COMMAND flowui_element_resources_tests)
set_tests_properties(flowui.elements.resources PROPERTIES
	LABELS "flowui;elements;storage;unit"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

if(FLOW_UI_DEV_MODE)
	add_executable(flowui_element_flow_id_dev_tests
		ElementResourceTests/PhaseADevFlowIdTests.cpp
		ElementResourceTests/DevFlowIdContractTestDriver.cpp
	)
	target_compile_features(flowui_element_flow_id_dev_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_element_flow_id_dev_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(flowui_element_flow_id_dev_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.elements.flow_id_dev COMMAND flowui_element_flow_id_dev_tests)
	set_tests_properties(flowui.elements.flow_id_dev PROPERTIES
		LABELS "flowui;elements;dev;unit"
		TIMEOUT 30
	)
else()
	add_executable(flowui_element_flow_id_production_isolation_tests
		ElementResourceTests/PhaseAProductionIsolationTests.cpp
	)
	target_compile_features(flowui_element_flow_id_production_isolation_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_element_flow_id_production_isolation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_definitions(flowui_element_flow_id_production_isolation_tests PRIVATE
		FLOWUI_UI_MANAGER_STATE_SOURCE="${PROJECT_SOURCE_DIR}/include/internal/ManagerStorage/UiManagerState.hpp"
		FLOWUI_UI_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/managers/UiManager.cpp"
		FLOWUI_ELEMENT_BUILDER_SOURCE="${PROJECT_SOURCE_DIR}/include/managers/FlowUiElementBuilder.hpp"
		FLOWUI_ELEMENT_BRIDGE_HEADER="${PROJECT_SOURCE_DIR}/include/internal/FlowUiElementBridge.hpp"
	)
	target_link_libraries(flowui_element_flow_id_production_isolation_tests PRIVATE FlowUi::FlowUi)
	add_test(
		NAME flowui.elements.flow_id_production_isolation
		COMMAND flowui_element_flow_id_production_isolation_tests
	)
	set_tests_properties(flowui.elements.flow_id_production_isolation PROPERTIES
		LABELS "flowui;elements;production;unit"
		TIMEOUT 30
	)
endif()

add_executable(flowui_phase4_window_tests WindowTests/Phase4WindowTests.cpp)
target_compile_features(flowui_phase4_window_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase4_window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_phase4_window_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.phase4.two_window COMMAND flowui_phase4_window_tests)
set_tests_properties(flowui.phase4.two_window PROPERTIES
	LABELS "flowui;phase4;integration;vulkan;window"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_executable(flowui_storage_system_tests
    StorageTests/FlowStorageSystemTests.cpp
    HeadlessVulkanFixture.cpp
)

set(FLOWUI_STORAGE_TEST_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/StorageTests/storage_pipeline_test.comp")
set(FLOWUI_STORAGE_TEST_SHADER_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/storage_pipeline_test.comp.spv")
add_custom_command(
    OUTPUT "${FLOWUI_STORAGE_TEST_SHADER_SPIRV}"
    COMMAND "${FLOWUI_SHADER_COMPILER}" ${FLOWUI_SHADER_COMPILE_ARGS}
        "${FLOWUI_STORAGE_TEST_SHADER_SOURCE}" -o "${FLOWUI_STORAGE_TEST_SHADER_SPIRV}"
    DEPENDS "${FLOWUI_STORAGE_TEST_SHADER_SOURCE}"
    COMMENT "Compiling FlowUi storage pipeline test shader"
    VERBATIM
)
add_custom_target(flowui_storage_test_shaders DEPENDS "${FLOWUI_STORAGE_TEST_SHADER_SPIRV}")
add_dependencies(flowui_storage_system_tests flowui_storage_test_shaders)

target_compile_features(flowui_storage_system_tests PRIVATE cxx_std_23)
target_include_directories(flowui_storage_system_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_storage_system_tests
    PRIVATE FLOWUI_STORAGE_TEST_COMPUTE_SPV="${FLOWUI_STORAGE_TEST_SHADER_SPIRV}"
)
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
    target_compile_definitions(flowui_storage_system_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_storage_system_tests
    PRIVATE
        FlowUi::FlowUi
        Vulkan::Vulkan
)

add_test(NAME flowui.storage.system COMMAND flowui_storage_system_tests)
set_tests_properties(flowui.storage.system PROPERTIES
    LABELS "flowui;storage;integration;vulkan"
    SKIP_RETURN_CODE 77
    TIMEOUT 60
)

if(FLOW_UI_DEV_MODE)
    add_executable(flowui_dev_error_tests
        DevMonitoringTests/DevErrorMonitoringTests.cpp
        DevSystemsTests/DevMemoryVulkanSupport.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/errors/DevError.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/errors/DevErrorStackProvider.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemory.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevErrorAdviceCatalogue.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTiming.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevGpuTiming.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTimingRecorder.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTimingZone.cpp
    )
    target_compile_features(flowui_dev_error_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_error_tests PRIVATE
        ${FLOWUI_GENERATED_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
    )
    target_link_libraries(flowui_dev_error_tests PRIVATE Vulkan::Vulkan)
    add_test(NAME flowui.dev.errors COMMAND flowui_dev_error_tests)
    set_tests_properties(flowui.dev.errors PROPERTIES
        LABELS "flowui;dev;errors;unit"
        TIMEOUT 30
    )

    add_executable(flowui_dev_memory_tests
        DevSystemsTests/DevMemoryTests.cpp
        DevSystemsTests/DevMemoryVulkanSupport.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemory.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.cpp
    )
    target_compile_features(flowui_dev_memory_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_memory_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
    )
    target_link_libraries(flowui_dev_memory_tests PRIVATE Vulkan::Vulkan)
    if(WIN32)
        target_link_libraries(flowui_dev_memory_tests PRIVATE Psapi)
    endif()
    add_test(NAME flowui.dev.memory COMMAND flowui_dev_memory_tests)
    set_tests_properties(flowui.dev.memory PROPERTIES LABELS "flowui;dev;memory")

    add_executable(flowui_storage_memory_dev_tests
        DevSystemsTests/DevMemoryStorageTests.cpp
        HeadlessVulkanFixture.cpp
        DevSystemsTests/DevMemoryVulkanSupport.cpp
        ${PROJECT_SOURCE_DIR}/src/Storagesystem/FlowStorageSystem.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.cpp
    )
    target_compile_features(flowui_storage_memory_dev_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_storage_memory_dev_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
    )
    target_compile_definitions(flowui_storage_memory_dev_tests PRIVATE
        FLOWUI_PUBLIC_VULKAN_INTEROP=1
    )
    if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
        target_compile_definitions(flowui_storage_memory_dev_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
    endif()
    target_link_libraries(flowui_storage_memory_dev_tests PRIVATE Vulkan::Vulkan)
    add_test(NAME flowui.dev.memory_storage COMMAND flowui_storage_memory_dev_tests)
    set_tests_properties(flowui.dev.memory_storage PROPERTIES
        LABELS "flowui;dev;memory;storage;integration;vulkan"
        SKIP_RETURN_CODE 77
        TIMEOUT 60
    )
endif()

add_executable(flowui_theme_manager_tests
    ThemesTests/ThemeManagerTests.cpp
    HeadlessVulkanFixture.cpp
)
target_compile_features(flowui_theme_manager_tests PRIVATE cxx_std_23)
target_include_directories(flowui_theme_manager_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
    target_compile_definitions(flowui_theme_manager_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_theme_manager_tests
    PRIVATE
        FlowUi::FlowUi
        Vulkan::Vulkan
)

add_test(NAME flowui.themes.manager COMMAND flowui_theme_manager_tests)
set_tests_properties(flowui.themes.manager PROPERTIES
    LABELS "flowui;themes;unit"
    SKIP_RETURN_CODE 77
    TIMEOUT 60
)
