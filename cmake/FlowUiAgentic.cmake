# Pure evidence tools remain usable without constructing FlowUi or a Vulkan instance.
find_package(Threads REQUIRED)
add_library(flowui_agentic_format STATIC
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticJson.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticTypes.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticCapture.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticSnapshot.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticManifest.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticAnalysis.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticKnowledge.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticCommands.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticImage.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticScenario.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticInteraction.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/transport/AgenticLocalTransport.cpp)
add_library(FlowUi::AgenticFormat ALIAS flowui_agentic_format)
set_target_properties(flowui_agentic_format PROPERTIES EXPORT_NAME AgenticFormat POSITION_INDEPENDENT_CODE ON)
target_compile_features(flowui_agentic_format PUBLIC cxx_std_23)
target_include_directories(flowui_agentic_format PUBLIC
    $<BUILD_INTERFACE:${FLOWUI_GENERATED_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(flowui_agentic_format PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(flowui_agentic_format PRIVATE advapi32)
endif()
target_sources(flowui PRIVATE
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticService.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticReporting.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticProvenance.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticAdapters.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticInteractionRuntime.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticInteractionTargets.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/AgenticAction.cpp
    ${PROJECT_SOURCE_DIR}/src/devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.cpp)
target_link_libraries(flowui PRIVATE flowui_agentic_format)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE FLOWUI_AGENTIC_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT FLOWUI_AGENTIC_REVISION)
    set(FLOWUI_AGENTIC_REVISION "unknown")
endif()
file(GLOB_RECURSE FLOWUI_AGENTIC_SOURCE_INPUTS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.hpp" "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/shaders/*" "${PROJECT_SOURCE_DIR}/cmake/*.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/*.in")
list(SORT FLOWUI_AGENTIC_SOURCE_INPUTS)
set(FLOWUI_AGENTIC_SOURCE_HASHES "")
foreach(source_input IN LISTS FLOWUI_AGENTIC_SOURCE_INPUTS)
    file(SHA256 "${source_input}" source_hash)
    file(RELATIVE_PATH source_name "${PROJECT_SOURCE_DIR}" "${source_input}")
    string(APPEND FLOWUI_AGENTIC_SOURCE_HASHES "${source_name}:${source_hash}\n")
endforeach()
string(SHA256 FLOWUI_AGENTIC_SOURCE_DIGEST "${FLOWUI_AGENTIC_SOURCE_HASHES}")
file(GLOB_RECURSE FLOWUI_AGENTIC_KNOWLEDGE_INPUTS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/devSystems/devAgenticWorkflow/knowledge/*")
list(SORT FLOWUI_AGENTIC_KNOWLEDGE_INPUTS)
set(FLOWUI_AGENTIC_KNOWLEDGE_HASHES "")
foreach(knowledge_input IN LISTS FLOWUI_AGENTIC_KNOWLEDGE_INPUTS)
    file(SHA256 "${knowledge_input}" knowledge_hash)
    file(RELATIVE_PATH knowledge_name "${PROJECT_SOURCE_DIR}" "${knowledge_input}")
    string(APPEND FLOWUI_AGENTIC_KNOWLEDGE_HASHES "${knowledge_name}:${knowledge_hash}\n")
endforeach()
string(SHA256 FLOWUI_AGENTIC_KNOWLEDGE_DIGEST "${FLOWUI_AGENTIC_KNOWLEDGE_HASHES}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${FLOWUI_AGENTIC_SOURCE_INPUTS} ${FLOWUI_AGENTIC_KNOWLEDGE_INPUTS})
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE FLOWUI_AGENTIC_WORKTREE_STATUS ERROR_QUIET)
set(FLOWUI_AGENTIC_WORKTREE_DIRTY false)
if(FLOWUI_AGENTIC_WORKTREE_STATUS)
    set(FLOWUI_AGENTIC_WORKTREE_DIRTY true)
endif()
configure_file(${PROJECT_SOURCE_DIR}/cmake/FlowUiAgenticBuildInfo.hpp.in
    ${FLOWUI_GENERATED_INCLUDE_DIR}/FlowUi/AgenticBuildInfo.hpp @ONLY)
string(TOUPPER "${CMAKE_BUILD_TYPE}" FLOWUI_AGENTIC_CONFIGURATION)
set(FLOWUI_AGENTIC_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${FLOWUI_AGENTIC_CONFIGURATION}}")
string(REPLACE "\\" "\\\\" FLOWUI_AGENTIC_FLAGS "${FLOWUI_AGENTIC_FLAGS}")
string(REPLACE "\"" "\\\"" FLOWUI_AGENTIC_FLAGS "${FLOWUI_AGENTIC_FLAGS}")
target_compile_definitions(flowui PRIVATE
    FLOWUI_AGENTIC_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    FLOWUI_AGENTIC_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    FLOWUI_AGENTIC_BUILD_FLAGS="${FLOWUI_AGENTIC_FLAGS}"
    FLOWUI_AGENTIC_LIBRARY_VERSION="${PROJECT_VERSION}"
    FLOWUI_AGENTIC_LIBRARY_REVISION="${FLOWUI_AGENTIC_REVISION}")
add_executable(flowui-agent
    ${PROJECT_SOURCE_DIR}/tools/flowui-agent/main.cpp
    ${PROJECT_SOURCE_DIR}/tools/flowui-agent/command_line.cpp)
target_link_libraries(flowui-agent PRIVATE flowui_agentic_format)
target_compile_definitions(flowui-agent PRIVATE
    FLOWUI_AGENTIC_SOURCE_KNOWLEDGE_DIR="${PROJECT_SOURCE_DIR}/include/devSystems/devAgenticWorkflow/knowledge"
    FLOWUI_AGENTIC_INSTALLED_KNOWLEDGE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/flowui/agentic/knowledge")
if(FLOWUI_INSTALL)
    install(TARGETS flowui_agentic_format EXPORT FlowUiTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(TARGETS flowui-agent RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/devSystems/devAgenticWorkflow/knowledge
        ${PROJECT_SOURCE_DIR}/include/devSystems/devAgenticWorkflow/schemas
        DESTINATION ${CMAKE_INSTALL_DATADIR}/flowui/agentic)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/tools/flowui-agent-mcp/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/flowui/agentic/mcp
        PATTERN "__pycache__" EXCLUDE)
endif()
