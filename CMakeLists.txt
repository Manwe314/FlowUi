cmake_minimum_required(VERSION 3.20)

project(FlowUi VERSION 0.1.0 LANGUAGES CXX)

option(FLOWUI_INSTALL "Enable install rules" ON)
option(FLOWUI_BUILD_FONT_BAKER "Build FlowUi offline font baker tool" ON)
option(FLOWUI_ENABLE_RUNTIME_FONT_BAKING "Enable runtime TTF->MSDF atlas generation" OFF)
option(FLOWUI_PUBLIC_VULKAN_INTEROP "Expose viewport Vulkan interop API in public headers" ON)
option(FLOWUI_INCLUDE_ICON_MANAGER "Enable IconManager support (PlutoSVG)." ON)
option(FLOW_UI_DEV_MODE "Enable FlowUi developer mode scaffolding and APIs." OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(FLOWUI_BUILD_TESTS_DEFAULT ON)
else()
    set(FLOWUI_BUILD_TESTS_DEFAULT OFF)
endif()
option(FLOWUI_BUILD_TESTS "Build FlowUi unit and storage integration tests." ${FLOWUI_BUILD_TESTS_DEFAULT})
option(
    FLOWUI_REQUIRE_VULKAN_TEST_DEVICE
    "Fail storage integration tests instead of skipping when no headless Vulkan device is available."
    OFF
)
unset(FLOWUI_BUILD_TESTS_DEFAULT)
set(FLOWUI_INCLUDE_ICON_MANAGER "${FLOWUI_INCLUDE_ICON_MANAGER}")
set(FLOWUI_GLFW_PROVIDER "auto" CACHE STRING "GLFW provider: auto, system, vendored")
set_property(CACHE FLOWUI_GLFW_PROVIDER PROPERTY STRINGS auto system vendored)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

# Default single-config generators (Ninja/Unix Makefiles) to optimized builds.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Ensure release configurations use -O3 on GCC/Clang-like toolchains.
if(NOT MSVC)
    string(FIND " ${CMAKE_CXX_FLAGS_RELEASE} " " -O3 " FLOWUI_HAS_O3_RELEASE)
    if(FLOWUI_HAS_O3_RELEASE EQUAL -1)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    endif()

    string(FIND " ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} " " -O3 " FLOWUI_HAS_O3_RELWITHDEBINFO)
    if(FLOWUI_HAS_O3_RELWITHDEBINFO EQUAL -1)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
    endif()
endif()

if(FLOWUI_PUBLIC_VULKAN_INTEROP)
    set(FLOWUI_PUBLIC_VULKAN_INTEROP_VALUE 1)
else()
    set(FLOWUI_PUBLIC_VULKAN_INTEROP_VALUE 0)
endif()

if(FLOWUI_INCLUDE_ICON_MANAGER)
    set(FLOWUI_INCLUDE_ICON_MANAGER_VALUE 1)
else()
    set(FLOWUI_INCLUDE_ICON_MANAGER_VALUE 0)
endif()

if(FLOW_UI_DEV_MODE)
    set(FLOW_UI_DEV_MODE_VALUE 1)
else()
    set(FLOW_UI_DEV_MODE_VALUE 0)
endif()

set(FLOWUI_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FlowUiBuildConfig.hpp.in
    ${FLOWUI_GENERATED_INCLUDE_DIR}/FlowUi/BuildConfig.hpp
    @ONLY
)

# Dependencies
find_package(Vulkan REQUIRED)

set(FLOWUI_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(FLOWUI_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(FLOWUI_SHADER_SOURCES
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_solid.vert"
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_msdf.vert"
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_textured.vert"
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_solid.frag"
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_msdf.frag"
    "${FLOWUI_SHADER_SOURCE_DIR}/flowui_ui_textured.frag"
)

if(Vulkan_GLSLC_EXECUTABLE)
    set(FLOWUI_SHADER_COMPILER "${Vulkan_GLSLC_EXECUTABLE}")
    set(FLOWUI_SHADER_COMPILE_ARGS -O)
elseif(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    set(FLOWUI_SHADER_COMPILER "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
    set(FLOWUI_SHADER_COMPILE_ARGS -V)
    message(WARNING "Vulkan glslc was not found; using glslangValidator to compile FlowUi shaders.")
else()
    message(FATAL_ERROR "No shader compiler found. Install Vulkan glslc (preferred) or glslangValidator.")
endif()

set(FLOWUI_SHADER_SPIRV_FILES)
foreach(FLOWUI_SHADER_SOURCE IN LISTS FLOWUI_SHADER_SOURCES)
    get_filename_component(FLOWUI_SHADER_FILE_NAME "${FLOWUI_SHADER_SOURCE}" NAME)
    set(FLOWUI_SHADER_SPIRV "${FLOWUI_SHADER_OUTPUT_DIR}/${FLOWUI_SHADER_FILE_NAME}.spv")

    add_custom_command(
        OUTPUT "${FLOWUI_SHADER_SPIRV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FLOWUI_SHADER_OUTPUT_DIR}"
        COMMAND "${FLOWUI_SHADER_COMPILER}" ${FLOWUI_SHADER_COMPILE_ARGS} "${FLOWUI_SHADER_SOURCE}" -o "${FLOWUI_SHADER_SPIRV}"
        DEPENDS "${FLOWUI_SHADER_SOURCE}"
        COMMENT "Compiling FlowUi shader ${FLOWUI_SHADER_FILE_NAME}"
        VERBATIM
    )

    list(APPEND FLOWUI_SHADER_SPIRV_FILES "${FLOWUI_SHADER_SPIRV}")
endforeach()

add_custom_target(flowui_shaders DEPENDS ${FLOWUI_SHADER_SPIRV_FILES})

set(FLOWUI_MSDF_ATLAS_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/msdf-atlas-gen")

set(FLOWUI_ARTERY_DIR "")
if(EXISTS "${FLOWUI_MSDF_ATLAS_GEN_DIR}/artery-font-format")
    set(FLOWUI_ARTERY_DIR "${FLOWUI_MSDF_ATLAS_GEN_DIR}/artery-font-format")
elseif(EXISTS "${FLOWUI_MSDF_ATLAS_GEN_DIR}/external/artery-font-format")
    set(FLOWUI_ARTERY_DIR "${FLOWUI_MSDF_ATLAS_GEN_DIR}/external/artery-font-format")
elseif(EXISTS "${FLOWUI_MSDF_ATLAS_GEN_DIR}/subprojects/artery-font-format")
    set(FLOWUI_ARTERY_DIR "${FLOWUI_MSDF_ATLAS_GEN_DIR}/subprojects/artery-font-format")
endif()

if(FLOWUI_ARTERY_DIR STREQUAL "")
    message(FATAL_ERROR "Could not find artery-font-format inside external/msdf-atlas-gen (did you init submodules with --recursive?)")
endif()

add_library(flowui_artery INTERFACE)
add_library(FlowUi::artery ALIAS flowui_artery)
set_target_properties(flowui_artery PROPERTIES EXPORT_NAME artery)
target_include_directories(flowui_artery SYSTEM INTERFACE
    $<BUILD_INTERFACE:${FLOWUI_ARTERY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/external/artery-font-format>
)

set(FLOWUI_NEED_MSDF_ATLAS_GEN OFF)
if(FLOWUI_BUILD_FONT_BAKER OR FLOWUI_ENABLE_RUNTIME_FONT_BAKING)
    set(FLOWUI_NEED_MSDF_ATLAS_GEN ON)
endif()

if(FLOWUI_NEED_MSDF_ATLAS_GEN)
    set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/msdf-atlas-gen)
endif()

if(FLOWUI_INCLUDE_ICON_MANAGER)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/plutosvg/CMakeLists.txt")
        message(FATAL_ERROR "Could not find external/plutosvg (did you init submodules with --recursive?)")
    endif()
    set(PLUTOSVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PLUTOSVG_ENABLE_FREETYPE OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/plutosvg)
endif()

if(FLOWUI_BUILD_FONT_BAKER)
    add_executable(flowui_font_baker tools/flowui_font_baker/main.cpp)
    target_link_libraries(flowui_font_baker PRIVATE msdf-atlas-gen::msdf-atlas-gen)
    target_compile_features(flowui_font_baker PRIVATE cxx_std_23)
endif()

if(FLOWUI_GLFW_PROVIDER STREQUAL "vendored" OR
   (FLOWUI_GLFW_PROVIDER STREQUAL "auto" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/CMakeLists.txt"))
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    add_subdirectory(external/glfw)
    set(FLOWUI_GLFW_TARGET glfw)
else()
    find_package(glfw3 CONFIG QUIET)
    if(NOT TARGET glfw AND NOT TARGET glfw3)
        find_package(glfw3 REQUIRED)
    endif()

    if(TARGET glfw)
        set(FLOWUI_GLFW_TARGET glfw)
    elseif(TARGET glfw3)
        set(FLOWUI_GLFW_TARGET glfw3)
    elseif(GLFW3_LIBRARY OR GLFW3_LIBRARIES OR GLFW_LIBRARIES)
        add_library(flowui_glfw INTERFACE)
        target_link_libraries(flowui_glfw INTERFACE ${GLFW3_LIBRARY} ${GLFW3_LIBRARIES} ${GLFW_LIBRARIES})
        target_include_directories(flowui_glfw INTERFACE ${GLFW3_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS})
        set(FLOWUI_GLFW_TARGET flowui_glfw)
    else()
        message(FATAL_ERROR "glfw3 was not found. Install it or set GLFW3_DIR.")
    endif()
endif()

add_library(flowui)
add_library(FlowUi::FlowUi ALIAS flowui)
set_target_properties(flowui PROPERTIES EXPORT_NAME FlowUi)
add_dependencies(flowui flowui_shaders)


target_sources(flowui
    PRIVATE
        src/clay.cpp
        src/FlowUi.cpp
        src/managers/FontManager.cpp
        src/managers/ImageManager.cpp
        src/managers/InputFieldManager.cpp
        src/managers/ShortcutManager.cpp
        src/managers/ViewPortManager.cpp
        src/managers/FlowUiElementSystem.cpp
        src/managers/UiManager.cpp
        src/devMode/debugView.cpp
        src/devMode/devJson.cpp
        src/devMode/devRuntime.cpp
        src/Ui/Vk_UiRenderer.cpp
        src/Storagesystem/FlowStorageSystem.cpp
        src/window/Inputs.cpp
        src/Vulkan/Vk_Context.cpp
        src/Vulkan/Vk_Frames.cpp
        src/Vulkan/Vk_Swapchain.cpp
        include/internal/StorageSystem/StorageTypes.hpp
		include/internal/StorageSystem/IStorageSystem.hpp
		include/internal/StorageSystem/FlowStorageSystem.hpp
        include/internal/UiTexturePublisher.hpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/App.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/Flow.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/FontResources.hpp>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/PublicStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/TextureHandle.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/FlowUi/WindowId.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/FlowUiElementBridge.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/InputFieldRenderOverrides.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/InputQueue.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/devApi.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/elementDevCapture.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/debugView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/devJson.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/devRuntime.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/devMode/registry.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/FontManagerStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/FlowUiElementStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/InputFieldManagerStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/InputStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/ShortcutManagerStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/structs/ViewPortManagerStructs.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/managers/FlowUiElementBuilder.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/window/Window.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/window/IWindow.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Vulkan/Vk_Context.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Vulkan/Vk_Frames.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Vulkan/Vk_Swapchain.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/Ui/Vk_UiRenderer.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/clay.h>
)

if(FLOWUI_INCLUDE_ICON_MANAGER)
    target_sources(flowui PRIVATE src/managers/IconManager.cpp)
endif()

target_include_directories(flowui
    PUBLIC
        $<BUILD_INTERFACE:${FLOWUI_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(flowui
    SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/external>
)

target_link_libraries(flowui
    PUBLIC
        Vulkan::Vulkan
        ${FLOWUI_GLFW_TARGET}
        FlowUi::artery
)

if(FLOWUI_INCLUDE_ICON_MANAGER)
    target_link_libraries(flowui PRIVATE plutosvg::plutosvg)
endif()

if(FLOWUI_ENABLE_RUNTIME_FONT_BAKING)
    target_link_libraries(flowui PRIVATE $<BUILD_INTERFACE:msdf-atlas-gen::msdf-atlas-gen>)
    target_compile_definitions(flowui PUBLIC FLOWUI_RUNTIME_FONT_BAKING=1)
endif()

target_compile_definitions(flowui
    PUBLIC
        FLOWUI_PUBLIC_VULKAN_INTEROP=${FLOWUI_PUBLIC_VULKAN_INTEROP_VALUE}
        FLOWUI_INCLUDE_ICON_MANAGER=${FLOWUI_INCLUDE_ICON_MANAGER_VALUE}
    PRIVATE
        FLOWUI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        FLOWUI_SHADER_OUTPUT_DIR="${FLOWUI_SHADER_OUTPUT_DIR}"
        FLOWUI_INTERNAL_VIEWPORT_MANAGER=1
)

target_compile_features(flowui PUBLIC cxx_std_23)

if(FLOWUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FLOWUI_INSTALL)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY "${FLOWUI_ARTERY_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/external/artery-font-format"
        REGEX "/\\.git$" EXCLUDE
        REGEX "/\\.gitmodules$" EXCLUDE
        PATTERN "example.arfont" EXCLUDE
    )

    install(TARGETS flowui flowui_artery
        EXPORT FlowUiTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    if(FLOWUI_BUILD_FONT_BAKER)
        install(TARGETS flowui_font_baker
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES external/clay.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/external)
    install(FILES
        "${FLOWUI_GENERATED_INCLUDE_DIR}/FlowUi/BuildConfig.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FlowUi"
    )

    install(EXPORT FlowUiTargets
        FILE FlowUiTargets.cmake
        NAMESPACE FlowUi::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlowUi
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FlowUiConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/FlowUiConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlowUi
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/FlowUiConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/FlowUiConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/FlowUiConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FlowUi
    )
endif()
