set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

# We want these tests to be a optional executable.
add_executable(tests EXCLUDE_FROM_ALL)
target_compile_features(tests PRIVATE cxx_std_20)
target_link_libraries(tests PRIVATE fastgltf glm::glm)
compiler_flags(TARGET tests)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/catch2")
    add_subdirectory(deps/catch2)
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
endif()

add_source_directory(TARGET tests FOLDER ".")
