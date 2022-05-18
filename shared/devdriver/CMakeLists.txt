##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.13..3.21)

project(DevDriver LANGUAGES CXX)

include(cmake/DevDriver.cmake)

### DevDriver Component Options ################################################

# Configure whether tool related libraries are built
dd_bp(DD_BP_ENABLE_TOOL_LIBRARIES ${DEVDRIVER_IS_TOP_LEVEL})

# DevDriver module APIs, module extension APIs and related utility libraries for
# building a DevDriver module.
dd_bp(DD_BP_ENABLE_DD_MODULE_APIS ${DEVDRIVER_IS_TOP_LEVEL})

# Configure whether CMake install commands are run.
# If we are building for tools we want to enable install logic.
dd_bp(DD_BP_INSTALL ${DEVDRIVER_IS_TOP_LEVEL})

add_subdirectory(third_party)
add_subdirectory(shared)
add_subdirectory(apis)
add_subdirectory(settings)

if (DD_BP_ENABLE_TOOL_LIBRARIES)
    add_subdirectory(modules)
endif()

option(DD_OPT_ENABLE_RUST "Configure whether Rust bindings are built")
if (DD_OPT_ENABLE_RUST)
    add_subdirectory(rust)
endif()

if (DEVDRIVER_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(examples)
endif()
