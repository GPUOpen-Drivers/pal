##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

set_target_properties(addrlib PROPERTIES
    CXX_STANDARD              11
    CXX_STANDARD_REQUIRED     ON
    CXX_EXTENSIONS            OFF
    POSITION_INDEPENDENT_CODE ON
)

if((CMAKE_CXX_COMPILER_ID MATCHES "GNU|[Cc]lang")
)
    if(ADDR_ENABLE_WERROR)
        target_compile_options(addrlib PRIVATE -Werror)
    endif()

    # [GCC] Exceptions
    #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
    #
    # [GCC] Options Controlling C++ Dialect
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/C_002b_002b-Dialect-Options.html
    #
    # [GCC] Options That Control Optimization
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
    target_compile_options(addrlib PRIVATE
        -fno-strict-aliasing  # Disable optimizations that assume strict aliasing rules
        -fno-exceptions       # Disable exception handling support.
        -fno-rtti             # Disable run-time type information support.
        -fcheck-new           # Check if pointer returned by operator new is non-null.
        -fno-math-errno       # Single instruction math operations do not set ERRNO.
    )

    # [GCC] Options to Request or Suppress Warnings
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Warning-Options.html
    target_compile_options(addrlib PRIVATE
        -Wall
        -Wextra
        -Wno-unused
        -Wno-unused-parameter
        -Wno-ignored-qualifiers
        -Wno-missing-field-initializers
        -Wno-implicit-fallthrough
        -Wno-shift-negative-value
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang")
        target_compile_options(addrlib PRIVATE
            -Wno-unused-command-line-argument
            -Wno-self-assign
        )
    else()
        target_compile_options(addrlib PRIVATE
            -Wno-type-limits
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Turns on static analysis only if addrlib is the top level project
    # This allows addrlib developers to fix the problem without impacting clients.
    if (ADDRLIB_IS_TOP_LEVEL)
        target_compile_options(addrlib PRIVATE /analyze)
    endif()

    if(ADDR_ENABLE_WERROR)
        target_compile_options(addrlib PRIVATE /WX)
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Do not remove frame pointer on x86-32 (eases debugging)
        target_compile_options(addrlib PRIVATE /Oy-)
    endif()

    target_compile_options(addrlib PRIVATE
        /EHsc # Catches only C++ exceptions and assumes
        # functions declared as extern "C" never throw a C++ exception.
        /GR-  # Disables run-time type information.
        /GS-  # Disables detection of buffer overruns.
    )

    target_compile_options(addrlib PRIVATE
        /W4      # Enable warning level 4.
        /wd4018  # signed/unsigned mismatch
        /wd4065  # switch statement contains 'default' but no 'case' labels
        /wd4100  # unreferenced formal parameter
        /wd4127  # conditional expression is constant
        /wd4189  # local variable is initialized but not referenced
        /wd4201  # nonstandard extension used : nameless struct/union
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4701  # potentially uninitialized local variable
        /wd6297  # arithmetic overflow: 32-bit value is shifted, then cast to 64-bit value
    )
else()
    message(FATAL_ERROR "addrlib: Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
endif()
