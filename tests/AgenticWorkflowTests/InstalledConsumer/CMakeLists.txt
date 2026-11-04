cmake_minimum_required(VERSION 3.25)
project(FlowUiAgenticConsumer LANGUAGES CXX)
find_package(FlowUi CONFIG REQUIRED)
if(NOT FLOW_UI_AGENTIC_WORKFLOW)
    message(FATAL_ERROR "This consumer requires an agentic-enabled FlowUi package.")
endif()
add_executable(flowui_agentic_consumer main.cpp)
target_link_libraries(flowui_agentic_consumer PRIVATE FlowUi::FlowUi FlowUi::AgenticFormat)
target_compile_features(flowui_agentic_consumer PRIVATE cxx_std_23)
get_target_property(installed_resources FlowUi::FlowUi FLOWUI_RESOURCE_DIRECTORY)
string(TOUPPER "${CMAKE_BUILD_TYPE}" consumer_configuration)
set(consumer_flags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${consumer_configuration}}")
target_compile_definitions(flowui_agentic_consumer PRIVATE
    FLOWUI_CONSUMER_RESOURCES="${installed_resources}"
    FLOWUI_CONSUMER_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    FLOWUI_CONSUMER_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    FLOWUI_CONSUMER_FLAGS="${consumer_flags}")
