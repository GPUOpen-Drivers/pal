##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

include_guard()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

include(PalCompileDefinitions)
include(PalCompilerOptions)
include(TestBigEndian)

# Store the file's location for relative paths used in the functions.
set(PAL_VERSION_HELPER_DIR ${CMAKE_CURRENT_LIST_DIR})

# A helper function to allow usage of DEBUG mode introduced in cmake 3.15
# This function is intended to prevent printing out everything to STATUS,
# which is undesirable for clients of PAL, since only cmake developers need to care.
function(message_debug)
    # DEBUG mode was introduced in 3.15
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
        message(DEBUG ${ARGV})
    else()
        message(STATUS "DEBUG: ${ARGV}")
    endif()
endfunction()

# A helper function to allow usage of VERBOSE mode introduced in cmake 3.15
# This function is intended to prevent printing out everything to STATUS,
# which is undesirable for clients of PAL, since only cmake developers need to care.
function(message_verbose)
    # VERBOSE mode was introduced in 3.15
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
        message(VERBOSE ${ARGV})
    else()
        message(STATUS "VERBOSE: ${ARGV}")
    endif()
endfunction()

# PAL uses specific asics, SC uses generations, Addrlib does both...
# That's why I made the helper function "pal_set_or", to help set variables that specify graphics generations.
# Basically the idea being if any are enabled it'll activate.
# Cmake doesn't have a 'set_or' function (ie |= )
function(pal_set_or VARIABLE VALUE)
    # Ex:
    # VARIABLE = FOOBAR
    # VALUE = OFF

    # If the variable hasn't been defined before then set it.
    if (NOT DEFINED ${VARIABLE})
        set(${VARIABLE} ${VALUE} PARENT_SCOPE)
        return()
    endif()

    set(cur_value ${${VARIABLE}})

    if (NOT ${cur_value})
        set(${VARIABLE} ${VALUE} PARENT_SCOPE)
    endif()
endfunction()

function(pal_bp AMD_VAR AMD_DFLT)
    set(singleValues MODE DEPENDS_ON MSG)
    set(multiValues ASIC_CONFIG)

    set(JUMP_TO_END 0)
    cmake_parse_arguments(PARSE_ARGV 0 "AMD" ""  "${singleValues}" "${multiValues}")

    # STATUS is a good default value
    if (NOT DEFINED AMD_MODE)
        set(AMD_MODE "STATUS")
    endif()

    # Default to nothing
    if (NOT DEFINED AMD_MSG)
        set(AMD_MSG "")
    endif()

    # If the user has requested that we force BUILD settings on, and
    # the variable has not already been defined, and
    # we've met the dependencies required, then
    # force the setting on.
    if ((DEFINED PAL_BUILD_FORCE_ON) AND
        (${PAL_BUILD_FORCE_ON})      AND
        (NOT DEFINED ${AMD_VAR})     AND
        ((NOT DEFINED AMD_DEPENDS_ON) OR (${AMD_DEPENDS_ON})))
        # We only want to match BuildParameters (PAL_BUILD*).
        string(REGEX MATCH "(PAL_BUILD).*" _MATCHED ${AMD_VAR})
        if (_MATCHED)
            # If the variable is not defined, set it locally so that the right value can be derefernced and passed to
            # pal_set_or at the end of the function while setting the ASIC configurations
            set(${AMD_VAR} ON)
            set(${AMD_VAR} ON PARENT_SCOPE)

            message(STATUS "${AMD_VAR} forced ON. ${AMD_MSG}")

            set(JUMP_TO_END 1)
        endif()
    endif()

    if(NOT JUMP_TO_END)
        # If the user specified a dependency. And that depedency is false.
        # Then we shouldn't define the build parameter
        foreach(d ${AMD_DEPENDS_ON})
            string(REPLACE "(" " ( " _CMAKE_PAL_DEP "${d}")
            string(REPLACE ")" " ) " _CMAKE_PAL_DEP "${_CMAKE_PAL_DEP}")
            string(REGEX REPLACE " +" ";" PAL_BP_DEP "${_CMAKE_PAL_DEP}")
            unset(_CMAKE_PAL_DEP)

            # If the dependency isn't defined or if the dependency is false,
            # then we need to disable the build parameter.
            if((NOT DEFINED ${PAL_BP_DEP}) OR (NOT ${${PAL_BP_DEP}}))
                if (DEFINED ${PAL_BP_DEP})
                    set(_NEW_VALUE ${${PAL_BP_DEP}})
                else()
                    # If the dependency is not defined, this isn't worthy of anything but a status message.
                    set(AMD_MODE "STATUS")
                    set(_NEW_VALUE OFF)
                endif()
                # If the variable is not defined, set it locally so that the right value can be derefernced and passed to
                # pal_set_or at the end of the function while setting the ASIC configurations
                set(${AMD_VAR} ${_NEW_VALUE})
                set(${AMD_VAR} ${_NEW_VALUE} PARENT_SCOPE)

                message(${AMD_MODE} "${AMD_VAR} dependency (${d}) failed. Defaulting to ${_NEW_VALUE}. ${AMD_MSG}")
                set(JUMP_TO_END 1)
                break()
            endif()
        endforeach()
    endif()

    if(NOT JUMP_TO_END)
        # If clients don't yet have 3.15 still allow them usage of DEBUG and VERBOSE
        if (${CMAKE_VERSION} VERSION_LESS "3.15" AND ${AMD_MODE} MATCHES "DEBUG|VERBOSE")
            set(AMD_MODE "STATUS")
        endif()

        # If this variable hasn't been defined by the client. Then we provide the default value.
        if (NOT DEFINED ${AMD_VAR})
            # If the variable is not defined, set it locally so that the right value can be derefernced and passed to
            # pal_set_or at the end of the function while setting the ASIC configurations
            set(${AMD_VAR} ${AMD_DFLT})
            set(${AMD_VAR} ${AMD_DFLT} PARENT_SCOPE)

            message(${AMD_MODE} "${AMD_VAR} not set. Defaulting to ${AMD_DFLT}. ${AMD_MSG}")

            set(JUMP_TO_END 1)
        endif()

        if(NOT JUMP_TO_END)
            # If we got to this point it means the build parameter is getting overriden.
            # To assist in potential debugging show what the value was set to.
            set(JUMP_TO_END 1)
            message(STATUS "${AMD_VAR} overridden to ${${AMD_VAR}}")
        endif()
    endif()

    # Set all the ASIC configurations that are passed as arguments if any
    if (DEFINED AMD_ASIC_CONFIG)
        foreach(elem ${AMD_ASIC_CONFIG})
            pal_set_or(${elem} ${${AMD_VAR}})
            set(${elem} ${${elem}} PARENT_SCOPE)
        endforeach()
    endif()
