include(CMakeFindDependencyMacro)

find_dependency(fastgltf_simdjson REQUIRED)

include (${CMAKE_CURRENT_LIST_DIR}/fastgltf-targets.cmake)