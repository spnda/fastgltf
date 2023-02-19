macro(compiler_flags TARGET)
    cmake_parse_arguments(PARAM "" "TARGET" "" ${ARGN})

    if (NOT PARAM_TARGET STREQUAL "" AND TARGET ${PARAM_TARGET})
        # Note that simdjson automatically figures out which SIMD intrinsics to use at runtime based on
        # cpuid, meaning no architecture flags or other compile flags need to be passed.
        # See https://github.com/simdjson/simdjson/blob/master/doc/implementation-selection.md.
        if (MSVC)
            target_compile_options(${PARAM_TARGET} PRIVATE /EHsc $<$<CONFIG:RELEASE>:/O2 /Ob3 /Ot>)
            if (MSVC_VERSION GREATER 1929)
                target_compile_options(${PARAM_TARGET} PRIVATE /external:anglebrackets)
            endif()
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${PARAM_TARGET} PRIVATE $<$<CONFIG:RELEASE>:-O3>)
        endif()
    endif()
endmacro()

macro(enable_debug_inlining TARGET)
    cmake_parse_arguments(PARAM "" "TARGET" "" ${ARGN})

    if (NOT PARAM_TARGET STREQUAL "" AND TARGET ${PARAM_TARGET})
        if (MSVC)
            target_compile_options(${PARAM_TARGET} PRIVATE $<$<CONFIG:DEBUG>:/Ob2>)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${PARAM_TARGET} PRIVATE $<$<CONFIG:DEBUG>:-finline-functions>)
        endif()
    endif()
endmacro()
