/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef INCLUDE_VK_FORMATS
#define INCLUDE_VK_VORMATS 0
#endif
#ifndef INCLUDE_WL_DRM_FORMATS
#define INCLUDE_WL_DRM_VORMATS 0
#endif
#ifndef INCLUDE_PAL_FORMATS
#define INCLUDE_PAL_VORMATS 0
#endif

#include "wsa.h"

#if (INCLUDE_WL_DRM_FORMATS)
#include "wayland-drm-client-protocol.h"
#define WL_DRM_FORMAT_UNDEFINED 0

typedef struct {
    WsaFormat            wsaFormat;
    WsaCompositeAlpha    wsaCompositeAlpha;
    uint32_t             wlDrmFormat;
} FormatMatch;

#define MATCH_ROW(wsaFormat, wsaComp, wlDrmFormat, vkFormat, vkComp, palChNum, palR, palG, palB, palA) \
   {                                                                                                   \
        WsaFormat##wsaFormat,                                                                          \
        WsaCompositeAlpha##wsaComp,                                                                    \
        WL_DRM_FORMAT_##wlDrmFormat,                                                                   \
    },

#elif (INCLUDE_VK_FORMATS)
#include "vulkan/vulkan_core.h"

typedef struct {
    WsaFormat            wsaFormat;
    WsaCompositeAlpha    wsaCompositeAlpha;
    VkFormat             vkFormat;
    VkCompositeAlphaFlagBitsKHR vkCompositeAlpha;
} FormatMatch;

#define MATCH_ROW(wsaFormat, wsaComp, wlDrmFormat, vkFormat, vkComp, palChNum, palR, palG, palB, palA) \
   {                                                                                                   \
        WsaFormat##wsaFormat,                                                                          \
        WsaCompositeAlpha##wsaComp,                                                                    \
        VK_FORMAT_##vkFormat,                                                                          \
        VK_COMPOSITE_ALPHA_##vkComp,                                                                   \
    },

#elif (INCLUDE_PAL_FORMATS)
#include "pal.h"

typedef struct {
    WsaFormat            wsaFormat;
    WsaCompositeAlpha    wsaCompositeAlpha;
    Pal::SwizzledFormat  palFormat;
} FormatMatch;

#define MATCH_ROW(wsaFormat, wsaComp, wlDrmFormat, vkFormat, vkComp, palChNum, palR, palG, palB, palA) \
   {                                                                                                   \
        WsaFormat##wsaFormat,                                                                          \
        WsaCompositeAlpha##wsaComp,                                                                    \
        {                                                                                              \
            Pal::ChNumFormat::palChNum,                                                                \
            { Pal::ChannelSwizzle::palR, Pal::ChannelSwizzle::palG,                                    \
              Pal::ChannelSwizzle::palB, Pal::ChannelSwizzle::palA },                                  \
        },                                                                                             \
    },

#endif

// Use the same wayland-drm format for both sRGB and Unorm, because wayland-drm can not distinguish them right now

