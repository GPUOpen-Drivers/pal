##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

# FindWDK
# ----------
#
# This module searches for the installed Windows Development Kit (WDK) and
# exposes commands for creating kernel drivers and kernel libraries.
#
# Output variables:
# - `WDK_FOUND` -- if false, do not try to use WDK
# - `WDK_ROOT` -- where WDK is installed
# - `WDK_VERSION` -- the version of the selected WDK
# - `WDK_WINVER` -- the WINVER used for kernel drivers and libraries
#        (default value is `0x0601` and can be changed per target or globally)
#
# Example usage:
#
#   find_package(WDK REQUIRED)
#
#   wdk_add_library(KmdfCppLib STATIC KMDF 1.15
#       KmdfCppLib.h
#       KmdfCppLib.cpp
#   )
#   target_include_directories(KmdfCppLib INTERFACE .)
#
#   wdk_add_driver(KmdfCppDriver KMDF 1.15
#       Main.cpp
#   )
#   target_link_libraries(KmdfCppDriver KmdfCppLib)
#

if(NOT DEFINED WDK_TARGET_VERSION)
    set(WDK_TARGET_VERSION *)
endif()

# If that fails, search in the default install locations.
message(STATUS "Looking for ntddk.h with GLOB: C:/Program Files*/Windows Kits/10/Include/10.0.${WDK_TARGET_VERSION}.0/km/ntddk.h")
file(GLOB WDK_NTDDK_FILES
    "C:/Program Files*/Windows Kits/10/Include/10.0.${WDK_TARGET_VERSION}.0/km/ntddk.h"
)

message(STATUS "WDK_NTDDK_FILES: ${WDK_NTDDK_FILES}")
if(WDK_NTDDK_FILES)
    list(GET WDK_NTDDK_FILES -1 WDK_LATEST_NTDDK_FILE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WDK REQUIRED_VARS WDK_LATEST_NTDDK_FILE)

if (NOT WDK_LATEST_NTDDK_FILE)
    return()
endif()

get_filename_component(WDK_ROOT     ${WDK_LATEST_NTDDK_FILE} DIRECTORY)
get_filename_component(WDK_ROOT     ${WDK_ROOT}              DIRECTORY)
get_filename_component(WDK_VERSION  ${WDK_ROOT}              NAME)
get_filename_component(WDK_ROOT     ${WDK_ROOT}              DIRECTORY)
get_filename_component(WDK_ROOT     ${WDK_ROOT}              DIRECTORY)

message(STATUS "WDK_ROOT: " ${WDK_ROOT})
message(STATUS "WDK_VERSION: " ${WDK_VERSION})

# 0x0601 is the enum value for Win7
set(WDK_WINVER "0x0601" CACHE STRING "Default WINVER for WDK targets")

set(WDK_ADDITIONAL_FLAGS_FILE "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/wdkflags.h")
file(WRITE ${WDK_ADDITIONAL_FLAGS_FILE} "#pragma runtime_checks(\"suc\", off)")

set(WDK_COMPILE_FLAGS
    "/Zp8" # set struct alignment
    "/GF"  # enable string pooling
    "/GR-" # disable RTTI
    "/Gz" # __stdcall by default
    "/kernel"  # create kernel mode binary
    "/FIwarning.h" # disable warnings in WDK headers
    "/FI${WDK_ADDITIONAL_FLAGS_FILE}" # include file to disable RTC
    )

set(WDK_COMPILE_DEFINITIONS "WINNT=1")
set(WDK_COMPILE_DEFINITIONS_DEBUG "MSC_NOOPT;DEPRECATE_DDK_FUNCTIONS=1;DBG=1")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND WDK_COMPILE_DEFINITIONS "_X86_=1;i386=1;STD_CALL")
    set(WDK_PLATFORM "x86")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND WDK_COMPILE_DEFINITIONS "_WIN64;_AMD64_;AMD64")
    set(WDK_PLATFORM "x64")
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()

string(CONCAT WDK_LINK_FLAGS
    "/MANIFEST:NO " #
    "/DRIVER " #
    "/OPT:REF " #
    "/INCREMENTAL:NO " #
    "/OPT:ICF " #
    "/SUBSYSTEM:NATIVE " #
    "/MERGE:_TEXT=.text;_PAGE=PAGE " #
    "/NODEFAULTLIB " # do not link default CRT
    "/SECTION:INIT,d " #
    "/VERSION:10.0 " #
    )

