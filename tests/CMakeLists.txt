FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.1.0)
FetchContent_MakeAvailable(Catch2)
set_target_properties(Catch2WithMain PROPERTIES EXCLUDE_FROM_ALL TRUE)

# We want these tests to be a optional executable.
add_executable(tests EXCLUDE_FROM_ALL)
target_compile_features(tests PRIVATE cxx_std_20)
target_link_libraries(tests PRIVATE fastgltf)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

add_source_directory(TARGET tests FOLDER ".")
