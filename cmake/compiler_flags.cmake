macro(compiler_flags)
    cmake_parse_arguments(PARAM "" "TARGET" "" ${ARGN})

    if (PARAM_TARGET STREQUAL "" OR NOT TARGET ${PARAM_TARGET})
        return()
    endif()

    # Note that simdjson automatically figures out which SIMD intrinsics to use at runtime based on
    # cpuid, meaning no architecture flags or other compile flags need to be passed.
    # See https://github.com/simdjson/simdjson/blob/master/doc/implementation-selection.md.
    if (MSVC)
        target_compile_options(${PARAM_TARGET} PRIVATE /EHsc $<$<CONFIG:RELEASE>:/O2 /Ob2 /Ot>)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PARAM_TARGET} PRIVATE $<$<CONFIG:RELEASE>:-O3>)
    endif()
endmacro()
