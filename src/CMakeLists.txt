add_library(fastgltf)
add_library(fastgltf::fastgltf ALIAS fastgltf)
target_include_directories(fastgltf PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include>)
target_compile_features(fastgltf PUBLIC cxx_std_17)
compiler_flags(TARGET fastgltf)
enable_debug_inlining(TARGET fastgltf_simdjson)

set_target_properties(fastgltf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)

if (TARGET fastgltf_simdjson)
    target_link_libraries(fastgltf PRIVATE fastgltf_simdjson)
elseif(TARGET simdjson::simdjson)
    target_link_libraries(fastgltf PRIVATE simdjson::simdjson)
endif()

if (SIMDJSON_TARGET_VERSION)
    target_compile_definitions(fastgltf PRIVATE SIMDJSON_TARGET_VERSION="${SIMDJSON_TARGET_VERSION}")
endif()

add_source_directory(TARGET fastgltf FOLDER ".")

install(
    FILES "base64_decode.hpp" "fastgltf_parser.hpp" "fastgltf_types.hpp" "fastgltf_util.hpp"
    TYPE INCLUDE
)

install(
    TARGETS fastgltf
    EXPORT fastgltf-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(
    EXPORT fastgltf-targets
    FILE fastgltfConfig.cmake
    NAMESPACE fastgltf::
    DESTINATION lib/cmake/fastgltf
)
