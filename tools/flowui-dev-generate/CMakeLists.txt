add_executable(flowui-dev-generate
	main.cpp
	ManifestParser.cpp
	CppCodeGenerator.cpp
)
target_compile_features(flowui-dev-generate PRIVATE cxx_std_23)
