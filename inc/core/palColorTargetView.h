/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palColorTargetView.h
 * @brief Defines the Platform Abstraction Library (PAL) IColorTargetView interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "palImage.h"

namespace Pal
{

// Forward declarations.
class IGpuMemory;
class IImage;

/// Specifies properties for @ref IColorTargetView creation.  Input structure to IDevice::CreateColorTargetView().
/// Color target views can be image views or buffer views; the client must set isBufferView appropriately and fill out
/// either ColorTargetViewCreateInfo::imageInfo or ColorTargetViewCreateInfo::bufferInfo.
struct ColorTargetViewCreateInfo
{
    SwizzledFormat swizzledFormat;   ///< Color target view format and swizzle.
    Range          zRange;           ///< Specifies the z offset and z range for 3D images.
    union
    {
        struct
        {
            uint32 isBufferView  :  1;  ///< Indicates that this is a buffer view instead of an image view.
            uint32 imageVaLocked :  1;  ///< Whether or not the image's virtual address range is locked and never
                                        ///< changes.  It is ignored by buffer views because their address can't change.
            uint32 zRangeValid   :  1;  ///< whether z offset/ range value is valid.
            uint32 bypassMall    :  1;  ///< Set to have this surface bypass the MALL.  If zero, then this surface
                                        ///  obeys the  GpuMemMallPolicy specified at memory allocation time.
                                        ///  Meaningful only on GPUs that have supportsMall set in DeviceProperties.
            uint32 reserved      : 28;  ///< Reserved for future use
        };
        uint32 u32All;                  ///< Flags packed as 32-bit uint.
    } flags;                            ///< Color target view creation flags.

    union
    {
        struct
        {
            const IImage* pImage;          ///< Image associated with the view.
            SubresId      baseSubRes;      ///< Defines the base subresource to be associated with the view.  Most
                                           ///  views will always use the Color plane, except YUV Images.  Clients
                                           ///  must specify one of the YUV Image planes for YUV Images.  The view
                                           ///  format must be compatible with the plane being rendered-to.
                                           ///  The arraySlice must be 0 for 3D images.
            uint32        arraySize;       ///< Number of slices in the view.  Must be one for 3D images.
        } imageInfo;                       ///< Information that describes a color target image view.

        struct
        {
            const IGpuMemory* pGpuMemory;  ///< GPU memory, interpreted as a buffer, associated with the view.
            uint32            offset;      ///< The offset of the view within the buffer, in units of pixels.
            uint32            extent;      ///< The extent of the view within the buffer, in units of pixels.
        } bufferInfo;                      ///< Information that describes a color target buffer view.
    };
};

/**
 ***********************************************************************************************************************
 * @interface IColorTargetView
 * @brief     View of an image resource used to render it as a color target.
 *
 * @warning IColorTargetView does not inherit the IDestroyable interface.  PAL guarantees that no cleanup actions need
 *          to be taken for this object.  Clients should simply free the system memory allocated for this object, and
 *          never need to explicitly destroy this object.  This is a requirement for DX12, which manages render target
 *          views as a special type of descriptor, and therefore never get a chance to destroy a corresponding PAL
 *          object.
 *
 * @see IDevice::CreateColorTargetView()
 ***********************************************************************************************************************
 */
class IColorTargetView
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
    IColorTargetView() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.
    virtual ~IColorTargetView() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
