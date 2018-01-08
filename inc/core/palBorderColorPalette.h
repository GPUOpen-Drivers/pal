/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palBorderColorPalette.h
 * @brief Defines the Platform Abstraction Library (PAL) IBorderColorPalette interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palGpuMemoryBindable.h"

namespace Pal
{

/// Specifies properties for the creation of an @ref IBorderColorPalette object.  Input structure to
/// IDevice::CreateBorderColorPalette().
struct BorderColorPaletteCreateInfo
{
    uint32 paletteSize;  ///< Number of entries in the palette.
};

/**
 ***********************************************************************************************************************
 * @interface IBorderColorPalette
 * @brief     Represents a set of 4-component, RGBA float colors that can be selected from a sampler to be displayed
 *            when a texture coordinate is clamped.
 *
 * @see IDevice::CreateBorerColorPalette()
 ***********************************************************************************************************************
 */
class IBorderColorPalette : public IGpuMemoryBindable
{
public:
    /// Replaces a range of colors in the palette with the newly specified colors.
    ///
    /// @param [in] firstEntry Index of the first palette entry to be updated.
    /// @param [in] entryCount Number of entries to be updated.
    /// @param [in] pEntries   Pointer to an array of 4*entryCount floating point values specifying colors for the
    ///                        palette entries in RGBA format.
    ///
    /// @returns Success if the update was successfully executed.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidValue if some of the specified slots extend past the end of the palette.
    ///          + ErrorGpuMemoryNotBound if this method is called while no GPU memory is bound to the
    ///            palette object.
    virtual Result Update(
        uint32       firstEntry,
        uint32       entryCount,
        const float* pEntries) = 0;

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
    IBorderColorPalette() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IBorderColorPalette() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
