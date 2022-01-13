##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

include(FindCargo)

# Cargo's "target" directory for all build files
set(DEVDRIVER_RS_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/target)

# Crate's artifacts directory
# If you know your crate's artifacts by name, they should be listed here.
set(DEVDRIVER_RS_OUTPUT_DIR ${DEVDRIVER_RS_TARGET_DIR}/${DD_CARGO_TARGET_TRIPLE}/$<IF:$<CONFIG:Debug>,debug,release>)

# Runtime and library output directories
if (MSVC)
    # On Windows with MSVC, there is an extra layer of folders here
    # TODO: XCode may do something similar. We should have this hidden behind CMake,
    # so that we do not need to keep track of this.
    set(DD_BIN_DIR ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(DD_LIB_DIR ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
else()
    set(DD_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
    set(DD_LIB_DIR ${CMAKE_BINARY_DIR}/lib)
endif()

set(CARGO_FLAGS "")
list(APPEND CARGO_FLAGS "--target=${DD_CARGO_TARGET_TRIPLE}")
list(APPEND CARGO_FLAGS --target-dir=${DEVDRIVER_RS_TARGET_DIR})
list(APPEND CARGO_FLAGS --manifest-path=${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)
list(APPEND CARGO_FLAGS $<IF:$<NOT:$<CONFIG:Debug>>,--release,>)

if (DD_OPT_CI_BUILD)

    # Normal developers don't need ths kind of verbosity, but it's useful when debugging CI
    list(APPEND CARGO_FLAGS "-v")

    # Normal developers can write over their Cargo.lock, but CI must not. (its lock file is mounted as R/O)
    list(APPEND CARGO_FLAGS --locked)

endif()

# TODO: Add support for Rust static libs
