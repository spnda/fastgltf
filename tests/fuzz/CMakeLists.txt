function (fastgltf_add_fuzzer NAME)
    add_executable(${NAME} "${NAME}.cpp")
    fastgltf_compiler_flags(${NAME})

    target_compile_features(${NAME} PRIVATE ${FASTGLTF_COMPILE_TARGET})
    target_compile_options(${NAME} PRIVATE
        -fsanitize=fuzzer
        ${FASTGLTF_SANITIZER_FLAGS}
    )
    target_link_options(${NAME} PRIVATE
        -fsanitize=fuzzer
        ${FASTGLTF_SANITIZER_FLAGS}
    )

    target_link_libraries(${NAME} PRIVATE fastgltf::fastgltf)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus/${NAME})
endfunction()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/artifacts)

fastgltf_add_fuzzer(gltf_fuzzer)
fastgltf_add_fuzzer(uri_fuzzer)
