function(flowui_configure_baking target)
    if(NOT FLOW_UI_ENABLE_BAKED_CHANGES)
        target_compile_definitions(${target} PUBLIC FLOW_UI_HAS_BAKED_CHANGES=0)
        return()
    endif()

    set(FLOWUI_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/.flowui/changes/active.flowchanges")
    set(FLOWUI_BAKED_GENERATED_DIR "${CMAKE_BINARY_DIR}/flowui_generated")
    set(FLOWUI_BAKED_CPP "${FLOWUI_BAKED_GENERATED_DIR}/FlowUiBakedChanges.cpp")
    set(FLOWUI_BAKED_HPP "${FLOWUI_BAKED_GENERATED_DIR}/FlowUiBakedChanges.hpp")

    if(NOT EXISTS "${FLOWUI_MANIFEST_FILE}")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.flowui/changes")
        file(WRITE "${FLOWUI_MANIFEST_FILE}"
            "{\n  \"manifestVersion\": 1,\n  \"schemaFingerprint\": \"0x0000000000000000\",\n  \"buildFingerprint\": \"\",\n  \"createdTimestamp\": \"\",\n  \"bakedChanges\": []\n}\n")
    endif()

    file(MAKE_DIRECTORY "${FLOWUI_BAKED_GENERATED_DIR}")

    if(NOT EXISTS "${FLOWUI_BAKED_HPP}")
        file(WRITE "${FLOWUI_BAKED_HPP}"
"// Auto-generated default stub by CMake. Overwritten when changes are baked.
#pragma once

#include <cstdint>
#include <string_view>
#include \"FlowUi/ElementID.hpp\"
#include \"internal/ElementInstanceKey.hpp\"

namespace FlowUi::baked {

inline constexpr std::uint64_t schemaFingerprint = 0x0ull;

[[nodiscard]] constexpr bool hasBakedDefinitionChanges(FlowDefinitionID definition) noexcept {
	switch (definition.value) {
	default: return false;
	}
}

[[nodiscard]] constexpr bool hasBakedInstanceChanges(FlowDefinitionID definition) noexcept {
	switch (definition.value) {
	default: return false;
	}
}

[[nodiscard]] constexpr bool hasBakedThemeChanges(std::uint64_t themeType) noexcept {
	switch (themeType) {
	default: return false;
	}
}

void applyBakedParametersErased(FlowDefinitionID definition,
	detail::element::ElementInstanceKey instance, void* parametersDraft) noexcept;
void applyBakedThemeErased(std::uint64_t themeType, std::string_view variant,
	void* themeDraft) noexcept;

} // namespace FlowUi::baked
")
    endif()

    if(NOT EXISTS "${FLOWUI_BAKED_CPP}")
        file(WRITE "${FLOWUI_BAKED_CPP}"
"// Auto-generated default stub by CMake. Overwritten when changes are baked.
#include \"FlowUiBakedChanges.hpp\"

#include <algorithm>
#include <array>
#include <cstddef>

namespace FlowUi::baked {

void applyBakedParametersErased(FlowDefinitionID definition,
	detail::element::ElementInstanceKey instance, void* parametersDraft) noexcept {
	if (!parametersDraft) return;
	switch (definition.value) {
	default: break;
	}
}

void applyBakedThemeErased(std::uint64_t themeType, std::string_view variant,
	void* themeDraft) noexcept {
	if (!themeDraft) return;
	switch (themeType) {
	default: break;
	}
}

} // namespace FlowUi::baked
")
    endif()

    set(FLOWUI_BAKED_CODEGEN_TARGET "${target}_baked_changes_codegen")
    add_custom_command(
        OUTPUT "${FLOWUI_BAKED_CPP}" "${FLOWUI_BAKED_HPP}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FLOWUI_BAKED_GENERATED_DIR}"
        COMMAND $<TARGET_FILE:flowui-dev-generate>
            --manifest "${FLOWUI_MANIFEST_FILE}"
            --output-dir "${FLOWUI_BAKED_GENERATED_DIR}"
        DEPENDS "${FLOWUI_MANIFEST_FILE}" flowui-dev-generate
        COMMENT "Generating FlowUi baked C++ changes"
        VERBATIM
    )
    add_custom_target(${FLOWUI_BAKED_CODEGEN_TARGET}
        DEPENDS "${FLOWUI_BAKED_CPP}" "${FLOWUI_BAKED_HPP}"
    )

    set_source_files_properties(
        "${FLOWUI_BAKED_CPP}" "${FLOWUI_BAKED_HPP}"
        PROPERTIES GENERATED TRUE
    )
    add_dependencies(${target} ${FLOWUI_BAKED_CODEGEN_TARGET})
    target_sources(${target} PRIVATE "${FLOWUI_BAKED_CPP}")
    include_directories("${FLOWUI_BAKED_GENERATED_DIR}")

    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${FLOWUI_BAKED_GENERATED_DIR}>
    )
    # Absolute source-header includes handle the common add_subdirectory case;
    # this also supplies project-root includes used by those headers.
    target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(${target} PUBLIC FLOW_UI_HAS_BAKED_CHANGES=1)
    target_compile_definitions(${target} PRIVATE
        FLOWUI_BAKE_MANIFEST_PATH="${FLOWUI_MANIFEST_FILE}"
    )

    set(FLOWUI_BAKED_GENERATED_HPP "${FLOWUI_BAKED_HPP}" PARENT_SCOPE)
endfunction()
