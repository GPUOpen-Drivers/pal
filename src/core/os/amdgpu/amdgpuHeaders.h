/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#pragma once

// NOTE: Some of the Linux driver stack's headers don't wrap their C-style interface names in 'extern "C" { ... }'
// blocks when building with a C++ compiler, so we need to add that ourselves.
#if __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <amdgpu_shared.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#define CIASICIDGFXENGINE_UNKNOWN        0x00000000
#define CIASICIDGFXENGINE_SOUTHERNISLAND 0x0000000A
#define CIASICIDGFXENGINE_ARCTICISLAND   0x0000000D
#define MAX_MULTIVPU_ADAPTERS            4

#if __cplusplus
} // extern "C"
#endif

// NOTE: DRM refuses to define this helper type for AMD Vulkan usage.
typedef uint32_t  amdgpu_syncobj_handle;

// Maximum length of an LDA chain.
constexpr uint32_t MaxLdaChainLength = MAX_MULTIVPU_ADAPTERS;
