##
 #######################################################################################################################
 #
 #  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

include(PalVersionHelper)
include(PalCompilerWarnings)

pal_include_guard(PalCompilerOptions)

function(pal_compiler_options)

    # Set the C++ standard
    set_target_properties(pal PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE TRUE
    )

    set(isGNU   FALSE)
    set(isClang FALSE)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(isGNU   TRUE)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(isClang TRUE)
    endif()

    if(${isGNU} OR ${isClang})
        # Setup warnings
        pal_compiler_warnings_gnu_or_clang()

        target_compile_options(pal PRIVATE
            # Disables exception handling
            -fno-exceptions

            # Disable optimizations that assume strict aliasing rules
            -fno-strict-aliasing

            # PAL depends on the code generated for PAL_NEW to check for nullptr before calling the constructor.  The current
            # implementation of PAL_NEW uses a user-defined placement new operator for which an input null pointer is undefined
            # by the C++ standard.  GCC leverages this undefined behavior, and optimizes away this nullptr check that we require,
            # so we need to enable this switch to have GCC include that if-check.
            -fcheck-new

            # Having simple optimization on results in dramatically smaller debug builds (and they actually build faster).
            # This is mostly due to constant-folding and dead-code-elimination of registers.
            $<$<CONFIG:Debug>:
                -Og
            >

        )

        # TODO: Investigate why the "$<$<COMPILE_LANGUAGE:CXX>:" is neccessary
        target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
            # Disable run time type information
            # This means breaking dynamic_cast and typeid
            -fno-rtti
        >)

        # Setup any compiler flags specific to GCC
        if(${isGNU})

            target_compile_options(pal PRIVATE
                # Align the stack pointer on a 64 byte boundary (2^6)
                # This was added to resolve some issues after enabling SSE.
                -mpreferred-stack-boundary=6
            )

            if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER "5.0")
                target_compile_options(pal PRIVATE
                    # Compress the debug symbols.
                    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
                        -gz
                    >
                )
            endif()

            # LONG_TERM_TODO:
            # PAL shouldn't be setting up IPO ever. That is the responsibility of the client API (DXCP, XGL, etc.)
            # using CMAKE_INTERPROCEDURAL_OPTIMIZATION.
            # Perhaps setup a warning if the client doesn't set it up.
            # This requires good cmake practice among all components so this is a fight for another day.
            if(${PAL_ENABLE_LTO})
                pal_setup_gcc_ipo()
            endif()

        elseif(${isClang})

            message(WARNING "Clang is untested.")

        endif()
    else()
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()
