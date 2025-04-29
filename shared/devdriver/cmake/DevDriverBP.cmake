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

include_guard(GLOBAL)

# dd_bp stands for "DevDriver Build Parameter"
function(dd_bp AMD_VAR AMD_DFLT)
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
