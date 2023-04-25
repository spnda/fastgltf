add_library(fastgltf
    "fastgltf.cpp" "fastgltf_util.hpp" "fastgltf_types.hpp" "fastgltf_parser.hpp"
    "base64_decode.cpp" "base64_decode.hpp")

add_library(fastgltf::fastgltf ALIAS fastgltf)
target_include_directories(fastgltf PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include>)
target_compile_features(fastgltf PUBLIC cxx_std_17)
compiler_flags(fastgltf)
enable_debug_inlining(fastgltf_simdjson)

set_target_properties(fastgltf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
set_target_properties(fastgltf PROPERTIES VERSION ${PROJECT_VERSION})

if (TARGET fastgltf_simdjson)
    target_link_libraries(fastgltf PRIVATE fastgltf_simdjson)
elseif(TARGET simdjson::simdjson)
    target_link_libraries(fastgltf PRIVATE simdjson::simdjson)
endif()

if (SIMDJSON_TARGET_VERSION)
    target_compile_definitions(fastgltf PRIVATE SIMDJSON_TARGET_VERSION="${SIMDJSON_TARGET_VERSION}")
endif()

target_compile_definitions(fastgltf PRIVATE "FASTGLTF_USE_CUSTOM_SMALLVECTOR=$<BOOL:${FASTGLTF_USE_CUSTOM_SMALLVECTOR}>")

if (ANDROID)
    target_link_libraries(fastgltf PRIVATE android)
endif()

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
