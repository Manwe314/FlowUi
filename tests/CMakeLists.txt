add_executable(flowui_storage_types_tests
    StorageTypesTests.cpp
)

target_compile_features(flowui_storage_types_tests PRIVATE cxx_std_23)
target_include_directories(flowui_storage_types_tests
    PRIVATE
        ${FLOWUI_GENERATED_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME flowui.storage.types COMMAND flowui_storage_types_tests)
set_tests_properties(flowui.storage.types PROPERTIES
    LABELS "flowui;storage;unit"
    TIMEOUT 30
)

add_executable(flowui_storage_system_tests
    FlowStorageSystemTests.cpp
    HeadlessVulkanFixture.cpp
)

set(FLOWUI_STORAGE_TEST_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/storage_pipeline_test.comp")
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
