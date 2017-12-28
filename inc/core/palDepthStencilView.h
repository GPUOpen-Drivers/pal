/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
            uint32 readOnlyDepth     :  1;  ///< Disable writes to the depth aspect.
            uint32 readOnlyStencil   :  1;  ///< Disable writes to the stencil aspect.
            uint32 imageVaLocked     :  1;  ///< Whether or not the image's virtual address range is locked and
                                            ///  never changes.
            uint32 absoluteDepthBias :  1;  ///< Whether or not use absolute depth bias.
                                            ///  Absolute depth bias: depth bias will be added to z value directly.
                                            ///  Scaled depth bias: before adding to z value, depth bias will be
                                            ///  multiplied to minimum representable z value.
            uint32 reserved          : 28;  ///< Reserved for future use.
        };
        uint32 u32All;                    ///< Flags packed as 32-bit uint.
    } flags;                              ///< Depth/stencil view creation flags.

    const IImage* pImage;                 ///< Image associated with the view.
    uint32        mipLevel;               ///< Mip level to be rendered with this view.
    uint32        baseArraySlice;         ///< First array slice in the view.
    uint32        arraySize;              ///< Number of slices in the view.
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
