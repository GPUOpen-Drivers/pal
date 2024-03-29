##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

devdriver_library(ddSettings STATIC)

add_subdirectory(rpcClient)
add_subdirectory(rpcService)
add_subdirectory(settingsblob)

target_link_libraries(ddSettings
    PUBLIC
        ddApi
    PRIVATE
        devdriver
        ddCommon
        ddYaml
        SettingsRpcClient
)

target_include_directories(ddSettings
    PUBLIC
        base/inc
        # For "reasons" we need to include dd_settings_api.h which in turn includes dd_allocator_api.h. This gives
        # us access to dd_allocator_api.h. This hack and the entire ddSettings will be removed once dd_settings
        # is ready.
        ../../apis/common/inc
)

target_sources(ddSettings PRIVATE
    base/inc/ddSettingsTypes.h
    base/inc/ddSettingsBase.h
    base/inc/ddSettingsUserOverrides.h
    base/inc/ddSettingsUtils.h
    base/src/ddSettingsBase.cpp
    base/src/ddSettingsUserOverrides.cpp
)
