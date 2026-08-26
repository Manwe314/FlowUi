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

    target_sources(${target} PRIVATE "${FLOWUI_BAKED_CPP}")
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
