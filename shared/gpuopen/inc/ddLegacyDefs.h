/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#ifndef DD_PLATFORM_WINDOWS_UM
    #if _WIN32 && !_KERNEL_MODE
        #define DD_PLATFORM_WINDOWS_UM 1
        #define DD_PLATFORM_IS_UM      1
    #endif
#endif

    #if _WIN32 && _KERNEL_MODE
        #define DD_PLATFORM_WINDOWS_KM 1
        #define DD_PLATFORM_IS_KM      1
    #endif

#ifndef DD_PLATFORM_LINUX_UM
    #ifdef __linux__
        #define DD_PLATFORM_LINUX_UM 1
        #define DD_PLATFORM_IS_UM    1
        #define DD_PLATFORM_IS_GNU   1
    #endif
#endif
