##
 #######################################################################################################################
 #
 #  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

set(CMAKE_FOLDER ${CMAKE_FOLDER}/imported)

# RDF
if (DD_BUILD_RDF)
    set(RDF_ENABLE_CXX_BINDINGS OFF)
    set(RDF_STATIC ON)

    if (NOT TARGET amdrdf)
        add_subdirectory(rdf)
    endif ()
endif ()

# system_info_utils
if (DD_BP_ENABLE_TOOL_LIBRARIES)
    if (NOT TARGET system_info)
        set(SYSTEM_INFO_BUILD_WRITER ON)
        set(SYSTEM_INFO_BUILD_RDF_INTERFACES ON)
        if (SYSTEM_INFO_UTILS_DIR)
            add_subdirectory(${SYSTEM_INFO_UTILS_DIR} system_info_utils)
        else ()
            add_subdirectory(system_info_utils)
        endif ()
    endif ()
endif ()

# dd-registry is used in both PAL (server) and Tools (clients).
add_subdirectory(dd-registry/events)
