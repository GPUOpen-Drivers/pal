
##
 #######################################################################################################################
 #
 #  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.5)

find_program(CARGO_BIN cargo $ENV{HOME}/.cargo/bin)
set(CARGO_BIN "${CARGO_BIN}" CACHE PATH "Path to cargo executable")

message(STATUS "Cargo: ${CARGO_BIN}")

if (CARGO_BIN)

    if (NOT DD_DEV_CARGO_TARGET_TRIPLE)

        # Figure out the appropriate target for cargo
        # To enumerate all available targets, use `rustup`:
        #       rustup target list
        # We make some assumptions for our code base - namely that everything is always x86/x64
        if(CMAKE_SYSTEM_NAME MATCHES "Linux")

            # We're assuming desktop x86/x64 GNU-Linux
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CARGO_TARGET_TRIPLE "x86_64-unknown-linux-gnu")
            else()
                message(WARNING "32-bit Desktop Linux is completely untested")
                set(CARGO_TARGET_TRIPLE "i686-unknown-linux-gnu")
            endif()

        elseif(CMAKE_SYSTEM_NAME MATCHES "Android")

            message(WARNING "Android is completely untested - assuming x86/x64 system")
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CARGO_TARGET_TRIPLE "x86_64-linux-android")
            else()
                set(CARGO_TARGET_TRIPLE "i686-linux-android")
            endif()

        elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")

            message(WARNING "macOS is completely untested")
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CARGO_TARGET_TRIPLE "x86_64-apple-darwin")
            else()
                set(CARGO_TARGET_TRIPLE "i686-apple-darwin")
            endif()

        elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")

            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CARGO_TARGET_TRIPLE "x86_64-pc-windows-msvc")
            else()
                set(CARGO_TARGET_TRIPLE "i686-pc-windows-msvc")
            endif()

        else()

            message(WARNING "Unknown CMake System: ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR} - using default Cargo target.")

        endif()

    endif()

    # Save this in the cache - it is user configurable
    set(DD_DEV_CARGO_TARGET_TRIPLE "${CARGO_TARGET_TRIPLE}" CACHE STRING "Target triple for rustc's LLVM backend")
    message(STATUS "Cargo target-triple: ${DD_DEV_CARGO_TARGET_TRIPLE}")

    if (DD_DEV_CARGO_TARGET_TRIPLE)
        set(CARGO_TARGET_TRIPLE_FLAG "--target=${DD_DEV_CARGO_TARGET_TRIPLE}")
    else()
        # This will use the default target as setup with rustup.
        set(CARGO_TARGET_TRIPLE_FLAG "")
    endif()

endif()
