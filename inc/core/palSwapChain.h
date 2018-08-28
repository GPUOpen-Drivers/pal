/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palSwapChain.h
 * @brief Defines the Platform Abstraction Library (PAL) ISwapChain interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"
#include "palImage.h"
#include "palQueue.h"

namespace Pal
{
// Forward declarations.
class IQueueSemaphore;
class IFence;
class IScreen;

/// Maximum number of format supported by presentable image. @see SwapChainProperties
constexpr uint32 MaxPresentableImageFormat = 16;

/// Swap chain mode which determines how to process and queue incoming present requests.
enum class SwapChainMode : uint32
{
    Immediate   = 0x0, ///< The presentation engine doesn't wait for vsync to display an image.
    Mailbox     = 0x1, ///< The presentation engine waits for vsync to display an image.  A single-entry queue is used
                       ///  to hold pending presentation request.
    Fifo        = 0x2, ///< The presentation engine waits for vsync to display an image.  A multi-entry queue is used
                       ///  to hold pending presentation requests.  It cannot drop presentation requests.
    FifoRelaxed = 0x3, ///< The presentation engine waits for vsync to display an image. If the displayed image was not
                       ///  updated during the last vblank period the presentation engine should not wait for vsync.
    Count
};

/// Defines flags for describing which swap chain modes are supported.
enum SwapChainModeSupport : uint32
{
    SupportImmediateSwapChain   = (1 << static_cast<uint32>(SwapChainMode::Immediate)),
    SupportMailboxSwapChain     = (1 << static_cast<uint32>(SwapChainMode::Mailbox)),
    SupportFifoSwapChain        = (1 << static_cast<uint32>(SwapChainMode::Fifo)),
    SupportFifoRelaxedSwapChain = (1 << static_cast<uint32>(SwapChainMode::FifoRelaxed)),
};

/// Wsi Platform type which determines the window system the swapChain supposed to work upon
enum WsiPlatform : uint32
{
    Win32                       = 0x00000001,  ///< Win32 platform which is the only supported platform for windows OS.
    Xcb                         = 0x00000002,  ///< Xcb platform, which supposed to be run upon DRI3 infrastructure
    Xlib                        = 0x00000004,  ///< Xlib platform, which supposed to be run upon DRI2 infrastructure
    Wayland                     = 0x00000008,  ///< Wayland platform, which is running upon wayland protocol.
    Mir                         = 0x00000010,  ///< Mir platform, which is running upon Mir protocol developed by Canonical
    DirectDisplay               = 0x00000020,  ///< DirectDisplay platform, which can render and present directly to
                                               ///  display without using an intermediate window system.
};

/// Describe the surface transform capability or status.
enum SurfaceTransformFlags : uint32
{
    SurfaceTransformNone            = 0x00000001,   ///< None rotation.
    SurfaceTransformRot90           = 0x00000002,   ///< 90-degree rotation.
    SurfaceTransformRot180          = 0x00000004,   ///< 180-degree rotation.
    SurfaceTransformRot270          = 0x00000008,   ///< 270-degree rotation.
    SurfaceTransformHMirror         = 0x00000010,   ///< Horizontal mirror.
    SurfaceTransformHMirrorRot90    = 0x00000020,   ///< Horizontal mirror and rotate 90 degree.
    SurfaceTransformHMirrorRot180   = 0x00000040,   ///< Horizontal mirror and rotate 180 degree.
    SurfaceTransformHMirrorRot270   = 0x00000080,   ///< Horizontal mirror and rotate 270 degree.
    SurfaceTransformInherit         = 0x00000100,   ///< Client is responsible for setting the transform using native
                                                    ///  window system commands.
};

/// This structure specifies the information needed by client to create swap chain and to present an image. Surface
/// here is an abstraction for a window and a physical output device.
struct SwapChainProperties
{
    uint32                minImageCount;       ///< Supported minimum number of images for the swap chain.
    uint32                maxImageCount;       ///< Supported maximum number of images, 0 for unlimited.
    Extent2d              currentExtent;       ///< Current image width and height for the swap chain.
    Extent2d              minImageExtent;      ///< Supported minimum image width and height for the swap chain.
    Extent2d              maxImageExtent;      ///< Supported maximum image width and height for the swap chain.
    uint32                supportedTransforms; ///< 1 or more bits representing the transforms supported. It should
                                               ///  be a mask of SurfaceTransformFlags.
    SurfaceTransformFlags currentTransforms;   ///< The surface's current transform.
    uint32                maxImageArraySize;   ///< Supported maximum number of image layers for the swap chain.
    ImageUsageFlags       supportedUsageFlags; ///< Supported image usage flags for the swap chain.
    uint32                imageFormatCount;    ///< Supported image format count for the swap chain.

