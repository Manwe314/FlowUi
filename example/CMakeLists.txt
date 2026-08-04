cmake_minimum_required(VERSION 3.20)

project(FlowUiMultiWindowExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The demo intentionally uses FlowUi's built-in developer elements.
set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)
set(FLOWUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLOWUI_BUILD_FONT_BAKER OFF CACHE BOOL "" FORCE)
set(FLOWUI_INSTALL OFF CACHE BOOL "" FORCE)

if(NOT TARGET FlowUi::FlowUi)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/flowui")
endif()

add_executable(flowui_multi_window_example main.cpp)
target_link_libraries(flowui_multi_window_example PRIVATE FlowUi::FlowUi)

