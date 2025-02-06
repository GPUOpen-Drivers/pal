/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palDepthStencilView.h
 * @brief Defines the Platform Abstraction Library (PAL) IDepthStencilView interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"

namespace Pal
{

// Forward declarations.
class IImage;

/// Specifies properties for @ref IDepthStencilView creation.  Input structure to IDevice::CreateDepthStencilView().
struct DepthStencilViewCreateInfo
{
    union
    {
        struct
        {
            uint32 readOnlyDepth           :  1; ///< Disable writes to the depth plane.
            uint32 readOnlyStencil         :  1; ///< Disable writes to the stencil plane.
            uint32 imageVaLocked           :  1; ///< Whether or not the image's virtual address range is locked and
                                                 ///  never changes.
            uint32 absoluteDepthBias       :  1; ///< Whether or not use absolute depth bias.
                                                 ///  Absolute depth bias: depth bias will be added to z value directly.
                                                 ///  Scaled depth bias: before adding to z value, depth bias will be
                                                 ///  multiplied to minimum representable z value.
            uint32 useHwFmtforDepthOffset  :  1; ///< Exlusively use Hw format for programming depth offset
                                                 ///  In practice setting this to true ignores depthAsZ24 but only in
                                                 ///  regards to depth offset functionality
            uint32 bypassMall              :  1; ///< Set to have this surface bypass the MALL. If zero, this surface
                                                 ///  obeys the GpuMemMallPolicy specified at memory allocation time.
                                                 ///  Meaningful only if supportsMall is set in DeviceProperties.
            uint32 depthOnlyView           :  1; ///< If set, this is a depth-only view of the specified Image.
                                                 ///  It's illegal to use this flag on an Image with no depth plane.
                                                 ///  It's illegal to set both depthOnlyView and stencilOnlyView.
                                                 ///  It's illegal to use this flag if the stencil test is enabled in
                                                 ///  the @ref IDepthStencilState, @see DepthStencilStateCreateInfo.
            uint32 stencilOnlyView         :  1; ///< If set, this is a stencil-only view of the specified Image.
                                                 ///  It's illegal to use this flag on an Image with no stencil plane.
                                                 ///  It's illegal to set both depthOnlyView and stencilOnlyView.
                                                 ///  It's illegal to use this flag if the depth test is enabled in
                                                 ///  the @ref IDepthStencilState, @see DepthStencilStateCreateInfo.
            uint32 resummarizeHiZ          :  1; ///< Enables resummarizing Hi-Z for DB tiles touched by drawing with
                                                 ///  this view. This has no effect if the source Image does not have
                                                 ///  depth compression or if the @ref readOnlyDepth flag is set.
            uint32 lowZplanePolyOffsetBits :  1; ///< If set, use decreased precision for Z_16/Z_24 formats.
            uint32 reserved                : 22; ///< Reserved for future use.
        };
        uint32 u32All;            ///< Flags packed as 32-bit uint.
    } flags;                      ///< Depth/stencil view creation flags.

    const IImage* pImage;         ///< Image associated with the view.
    uint32        mipLevel;       ///< Mip level to be rendered with this view.
    uint32        baseArraySlice; ///< First array slice in the view.
    uint32        arraySize;      ///< Number of slices in the view.

};

/**
 ***********************************************************************************************************************
 * @interface IDepthStencilView
 * @brief     View of an image resource used for depth/stencil rendering.
 *
 * @warning IDepthStencilView does not inherit the IDestroyable interface.  PAL guarantees that no cleanup actions need
 *          to be taken for this object.  Clients should simply free the system memory allocated for this object, and
 *          never need to explicitly destroy this object.  This is a requirement for DX12, which manages depth/stencil
 *          views as a special type of descriptor, and therefore never get a chance to destroy a corresponding PAL
 *          object.
 *
 * @see IDevice::CreateDepthStenciliew()
 ***********************************************************************************************************************
 */
class IDepthStencilView
{
public:

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IDepthStencilView() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.
    virtual ~IDepthStencilView() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
