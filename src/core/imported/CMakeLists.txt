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

target_sources(pal PRIVATE CMakeLists.txt)

### ADDRLIB ####################################################################
set(PAL_ADDR_PATH "default" CACHE PATH "Specify the path to the ADDRLIB project.")

if (${PAL_ADDR_PATH} STREQUAL "default")
    add_subdirectory(addrlib)
else()
    add_subdirectory(${PAL_ADDR_PATH} ${CMAKE_CURRENT_BINARY_DIR}/addrlib_overridden)
endif()

target_link_libraries(pal PRIVATE addrlib)

### HSA ########################################################################
target_include_directories(pal PRIVATE hsa)

### RDF ########################################################################
set(RDF_ENABLE_CXX_BINDINGS OFF)
set(RDF_STATIC ON)

add_subdirectory(rdf)

target_link_libraries(pal PRIVATE rdf)
target_compile_definitions(pal PUBLIC PAL_BUILD_RDF=1)

### VAM ########################################################################
add_subdirectory(vam)
target_link_libraries(pal PRIVATE vam)

