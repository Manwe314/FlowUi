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
