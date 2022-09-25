add_executable(fastgltf_gl_viewer EXCLUDE_FROM_ALL)
target_compile_features(fastgltf_gl_viewer PUBLIC cxx_std_17)
target_link_libraries(fastgltf_gl_viewer PRIVATE fastgltf fastgltf_examples_glad glfw glm)

add_source_directory(TARGET fastgltf_gl_viewer FOLDER ".")

# This is from tests/ and is a clone of KhronosGroup/glTF-Sample-Models
add_dependencies(fastgltf_gl_viewer gltf-sample-models)
