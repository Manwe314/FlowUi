cmake_minimum_required(VERSION 3.25)
project(FlowUiRecordingStorage LANGUAGES CXX)

if(NOT FLOWUI_SOURCE_DIRECTORY)
    get_filename_component(FLOWUI_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()

include(GNUInstallDirs)
add_library(flowui_recording_storage_support STATIC RecordingStorageSupport.cpp)
add_library(FlowUiRecordingStorageSupport::Support ALIAS flowui_recording_storage_support)
set_target_properties(flowui_recording_storage_support PROPERTIES EXPORT_NAME Support)
target_compile_features(flowui_recording_storage_support PRIVATE cxx_std_23)
include("${FLOWUI_SOURCE_DIRECTORY}/cmake/FlowUiStorageSystem.cmake")
flowui_register_storage_dependencies(
    TARGETS FlowUiRecordingStorageSupport::Support PACKAGES FlowUiRecordingStorageSupport)
set(FLOWUI_STORAGE_SYSTEM_HEADER "RecordingStorageSystem.hpp" CACHE STRING "" FORCE)
set(FLOWUI_STORAGE_SYSTEM_TYPE "::FlowUi::test::RecordingStorageSystem" CACHE STRING "" FORCE)
set(FLOWUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLOWUI_BUILD_FONT_BAKER OFF CACHE BOOL "" FORCE)
add_subdirectory("${FLOWUI_SOURCE_DIRECTORY}" flowui)
target_include_directories(flowui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(flowui_recording_storage_consumer main.cpp)
target_include_directories(flowui_recording_storage_consumer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(flowui_recording_storage_consumer PRIVATE FlowUi::FlowUi)
flowui_package_resources(flowui_recording_storage_consumer)
enable_testing()
add_test(NAME flowui.storage.recording_provider COMMAND flowui_recording_storage_consumer)
set_tests_properties(flowui.storage.recording_provider PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

if(FLOWUI_INSTALL)
    install(TARGETS flowui_recording_storage_support EXPORT FlowUiRecordingStorageSupportTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT FlowUiRecordingStorageSupportTargets NAMESPACE FlowUiRecordingStorageSupport::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlowUiRecordingStorageSupport)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/FlowUiRecordingStorageSupportConfig.cmake"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/FlowUiRecordingStorageSupportTargets.cmake\")\n")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/FlowUiRecordingStorageSupportConfig.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlowUiRecordingStorageSupport)
endif()
