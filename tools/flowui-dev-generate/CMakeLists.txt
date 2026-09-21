cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FlowUiHostGenerator LANGUAGES CXX)
endif()
add_executable(flowui-dev-generate
	main.cpp
	ManifestParser.cpp
	CppCodeGenerator.cpp
)
target_compile_features(flowui-dev-generate PRIVATE cxx_std_23)
target_compile_options(flowui-dev-generate PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
if(MSVC)
    target_sources(flowui-dev-generate PRIVATE ../utf8.manifest)
endif()