# Generate imported targets for WDK lib files
file(GLOB WDK_LIBRARIES "${WDK_ROOT}/Lib/${WDK_VERSION}/km/${WDK_PLATFORM}/*.lib")
foreach(LIBRARY IN LISTS WDK_LIBRARIES)
    get_filename_component(LIBRARY_NAME ${LIBRARY} NAME_WE)
    string(TOUPPER ${LIBRARY_NAME} LIBRARY_NAME)
    add_library(WDK::${LIBRARY_NAME} INTERFACE IMPORTED)
    set_property(TARGET WDK::${LIBRARY_NAME} PROPERTY INTERFACE_LINK_LIBRARIES  ${LIBRARY})
endforeach(LIBRARY)
unset(WDK_LIBRARIES)

function(wdk_add_driver _target)
    cmake_parse_arguments(WDK "" "KMDF;WINVER" "" ${ARGN})

    add_executable(${_target} ${WDK_UNPARSED_ARGUMENTS})

    set_target_properties(${_target} PROPERTIES SUFFIX ".sys")
    set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${WDK_COMPILE_FLAGS}")
    set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS
        "${WDK_COMPILE_DEFINITIONS};$<$<CONFIG:Debug>:${WDK_COMPILE_DEFINITIONS_DEBUG};>_WIN32_WINNT=${WDK_WINVER}"
        )
    set_target_properties(${_target} PROPERTIES LINK_FLAGS "${WDK_LINK_FLAGS}")

    target_include_directories(${_target} SYSTEM PRIVATE
        "${WDK_ROOT}/Include/${WDK_VERSION}/shared"
        "${WDK_ROOT}/Include/${WDK_VERSION}/km"
        )

    target_link_libraries(${_target} PRIVATE
        WDK::NTOSKRNL
        WDK::HAL
        WDK::BUFFEROVERFLOWK
        WDK::WMILIB
        WDK::LIBCNTPR  # Required for floating point support
        WDK::NTSTRSAFE # Required for vsnprintf support
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_link_libraries(${_target} PRIVATE WDK::MEMCMP)
    endif()

    if(DEFINED WDK_KMDF)
        target_include_directories(${_target} SYSTEM PRIVATE "${WDK_ROOT}/Include/wdf/kmdf/${WDK_KMDF}")
        target_link_libraries(${_target} PRIVATE
            "${WDK_ROOT}/Lib/wdf/kmdf/${WDK_PLATFORM}/${WDK_KMDF}/WdfDriverEntry.lib"
            "${WDK_ROOT}/Lib/wdf/kmdf/${WDK_PLATFORM}/${WDK_KMDF}/WdfLdr.lib"
        )

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS "/ENTRY:FxDriverEntry@8")
        elseif(CMAKE_SIZEOF_VOID_P  EQUAL 8)
            set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS "/ENTRY:FxDriverEntry")
        endif()
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS "/ENTRY:GsDriverEntry@8")
        elseif(CMAKE_SIZEOF_VOID_P  EQUAL 8)
            set_property(TARGET ${_target} APPEND_STRING PROPERTY LINK_FLAGS "/ENTRY:GsDriverEntry")
        endif()
    endif()
endfunction()

function(wdk_add_library _target)
    cmake_parse_arguments(WDK "" "KMDF;WINVER" "" ${ARGN})

    add_library(${_target} ${WDK_UNPARSED_ARGUMENTS})

    set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${WDK_COMPILE_FLAGS}")
    set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS
        "${WDK_COMPILE_DEFINITIONS};$<$<CONFIG:Debug>:${WDK_COMPILE_DEFINITIONS_DEBUG};>_WIN32_WINNT=${WDK_WINVER}"
        )

    target_include_directories(${_target} SYSTEM PRIVATE
        "${WDK_ROOT}/Include/${WDK_VERSION}/shared"
        "${WDK_ROOT}/Include/${WDK_VERSION}/km"
        )

    if(DEFINED WDK_KMDF)
        target_include_directories(${_target} SYSTEM PRIVATE "${WDK_ROOT}/Include/wdf/kmdf/${WDK_KMDF}")
    endif()
endfunction()
