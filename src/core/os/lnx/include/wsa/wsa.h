/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

/*
***********************************************************************************************************************
*
* Window system agent(WSA): encapsulate window system data and protocol to keep icd driver not involved in the details
* of the native window system. the window system agent should only be loaded when the corresponding window system is
* used in the icd driver
*
***********************************************************************************************************************
*/

#pragma once

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#define WSA_INTERFACE_VER 1

#ifdef DEBUG
    #define WSA_ASSERT(str) assert(str)
#else
    #define WSA_ASSERT(str)
#endif

typedef unsigned int           uint32;
typedef int                    int32;
typedef unsigned long long int uint64;
typedef long long int          int64;

//wsa error type.
typedef enum wsaError
{
    Success               = 0,
    NotEnoughResource     = 1,
    ResourceBusy          = 2,
    UnknownFailure        = 3,
}WsaError;

// Method used to composite an alpha format swapchain image onto the display
typedef enum wsaCompositeAlpha
{
    WsaCompositeAlphaOpaque = 1,
    WsaCompositeAlphaPreMultiplied = 2,
    WsaCompositeAlphaPostMultiplied = 4,
    WsaCompositeAlphaInherit = 8,
}WsaCompositeAlpha;

// Pal presentable image formats, using names from vulkan_core.h in CamelCase
typedef enum wsaFormat
{
    WsaFormatR5G6B5UnormPack16,
    WsaFormatB8G8R8A8Unorm,
    WsaFormatR8G8B8A8Unorm,
    WsaFormatA2R10G10B10UnormPack32,
    WsaFormatA2B10G10R10UnormPack32,
    WsaFormatB8G8R8A8Srgb,
    WsaFormatR8G8B8A8Srgb,
    WsaFormatR16G16B16A16SFloat,
}WsaFormat;

// region
typedef struct wsaRegion
{
    int32      x;
    int32      y;
    int32      width;
    int32      height;
}WsaRegion;

// region list
typedef struct wsaRegionList
{
    int32      count;
    WsaRegion* regions;
}WsaRegionList;

// Query wsa interface version
typedef uint32 (*QueryVersion)(void);

// Create a window system agent(WSA).
// Wsa handle is returned by hWsa.
typedef WsaError (*CreateWsa)(int32* pWsa);

// Initialize window system agent.
typedef WsaError (*Initialize)(int32 hWsa,
                               WsaFormat format,
                               WsaCompositeAlpha compositeAlpha,
                               void* pDisplay,
                               void* pSurface);

// Destroy WSA.
typedef void (*DestroyWsa)(int32 hWsa);

// Create a presentable image.
// Image handle is returned by pImage.
typedef WsaError (*CreateImage)(int32 hWsa,
                                int32 fd,
                                uint32 width,
                                uint32 height,
                                WsaFormat format,
                                uint32 stride,
                                int32* pImage);

// Destroy an image.
typedef void (*DestroyImage)(int32 hImage);

// Present.
typedef WsaError (*Present)(int32 hWsa, int32 hImage, WsaRegionList* presentRegions);

// Return when the last image has been presented.
typedef WsaError (*WaitForLastImagePresented)(int32 hWsa);

// Check whether the image is avaiable (not used by the server side).
typedef WsaError (*ImageAvailable)(int32 hWsa, int32 hImage);

// Get GPU number (minor type of primary node)
typedef uint32 (*GetGpuNumber)(int32 hWsa);

// Get window size, helper function, don't need an instance.
typedef WsaError (*GetWindowGeometry)(void* pDisplay, void* pSurface, uint32* pWidth, uint32* pHeight);

// Check whether the presentation is supported, helper function, don't need an instance.
typedef WsaError (*PresentationSupported)(void* pDisplay, void* pData);


typedef struct wsaInterface
{
    QueryVersion                pfnQueryVersion;
    CreateWsa                   pfnCreateWsa;
    Initialize                  pfnInitialize;
    DestroyWsa                  pfnDestroyWsa;
    CreateImage                 pfnCreateImage;
    DestroyImage                pfnDestroyImage;
    Present                     pfnPresent;
    WaitForLastImagePresented   pfnWaitForLastImagePresented;
    ImageAvailable              pfnImageAvailable;
    GetGpuNumber                pfnGetGpuNumber;
    GetWindowGeometry           pfnGetWindowGeometry;
    PresentationSupported       pfnPresentationSupported;
}WsaInterface;
