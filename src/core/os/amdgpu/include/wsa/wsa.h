/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

// image format type, values are from wayland-drm-client-protocol.h.
typedef enum wsaFormat
{
    WsaFormatARGB8888     = 0x34325241, //WL_DRM_FORMAT_ARGB8888
    WsaFormatXRGB8888     = 0x34325258, //WL_DRM_FORMAT_XRGB8888
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
typedef WsaError (*Initialize)(int32 hWsa, void* pDisplay, void* pSurface);

// Destroy WSA.
typedef void (*DestroyWsa)(int32 hWsa);

// Create a presentable image.
// Image handle is returned by pImage.
typedef WsaError (*CreateImage)(int32 hWsa, int32 fd, uint32 width, uint32 height, WsaFormat format, uint32 stride, int32* pImage);

// Destroy an image.
typedef void (*DestroyImage)(int32 hImage);

// Present.
typedef WsaError (*Present)(int32 hWsa, int32 hImage, WsaRegionList* presentRegions);

// Return when the last image has been presented.
typedef WsaError (*WaitForLastImagePresented)(int32 hWsa);

// Check whether the image is avaiable (not used by the server side).
typedef WsaError (*ImageAvailable)(int32 hWsa, int32 hImage);

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
    GetWindowGeometry           pfnGetWindowGeometry;
    PresentationSupported       pfnPresentationSupported;
}WsaInterface;