endfunction()

# Helper function that sets the value of the variable passed in to the current value of
# PAL_INTERFACE_MAJOR_VERSION defined in palLib.h.
function(pal_get_current_pal_interface_major_version VARIABLE)
    # Relative path to palLib.h from this file.
    set(LIB_H_PATH ${PAL_VERSION_HELPER_DIR}/../inc/core/palLib.h)

    if(NOT EXISTS ${LIB_H_PATH})
        message(FATAL_ERROR "Could not find palLib.h at \"${LIB_H_PATH}\"")
    endif()

    # When the header file changes re-run the configure step
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LIB_H_PATH})

    # Read the line from the file where the version is defined.
    file(STRINGS
        ${LIB_H_PATH}
        PAL_INTERFACE_MAJOR_VERSION
        REGEX "^#define PAL_INTERFACE_MAJOR_VERSION [0-9]+$"
    )
    # Validate the version was found.
    if(NOT PAL_INTERFACE_MAJOR_VERSION)
        message(FATAL_ERROR "Could not find PAL_INTERFACE_MAJOR_VERSION in \"${LIB_H_PATH}\"")
    endif()
    # Read the version from the line.
    string(REGEX REPLACE
        "#define PAL_INTERFACE_MAJOR_VERSION " ""
        PAL_INTERFACE_MAJOR_VERSIONX ${PAL_INTERFACE_MAJOR_VERSION}
    )
    # Set the value of VARIABLE to the version.
    set(${VARIABLE} ${PAL_INTERFACE_MAJOR_VERSIONX} PARENT_SCOPE)
endfunction()

# Returns either LITTLE or BIG as a string
function(pal_get_cpu_endianness CPU_ENDIANNESS)
    # Until CMake 3.20 test big endian is very slow.
    # This hardcodes the answer to avoid slowing down configure time for most users.
    # In the future just use CMAKE_<LANG>_BYTE_ORDER
    if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64|x86")
        set(${CPU_ENDIANNESS} "LITTLE" PARENT_SCOPE)
        return()
    endif()

    # If we can't use the hardcoded answer. Then calculate it.
    test_big_endian(isBigEndianCpu)

    if(isBigEndianCpu)
        set(${CPU_ENDIANNESS} "BIG" PARENT_SCOPE)
    else()
        set(${CPU_ENDIANNESS} "LITTLE" PARENT_SCOPE)
    endif()
endfunction()

function(pal_get_system_architecture_bits bits)
    math(EXPR ${bits} "8 * ${CMAKE_SIZEOF_VOID_P}")
    set(${bits} ${${bits}} PARENT_SCOPE)
endfunction()

