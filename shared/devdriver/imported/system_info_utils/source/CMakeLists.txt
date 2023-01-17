##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

project(system_info)

add_library(${PROJECT_NAME} STATIC
        definitions.h
        system_info_reader.h
        system_info_reader.cpp)

if (SYSTEM_INFO_BUILD_RDF_INTERFACES)
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DSYSTEM_INFO_ENABLE_RDF)
    target_link_libraries(${PROJECT_NAME} PUBLIC amdrdf)
endif ()

if (SYSTEM_INFO_BUILD_WRITER)
    target_sources(${PROJECT_NAME} PUBLIC
            system_info_writer.h
            system_info_writer.cpp)
    target_link_libraries(${PROJECT_NAME} PUBLIC devdriver ddApi ddGpuInfo)
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PUBLIC wmi)
    endif ()
endif ()
target_include_directories(${PROJECT_NAME}
        PUBLIC . ./third_party/json/include/)
