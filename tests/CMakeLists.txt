set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

# We want these tests to be a optional executable.
add_executable(fastgltf_tests EXCLUDE_FROM_ALL "base64_tests.cpp" "basic_test.cpp" "benchmarks.cpp" "glb_tests.cpp" "gltf_path.hpp" "vector_tests.cpp")
target_compile_features(fastgltf_tests PRIVATE cxx_std_20)
target_link_libraries(fastgltf_tests PRIVATE fastgltf fastgltf_simdjson)
if (TARGET glm::glm)
    target_link_libraries(fastgltf_tests PRIVATE glm::glm)
endif()
compiler_flags(TARGET fastgltf_tests)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/catch2")
    add_subdirectory(deps/catch2)
    target_link_libraries(fastgltf_tests PRIVATE Catch2::Catch2WithMain)
endif()
