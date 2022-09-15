set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.1.0)
FetchContent_MakeAvailable(Catch2)

message(STATUS "Downloading glTF Sample Models")
ExternalProject_Add(gltf-sample-models
    URL "https://github.com/KhronosGroup/glTF-Sample-Models/archive/8e9a5a6ad1a2790e2333e3eb48a1ee39f9e0e31b.zip"
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gltf/sample-models"
    DOWNLOAD_EXTRACT_TIMESTAMP false
    DOWNLOAD_NO_PROGRESS FALSE
    EXCLUDE_FROM_ALL
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

# We want these tests to be a optional executable.
add_executable(tests EXCLUDE_FROM_ALL)
target_compile_features(tests PRIVATE cxx_std_20)
target_link_libraries(tests PRIVATE fastgltf)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
compiler_flags(TARGET tests)

add_dependencies(tests gltf-sample-models)

add_source_directory(TARGET tests FOLDER ".")
