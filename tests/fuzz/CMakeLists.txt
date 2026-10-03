function (fastgltf_add_fuzzer NAME)
    add_executable(${NAME} "${NAME}.cpp")

    target_compile_features(${NAME} PRIVATE ${FASTGLTF_COMPILE_TARGET})
    target_compile_options(${NAME} PRIVATE -fsanitize=fuzzer ${FASTGLTF_SANITIZER_FLAGS})
    target_link_options(${NAME} PRIVATE -fsanitize=fuzzer ${FASTGLTF_SANITIZER_FLAGS})

    target_link_libraries(${NAME} PRIVATE fastgltf::fastgltf)
endfunction()

fastgltf_add_fuzzer(base64_fuzzer)
