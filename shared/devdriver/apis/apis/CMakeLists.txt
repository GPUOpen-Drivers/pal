##
 #######################################################################################################################
 #
 #  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

devdriver_library(dd_interface INTERFACE)

target_sources(dd_interface
    INTERFACE
        dd_allocator_api.h
        dd_api_registry_api.h
        dd_clocks_api.h
        dd_common_api.h
        dd_connection_api.h
        dd_driver_utils_api.h
        dd_enhanced_crash_info_api.h
        dd_event_logging_api.h
        dd_gpu_detective_api.h
        dd_gpu_profiling_api.h
        dd_logger_api.h
        dd_memory_trace_api.h
        dd_modules_api.h
        dd_pipelines_api.h
        dd_router_utils_api.h
        dd_settings_api.h
        dd_tool_api.h
        dd_uber_trace_api.h)

target_include_directories(dd_interface INTERFACE .)