    SwizzledFormat        imageFormat[MaxPresentableImageFormat];  ///< Supported image formats for the swap chain.
};

/// Specifies all the information needed by local window system to present. Input structure to IDevice::CreateSwapChain
struct SwapChainCreateInfo
{
    union
    {
        struct
        {
            uint32 clipped                   :  1; ///< If presentable images may be affected by window clip regions.
            uint32 canAcquireBeforeSignaling :  1; ///< If AcquireNextImage can return before queueing the signals to
                                                   ///  the client's sync objects.  This can improve performance but
                                                   ///  may trigger queue batching on internal and client queues.
            uint32 reserved                  : 31; ///< Reserved for future use.
        };
        uint32 u32All;                         ///< Flags packed as 32-bit uint.
    } flags;                                   ///< Swap chain flags.

    OsDisplayHandle       hDisplay;            ///< Display handle of the local display system.
    OsWindowHandle        hWindow;             ///< Window handle of local display system.
    WsiPlatform           wsiPlatform;         ///< The WSI Platform the swapchain supposed to work on
    uint32                imageCount;          ///< Presentable image count in this swap chain.
    SwizzledFormat        imageSwizzledFormat; ///< Format and channel swizzle of the presentable images.
    Extent2d              imageExtent;         ///< Dimensions of the presentable images.
    ImageUsageFlags       imageUsageFlags;     ///< Indicate how the presentation images will be used.
    SurfaceTransformFlags preTransform;        ///< The transform, relative to the device's natural orientation, applied
                                               ///  to the image content prior to presentation.
    uint32                imageArraySize;      ///< Determines the number of views for multiview/stereo presentation.
    SwapChainMode         swapChainMode;       ///< How to process and queue this swap chain's presentation requests.
    IScreen*              pScreen;             ///< The IScreen object associated with swap chain. It's needed only when
                                               ///  creating a swap chain on DirectDisplay platform, and exclusive
                                               ///  access to the IScreen is required, that is the IScreen needs to call
                                               ///  AcquireScreenAccess before swap chain creation.
};

/// Specifies the properties of acquiring next presentable image. Input structure to ISwapChain::AcquireNextImage
struct AcquireNextImageInfo
{
    uint64           timeout;    ///< How long the function should block, in nanoseconds, if no image is available.
                                 ///  If zero the function will not block. If UINT64_MAX it will block indefinitely.
    IQueueSemaphore* pSemaphore; ///< If non-null, signal this semaphore when it is safe to render into the image.
    IFence*          pFence;     ///< If non-null, signal this fence when it is safe to render into the image.
};

/**
 ***********************************************************************************************************************
 * @interface ISwapChain
 * @brief     An abstraction that manages ownership of an synchronization of an array of presentable images. The array
 *            of presentable images is not needed by the swap chain and must be managed by the client.
 *
 * The client must acquire ownership of a presentable image index from the swap chain and wait on the provided fence
 * or queue semaphore before rendering into the relevant image. Swap chain images should be presented using the
 * IQueue::PresentSwapChain() function because it releases ownership of the presentable image index and triggers
 * necessary swap chain synchronization.
 *
 * @see IDevice::CreateSwapChain()
 ***********************************************************************************************************************
 */
class ISwapChain : public IDestroyable
{
public:
    /// Retrieve the index of the next available presentation image.
    ///
    /// @param [in]  acquireInfo Input information controling this function.
    /// @param [out] pImageIndex Next presentable image which is safe for application to access.
    ///
    /// @returns Success if get next presentable image successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + NotReady if acquireInfo.timeout is zero and no image is available for acquisition.
    ///          + Timeout if acquireInfo.timeout is greater than zero and less than the max vaule, and no image became
    ///            available within the allowed time.
    ///          + ErrorInvalidPointer if pImageIndex is null.
    ///          + ErrorUnknown when an unexpected condition is encountered.
    virtual Result AcquireNextImage(
        const AcquireNextImageInfo& acquireInfo,
        uint32*                     pImageIndex) = 0;

    /// Wait for the Swapchain to be idle.
    ///
    /// @returns Success when all presentable images in the swapchain are idle or safe to be deleted.  Otherwise, one
    ///          of the following errors may be returned:
    ///          + ErrorUnknown when an unexpected condition is encountered.
    virtual Result WaitIdle() = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    ISwapChain() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~ISwapChain() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal

