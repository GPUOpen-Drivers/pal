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

# This file is dedicate to helper function to assist in writing the cmake build

include(TestBigEndian)

function(addr_get_cpu_endianness CPU_ENDIANNESS)
    # Until CMake 3.20 test big endian is very slow.
    # This hardcodes the answer to avoid slowing down configure time for most users.
    # In the future just use CMAKE_<LANG>_BYTE_ORDER
    if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64|x86")
        set(${CPU_ENDIANNESS} "LITTLEENDIAN_CPU" PARENT_SCOPE)
        return()
    endif()

    # If we can't use the hardcoded answer. Then calculate it.
    test_big_endian(isBigEndianCpu)

    if(isBigEndianCpu)
        set(${CPU_ENDIANNESS}    "BIGENDIAN_CPU" PARENT_SCOPE)
    else()
        set(${CPU_ENDIANNESS} "LITTLEENDIAN_CPU" PARENT_SCOPE)
    endif()
endfunction()

function(addrlib_bp AMD_VAR AMD_DFLT)
    set(singleValues MODE DEPENDS_ON MSG)
    cmake_parse_arguments(PARSE_ARGV 0 "AMD" ""  "${singleValues}" "")

    # STATUS is a good default value
    if (NOT DEFINED AMD_MODE)
        set(AMD_MODE "STATUS")
    endif()

    # Default to nothing
    if (NOT DEFINED AMD_MSG)
        set(AMD_MSG "")
    endif()

    # If the user specified a dependency. And that depedency is false.
    # Then we shouldn't define the build parameter
    if (DEFINED AMD_DEPENDS_ON AND (NOT ${AMD_DEPENDS_ON}))
        return()
    endif()

    # If clients don't yet have 3.15 still allow them usage of DEBUG and VERBOSE
    if (${CMAKE_VERSION} VERSION_LESS "3.15" AND ${AMD_MODE} MATCHES "DEBUG|VERBOSE")
        set(AMD_MODE "STATUS")
    endif()

    # If this variable hasn't been defined by the client. Then we provide the default value.
    if (NOT DEFINED ${AMD_VAR})
        set(${AMD_VAR} ${AMD_DFLT} PARENT_SCOPE)

        message(${AMD_MODE} "${AMD_VAR} not set. Defaulting to ${AMD_DFLT}. ${AMD_MSG}")

        return()
    endif()

    # If we got to this point it means the build parameter is getting overriden.
    # To assist in potential debugging show what the value was set to.
    message(STATUS "${AMD_VAR} overridden to ${${AMD_VAR}}")
endfunction()
