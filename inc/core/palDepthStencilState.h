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
 * @file  palDepthStencilState.h
 * @brief Defines the Platform Abstraction Library (PAL) IDepthStencilState interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

/// Defines a stencil operation performed during the stencil test.
enum class StencilOp : uint32
{
    Keep     = 0x0,
    Zero     = 0x1,
    Replace  = 0x2,
    IncClamp = 0x3,
    DecClamp = 0x4,
    Invert   = 0x5,
    IncWrap  = 0x6,
    DecWrap  = 0x7,
    Count
};

/// Specifies properties for creation of an @ref IDepthStencilState object.  Input structure to
/// IDevice::CreateDepthStencilState().
struct DepthStencilStateCreateInfo
{
    /// Specifies a complete stencil test configuration.
    struct DepthStencilOp
    {
        StencilOp   stencilFailOp;       ///< Stencil op performed when the stencil test fails.
        StencilOp   stencilPassOp;       ///< Stencil op performed when the stencil and depth tests pass.
        StencilOp   stencilDepthFailOp;  ///< Stencil op performed when the stencil test passes but the depth test
                                         ///  fails.
        CompareFunc stencilFunc;         ///< Stencil comparison function.
    };

    bool            depthEnable;         ///< Enable depth testing.
    bool            depthWriteEnable;    ///< Enable depth writes.
    CompareFunc     depthFunc;           ///< Depth comparison function.
    bool            depthBoundsEnable;   ///< Enables depth bounds testing.
    bool            stencilEnable;       ///< Enable stencil testing.
    DepthStencilOp  front;               ///< Stencil operation for front-facing geometry.
    DepthStencilOp  back;                ///< Stencil operation for back-facing geometry.
};

/**
 ***************************************************************************************************
 * @interface IDepthStencilState
 * @brief     Dynamic state object controlling fixed function depth/stencil state.
 *
 * Configures depth and stencil test parameters.
 *
 * @see IDevice::CreateDepthStencilState
 ***************************************************************************************************
 */
class IDepthStencilState : public IDestroyable
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
    IDepthStencilState() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IDepthStencilState() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
