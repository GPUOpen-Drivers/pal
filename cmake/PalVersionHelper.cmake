##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Helper wrapper around pal_set_or which allows multiple CMake variables to be OR'd from a single SOURCE.
# This function accepts multiple arguments, so long as the first argument is the SOURCE variable.
function(pal_propagate_internal_or SOURCE)
    # If our source variable for propagation is not defined, we don't need to propagate at all.
    if (NOT DEFINED ${SOURCE})
        return()
    endif()

    # For every destination specified (as arguments after SOURCE):
    #     - Call pal_set_or to perform _ARG |= SOURCE
    #     - If defined, forward the result of that to the caller.
    foreach(_ARG IN LISTS ARGN)
        pal_set_or(${_ARG} ${${SOURCE}})
        if (DEFINED ${_ARG})
            set(${_ARG} ${${_ARG}} PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Internal helper macro which is used to support the pal_*_bp functions.
# Input to this macro:
#     - VAR_NAME (required)
#           The name of the CMake variable to set.
#     - VAR_DFLT (required)
#           The default value to set the CMake variable if it has not yet been set.
#     - VAR_VERSION
#           Requires _DEPRECATION=ON, and is used to determine when this build parameter should
#           be set by this function. If VAR_VERSION equates to false, this build parameter remains unset.
#     - REQUIRED
#           Indicates that this build parameter must be set prior to the pal_*_bp function being called.
#           Usually used for client build parameters that they **must** set prior to adding PAL.
#     - MODE [mode]
#           Indicates the message mode type: FATAL_ERROR, SEND_ERROR, WARNING, AUTHOR_WARNING,
#           DEPRECATION, NOTICE, STATUS, VERBOSE, DEBUG, or TRACE.
#     - DEPENDS_ON [ dependency0, dependency1, ..., dependencyN ]
#           A list of evaluateable CMake expressions.
#           If any single one of these dependencies evaluates to false, the build parameter in VAR_NAME
#           is forced OFF (opposite of FOR_ANY, below).
#     - FOR_ANY [ dependency0, dependency1, ..., dependencyN ]
#           A list of evaluateable CMake expressions.
#           If any single one of these dependencies evaluates to true, the build parameter in VAR_NAME
#           is forced ON (opposite of DEPENDS_ON, above).
#     - DESC [ description0, description1, ..., descriptionN ]
#           A list of strings describing what this VAR_NAME does.
#           This is a list so that CMake can concatenate the strings into a single line and proper indent it.
#
# Variables that can be set prior to this macro being called:
#     - _CLIENT
#           Set this to TRUE/ON to indicate the build parameter is to be used for client override purposes.
#           Clients are expected to either leave this unset (and let PAL pick some default) or to set it
#           [via a normal set(...)] to control PAL enablement.
#           Client build parameters can be later queried by the client via:
#               get_target_property(<var> pal <build-parameter>)
#     - _INTERNAL
#           Set this to TRUE/ON to indicate the build parameter is to be used for internal-to-PAL purposes.
#           Clients are not expected to have any control for these build parameters, and only CACHE versions
#           of the build parameter variable can override the defaults (other than DEPENDS_ON or IF_ANY).
#     - _DEPRECATED
#           Set this to TRUE/ON to indicate the client-facing build parameter is being deprecated. If a client
#           has overridden this value, a warning will inform them to desist.
#           Once the deprecation condition (set in VAR_VERSION) is no longer evaluates to true, the build parameter
#           will no longer be defined.
macro(__pal_bp)
    set(options       REQUIRED)
    set(single_values MODE)
    set(multi_values  DEPENDS_ON FOR_ANY DESC)
    cmake_parse_arguments(PARSE_ARGV 0 "" "${options}" "${single_values}" "${multi_values}")

    # All unparsed arguments go into {prefix_}UNPARSED_ARGUMENTS.
    if (DEFINED _UNPARSED_ARGUMENTS)
        # Because we're in a macro with no arguments, we are going to remove the values we
        # expect from the various pal_*_bp functions.
        list(REMOVE_ITEM _UNPARSED_ARGUMENTS ${VAR_NAME})
        list(REMOVE_ITEM _UNPARSED_ARGUMENTS ${VAR_DFLT})
        if (DEFINED VAR_VERSION)
            list(REMOVE_ITEM _UNPARSED_ARGUMENTS ${VAR_VERSION})
        endif()

        # If we still have items in this list, error out so they can be fixed.
        list(LENGTH _UNPARSED_ARGUMENTS _UNPARSED_ARGUMENTS_LEN)
        if (_UNPARSED_ARGUMENTS_LEN GREATER 0)
            message(FATAL_ERROR "pal_*_bp called with an unexpected argument(s)! : ${_UNPARSED_ARGUMENTS}")
        endif()
    endif()

    # If a MODE is not defined, default to STATUS.
    if (NOT DEFINED _MODE OR NOT _MODE)
        set(_MODE "STATUS")
    endif()

    # If no description was specified, default to an empty string to avoid handling special case later.
    if (NOT DEFINED _DESC OR NOT _DESC)
        set(_DESC "")
    endif()

    # Join a potential multi-line description into one longer line.
    string(JOIN " " _DESC ${_DESC})

    # If this is an internal build parameter, we want to look for the variable in the CACHE
    # and ignore anything set elsewhere.
    if (_INTERNAL)
        # In CMake, DEFINED searches for a variable definition in this order:
        #     1. Local function scope
        #     2. CACHE scope
        #
        # What this means is that it is impossible to tell if a variable is defined only in the local function scope.
        # However, we can check to see if the value in VAR_NAME is equivalent to the same value in the CACHED version
        # of VAR_NAME. If they are not equivalent, then a local scope version exists and we want to warn about that.
        if (DEFINED ${VAR_NAME} AND NOT "${${VAR_NAME}}" STREQUAL "$CACHE{${VAR_NAME}}")
            message(WARNING "${VAR_NAME} is an internal build parameter. "
                            "As such, the local CMake variable is ignored in favor of a command line or CACHE version.")
        endif()

        # For any internal build parameters, we want to unset the current value in this function and unset
        # the value in the parent scope.
        # By doing this, we guarantee that later calls to `DEFINED ${VAR_NAME}` are only true if it is defined
        # in the CACHE.
        # We'll set the proper value later.
        unset(${VAR_NAME})
        unset(${VAR_NAME} PARENT_SCOPE)
    endif()

    if (_DEPRECATED)
        # CMake can evaluate its code, which allows us to check the deprecation case.
        cmake_language(EVAL CODE "
            if (DEFINED ${VAR_NAME})
                message(WARNING \"${VAR_NAME} has been deprecated. Please work to remove your override.\")
                message(STATUS \"${VAR_NAME} overridden to ${${VAR_NAME}} (deprecated)\")
                if (${VAR_VERSION})
                    set(_EXIT_EARLY ON)
                endif()
            endif()
            if (NOT (${VAR_VERSION}))
                set(_EXIT_EARLY ON)
            endif()
        ")

        # If:
        #   - our deprecated variable was either overridden by the client and the deprecation condition is true, or
        #   - our deprecation condition is false,
        # Then we've handled this deprecated variable and no further processing is necessary.
        #   Note: This means that the deprecated variable can leave this function being undefined, which is fine!
        if (_EXIT_EARLY)
            # If a client build parameter, we need to set it as a target property as well.
            if (_CLIENT AND DEFINED ${VAR_NAME})
                set_target_properties(pal PROPERTIES ${VAR_NAME} ${${VAR_NAME}})
            endif()

            # Note: return() is generally unsafe in macros... unless that macro is called from a function.
            return()
        endif()
    endif()

    # If the user specified any dependencies, we need to walk through them all.
    # If any single dependency is OFF/FALSE/UNDEFINED, then VAR_NAME will be set to OFF.
    foreach(d ${_DEPENDS_ON})
        cmake_language(EVAL CODE "
            if (${d})
                set(_INVALID_DEP FALSE)
            else()
                set(_INVALID_DEP TRUE)
            endif()
        ")
        if (_INVALID_DEP)
            set(${VAR_NAME} OFF PARENT_SCOPE)
            message(${_MODE} "${VAR_NAME} dependency (${d}) failed. Defaulting to OFF. ${_DESC}")

            # If a client build parameter, we need to set it as a target property as well.
            if (_CLIENT)
                set_target_properties(pal PROPERTIES ${VAR_NAME} OFF)
            endif()

            # Note: return() is generally unsafe in macros... unless that macro is called from a function.
            return()
        endif()
    endforeach()

    # If the user specified any "for-any" dependencies, we need to walk through them all.
    # If any single "for-any" dependency is ON or TRUE, then VAR_NAME will be set to ON.
    # However, if this is an internal setting, we want to allow cache variable overrides.
    foreach(d ${_FOR_ANY})
        cmake_language(EVAL CODE "
            if (${d})
                set(_VALID_DEP TRUE)
            else()
                set(_VALID_DEP FALSE)
            endif()

            if (_INTERNAL AND DEFINED CACHE{${VAR_NAME}})
                set(_VALID_DEP FALSE)
            endif()
        ")
        if (_VALID_DEP)
            set(${VAR_NAME} ON PARENT_SCOPE)
            message(${_MODE} "${VAR_NAME} 'for-any' dependency (${d}) met. Defaulting to ON. ${_DESC}")

            # If a client build parameter, we need to set it as a target property as well.
            if (_CLIENT)
                set_target_properties(pal PROPERTIES ${VAR_NAME} ON)
            endif()

            # Note: return() is generally unsafe in macros... unless that macro is called from a function.
            return()
        endif()
    endforeach()

    # If we've reached this point, we've successfully passed all dependencies.

    # If the user has requested that we force BUILD settings on, and
    # the variable has not already been defined, then force the setting on.
    #   Note: We're explicitly handling this *before* dependencies are checked, as this setting
    #         expects to force support for things.
    if (PAL_BUILD_FORCE_ON AND NOT DEFINED ${VAR_NAME})
        # We only want to match BuildParameters (PAL_BUILD*).
        string(REGEX MATCH "(PAL_BUILD).*" _MATCHED ${VAR_NAME})
        if (_MATCHED)
            set(${VAR_NAME} ON PARENT_SCOPE)
            message(STATUS "${VAR_NAME} forced ON by PAL_BUILD_FORCE_ON. ${_DESC}")

            # If a client build parameter, we need to set it as a target property as well.
            if (_CLIENT)
                set_target_properties(pal PROPERTIES ${VAR_NAME} ON)
            endif()

            # Note: return() is generally unsafe in macros... unless that macro is called from a function.
            return()
        endif()
    endif()

    # Now, we need to check to see if the variable has already been defined.
    #   Note: DEFINED here will first check the local scope [set(...)], and if it cannot find the variable there,
    #         will then check the cache scope [command line or set(... CACHE)].
    # If the client has already defined this variable, their value should be honored.
    if (NOT DEFINED ${VAR_NAME})
        if (_REQUIRED)
            message(FATAL_ERROR "${VAR_NAME} not set and required.")
        else()
            set(${VAR_NAME} ${VAR_DFLT} PARENT_SCOPE)
            message(STATUS "${VAR_NAME} not set. Defaulting to ${VAR_DFLT}. ${_DESC}")

            # If a client build parameter, we need to set it as a target property as well.
            if (_CLIENT)
                set_target_properties(pal PROPERTIES ${VAR_NAME} ${VAR_DFLT})
            endif()
        endif()
        return()
    endif()

    if (NOT _DEPRECATED)
        # If we've reached this point, the setting has been overridden by the command line or a client.
        # We have no need to set it (it's already defined), so just print out the value.
        message(STATUS "${VAR_NAME} overridden ${${VAR_NAME}}.")

        # If a client build parameter, we need to set it as a target property as well.
        if (_CLIENT)
            set_target_properties(pal PROPERTIES ${VAR_NAME} ${${VAR_NAME}})
        endif()
    endif()
endmacro()

# Function to specify CMake variables that are available to the client to override.
# Should be exclusively used in PalBuildParameters.cmake.
# SEE: __pal_bp() definition above for an explanation of the input arguments.
function(pal_client_bp VAR_NAME VAR_DFLT)
    set(_CLIENT TRUE)
    __pal_bp()
endfunction()

# Function to specify CMake variables that were once available to the client to override, but are now
# deprecated. Clients should avoid setting these variables in the future.
# Should be exclusively used in PalBuildParameters.cmake.
# SEE: __pal_bp() definition above for an explanation of the input arguments.
function(pal_deprecated_bp VAR_NAME VAR_DFLT VAR_VERSION)
    set(_CLIENT TRUE)
    set(_DEPRECATED TRUE)
    __pal_bp()
endfunction()

# Function to specify CMake variables that are for use exclusively to PAL, and cannot be overridden or
# controlled by clients.
# Should be exclusively used in PalOnlyBuildParameters.cmake.
# SEE: __pal_bp() definition above for an explanation of the input arguments.
function(pal_internal_bp VAR_NAME VAR_DFLT)
    set(_INTERNAL TRUE)
    __pal_bp()
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

