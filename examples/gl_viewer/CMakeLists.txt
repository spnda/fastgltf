add_executable(fastgltf_gl_viewer EXCLUDE_FROM_ALL)
target_compile_features(fastgltf_gl_viewer PUBLIC cxx_std_17)
target_link_libraries(fastgltf_gl_viewer PRIVATE fastgltf glfw::glfw glm::glm stb)
target_link_libraries(fastgltf_gl_viewer PRIVATE fg_glad_gl46)

fastgltf_add_source_directory(TARGET fastgltf_gl_viewer FOLDER ".")