const FormatMatch presentableFormats[] = {
// wayland-drm format WL_DRM_FORMAT_XRGB8888 that suppresses alpha when compositing
MATCH_ROW(B8G8R8A8Unorm,  Opaque,
          XRGB8888,
          B8G8R8A8_UNORM, OPAQUE_BIT_KH,
          X8Y8Z8W8_Unorm, Z, Y, X, W)
MATCH_ROW(B8G8R8A8Srgb,   Opaque,
          XRGB8888,
          B8G8R8A8_SRGB,  OPAQUE_BIT_KH,
          X8Y8Z8W8_Srgb,  Z, Y, X, W)
// wayland-drm format WL_DRM_FORMAT_ARGB8888 that premultiplies alpha when compositing
MATCH_ROW(B8G8R8A8Unorm,  PreMultiplied,
          ARGB8888,
          B8G8R8A8_UNORM, PRE_MULTIPLIED_BIT_KH,
          X8Y8Z8W8_Unorm, Z, Y, X, W)
MATCH_ROW(B8G8R8A8Srgb,   PreMultiplied,
          ARGB8888,
          B8G8R8A8_SRGB,  PRE_MULTIPLIED_BIT_KH,
          X8Y8Z8W8_Srgb,  Z, Y, X, W)
// wayland-drm format WL_DRM_FORMAT_XBGR8888 that suppresses alpha when compositing
// Wayland does not report this format as available on AMDGPU hardware right now
MATCH_ROW(R8G8B8A8Unorm,  Opaque,
          XBGR8888,
          R8G8B8A8_UNORM, OPAQUE_BIT_KH,
          X8Y8Z8W8_Unorm, X, Y, Z, W)
MATCH_ROW(R8G8B8A8Srgb,   Opaque,
          XBGR8888,
          R8G8B8A8_SRGB,  OPAQUE_BIT_KH,
          X8Y8Z8W8_Srgb,  X, Y, Z, W)
// wayland-drm format WL_DRM_FORMAT_ABGR8888 that premultiplies alpha when compositing
// Wayland does not report this format as available on AMDGPU hardware right now
MATCH_ROW(R8G8B8A8Unorm, PreMultiplied,
          ABGR8888,
          R8G8B8A8_UNORM, PRE_MULTIPLIED_BIT_KH,
          X8Y8Z8W8_Unorm, X, Y, Z, W)
MATCH_ROW(R8G8B8A8Srgb,   PreMultiplied,
          ABGR8888,
          R8G8B8A8_SRGB,  PRE_MULTIPLIED_BIT_KH,
          X8Y8Z8W8_Srgb,  X, Y, Z, W)
// wayland-drm format WL_DRM_FORMAT_XRGB2101010 that suppresses alpha when compositing
MATCH_ROW(A2R10G10B10UnormPack32,   Opaque,
          XRGB2101010,
          A2R10G10B10_UNORM_PACK32, OPAQUE_BIT_KH,
          X10Y10Z10W2_Unorm, Z, Y, X, W)
// wayland-drm format WL_DRM_FORMAT_ARGB2101010 that premultiplies alpha when compositing
MATCH_ROW(A2R10G10B10UnormPack32, PreMultiplied,
          ARGB2101010,
          A2R10G10B10_UNORM_PACK32, PRE_MULTIPLIED_BIT_KH,
          X10Y10Z10W2_Unorm, Z, Y, X, W)
// wayland-drm format WL_DRM_FORMAT_XBGR2101010 that suppresses alpha when compositing
// Wayland does not report this format as available on AMDGPU hardware right now
MATCH_ROW(A2B10G10R10UnormPack32,   Opaque,
          XBGR2101010,
          A2B10G10R10_UNORM_PACK32, OPAQUE_BIT_KH,
          X10Y10Z10W2_Unorm, X, Y, Z, W)
// wayland-drm format WL_DRM_FORMAT_ARGB2101010 that premultiplies alpha when compositing
// Wayland does not report this format as available on AMDGPU hardware right now
MATCH_ROW(A2B10G10R10UnormPack32,   PreMultiplied,
          ABGR2101010,
          A2B10G10R10_UNORM_PACK32, PRE_MULTIPLIED_BIT_KH,
          X10Y10Z10W2_Unorm, X, Y, Z, W)
// wayland-drm format WL_DRM_FORMAT_RGB565
MATCH_ROW(R5G6B5UnormPack16,   Opaque,
          RGB565,
          R5G6B5_UNORM_PACK16, OPAQUE_BIT_KH,
          X5Y6Z5_Unorm, Z, Y, X, One)
// Wayland has no float support right now
// These 2 rows cover future expansion of the protocol to match capabilities of pal and vulkan
MATCH_ROW(R16G16B16A16SFloat,  Opaque,
          UNDEFINED,
          R16G16B16A16_SFLOAT, OPAQUE_BIT_KH,
          X16Y16Z16W16_Float, X, Y, Z, W)
MATCH_ROW(R16G16B16A16SFloat,  PreMultiplied,
          UNDEFINED,
          R16G16B16A16_SFLOAT, PRE_MULTIPLIED_BIT_KH,
          X16Y16Z16W16_Float, X, Y, Z, W)
};
