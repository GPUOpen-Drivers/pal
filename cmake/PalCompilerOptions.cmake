##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

include_guard()

include(PalCompilerWarnings)
include(CheckCXXCompilerFlag)

function(pal_compiler_options TARGET)
    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE TRUE
    )

    target_compile_features(${TARGET} PUBLIC cxx_std_17)

    set(isGNU   FALSE)
    set(isClang FALSE)

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(isGNU   TRUE)
        # Output with color if in terminal: https://github.com/ninja-build/ninja/wiki/FAQ
        target_compile_options(${TARGET} PRIVATE -fdiagnostics-color=always)
    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        set(isClang TRUE)
        target_compile_options(${TARGET} PRIVATE -fcolor-diagnostics)
    endif()

    # Enable Large File Support for 32-bit Linux.
    # On 64-bit these flags aren't necessary. The 64-bit versions of these functions are automatically selected.
    if ((CMAKE_HOST_SYSTEM_NAME MATCHES "Linux") AND (CMAKE_SIZEOF_VOID_P EQUAL 4))
        target_compile_definitions(${TARGET} PRIVATE
            # fseeko64() is limited to seeking ~2GB at a time without this.
            _FILE_OFFSET_BITS=64
            # stat64 is undefined in glibc unless this is defined.
            _LARGEFILE64_SOURCE=1
        )
    endif()

    if (isGNU OR isClang)
        # Setup warnings
        pal_compiler_warnings_gnu_or_clang(${TARGET})

        target_compile_options(${TARGET} PRIVATE
            # Disables exception handling
            -fno-exceptions

            # Disable run time type information
            # This means breaking dynamic_cast and typeid
            -fno-rtti

            # Disable optimizations that assume strict aliasing rules
            -fno-strict-aliasing

            # PAL depends on the code generated for PAL_NEW to check for nullptr before calling the constructor.  The current
            # implementation of PAL_NEW uses a user-defined placement new operator for which an input null pointer is undefined
            # by the C++ standard.  GCC leverages this undefined behavior, and optimizes away this nullptr check that we require,
            # so we need to enable this switch to have GCC include that if-check.
            -fcheck-new
        )

        # If we're using a build type that generates debug syms, compress them to save significant disk space.
        check_cxx_compiler_flag(-gz HAS_COMPRESSED_DEBUG)
        if (HAS_COMPRESSED_DEBUG)
            target_compile_options(${TARGET} PRIVATE
                $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:
                    -gz
                >
            )
        endif()

        # The MacOS linker does not have --build-id. That is OK because we do not support building
        # a driver on MacOS; it is only used for experimental building of standalone tools.
        if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            target_link_options(${TARGET} PUBLIC "LINKER:--build-id")
        endif()
    else()
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()
