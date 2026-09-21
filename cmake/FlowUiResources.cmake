include_guard(GLOBAL)

# Copy the shared pack next to an application using the platform's deployment layout.
function(flowui_package_resources target)
    cmake_parse_arguments(PACK "" "DESTINATION;EXTRA_DIRECTORY" "" ${ARGN})
    if(PACK_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown flowui_package_resources arguments: ${PACK_UNPARSED_ARGUMENTS}")
    endif()
    get_target_property(resource_directory FlowUi::FlowUi FLOWUI_RESOURCE_DIRECTORY)
    if(NOT resource_directory)
        message(FATAL_ERROR "FlowUi resource directory is unavailable")
    endif()
    if(NOT PACK_DESTINATION)
        get_target_property(is_bundle ${target} MACOSX_BUNDLE)
        if(APPLE AND is_bundle)
            set(PACK_DESTINATION "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/${FLOWUI_RESOURCE_SUBDIRECTORY}")
        elseif(WIN32)
            set(PACK_DESTINATION "$<TARGET_FILE_DIR:${target}>/${FLOWUI_RESOURCE_SUBDIRECTORY}")
        else()
            set(PACK_DESTINATION "$<TARGET_FILE_DIR:${target}>/../share/${FLOWUI_RESOURCE_SUBDIRECTORY}")
        endif()
    endif()
    add_custom_target(${target}_flowui_resources
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIRECTORY=${resource_directory}"
            "-DDESTINATION_DIRECTORY=${PACK_DESTINATION}"
            "-DEXTRA_DIRECTORY=${PACK_EXTRA_DIRECTORY}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FlowUiCopyResources.cmake"
        VERBATIM)
    if(TARGET flowui_resources)
        add_dependencies(${target}_flowui_resources flowui_resources)
    endif()
    add_dependencies(${target} ${target}_flowui_resources)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DRUNTIME_LIBRARIES=$<TARGET_RUNTIME_DLLS:${target}>"
                "-DDESTINATION_DIRECTORY=$<TARGET_FILE_DIR:${target}>"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FlowUiCopyRuntime.cmake"
            VERBATIM)
    endif()
endfunction()
