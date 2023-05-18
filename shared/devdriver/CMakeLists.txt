##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Force the new CMP0091 policy: use the CMAKE_MSVC_RUNTIME_LIBRARY abstraction instead of placing MSVC runtime library flags in the default cache entries
cmake_policy(SET CMP0091 NEW)

project(DevDriver LANGUAGES CXX)

include(cmake/DevDriver.cmake)

### DevDriver Component Options ################################################

# Configure whether tool related libraries are built
dd_bp(DD_BP_ENABLE_TOOL_LIBRARIES ${DEVDRIVER_IS_TOP_LEVEL})

# DevDriver module APIs, module extension APIs and related utility libraries for
# building a DevDriver module.
dd_bp(DD_BP_ENABLE_DD_MODULE_APIS ${DEVDRIVER_IS_TOP_LEVEL})

# Enable building existing modules.
dd_bp(DD_BP_BUILD_MODULES ${DEVDRIVER_IS_TOP_LEVEL})

# Configure whether CMake install commands are run.
# If we are building for tools we want to enable install logic.
dd_bp(DD_BP_INSTALL ${DEVDRIVER_IS_TOP_LEVEL})

# ddTestUtil contains utilities for tests that rely on network, RPC, router,
# and etc.
dd_bp(DD_BP_ENABLE_DD_TEST_UTIL ${DEVDRIVER_IS_TOP_LEVEL})

# Configure whether to build ddSettings.
dd_bp(DD_BP_ENABLE_DD_SETTINGS ${DEVDRIVER_IS_TOP_LEVEL})

# Configure whether to build tests and examples.
dd_bp(DD_BP_BUILD_TESTS_EXAMPLES ${DEVDRIVER_IS_TOP_LEVEL})

# Configure whether documentation is built
dd_bp(DD_BP_BUILD_DOCS OFF)

# Configure whether to enable MSVC Code Analysis. Code Analysis causes significantly longer build time.
dd_bp(DD_MSVC_CODE_ANALYZE OFF)

# Currently rdf lib is only used by memory-trace and rgd modules. So enable it
# when those modules are built.
dd_bp(DD_BUILD_RDF ${DD_BP_BUILD_MODULES})

# Flag for building RDF versions of MemoryModule, GPUDetective, and RMV Streamer, default off for now:
dd_bp(DD_BUILD_RDF_MODULES OFF)

add_subdirectory(third_party)
add_subdirectory(imported)
add_subdirectory(shared)
add_subdirectory(apis)

if (DD_BP_BUILD_MODULES)
    add_subdirectory(modules)
endif()

option(DD_OPT_ENABLE_RUST "Configure whether Rust bindings are built")
if (DD_OPT_ENABLE_RUST)
    add_subdirectory(rust)
endif()

if (DD_BP_BUILD_DOCS)
    add_subdirectory(doc)
endif()
