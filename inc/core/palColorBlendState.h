/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palColorBlendState.h
 * @brief Defines the Platform Abstraction Library (PAL) IColorBlendState interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

/// Specifies coefficient for the source or destination part of the blend equation.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 853
enum class Blend : uint8
#else
enum class Blend : uint32
#endif
{
    Zero                  = 0x00,
    One                   = 0x01,
    SrcColor              = 0x02,
    OneMinusSrcColor      = 0x03,
    DstColor              = 0x04,
    OneMinusDstColor      = 0x05,
    SrcAlpha              = 0x06,
    OneMinusSrcAlpha      = 0x07,
    DstAlpha              = 0x08,
    OneMinusDstAlpha      = 0x09,
    ConstantColor         = 0x0A,
    OneMinusConstantColor = 0x0B,
    ConstantAlpha         = 0x0C,
    OneMinusConstantAlpha = 0x0D,
    SrcAlphaSaturate      = 0x0E,
    Src1Color             = 0x0F,
    OneMinusSrc1Color     = 0x10,
    Src1Alpha             = 0x11,
    OneMinusSrc1Alpha     = 0x12,
    Count
};

/// Specifies the blend function in a blend equation.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 853
enum class BlendFunc : uint8
#else
enum class BlendFunc : uint32
#endif
{
    Add             = 0x0,
    Subtract        = 0x1,
    ReverseSubtract = 0x2,
    Min             = 0x3, // minimum of src color and dst color, min(Rs, Rd).
    Max             = 0x4, // maximum of src color and dst color, max(Rs, Rd).
    ScaledMin       = 0x5, // minimum of src color and src factor, dst color and dst factor, min(Rs * Sr, Rd * Dr).
    ScaledMax       = 0x6, // maximum of src color and src factor, dst color and dst factor, max(Rs * Sr, Rd * Dr).
    Count
};

/// Specifies properties for creation of an @ref IColorBlendState object.  Input structure to
/// IDevice::CreateColorBlendState().
struct ColorBlendStateCreateInfo
{
    struct
    {
        bool      blendEnable;     ///< Enable blending per color target.
        Blend     srcBlendColor;   ///< Source blend equation coefficient for color.
        Blend     dstBlendColor;   ///< Destination blend equation coefficient for color.
        BlendFunc blendFuncColor;  ///< Blend function for color.
        Blend     srcBlendAlpha;   ///< Source blend equation coefficient for alpha.
        Blend     dstBlendAlpha;   ///< Destination blend equation coefficient for alpha.
        BlendFunc blendFuncAlpha;  ///< Blend function for alpha.
    } targets[MaxColorTargets];    ///< Blending info for each color target.
};

/**
 ***********************************************************************************************************************
 * @interface IColorBlendState
 * @brief     Dynamic state object controlling fixed function blend state.
 *
 * Describes how colors values outputted by the pixel shader should be blended with the existing color data in the
 * render target.
 *
 * A blend state defined to use a second pixel shader output is considered to be a "dual source" blend mode.  Dual-
 * source blending is specified by one of the following blend values:
 *
 * + Blend::Src1Color
 * + Blend::OneMinusSrc1Color
 * + Blend::Src1Alpha
 * + Blend::OneMinusSrc1Alpha
 *
 * A blend state object with dual-source blending must only be used with pipelines that enable dual-source blending.
 *
 * At draw time, the blend enable specified in the color blend state for each color target must match the blend state
 * defined in the bound pipeline.  Mismatches between the pipeline blend state and dynamic color blend state will cause
 * undefined results.
 *
 * @see IDevice::CreateColorBlendState
 ***********************************************************************************************************************
 */
class IColorBlendState : public IDestroyable
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
    IColorBlendState() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IColorBlendState() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
