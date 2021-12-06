##
 #######################################################################################################################
 #
 #  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

find_program(CARGO cargo $ENV{HOME}/.cargo/bin)

set(CARGO "${CARGO}" CACHE PATH "Path to cargo executable")
message(STATUS "Cargo: ${CARGO}")

# Figure out the appropriate target for cargo
if (NOT DD_CARGO_TARGET_TRIPLE)

    # To enumerate all available targets, use `rustup`:
    #       rustup target list
    # I recommend using grep to narrow the search for something interesting
    #       e.g. rustup target list | grep musl
    # Otherwise, the list is very long.
    #
    # We may make some assumptions for our code bases here, so YMMV while using this file

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")

        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CARGO_TARGET_TRIPLE "x86_64-unknown-linux-gnu")
        else()
            set(CARGO_TARGET_TRIPLE "i686-unknown-linux-gnu")
        endif()

    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")

        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CARGO_TARGET_TRIPLE "x86_64-apple-darwin")
        else()
            message(FATAL_ERROR "32-bit Apple platforms are unsupported")
        endif()

    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")

        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CARGO_TARGET_TRIPLE "x86_64-pc-windows-msvc")
        else()
            set(CARGO_TARGET_TRIPLE "i686-pc-windows-msvc")
        endif()

    else()
        message(FATAL_ERROR "Unknown CMake System: ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    endif()

endif()

# Save this in the cache - it needs to be user configurable
set(DD_CARGO_TARGET_TRIPLE "${CARGO_TARGET_TRIPLE}" CACHE STRING "Target triple for rustc's LLVM backend")
message(VERBOSE "Cargo target-triple: ${DD_CARGO_TARGET_TRIPLE}")
