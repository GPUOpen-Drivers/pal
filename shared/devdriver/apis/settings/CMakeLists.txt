##
 #######################################################################################################################
 #
 #  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

add_subdirectory(rpc/g_service)

devdriver_library(dd_settings STATIC)
target_sources(dd_settings
    PRIVATE
        inc/dd_settings_base.h
        inc/dd_settings_blob.h
        inc/dd_settings_iterator.h
        inc/dd_settings_rpc_types.h
        inc/dd_settings_service.h
        src/dd_settings_base.cpp
        src/dd_settings_blob.cpp
        src/dd_settings_iterator.cpp
        src/dd_settings_service.cpp
)
target_include_directories(dd_settings PUBLIC inc)
target_link_libraries(dd_settings
    PUBLIC
        dd_common
        dd_interface
        devdriver
        SettingsRpcService2
    PRIVATE
        ddCommon
)

