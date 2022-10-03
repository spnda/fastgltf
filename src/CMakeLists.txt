add_library(fastgltf)
add_library(fastgltf::fastgltf ALIAS fastgltf)
target_include_directories(fastgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(fastgltf PUBLIC cxx_std_17)
target_compile_definitions(fastgltf PRIVATE SIMDJSON_TARGET_VERSION="${SIMDJSON_TARGET_VERSION}")
target_link_libraries(fastgltf PRIVATE fastgltf_simdjson)
compiler_flags(TARGET fastgltf)

add_source_directory(TARGET fastgltf FOLDER ".")
