##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
project(VAM C CXX)

add_library(vam STATIC "")
install(TARGETS vam DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
# No exposed VAM options

### Compiler Options ###################################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(vam PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fPIC -fcheck-new -fno-rtti -fno-math-errno -std=c++0x>)
        target_compile_options(vam PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-ignored-qualifiers -Wno-missing-field-initializers>)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Include Directories ################################################################################################
target_include_directories(vam
    PUBLIC
        ${PROJECT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/core
        ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared
        ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/asic_reg
)

### VAM Sources ########################################################################################################
target_sources(vam PRIVATE
    ${PROJECT_SOURCE_DIR}/vaminterface.cpp
    ${PROJECT_SOURCE_DIR}/core/vamobject.cpp
    ${PROJECT_SOURCE_DIR}/core/vamdevice.cpp
    ${PROJECT_SOURCE_DIR}/core/vamptb.cpp
    ${PROJECT_SOURCE_DIR}/core/vamsuballoc.cpp
    ${PROJECT_SOURCE_DIR}/core/vamsectionalloc.cpp
    ${PROJECT_SOURCE_DIR}/core/vamrange.cpp
)

### Visual Studio Filters ##############################################################################################
target_vs_filters(vam)
