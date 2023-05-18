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

if (RDF_STATIC)
    add_library(amdrdf STATIC)
else()
    add_library(amdrdf SHARED)
endif()

target_include_directories(amdrdf PUBLIC inc)

add_library(rdf ALIAS amdrdf)

set(HEADERS inc/amdrdf.h)

target_sources(amdrdf PRIVATE
    ${HEADERS}
    src/amdrdf.cpp
)

target_link_libraries(amdrdf PRIVATE zstd)
target_compile_definitions(amdrdf PRIVATE
    RDF_BUILD_LIBRARY
    RDF_BUILD_STATIC=$<BOOL:${RDF_STATIC}>
)

if (RDF_ENABLE_CXX_BINDINGS)
    # This is needed in order to be absorbed by libraries that don't allow exceptions
    target_compile_definitions(amdrdf PUBLIC RDF_CXX_BINDINGS=1)
endif()

if (UNIX)
    target_compile_definitions(amdrdf PRIVATE RDF_PLATFORM_UNIX=1)
endif()

if (WIN32)
    target_compile_definitions(amdrdf PRIVATE
        RDF_PLATFORM_WINDOWS=1
        # Disable warnings like "fopen has been marked deprecated"
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(RDF_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(RDF_BUILD_INSTALL)
    install(TARGETS amdrdf
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
    install(FILES ${HEADERS} DESTINATION include/rdf)
endif()
