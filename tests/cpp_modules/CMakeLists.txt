# Standalone consumer project that imports the fastgltf module, used by CI to verify that the module
# compiles and that the public API is exported. Configure this directory directly, e.g.:
#   cmake -S tests/cpp_modules -B build -G Ninja
cmake_minimum_required(VERSION 3.28)
project(fastgltf_module_consumer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FASTGLTF_ENABLE_CPP_MODULES ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. fastgltf)
if (NOT TARGET fastgltf::module)
    message(FATAL_ERROR "fastgltf::module target was not created")
endif ()

add_executable(fastgltf_module_consumer main.cpp)
target_link_libraries(fastgltf_module_consumer PRIVATE fastgltf::module)
