include_guard(GLOBAL)

# Include this module before add_subdirectory(FlowUi) to register dependencies of
# a storage implementation. PACKAGES are relocatable
# find_dependency names; their targets must already exist when FlowUi is configured.
function(flowui_register_storage_dependencies)
    cmake_parse_arguments(PARSE_ARGV 0 provider "" "" "TARGETS;PACKAGES")
    if(provider_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown storage dependency arguments: ${provider_UNPARSED_ARGUMENTS}")
    endif()
    foreach(package IN LISTS provider_PACKAGES)
        if(NOT package MATCHES "^[A-Za-z0-9_.+-]+$")
            message(FATAL_ERROR "Storage dependency PACKAGES must contain package names: ${package}")
        endif()
    endforeach()
    set_property(GLOBAL APPEND PROPERTY FLOWUI_STORAGE_DEPENDENCY_TARGETS ${provider_TARGETS})
    set_property(GLOBAL APPEND PROPERTY FLOWUI_STORAGE_DEPENDENCY_PACKAGES ${provider_PACKAGES})
endfunction()
