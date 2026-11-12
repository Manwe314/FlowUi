cmake_minimum_required(VERSION 3.25)
project(FlowUiRecordingProviderConsumer LANGUAGES CXX)

if(NOT FLOWUI_SOURCE_DIRECTORY)
    get_filename_component(FLOWUI_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()

find_package(glfw3 CONFIG REQUIRED)
include("${FLOWUI_SOURCE_DIRECTORY}/cmake/FlowUiPlatformProvider.cmake")
flowui_register_platform_dependencies(TARGETS glfw PACKAGES glfw3)
set(FLOWUI_PLATFORM_PROVIDER_HEADER "FlowUiTestRecordingProvider.hpp" CACHE STRING "" FORCE)
set(FLOWUI_PLATFORM_PROVIDER_TYPE "::FlowUi::test::RecordingPlatformProvider" CACHE STRING "" FORCE)
set(FLOWUI_ADDITIONAL_TRANSLATORS_HEADER "DeterministicFootpad.hpp" CACHE STRING "" FORCE)
set(FLOWUI_ADDITIONAL_TRANSLATORS_TYPE "::FlowUi::examples::DeterministicFootpadPack" CACHE STRING "" FORCE)
set(FLOWUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLOWUI_BUILD_FONT_BAKER OFF CACHE BOOL "" FORCE)
set(FLOW_UI_ENABLE_BAKED_CHANGES OFF CACHE BOOL "" FORCE)
add_subdirectory("${FLOWUI_SOURCE_DIRECTORY}" flowui)
target_include_directories(flowui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${FLOWUI_SOURCE_DIRECTORY}/example/input")

add_executable(flowui_recording_provider_consumer main.cpp)
target_include_directories(flowui_recording_provider_consumer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(flowui_recording_provider_consumer PRIVATE FlowUi::FlowUi glfw)
flowui_package_resources(flowui_recording_provider_consumer)
enable_testing()
add_test(NAME flowui.platform.recording_provider COMMAND flowui_recording_provider_consumer)
set_tests_properties(flowui.platform.recording_provider PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

add_executable(flowui_input_translator_ui_tests "${FLOWUI_SOURCE_DIRECTORY}/tests/InputServiceTests/TranslatorUiTests.cpp")
target_include_directories(flowui_input_translator_ui_tests PRIVATE
    "${FLOWUI_SOURCE_DIRECTORY}/example/input" "${FLOWUI_SOURCE_DIRECTORY}/tests")
target_link_libraries(flowui_input_translator_ui_tests PRIVATE FlowUi::FlowUi glfw)
flowui_package_resources(flowui_input_translator_ui_tests)
add_test(NAME flowui.input.translator_ui COMMAND flowui_input_translator_ui_tests)
set_tests_properties(flowui.input.translator_ui PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

# A custom distribution is responsible for installing the selected authoring header.
if(FLOWUI_INSTALL)
    install(FILES FlowUiTestRecordingProvider.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES "${FLOWUI_SOURCE_DIRECTORY}/example/input/DeterministicFootpad.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()
