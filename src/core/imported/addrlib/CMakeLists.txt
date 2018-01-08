##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
project(ADDRLIB C CXX)

add_library(addrlib STATIC "")
install(TARGETS addrlib DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
option(ADDR_R600_BUILD "Build ${PROJECT_NAME} with R600 support?" OFF)
option(ADDR_R800_BUILD "Build ${PROJECT_NAME} with R800 support?" OFF)
option(ADDR_R900_BUILD "Build ${PROJECT_NAME} with R900 support?" OFF)

option(ADDR_SI_BUILD "Build ${PROJECT_NAME} with SI support?" OFF)
option(ADDR_CI_BUILD "Build ${PROJECT_NAME} with CI support?" OFF)
option(ADDR_VI_BUILD "Build ${PROJECT_NAME} with VI support?" OFF)
option(ADDR_GFX9_BUILD "Build ${PROJECT_NAME} with GFX9 support?" OFF)

if(ADDR_R900_BUILD AND NOT ADDR_R800_BUILD)
    set(ADDR_R800_BUILD ON CACHE BOOL "Forcing R800 ON as ADDR_R900_BUILD is ON." FORCE)
endif()

if(ADDR_CI_BUILD AND NOT ADDR_SI_BUILD)
    set(ADDR_SI_BUILD ON CACHE BOOL "Forcing SI ON as it is required when ADDR_CI_BUILD is ON." FORCE)
elseif(ADDR_VI_BUILD AND NOT ADDR_SI_BUILD)
    set(ADDR_SI_BUILD ON CACHE BOOL "Forcing SI ON as it is required when ADDR_VI_BUILD is ON." FORCE)
endif()

if(ADDR_R600_BUILD)
    set(ADDR_R600_CHIP_DIR ${PROJECT_SOURCE_DIR}/r600/chip CACHE PATH "Specify the path to the r600 register chip headers.")
endif()
if(ADDR_R800_BUILD)
    set(ADDR_R800_CHIP_DIR ${PROJECT_SOURCE_DIR}/r800/chip CACHE PATH "Specify the path to the r800 register chip headers.")
endif()
if(ADDR_SI_BUILD)
    set(ADDR_SI_CHIP_DIR   ${PROJECT_SOURCE_DIR}/r800/chip CACHE PATH "Specify the path to the SI register chip headers.")
endif()
if(ADDR_GFX9_BUILD)
    set(ADDR_GFX9_CHIP_DIR ${PROJECT_SOURCE_DIR}/gfx9/chip CACHE PATH "Specify the path to the GFX9 register chip headers.")
endif()

# CMAKE-TODO: What is this used for? PAL never sets this.
# option(ADDR_AM_BUILD  "Build ADDRLIB as an AM Build?" OFF)
# ADDR_LNX_KERNEL_BUILD
# ADDR_LNX_KERNEL_ESX_BUILD

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: These are /W4 (level 4) warnings
        # Review what is acceptable and what is not.
        target_compile_options(addrlib PRIVATE
            /wd4100 # unreferenced formal parameter
            /wd4127 # conditional expression is constant
            /wd4189 # local variable is initialized but not referenced
            /wd4201 # nonstandard extension used : nameless struct/union
            /wd4701 # potentially uninitialized local variable
        )

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(addrlib PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fPIC -fcheck-new -fno-rtti -fno-math-errno>)
        target_compile_options(addrlib PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers>)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured ${PROJECT_NAME} compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Defines/Includes/Sources ###########################################################################################
target_compile_definitions(addrlib PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)

#if(ADDR_AM_BUILD)
#    target_compile_definitions(addrlib PRIVATE ADDR_AM_BUILD)
#endif()

# CMAKE-TODO: What is this used for? PAL never sets this.
#if(ADDR_LNX_KERNEL_BUILD)
#    target_include_directories(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/inc)
#    target_compile_definitions(addrlib PRIVATE ADDR_LNX_KERNEL_BUILD)
#endif()

# CMAKE-TODO: What is this used for? PAL never sets this.
#if(ADDR_LNX_KERNEL_ESX_BUILD)
#    target_compile_definitions(addrlib PRIVATE ADDR_LNX_KERNEL_BUILD)

#    target_include_directories(addrlib PRIVATE
#        ${ADDR_GCC64}/RHEL5/usr/include
#        ${GLOBAL_ROOT_SRC_DIR}/drivers/cmmqs/driver/src/lnx/visibility.hpp
#    )
#endif()

target_include_directories(addrlib
    PUBLIC
        ${PROJECT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/core
        ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/asic_reg
)

target_sources(addrlib PRIVATE
    ${PROJECT_SOURCE_DIR}/addrinterface.cpp
    ${PROJECT_SOURCE_DIR}/core/addrobject.cpp
    ${PROJECT_SOURCE_DIR}/core/addrlib.cpp
    ${PROJECT_SOURCE_DIR}/core/addrlib1.cpp
    ${PROJECT_SOURCE_DIR}/core/addrlib2.cpp
    ${PROJECT_SOURCE_DIR}/core/addrelemlib.cpp
)

if(ADDR_R600_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_R600_BUILD)

    target_include_directories(addrlib PRIVATE
        ${PROJECT_SOURCE_DIR}/r600
        ${ADDR_R600_CHIP_DIR}
        ${PROJECT_SOURCE_DIR}/inc/chip/r600
    )

    target_sources(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r600/r600addrlib.cpp)
endif()

if(ADDR_R900_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_R900_BUILD)
endif()

if(ADDR_GFX9_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_GFX9_BUILD)

    target_include_directories(addrlib PRIVATE
        ${PROJECT_SOURCE_DIR}/gfx9
        ${ADDR_GFX9_CHIP_DIR}
        ${PROJECT_SOURCE_DIR}/inc/chip/gfx9
    )

    target_sources(addrlib PRIVATE
        ${PROJECT_SOURCE_DIR}/gfx9/gfx9addrlib.cpp
        ${PROJECT_SOURCE_DIR}/gfx9/coord.cpp
    )
endif()

# CMAKE-TODO: Check this for correctness. Previously ADDR_EGBASED
if(ADDR_R800_BUILD OR ADDR_SI_BUILD)
    target_include_directories(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r800)
    target_include_directories(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/inc/chip/r800)
    target_sources(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r800/egbaddrlib.cpp)
endif()

if(ADDR_R800_BUILD)
    target_include_directories(addrlib PRIVATE ${ADDR_R800_CHIP_DIR})
    target_compile_definitions(addrlib PRIVATE ADDR_R800_BUILD)
    target_sources(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r800/r800addrlib.cpp)
endif()

if(ADDR_SI_BUILD)
    target_include_directories(addrlib PRIVATE ${ADDR_SI_CHIP_DIR})
    target_compile_definitions(addrlib PRIVATE ADDR_SI_BUILD)
    target_sources(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r800/siaddrlib.cpp)

    if(ADDR_CI_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_CI_BUILD)
        target_sources(addrlib PRIVATE ${PROJECT_SOURCE_DIR}/r800/ciaddrlib.cpp)
    endif()

    if(ADDR_VI_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_VI_BUILD)
    endif()
endif()

### Visual Studio Filters ##############################################################################################
target_vs_filters(addrlib)

### Mark all options as advanced #######################################################################################
if(ADDR_OPTIONS_MARK_ADVANCED)
    mark_grouped_as_advanced(ADDR)
endif()
