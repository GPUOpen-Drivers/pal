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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DevDriver)

# This should be defined anytime we're consumed with this top-level CML
# We'll use this to root all of our CMake folders instead of polluting the global space.
set(DEVDRIVER_FOLDER_ROOT "${CMAKE_FOLDER}/DevDriver")

# This should be defined anytime we're consumed with this top-level CML
# We use this to ensure that all of our test executables end up in the same place
set(DEVDRIVER_TESTS_ROOT "${DEVDRIVER_FOLDER_ROOT}/Tests")

option(DD_OPT_ENABLE_TESTS "Configure whether tests are built" ${DEVDRIVER_IS_TOP_LEVEL})
if (DD_OPT_ENABLE_TESTS)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake")
    include(DevDriver_Tests)
    enable_testing()

    # Tool libraries are required for testing
    set(DD_BP_ENABLE_TOOL_LIBRARIES ON)
endif()

### DevDriver Component Options ########################################################################################

# Configure whether tool related libraries are built
dd_bp(DD_BP_ENABLE_TOOL_LIBRARIES OFF)

# DevDriver module APIs, module extension APIs and related utility libraries for building a DevDriver module.
dd_bp(DD_BP_ENABLE_DD_MODULE_APIS OFF)

# Configure whether CMake install commands are run.
# If we are building for tools we want to enable install logic.
dd_bp(DD_BP_INSTALL ${DD_BP_ENABLE_TOOL_LIBRARIES})

if (DD_BP_ENABLE_TOOL_LIBRARIES)
    set(DD_BP_ENABLE_DD_MODULE_APIS ON)
endif()

### DevDriver CMake IDE Folder management
devdriver_push_scope("DevDriver")

### Shared Components #################################################################################################
## These are never disabled, since most things depend on them (directly or indirectly)
add_subdirectory(shared)

### APIs ##############################################################################################################
## These are never disabled, since most things depend on them (directly or indirectly)
add_subdirectory(apis)

### Modules ###########################################################################################################
if (DD_BP_ENABLE_TOOL_LIBRARIES)
    add_subdirectory(modules)
endif()

### Rust ###########################################################################################################
option(DD_OPT_ENABLE_RUST OFF "Configure whether Rust bindings are built")
if (DD_OPT_ENABLE_RUST)
    if (CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
        message(WARNING "Ninja Multi-Config is not supported by Corrosion, so ignoring Rust targets")
    else()
        add_subdirectory(rust)
    endif()
endif()

### Tests #############################################################################################################
if (DD_OPT_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

### Examples ##########################################################################################################
option(DD_OPT_ENABLE_EXAMPLES "Configure whether examples are built" OFF)
if (DD_OPT_ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()
