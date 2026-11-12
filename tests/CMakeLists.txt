add_executable(flowui_resource_tests ResourceTests.cpp)
target_link_libraries(flowui_resource_tests PRIVATE FlowUi::FlowUi)
target_compile_definitions(flowui_resource_tests PRIVATE FLOWUI_TEST_RESOURCE_SUBDIRECTORY="${FLOWUI_RESOURCE_SUBDIRECTORY}")
add_test(NAME flowui.resources COMMAND flowui_resource_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND NOT MSVC)
    add_test(NAME flowui.platform.selection_diagnostics
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PlatformTests/SelectionDiagnosticsTests.py
            --source ${PROJECT_SOURCE_DIR} --build ${PROJECT_BINARY_DIR}
            --compiler ${CMAKE_CXX_COMPILER} --cmake ${CMAKE_COMMAND})
    set_tests_properties(flowui.platform.selection_diagnostics PROPERTIES
        LABELS "flowui;platform;configuration" TIMEOUT 60)
    if(FLOWUI_HAS_GLFW_PROVIDER)
        add_test(NAME flowui.platform.concept_diagnostics
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/PlatformTests/ProviderConceptDiagnosticsTests.py
                --source ${PROJECT_SOURCE_DIR} --build ${PROJECT_BINARY_DIR}
                --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(flowui.platform.concept_diagnostics PROPERTIES
            LABELS "flowui;platform;concepts" TIMEOUT 120)
    endif()
endif()

add_executable(flowui_default_value_tests Defaults/DefaultValueTests.cpp)
target_include_directories(flowui_default_value_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_default_value_tests PRIVATE FlowUi::FlowUi)
if(FLOWUI_INCLUDE_ICON_MANAGER)
    target_link_libraries(flowui_default_value_tests PRIVATE plutosvg::plutosvg)
endif()
add_test(NAME flowui.defaults.values COMMAND flowui_default_value_tests)

add_executable(flowui_default_shortcut_tests Defaults/DefaultShortcutTests.cpp)
target_include_directories(flowui_default_shortcut_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_default_shortcut_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.defaults.shortcuts COMMAND flowui_default_shortcut_tests)

add_executable(flowui_input_service_tests InputServiceTests/InputServiceTests.cpp)
target_include_directories(flowui_input_service_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_input_service_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.input.semantic_service COMMAND flowui_input_service_tests)
set_tests_properties(flowui.input.semantic_service PROPERTIES LABELS "flowui;input;unit")

add_executable(flowui_input_public_headers_tests InputServiceTests/InputPublicHeadersTests.cpp)
target_compile_features(flowui_input_public_headers_tests PRIVATE cxx_std_23)
target_include_directories(flowui_input_public_headers_tests PRIVATE
    ${FLOWUI_GENERATED_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include)
add_test(NAME flowui.input.public_headers COMMAND flowui_input_public_headers_tests)

if(FLOWUI_ADDITIONAL_TRANSLATORS_TYPE STREQUAL "::FlowUi::examples::DeterministicFootpadPack" AND COMPILE_FSELI)
    add_executable(flowui_input_translator_ui_tests InputServiceTests/TranslatorUiTests.cpp)
    target_include_directories(flowui_input_translator_ui_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/example/input)
    target_link_libraries(flowui_input_translator_ui_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    flowui_package_resources(flowui_input_translator_ui_tests)
    add_test(NAME flowui.input.translator_ui COMMAND flowui_input_translator_ui_tests)
    set_tests_properties(flowui.input.translator_ui PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
endif()

add_executable(flowui_default_font_tests Defaults/DefaultFontTests.cpp)
target_include_directories(flowui_default_font_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_default_font_tests PRIVATE FlowUi::FlowUi FlowUi::artery)
add_test(NAME flowui.defaults.font_payload COMMAND flowui_default_font_tests --payload)
add_test(NAME flowui.defaults.font_recovery COMMAND flowui_default_font_tests)
add_test(NAME flowui.defaults.font_strict_recovery COMMAND flowui_default_font_tests --strict)
add_test(NAME flowui.defaults.font_immediate_failure COMMAND flowui_default_font_tests --immediate)
add_test(NAME flowui.defaults.font_configured_memory COMMAND flowui_default_font_tests --configured-memory)
set_tests_properties(
    flowui.defaults.font_recovery flowui.defaults.font_strict_recovery
    flowui.defaults.font_immediate_failure
    flowui.defaults.font_configured_memory
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 RUN_SERIAL TRUE
)

add_executable(flowui_internal_flags_tests InternalFlagsTests.cpp)
target_compile_features(flowui_internal_flags_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_internal_flags_tests PRIVATE FLOWUI_INTERNAL_VIEWPORT_MANAGER=1)
target_include_directories(flowui_internal_flags_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_internal_flags_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.internal.flags COMMAND flowui_internal_flags_tests)
set_tests_properties(flowui.internal.flags PROPERTIES LABELS "flowui;flags;unit" TIMEOUT 30)

add_executable(flowui_storage_types_tests
    StorageTests/StorageTypesTests.cpp
)

target_compile_features(flowui_storage_types_tests PRIVATE cxx_std_23)
target_include_directories(flowui_storage_types_tests
    PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
		${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME flowui.storage.types COMMAND flowui_storage_types_tests)
set_tests_properties(flowui.storage.types PROPERTIES
    LABELS "flowui;storage;unit"
    TIMEOUT 30
)

# Self-contained DEV tree tests deliberately compile the optional Clay bridge
# independently of the top-level DEV setting.
add_executable(flowui_dev_tree_capture_tests
	DevSystemsTests/DevTreeCaptureTests.cpp
	${PROJECT_SOURCE_DIR}/src/devSystems/devTooling/tree/DevTreeCapture.cpp
	${PROJECT_SOURCE_DIR}/src/FlowUi/Private/layoutService/Clay.cpp
)
target_compile_features(flowui_dev_tree_capture_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_tree_capture_tests PRIVATE
	FLOW_UI_DEV_MODE=1
	FLOW_UI_DEV_CAPTURE_CLAY=1
	FLOWUI_DEV_TIMING_LEVEL=0
	FLOWUI_DEV_MEMORY_LEVEL=0
	FLOWUI_DEV_ERROR_LEVEL=0
)
target_include_directories(flowui_dev_tree_capture_tests PRIVATE
	${FLOWUI_GENERATED_INCLUDE_DIR}
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/external
)
add_test(NAME flowui.dev.tree_capture COMMAND flowui_dev_tree_capture_tests)
set_tests_properties(flowui.dev.tree_capture PROPERTIES
	LABELS "flowui;dev;tree;unit"
	TIMEOUT 30
)

add_executable(flowui_dev_tree_capture_flow_only_tests
	DevSystemsTests/DevTreeCaptureFlowOnlyTests.cpp
	${PROJECT_SOURCE_DIR}/src/devSystems/devTooling/tree/DevTreeCapture.cpp
	${PROJECT_SOURCE_DIR}/src/FlowUi/Private/layoutService/Clay.cpp
)
target_compile_features(flowui_dev_tree_capture_flow_only_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_tree_capture_flow_only_tests PRIVATE
	FLOW_UI_DEV_MODE=1
	FLOW_UI_DEV_CAPTURE_CLAY=0
	FLOWUI_DEV_TIMING_LEVEL=0
	FLOWUI_DEV_MEMORY_LEVEL=0
	FLOWUI_DEV_ERROR_LEVEL=0
)
target_include_directories(flowui_dev_tree_capture_flow_only_tests PRIVATE
	${FLOWUI_GENERATED_INCLUDE_DIR}
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/external
)
add_test(NAME flowui.dev.tree_capture_flow_only COMMAND flowui_dev_tree_capture_flow_only_tests)
set_tests_properties(flowui.dev.tree_capture_flow_only PROPERTIES
	LABELS "flowui;dev;tree;unit"
	TIMEOUT 30
)

add_executable(flowui_dev_schema_registry_tests
	DevSystemsTests/DevSchemaRegistryTests.cpp
	${PROJECT_SOURCE_DIR}/src/devSystems/devTooling/schema/DevSchemaRegistry.cpp
)
target_compile_features(flowui_dev_schema_registry_tests PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_schema_registry_tests PRIVATE
	FLOW_UI_DEV_MODE=1
	FLOWUI_DEV_TIMING_LEVEL=0
	FLOWUI_DEV_MEMORY_LEVEL=0
	FLOWUI_DEV_ERROR_LEVEL=0
)
target_include_directories(flowui_dev_schema_registry_tests PRIVATE
	${FLOWUI_GENERATED_INCLUDE_DIR}
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/external
)
add_test(NAME flowui.dev.schema_registry COMMAND flowui_dev_schema_registry_tests)
set_tests_properties(flowui.dev.schema_registry PROPERTIES
	LABELS "flowui;dev;schema;unit"
	TIMEOUT 30
)

if(FLOW_UI_DEV_MODE)
	find_package(Threads REQUIRED)
	add_executable(flowui_dev_value_summary_tests DevSystemsTests/DevValueSummaryTests.cpp)
	target_compile_features(flowui_dev_value_summary_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_value_summary_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.value_summary COMMAND flowui_dev_value_summary_tests)
	if(AGENTIC_WORKFLOW AND FLOW_UI_DEV_CAPTURE_CLAY AND FLOWUI_HAS_GLFW_PROVIDER)
		add_executable(flowui_dev_interface_notes_fixture DevSystemsTests/DevInterfaceNotesFixture.cpp)
		target_compile_features(flowui_dev_interface_notes_fixture PRIVATE cxx_std_23)
		target_link_libraries(flowui_dev_interface_notes_fixture PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
		flowui_package_resources(flowui_dev_interface_notes_fixture)
		add_test(NAME flowui.dev.interface_notes_controls COMMAND flowui_dev_interface_notes_fixture --self-test)
		set_tests_properties(flowui.dev.interface_notes_controls PROPERTIES TIMEOUT 90)
	endif()
	if(FLOW_UI_HAS_BAKED_CHANGES_VALUE)
		add_executable(flowui_dev_baked_internal_isolation_tests
			DevSystemsTests/DevBakedInternalIsolationTests.cpp)
		target_compile_features(flowui_dev_baked_internal_isolation_tests PRIVATE cxx_std_23)
		target_link_libraries(flowui_dev_baked_internal_isolation_tests PRIVATE FlowUi::FlowUi)
		flowui_package_resources(flowui_dev_baked_internal_isolation_tests)
		add_test(NAME flowui.dev.baked_internal_isolation COMMAND flowui_dev_baked_internal_isolation_tests)
		set_tests_properties(flowui.dev.baked_internal_isolation PROPERTIES TIMEOUT 60)
	endif()

	foreach(timing_level RANGE 0 3)
		set(timing_test_target flowui_dev_timing_level_${timing_level}_tests)
		add_executable(${timing_test_target}
			DevSystemsTests/DevTimingLevelTests.cpp
			${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTiming.cpp
			${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTimingRecorder.cpp
			${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/timing/DevTimingZone.cpp)
		target_compile_features(${timing_test_target} PRIVATE cxx_std_23)
		target_compile_definitions(${timing_test_target} PRIVATE FLOWUI_DEV_TIMING_LEVEL=${timing_level})
		target_include_directories(${timing_test_target} PRIVATE
			${FLOWUI_GENERATED_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/external)
		target_link_libraries(${timing_test_target} PRIVATE Threads::Threads)
		add_test(NAME flowui.dev.timing_level_${timing_level} COMMAND ${timing_test_target})
	endforeach()

	add_executable(flowui_dev_performance_export_interaction_tests DevSystemsTests/DevPerformanceExportInteractionTests.cpp)
	target_compile_features(flowui_dev_performance_export_interaction_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_performance_export_interaction_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.performance_export_interaction COMMAND flowui_dev_performance_export_interaction_tests)
	set_tests_properties(flowui.dev.performance_export_interaction PROPERTIES TIMEOUT 60 LABELS "flowui;dev;integration")
	add_executable(flowui_dev_performance_export_tests DevSystemsTests/DevPerformanceExportTests.cpp)
	target_compile_features(flowui_dev_performance_export_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_performance_export_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.performance_export COMMAND flowui_dev_performance_export_tests)
	add_executable(flowui_dev_performance_inspector_tests DevSystemsTests/DevPerformanceInspectorTests.cpp)
	target_compile_features(flowui_dev_performance_inspector_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_performance_inspector_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.performance_inspector COMMAND flowui_dev_performance_inspector_tests)
	add_executable(flowui_dev_performance_inspector_interaction_tests DevSystemsTests/DevPerformanceInspectorInteractionTests.cpp)
	target_compile_features(flowui_dev_performance_inspector_interaction_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_performance_inspector_interaction_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.performance_inspector_interaction COMMAND flowui_dev_performance_inspector_interaction_tests)
	set_tests_properties(flowui.dev.performance_inspector_interaction PROPERTIES TIMEOUT 60 LABELS "flowui;dev;integration")
    add_executable(flowui_dev_performance_capture_interaction_tests DevSystemsTests/DevPerformanceCaptureInteractionTests.cpp)
    target_compile_features(flowui_dev_performance_capture_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_performance_capture_interaction_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.performance_capture_interaction COMMAND flowui_dev_performance_capture_interaction_tests)
    add_test(NAME flowui.dev.performance_startup_full COMMAND flowui_dev_performance_capture_interaction_tests startup-full)
    add_test(NAME flowui.dev.performance_startup_early COMMAND flowui_dev_performance_capture_interaction_tests startup-early)
    set_tests_properties(flowui.dev.performance_startup_full flowui.dev.performance_startup_early PROPERTIES TIMEOUT 60 LABELS "flowui;dev;integration")
    set_tests_properties(flowui.dev.performance_capture_interaction PROPERTIES TIMEOUT 60 LABELS "flowui;dev;integration")
    add_executable(flowui_dev_performance_capture_tests DevSystemsTests/DevPerformanceCaptureTests.cpp)
    target_compile_features(flowui_dev_performance_capture_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_performance_capture_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.performance_capture COMMAND flowui_dev_performance_capture_tests)
    add_executable(flowui_dev_timeline_layout_tests
        DevSystemsTests/DevTimelineLayoutTests.cpp)
    target_compile_features(flowui_dev_timeline_layout_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_timeline_layout_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.timeline_layout COMMAND flowui_dev_timeline_layout_tests)
    add_executable(flowui_dev_timeline_tests DevSystemsTests/DevTimelineTests.cpp)
    target_compile_features(flowui_dev_timeline_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_timeline_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.timeline COMMAND flowui_dev_timeline_tests)
    add_executable(flowui_dev_timeline_interaction_tests DevSystemsTests/DevTimelineInteractionTests.cpp)
    target_compile_features(flowui_dev_timeline_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_timeline_interaction_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.timeline_interaction COMMAND flowui_dev_timeline_interaction_tests)
    set_tests_properties(flowui.dev.timeline_interaction PROPERTIES LABELS "flowui;dev;integration" TIMEOUT 60)

    add_executable(flowui_dev_performance_selection_tests
        DevSystemsTests/DevPerformanceSelectionTests.cpp
    )
    target_compile_features(flowui_dev_performance_selection_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_performance_selection_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.performance_selection COMMAND flowui_dev_performance_selection_tests)

	add_executable(flowui_dev_catalogues_tests
		DevSystemsTests/DevCataloguesTests.cpp
	)
	target_compile_features(flowui_dev_catalogues_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_dev_catalogues_tests PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
	)
	target_link_libraries(flowui_dev_catalogues_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.catalogues COMMAND flowui_dev_catalogues_tests)
	add_executable(flowui_dev_catalogue_preview_tests DevSystemsTests/DevCataloguePreviewTests.cpp)
	target_compile_features(flowui_dev_catalogue_preview_tests PRIVATE cxx_std_23)
	target_link_libraries(flowui_dev_catalogue_preview_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.catalogue_previews COMMAND flowui_dev_catalogue_preview_tests)
	add_test(NAME flowui.dev.catalogue_preview_scopes COMMAND flowui_dev_catalogue_preview_tests --scopes)
	set_tests_properties(flowui.dev.catalogue_preview_scopes PROPERTIES LABELS "flowui;dev;catalogue;integration" TIMEOUT 30)
    add_executable(flowui_dev_font_atlas_layer_tests
        DevSystemsTests/DevFontAtlasLayerTests.cpp
        HeadlessVulkanFixture.cpp
    )
    target_compile_features(flowui_dev_font_atlas_layer_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_font_atlas_layer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_dev_font_atlas_layer_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
    flowui_package_resources(flowui_dev_font_atlas_layer_tests)
    set(FLOWUI_FONT_ATLAS_TEST_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/DevSystemsTests/font_atlas_sample.comp")
    set(FLOWUI_FONT_ATLAS_TEST_SHADER_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/font_atlas_sample.comp.spv")
    add_custom_command(
        OUTPUT "${FLOWUI_FONT_ATLAS_TEST_SHADER_SPIRV}"
        COMMAND "${FLOWUI_SHADER_COMPILER}" ${FLOWUI_SHADER_COMPILE_ARGS}
            "${FLOWUI_FONT_ATLAS_TEST_SHADER_SOURCE}" -o "${FLOWUI_FONT_ATLAS_TEST_SHADER_SPIRV}"
        DEPENDS "${FLOWUI_FONT_ATLAS_TEST_SHADER_SOURCE}"
        COMMENT "Compiling font atlas layer sampling regression shader"
        VERBATIM
    )
    add_custom_target(flowui_font_atlas_test_shader DEPENDS "${FLOWUI_FONT_ATLAS_TEST_SHADER_SPIRV}")
    add_dependencies(flowui_dev_font_atlas_layer_tests flowui_font_atlas_test_shader)
    target_compile_definitions(flowui_dev_font_atlas_layer_tests PRIVATE
        FLOWUI_FONT_ATLAS_TEST_COMPUTE_SPV="${FLOWUI_FONT_ATLAS_TEST_SHADER_SPIRV}"
        FLOWUI_FONT_ATLAS_TEST_FONT_PATH="${PROJECT_SOURCE_DIR}/assets/fonts/Inter.arfont"
    )
    if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
        target_compile_definitions(flowui_dev_font_atlas_layer_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
    endif()
    add_test(NAME flowui.dev.font_atlas_layers COMMAND flowui_dev_font_atlas_layer_tests)
    add_test(NAME flowui.dev.font_atlas_catalogue COMMAND flowui_dev_font_atlas_layer_tests --catalogue)
    set_tests_properties(flowui.dev.font_atlas_layers PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77 LABELS "flowui;dev;catalogue;integration;vulkan")
    set_tests_properties(flowui.dev.font_atlas_catalogue PROPERTIES TIMEOUT 60 LABELS "flowui;dev;catalogue;integration;vulkan;window")

	set_tests_properties(flowui.dev.catalogues PROPERTIES
		LABELS "flowui;dev;catalogue;unit"
		TIMEOUT 30
	)

	add_executable(flowui_dev_override_engine_tests
		DevSystemsTests/DevOverrideEngineTests.cpp
	)
	target_compile_features(flowui_dev_override_engine_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_dev_override_engine_tests PRIVATE
		${FLOWUI_GENERATED_INCLUDE_DIR}
		${PROJECT_SOURCE_DIR}/include
		${PROJECT_SOURCE_DIR}/external
	)
	target_link_libraries(flowui_dev_override_engine_tests PRIVATE FlowUi::FlowUi)
	add_test(NAME flowui.dev.override_engine COMMAND flowui_dev_override_engine_tests)
	set_tests_properties(flowui.dev.override_engine PROPERTIES
		LABELS "flowui;dev;override;unit"
		TIMEOUT 30
	)
endif()

add_executable(flowui_text_layout_service_tests
	TextTests/TextLayoutServiceTests.cpp
)
target_compile_features(flowui_text_layout_service_tests PRIVATE cxx_std_23)
target_include_directories(flowui_text_layout_service_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_text_layout_service_tests PRIVATE
	FLOWUI_UI_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/UiManager.cpp"
	FLOWUI_INPUT_FIELD_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/InputFieldManager.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/renderer/Vk_UiRenderer.cpp"
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
	FLOWUI_INPUT_FIELD_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/InputFieldManager.cpp"
	FLOWUI_INPUT_FIELD_STATE_SOURCE="${PROJECT_SOURCE_DIR}/include/FlowUi/Private/inputService/InputFieldManagerState.hpp"
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
	${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ActionManager.cpp
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
		FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/renderer/Vk_UiRenderer.cpp"
		FLOWUI_APP_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/AppRuntime.cpp"
		FLOWUI_APP_FACADE_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/App.cpp"
		FLOWUI_IMAGE_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ImageManager.cpp"
		FLOWUI_ICON_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/IconManager.cpp"
		FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ViewPortManager.cpp"
)
target_link_libraries(flowui_ui_renderer_conversion_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.ui.renderer_conversion COMMAND flowui_ui_renderer_conversion_tests)
set_tests_properties(flowui.ui.renderer_conversion PROPERTIES
    LABELS "flowui;ui;renderer;unit"
    TIMEOUT 30
)

add_executable(flowui_ui_renderer_antialiasing_tests
    RendererTests/UiRendererAntialiasingTests.cpp
    HeadlessVulkanFixture.cpp
)
target_compile_features(flowui_ui_renderer_antialiasing_tests PRIVATE cxx_std_23)
target_include_directories(flowui_ui_renderer_antialiasing_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_ui_renderer_antialiasing_tests PRIVATE
    FLOWUI_ANTIALIASING_SHADER_DIRECTORY="${FLOWUI_SHADER_OUTPUT_DIR}"
)
if(FLOWUI_REQUIRE_VULKAN_TEST_DEVICE)
    target_compile_definitions(flowui_ui_renderer_antialiasing_tests PRIVATE FLOWUI_TEST_REQUIRE_VULKAN_DEVICE=1)
endif()
target_link_libraries(flowui_ui_renderer_antialiasing_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
add_dependencies(flowui_ui_renderer_antialiasing_tests flowui_shaders)
add_test(NAME flowui.ui.renderer_antialiasing COMMAND flowui_ui_renderer_antialiasing_tests)
set_tests_properties(flowui.ui.renderer_antialiasing PROPERTIES
    LABELS "flowui;ui;renderer;integration;vulkan"
    SKIP_RETURN_CODE 77
    TIMEOUT 60
)

add_executable(flowui_phase4_api_tests ApiTests/Phase4ApiTests.cpp)
target_compile_features(flowui_phase4_api_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase4_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_phase4_api_tests PRIVATE
	FLOWUI_APP_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/AppRuntime.cpp"
	FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ViewPortManager.cpp"
	FLOWUI_SWAPCHAIN_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/renderer/vulkan/Vk_Swapchain.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/renderer/Vk_UiRenderer.cpp"
)
target_link_libraries(flowui_phase4_api_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.phase4.api COMMAND flowui_phase4_api_tests)

add_executable(flowui_multi_window_api_contract_tests ApiTests/MultiWindowApiContractTests.cpp)
target_compile_features(flowui_multi_window_api_contract_tests PRIVATE cxx_std_23)
target_include_directories(flowui_multi_window_api_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_multi_window_api_contract_tests PRIVATE FlowUi::FlowUi)
add_test(NAME flowui.multi_window.api_contract COMMAND flowui_multi_window_api_contract_tests)
set_tests_properties(flowui.multi_window.api_contract PROPERTIES
	LABELS "flowui;window;api;unit"
	TIMEOUT 30
)
set_tests_properties(flowui.phase4.api PROPERTIES LABELS "flowui;phase4;unit" TIMEOUT 30)

add_executable(flowui_phase5_manager_storage_tests ApiTests/Phase5ManagerStorageTests.cpp)
target_compile_features(flowui_phase5_manager_storage_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase5_manager_storage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(flowui_phase5_manager_storage_tests PRIVATE
	FLOWUI_IMAGE_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ImageManager.cpp"
	FLOWUI_ICON_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/IconManager.cpp"
	FLOWUI_FONT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/FontManager.cpp"
	FLOWUI_VIEWPORT_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/ViewPortManager.cpp"
	FLOWUI_UI_RENDERER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/renderer/Vk_UiRenderer.cpp"
	FLOWUI_STORAGE_INTERFACE_SOURCE="${PROJECT_SOURCE_DIR}/include/FlowUi/Private/storageSystem/IStorageSystem.hpp"
	FLOWUI_STORAGE_TYPES_SOURCE="${PROJECT_SOURCE_DIR}/include/FlowUi/Private/storageSystem/StorageTypes.hpp"
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

# Expected compiler failures must match a targeted contract identifier, while
# the same malformed types remain valid negative concept queries.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME flowui.elements.diagnostics.current
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/ElementResourceTests/ElementDiagnosticCompileTests.py
            --compiler ${CMAKE_CXX_COMPILER}
            --standard c++23
            --source-root ${PROJECT_SOURCE_DIR}
            --generated-include ${FLOWUI_GENERATED_INCLUDE_DIR}
            --output-dir ${PROJECT_SOURCE_DIR}/.flowui/agentic/verification/element-diagnostics/current
    )
    set_tests_properties(flowui.elements.diagnostics.current PROPERTIES
        LABELS "flowui;elements;compile" TIMEOUT 180)
    foreach(diagnostic_compiler IN ITEMS gcc clang)
        if(diagnostic_compiler STREQUAL "gcc")
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                set(diagnostic_compiler_path ${CMAKE_CXX_COMPILER})
            else()
                find_program(FLOWUI_DIAGNOSTIC_GCC NAMES g++-15 g++-14 g++-13 g++-12 g++)
                set(diagnostic_compiler_path ${FLOWUI_DIAGNOSTIC_GCC})
            endif()
        else()
            find_program(FLOWUI_DIAGNOSTIC_CLANG NAMES clang++ clang++-19 clang++-18 clang++-17 clang++-16 clang++-15 clang++-14 clang++-13 clang++-12)
            set(diagnostic_compiler_path ${FLOWUI_DIAGNOSTIC_CLANG})
        endif()
        if(diagnostic_compiler_path)
            add_test(NAME flowui.elements.diagnostics.${diagnostic_compiler}_cxx20
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/ElementResourceTests/ElementDiagnosticCompileTests.py
                    --compiler ${diagnostic_compiler_path}
                    --standard c++20
                    --source-root ${PROJECT_SOURCE_DIR}
                    --generated-include ${FLOWUI_GENERATED_INCLUDE_DIR}
                    --output-dir ${PROJECT_SOURCE_DIR}/.flowui/agentic/verification/element-diagnostics/${diagnostic_compiler}_cxx20
            )
            set_tests_properties(flowui.elements.diagnostics.${diagnostic_compiler}_cxx20 PROPERTIES
                LABELS "flowui;elements;compile" TIMEOUT 180)
        endif()
    endforeach()
endif()

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
	flowui_package_resources(flowui_fsel_demo)
	target_compile_features(flowui_fsel_demo PRIVATE cxx_std_23)
	target_link_libraries(flowui_fsel_demo PRIVATE FlowUi::FlowUi)
	if(AGENTIC_WORKFLOW)
		target_compile_definitions(flowui_fsel_demo PRIVATE
			FLOWUI_AGENTIC_APP_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
			FLOWUI_AGENTIC_APP_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
			FLOWUI_AGENTIC_APP_BUILD_FLAGS="${FLOWUI_AGENTIC_FLAGS}"
			FLOWUI_AGENTIC_APP_REVISION="${FLOWUI_AGENTIC_REVISION}")
	endif()
endif()

# Compile the typed builder's developer attachment points without linking against a development ABI.
add_library(flowui_element_builder_dev_compile_contract OBJECT
	ElementResourceTests/ElementConceptContractTests.cpp
)
target_compile_features(flowui_element_builder_dev_compile_contract PRIVATE cxx_std_23)
target_compile_definitions(flowui_element_builder_dev_compile_contract PRIVATE FLOW_UI_DEV_MODE=1)
target_include_directories(flowui_element_builder_dev_compile_contract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_element_builder_dev_compile_contract PRIVATE FlowUi::FlowUi)

# Compile UiManager's modern tooling and monitoring integration with developer systems enabled.
add_library(flowui_dev_manager_compile_contract OBJECT
	${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/UiManager.cpp
)
target_compile_features(flowui_dev_manager_compile_contract PRIVATE cxx_std_23)
target_compile_definitions(flowui_dev_manager_compile_contract PRIVATE
	FLOW_UI_DEV_MODE=1
)
target_include_directories(flowui_dev_manager_compile_contract
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
		FLOWUI_UI_MANAGER_STATE_SOURCE="${PROJECT_SOURCE_DIR}/include/FlowUi/Private/layoutService/UiManagerState.hpp"
		FLOWUI_UI_MANAGER_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Public/managers/UiManager.cpp"
		FLOWUI_ELEMENT_BUILDER_SOURCE="${PROJECT_SOURCE_DIR}/include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
		FLOWUI_ELEMENT_BRIDGE_HEADER="${PROJECT_SOURCE_DIR}/include/FlowUi/Private/elementSystem/FlowUiElementBridge.hpp"
		FLOWUI_ELEMENT_BRIDGE_SOURCE="${PROJECT_SOURCE_DIR}/src/FlowUi/Private/elementSystem/FlowUiElementBridge.cpp"
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

if(FLOWUI_HAS_GLFW_PROVIDER)
add_executable(flowui_phase4_window_tests WindowTests/Phase4WindowTests.cpp)
target_compile_features(flowui_phase4_window_tests PRIVATE cxx_std_23)
target_include_directories(flowui_phase4_window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_phase4_window_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
add_test(NAME flowui.phase4.two_window COMMAND flowui_phase4_window_tests)
add_test(NAME flowui.text.field_modes COMMAND flowui_phase4_window_tests --text-modes)
set_tests_properties(flowui.text.field_modes PROPERTIES
	LABELS "flowui;text;integration;vulkan;window"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)
set_tests_properties(flowui.phase4.two_window PROPERTIES
	LABELS "flowui;phase4;integration;vulkan;window"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_executable(flowui_window_operation_backend_tests WindowTests/WindowOperationBackendTests.cpp)
target_compile_features(flowui_window_operation_backend_tests PRIVATE cxx_std_23)
target_include_directories(flowui_window_operation_backend_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_window_operation_backend_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
add_test(NAME flowui.window.backend_operations COMMAND flowui_window_operation_backend_tests)
set_tests_properties(flowui.window.backend_operations PROPERTIES
	LABELS "flowui;window;integration"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)

add_executable(flowui_window_shortcut_tests WindowTests/WindowShortcutTests.cpp)
target_compile_features(flowui_window_shortcut_tests PRIVATE cxx_std_23)
target_include_directories(flowui_window_shortcut_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flowui_window_shortcut_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
flowui_package_resources(flowui_window_shortcut_tests)
add_test(NAME flowui.window.shortcuts COMMAND flowui_window_shortcut_tests)
set_tests_properties(flowui.window.shortcuts PROPERTIES
	LABELS "flowui;window;shortcuts;integration;vulkan"
	SKIP_RETURN_CODE 77
	TIMEOUT 60
)
endif()

if(AGENTIC_WORKFLOW)
    add_executable(flowui_agentic_command_tests AgenticWorkflowTests/AgenticCommandTests.cpp)
    target_link_libraries(flowui_agentic_command_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.commands COMMAND flowui_agentic_command_tests)
    add_executable(flowui_agentic_schema_tests AgenticWorkflowTests/AgenticSchemaTests.cpp)
    target_link_libraries(flowui_agentic_schema_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.schema_contracts COMMAND flowui_agentic_schema_tests)
	add_executable(flowui_agentic_capture_tests
		RendererTests/AgenticFrameCaptureTests.cpp
		HeadlessVulkanFixture.cpp
	)
	target_compile_features(flowui_agentic_capture_tests PRIVATE cxx_std_23)
	target_include_directories(flowui_agentic_capture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(flowui_agentic_capture_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
	add_test(NAME flowui.agentic.capture COMMAND flowui_agentic_capture_tests)
	set_tests_properties(flowui.agentic.capture PROPERTIES
		LABELS "flowui;agentic;integration;vulkan"
		SKIP_RETURN_CODE 77
		TIMEOUT 60
	)
endif()

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
    add_executable(flowui_dev_catalogue_model_tests DevSystemsTests/DevCatalogueModelTests.cpp)
    target_compile_features(flowui_dev_catalogue_model_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_catalogue_model_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.catalogue_model COMMAND flowui_dev_catalogue_model_tests)
    set_tests_properties(flowui.dev.catalogue_model PROPERTIES TIMEOUT 30 LABELS "flowui;dev;catalogue;unit")
    add_executable(flowui_dev_catalogue_selector_interaction_tests DevSystemsTests/DevCatalogueSelectorInteractionTests.cpp)
    target_compile_features(flowui_dev_catalogue_selector_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_catalogue_selector_interaction_tests PRIVATE FlowUi::FlowUi)
    flowui_package_resources(flowui_dev_catalogue_selector_interaction_tests)
    add_test(NAME flowui.dev.catalogue_selector_interaction COMMAND flowui_dev_catalogue_selector_interaction_tests)
    set_tests_properties(flowui.dev.catalogue_selector_interaction PROPERTIES TIMEOUT 60 LABELS "flowui;dev;catalogue;interaction")
    if(FLOWUI_PUBLIC_VULKAN_INTEROP)
        add_executable(flowui_dev_catalogue_viewport_tests DevSystemsTests/DevCatalogueViewportTests.cpp)
        target_compile_features(flowui_dev_catalogue_viewport_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_catalogue_viewport_tests PRIVATE FlowUi::FlowUi)
        add_test(NAME flowui.dev.catalogue_viewport_capture COMMAND flowui_dev_catalogue_viewport_tests)
        set_tests_properties(flowui.dev.catalogue_viewport_capture PROPERTIES TIMEOUT 60 LABELS "flowui;dev;catalogue;integration;vulkan")
    endif()
    add_executable(flowui_dev_changes_model_tests DevSystemsTests/DevChangesModelTests.cpp)
    target_compile_features(flowui_dev_changes_model_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_changes_model_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.changes_model COMMAND flowui_dev_changes_model_tests)
    set_tests_properties(flowui.dev.changes_model PROPERTIES TIMEOUT 30 LABELS "flowui;dev;changes;unit")
    add_executable(flowui_dev_changes_review_tests DevSystemsTests/DevChangesReviewTests.cpp)
    target_compile_features(flowui_dev_changes_review_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_changes_review_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.changes_review COMMAND flowui_dev_changes_review_tests ${CMAKE_CURRENT_BINARY_DIR}/changes-review-fixture)
    set_tests_properties(flowui.dev.changes_review PROPERTIES TIMEOUT 30 LABELS "flowui;dev;changes;unit")
    add_executable(flowui_dev_changes_runtime_tests DevSystemsTests/DevChangesRuntimeTests.cpp HeadlessVulkanFixture.cpp)
    target_compile_features(flowui_dev_changes_runtime_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_changes_runtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_dev_changes_runtime_tests PRIVATE FlowUi::FlowUi Vulkan::Vulkan)
    add_test(NAME flowui.dev.changes_runtime COMMAND flowui_dev_changes_runtime_tests)
    set_tests_properties(flowui.dev.changes_runtime PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 LABELS "flowui;dev;changes;runtime")
    # The integrated workload requires subsystem probes, causal errors, and CPU zones.
    if(FLOWUI_DEV_ERROR_LEVEL GREATER_EQUAL 2)
        add_executable(flowui_dev_diagnostics_model_tests DevSystemsTests/DevDiagnosticsModelTests.cpp)
        target_compile_features(flowui_dev_diagnostics_model_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_diagnostics_model_tests PRIVATE FlowUi::FlowUi)
        add_test(NAME flowui.dev.diagnostics_model COMMAND flowui_dev_diagnostics_model_tests)
        set_tests_properties(flowui.dev.diagnostics_model PROPERTIES TIMEOUT 30 LABELS "flowui;dev;errors;unit")
        add_executable(flowui_dev_diagnostics_reporting_tests DevMonitoringTests/DevDiagnosticsReportingTests.cpp)
        target_compile_features(flowui_dev_diagnostics_reporting_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_diagnostics_reporting_tests PRIVATE FlowUi::FlowUi)
        add_test(NAME flowui.dev.diagnostics_reporting COMMAND flowui_dev_diagnostics_reporting_tests)
        set_tests_properties(flowui.dev.diagnostics_reporting PROPERTIES TIMEOUT 30 LABELS "flowui;dev;errors;unit")
    endif()
    if(FLOWUI_DEV_MEMORY_LEVEL GREATER_EQUAL 2 AND FLOWUI_DEV_ERROR_LEVEL GREATER_EQUAL 2
       AND FLOWUI_DEV_TIMING_LEVEL GREATER_EQUAL 1)
        add_executable(flowui_dev_error_reporting_tests DevMonitoringTests/DevErrorReportingTests.cpp)
        target_compile_features(flowui_dev_error_reporting_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_error_reporting_tests PRIVATE FlowUi::FlowUi)
        add_test(NAME flowui.dev.error_reporting COMMAND flowui_dev_error_reporting_tests)
        set_tests_properties(flowui.dev.error_reporting PROPERTIES TIMEOUT 30 LABELS "flowui;dev;errors;unit")
    endif()

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
        ${PROJECT_SOURCE_DIR}/src/devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsReporting.cpp
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
    # This suite exercises stack/state snapshots even when the library's
    # configured default capture level is lower.
    target_compile_definitions(flowui_dev_error_tests PRIVATE FLOWUI_DEV_ERROR_LEVEL=4)
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
        DevSystemsTests/ErrorReportingStub.cpp
        HeadlessVulkanFixture.cpp
        DevSystemsTests/DevMemoryVulkanSupport.cpp
        ${PROJECT_SOURCE_DIR}/src/FlowUi/Private/storageSystem/FlowStorageSystem.cpp
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

add_executable(flowui_dev_inspect_scroll_tests DevSystemsTests/DevInspectScrollTests.cpp)
target_compile_features(flowui_dev_inspect_scroll_tests PRIVATE cxx_std_23)
target_include_directories(flowui_dev_inspect_scroll_tests PRIVATE ${PROJECT_SOURCE_DIR}/external)
add_test(NAME flowui.dev.inspect_scroll COMMAND flowui_dev_inspect_scroll_tests)

if(FLOW_UI_DEV_MODE AND FLOW_UI_DEV_CAPTURE_CLAY AND FLOWUI_HAS_GLFW_PROVIDER)
    add_executable(flowui_dev_changes_interaction_tests DevSystemsTests/DevChangesInteractionTests.cpp)
    target_compile_features(flowui_dev_changes_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_changes_interaction_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    flowui_package_resources(flowui_dev_changes_interaction_tests)
    add_test(NAME flowui.dev.changes_interaction COMMAND flowui_dev_changes_interaction_tests)
    # The wheel fixture keeps native GLFW focus/cursor state across multiple frames.
    set_tests_properties(flowui.dev.changes_interaction PROPERTIES
        TIMEOUT 90
        LABELS "flowui;dev;changes;interaction"
        RUN_SERIAL TRUE)
    if(FLOWUI_DEV_ERROR_LEVEL GREATER_EQUAL 2)
        add_executable(flowui_dev_diagnostics_interaction_tests DevSystemsTests/DevDiagnosticsInteractionTests.cpp)
        target_compile_features(flowui_dev_diagnostics_interaction_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_diagnostics_interaction_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
        add_test(NAME flowui.dev.diagnostics_interaction COMMAND flowui_dev_diagnostics_interaction_tests)
        flowui_package_resources(flowui_dev_diagnostics_interaction_tests)
        set_tests_properties(flowui.dev.diagnostics_interaction PROPERTIES TIMEOUT 90 LABELS "flowui;dev;errors;interaction")
        add_executable(flowui_dev_diagnostics_navigation_tests DevSystemsTests/DevDiagnosticsNavigationTests.cpp)
        target_compile_features(flowui_dev_diagnostics_navigation_tests PRIVATE cxx_std_23)
        target_link_libraries(flowui_dev_diagnostics_navigation_tests PRIVATE FlowUi::FlowUi)
        add_test(NAME flowui.dev.diagnostics_navigation COMMAND flowui_dev_diagnostics_navigation_tests)
        flowui_package_resources(flowui_dev_diagnostics_navigation_tests)
        set_tests_properties(flowui.dev.diagnostics_navigation PROPERTIES TIMEOUT 60 LABELS "flowui;dev;errors;interaction")
    endif()
    add_executable(flowui_dev_memory_surface_interaction_tests DevSystemsTests/DevMemorySurfaceInteractionTests.cpp)
    target_compile_features(flowui_dev_memory_surface_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_surface_interaction_tests PRIVATE FlowUi::FlowUi)
    flowui_package_resources(flowui_dev_memory_surface_interaction_tests)
    add_test(NAME flowui.dev.memory_surface_interaction COMMAND flowui_dev_memory_surface_interaction_tests)
    set_tests_properties(flowui.dev.memory_surface_interaction PROPERTIES TIMEOUT 60)

    add_executable(flowui_dev_memory_capacity_policy_tests DevSystemsTests/DevMemoryCapacityPolicyTests.cpp)
    target_compile_features(flowui_dev_memory_capacity_policy_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_capacity_policy_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.memory_capacity_policy COMMAND flowui_dev_memory_capacity_policy_tests)

    add_executable(flowui_dev_memory_planner_tests DevSystemsTests/DevMemoryPlannerTests.cpp)
    target_compile_features(flowui_dev_memory_planner_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_planner_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.memory_planner COMMAND flowui_dev_memory_planner_tests)

    add_executable(flowui_dev_memory_workbench_data_tests DevSystemsTests/DevMemoryWorkbenchDataTests.cpp)
    target_compile_features(flowui_dev_memory_workbench_data_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_workbench_data_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.memory_workbench_data COMMAND flowui_dev_memory_workbench_data_tests)

    if(FLOWUI_HAS_GLFW_PROVIDER)
    add_executable(flowui_dev_memory_workbench_interaction_tests DevSystemsTests/DevMemoryWorkbenchInteractionTests.cpp)
    target_compile_features(flowui_dev_memory_workbench_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_workbench_interaction_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    flowui_package_resources(flowui_dev_memory_workbench_interaction_tests)
    add_test(NAME flowui.dev.memory_workbench_interaction COMMAND flowui_dev_memory_workbench_interaction_tests)
    set_tests_properties(flowui.dev.memory_workbench_interaction PROPERTIES TIMEOUT 60)

    add_executable(flowui_dev_memory_selector_interaction_tests DevSystemsTests/DevMemorySelectorInteractionTests.cpp)
    target_compile_features(flowui_dev_memory_selector_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_memory_selector_interaction_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    flowui_package_resources(flowui_dev_memory_selector_interaction_tests)
    add_test(NAME flowui.dev.memory_selector_interaction COMMAND flowui_dev_memory_selector_interaction_tests)
    set_tests_properties(flowui.dev.memory_selector_interaction PROPERTIES TIMEOUT 60)
    endif()

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND AND NOT MSVC AND TARGET flowui-dev-generate)
        add_test(NAME flowui.dev.theme_codegen COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/DevSystemsTests/ThemeCodegenTests.py
            --generator $<TARGET_FILE:flowui-dev-generate> --compiler ${CMAKE_CXX_COMPILER}
            --source ${PROJECT_SOURCE_DIR} --build ${PROJECT_BINARY_DIR})
    endif()
    add_executable(flowui_dev_theme_editor_model_tests DevSystemsTests/DevThemeEditorModelTests.cpp)
    target_compile_features(flowui_dev_theme_editor_model_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_theme_editor_model_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_dev_theme_editor_model_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.theme_editor_model COMMAND flowui_dev_theme_editor_model_tests)

    if(FLOWUI_HAS_GLFW_PROVIDER)
    add_executable(flowui_dev_theme_editor_interaction_tests DevSystemsTests/DevThemeEditorInteractionTests.cpp)
    target_compile_features(flowui_dev_theme_editor_interaction_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_theme_editor_interaction_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    flowui_package_resources(flowui_dev_theme_editor_interaction_tests)
    add_test(NAME flowui.dev.theme_editor_interaction COMMAND flowui_dev_theme_editor_interaction_tests)
    set_tests_properties(flowui.dev.theme_editor_interaction PROPERTIES TIMEOUT 90)
    endif()

    add_executable(flowui_dev_inspect_overlay_menu_tests DevSystemsTests/DevInspectOverlayMenuTests.cpp)
    target_compile_features(flowui_dev_inspect_overlay_menu_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_inspect_overlay_menu_tests PRIVATE FlowUi::FlowUi)
    flowui_package_resources(flowui_dev_inspect_overlay_menu_tests)
    add_test(NAME flowui.dev.inspect_overlay_menu COMMAND flowui_dev_inspect_overlay_menu_tests)
    set_tests_properties(flowui.dev.inspect_overlay_menu PROPERTIES TIMEOUT 60)

    add_executable(flowui_dev_inspect_subtree_tests DevSystemsTests/DevInspectSubtreeTests.cpp)
    target_compile_features(flowui_dev_inspect_subtree_tests PRIVATE cxx_std_23)
    target_link_libraries(flowui_dev_inspect_subtree_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.inspect_subtree COMMAND flowui_dev_inspect_subtree_tests)
    set_tests_properties(flowui.dev.inspect_subtree PROPERTIES TIMEOUT 30)
endif()

# Inspection state and hit testing are independently exercised with/without Clay capture.
foreach(inspect_capture IN ITEMS 0 1)
    set(inspect_test_target flowui_dev_inspect_interaction_${inspect_capture}_tests)
    add_executable(${inspect_test_target}
        DevSystemsTests/DevInspectInteractionTests.cpp
        ${PROJECT_SOURCE_DIR}/src/devSystems/devTooling/inspect/DevInspectInteractionController.cpp)
    target_compile_features(${inspect_test_target} PRIVATE cxx_std_23)
    target_compile_definitions(${inspect_test_target} PRIVATE
        FLOW_UI_DEV_MODE=1 FLOW_UI_DEV_CAPTURE_CLAY=${inspect_capture}
        FLOWUI_DEV_TIMING_LEVEL=0 FLOWUI_DEV_MEMORY_LEVEL=0 FLOWUI_DEV_ERROR_LEVEL=0)
    target_include_directories(${inspect_test_target} PRIVATE
        ${FLOWUI_GENERATED_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/external)
    add_test(NAME flowui.dev.inspect_interaction_${inspect_capture} COMMAND ${inspect_test_target})
endforeach()

if(FLOW_UI_DEV_MODE AND FLOWUI_DEV_MEMORY_LEVEL GREATER_EQUAL 2)
    add_executable(flowui_dev_memory_capacity_tests
        DevSystemsTests/DevMemoryCapacityTests.cpp
        ${PROJECT_SOURCE_DIR}/tools/flowui-dev-generate/ManifestParser.cpp
        ${PROJECT_SOURCE_DIR}/tools/flowui-dev-generate/CppCodeGenerator.cpp)
    target_compile_features(flowui_dev_memory_capacity_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_memory_capacity_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/tools/flowui-dev-generate)
    target_link_libraries(flowui_dev_memory_capacity_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.memory_capacity COMMAND flowui_dev_memory_capacity_tests
        ${CMAKE_CURRENT_BINARY_DIR}/memory-capacity-fixture)
    add_executable(flowui_dev_memory_capacity_storage_tests
        DevSystemsTests/DevMemoryCapacityStorageTests.cpp HeadlessVulkanFixture.cpp)
    target_compile_features(flowui_dev_memory_capacity_storage_tests PRIVATE cxx_std_23)
    target_include_directories(flowui_dev_memory_capacity_storage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_dev_memory_capacity_storage_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.dev.memory_capacity_storage COMMAND flowui_dev_memory_capacity_storage_tests)
    set_tests_properties(flowui.dev.memory_capacity_storage PROPERTIES SKIP_RETURN_CODE 77)

endif()

if(AGENTIC_WORKFLOW)
    add_executable(flowui_agentic_format_tests AgenticWorkflowTests/AgenticFormatTests.cpp)
    target_link_libraries(flowui_agentic_format_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.format COMMAND flowui_agentic_format_tests)
    add_executable(flowui_agentic_interaction_tests AgenticWorkflowTests/AgenticInteractionTests.cpp)
    target_link_libraries(flowui_agentic_interaction_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.interaction_contracts COMMAND flowui_agentic_interaction_tests)
    add_executable(flowui_agentic_verification_tests AgenticWorkflowTests/AgenticVerificationTests.cpp)
    target_link_libraries(flowui_agentic_verification_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.verification_contracts COMMAND flowui_agentic_verification_tests)
    add_executable(flowui_agentic_local_transport_tests AgenticWorkflowTests/AgenticLocalTransportTests.cpp)
    target_link_libraries(flowui_agentic_local_transport_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.local_transport COMMAND flowui_agentic_local_transport_tests)
    set_tests_properties(flowui.agentic.local_transport PROPERTIES TIMEOUT 15 LABELS "flowui;agentic;ipc")
    add_executable(flowui_agentic_verification_service_tests AgenticWorkflowTests/AgenticVerificationServiceTests.cpp)
    target_include_directories(flowui_agentic_verification_service_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_agentic_verification_service_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.agentic.verification_service COMMAND flowui_agentic_verification_service_tests)
    set_tests_properties(flowui.agentic.verification_service PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan;verification")
    if(FLOWUI_HAS_GLFW_PROVIDER)
    add_executable(flowui_agentic_interaction_runtime_tests AgenticWorkflowTests/AgenticInteractionRuntimeTests.cpp)
    target_include_directories(flowui_agentic_interaction_runtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_agentic_interaction_runtime_tests PRIVATE FlowUi::FlowUi ${FLOWUI_GLFW_TARGET})
    add_test(NAME flowui.agentic.interaction_runtime COMMAND flowui_agentic_interaction_runtime_tests)
    set_tests_properties(flowui.agentic.interaction_runtime PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan")
    endif()
    add_executable(flowui_agentic_target_action_tests AgenticWorkflowTests/AgenticTargetActionTests.cpp)
    target_include_directories(flowui_agentic_target_action_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_agentic_target_action_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.agentic.interaction_targets COMMAND flowui_agentic_target_action_tests)
    add_test(NAME flowui.agentic.exported_actions COMMAND flowui_agentic_target_action_tests native)
    set_tests_properties(flowui.agentic.exported_actions PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan")
    add_executable(flowui_agentic_image_tests AgenticWorkflowTests/AgenticImageTests.cpp)
    target_include_directories(flowui_agentic_image_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_agentic_image_tests PRIVATE flowui_agentic_format)
    add_test(NAME flowui.agentic.image COMMAND flowui_agentic_image_tests)
    add_executable(flowui_agentic_service_tests AgenticWorkflowTests/AgenticServiceTests.cpp)
    target_include_directories(flowui_agentic_service_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(flowui_agentic_service_tests PRIVATE FlowUi::FlowUi)
    add_test(NAME flowui.agentic.service COMMAND flowui_agentic_service_tests capture)
    add_test(NAME flowui.agentic.capture_retention COMMAND flowui_agentic_service_tests capture_retention)
    set_tests_properties(flowui.agentic.capture_retention PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan;retention")
    add_test(NAME flowui.agentic.dormant COMMAND flowui_agentic_service_tests dormant)
    add_test(NAME flowui.agentic.ipc COMMAND flowui_agentic_service_tests ipc)
    set_tests_properties(flowui.agentic.service flowui.agentic.dormant flowui.agentic.ipc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        if(TARGET flowui_fsel_demo)
            add_test(NAME flowui.agentic.live_cli COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/AgenticWorkflowTests/AgenticLiveCliTests.py
                $<TARGET_FILE:flowui-agent> $<TARGET_FILE:flowui_fsel_demo>
                ${PROJECT_SOURCE_DIR}/tests/demo/agentic-click.json)
            set_tests_properties(flowui.agentic.live_cli PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 LABELS "flowui;agentic;window;vulkan;cli")
            add_test(NAME flowui.agentic.verification_live COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/AgenticWorkflowTests/AgenticVerificationLiveTests.py
                $<TARGET_FILE:flowui-agent> $<TARGET_FILE:flowui_fsel_demo>
                --output-directory ${PROJECT_SOURCE_DIR}/.flowui/agentic/verification/ctest-live)
            set_tests_properties(flowui.agentic.verification_live PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180 LABELS "flowui;agentic;window;vulkan;cli;verification")
            option(FLOWUI_AGENTIC_MCP_LIVE_TESTS "Enable real MCP SDK/App integration tests (requires the optional pinned MCP dependencies)." OFF)
            if(FLOWUI_AGENTIC_MCP_LIVE_TESTS)
                add_test(NAME flowui.agentic.live_mcp COMMAND ${Python3_EXECUTABLE}
                    ${PROJECT_SOURCE_DIR}/tests/AgenticWorkflowTests/AgenticLiveMcpTests.py
                    --cli $<TARGET_FILE:flowui-agent> --demo $<TARGET_FILE:flowui_fsel_demo>
                    --workspace ${PROJECT_SOURCE_DIR}
                    --output-directory ${PROJECT_SOURCE_DIR}/.flowui/agentic/verification/ctest-mcp)
                set_tests_properties(flowui.agentic.live_mcp PROPERTIES TIMEOUT 300 LABELS "flowui;agentic;window;vulkan;mcp;integration")
            endif()
        endif()
        add_test(NAME flowui.agentic.mcp_schemas COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/flowui-agent-mcp/update_schemas.py --check)
        set_tests_properties(flowui.agentic.mcp_schemas PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME flowui.agentic.cli COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/AgenticWorkflowTests/AgenticCliTests.py $<TARGET_FILE:flowui-agent>)
        add_test(NAME flowui.agentic.schemas COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/flowui-agent/check_schemas.py --check)
        set_tests_properties(flowui.agentic.schemas PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME flowui.agentic.knowledge COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tools/flowui-agent/update_knowledge.py --check)
        set_tests_properties(flowui.agentic.knowledge PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    endif()
endif()
