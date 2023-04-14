/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMsaaState.h
 * @brief Defines the Platform Abstraction Library (PAL) IMsaaState interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

/// Specifies conservative rasterization mode
enum class ConservativeRasterizationMode : uint32
{
    Overestimate    = 0x0,  ///< Fragments will be generated if the primitive area covers any portion of the pixel.
    Underestimate   = 0x1,  ///< Fragments will be generated if all of the pixel is covered by the primitive.
    Count
};

/// Maximum supported number of MSAA color samples.
constexpr uint32 MaxMsaaColorSamples = 16;

/// Maximum supported number of MSAA depth samples.
constexpr uint32 MaxMsaaDepthSamples = 8;

/// Maximum supported number of MSAA fragments.
constexpr uint32 MaxMsaaFragments = 8;

/// Sampling pattern grid size. This is a quad of pixels, i.e. 2x2 grid of pixels.
constexpr Extent2d MaxGridSize = { 2, 2 };

/// The positions are rounded to 1/Pow2(SubPixelBits)
constexpr uint32 SubPixelBits = 4;

/// Each pixel is subdivided into Pow2(SubPixelBits) x Pow2(SubPixelBits) grid of possible sample locations.
constexpr Extent2d SubPixelGridSize = { 16, 16 };

/// Specifies a custom multisample pattern for a pixel quad.
struct MsaaQuadSamplePattern
{
    Offset2d topLeft[MaxMsaaRasterizerSamples];      ///< Sample locations for TL pixel of quad.
    Offset2d topRight[MaxMsaaRasterizerSamples];     ///< Sample locations for TR pixel of quad.
    Offset2d bottomLeft[MaxMsaaRasterizerSamples];   ///< Sample locations for BL pixel of quad.
    Offset2d bottomRight[MaxMsaaRasterizerSamples];  ///< Sample locations for BR pixel of quad.
};

/// Specifies properties for creation of an @ref IMsaaState object.  Input structure to IDevice::CreateMsaaState().
struct MsaaStateCreateInfo
{
    uint32 coverageSamples;               ///< Number of rasterizer samples. Must be greater than or equal to all
                                          ///  sample rates in the pipeline. Valid values are 1, 2, 4, 8, and 16.
    uint32 exposedSamples;                ///< Number of samples exposed in the pixel shader coverage mask.
                                          ///  Must be less than or equal to coverageSamples.
                                          ///  Valid values are 1, 2, 4, and 8.
    uint32 pixelShaderSamples;            ///< Controls the pixel shader execution rate. Must be less than or equal to
                                          ///  coverageSamples. Valid values are 1, 2, 4, and 8.
    uint32 depthStencilSamples;           ///< Number of samples in the bound depth target. Must be less than or equal
                                          ///  to coverageSamples. Valid values are 1, 2, 4, and 8.
    uint32 shaderExportMaskSamples;       ///< Number of samples to use in the shader export mask. Should match the
                                          ///  number of color target fragments clamped to
                                          ///  @ref DeviceProperties imageProperties.maxMsaaFragments.
    uint32 sampleMask;                    ///< Bitmask of which color target and depth/stencil samples should be
                                          ///  updated.  The lowest bit corresponds to sample 0.
    uint32 sampleClusters;                ///< Number of sample clusters to control over-rasterization (all samples
                                          ///  in a cluster are rasterized if any are hit). Must be less than or
                                          ///  equal to coverageSamples. Valid values are 1, 2, 4, and 8.
    uint32 alphaToCoverageSamples;        ///< How many samples of quality to generate with alpha-to-coverage. Must be
                                          ///  less than or equal to coverageSamples.
                                          ///  Valid values are 1, 2, 4, 8, and 16.
    uint32 occlusionQuerySamples;         ///< Controls the number of samples to use for occlusion queries
                                          ///  This value must never exceed the MSAA rate.

    ConservativeRasterizationMode conservativeRasterizationMode;
                                          ///< Selects overestimate or underestimate conservative
                                          ///  rasterization mode. Used only if
                                          ///  @ref MsaaStateCreateInfo::flags::enableConservativeRasterization
                                          ///  is set to true.
    union
    {
        struct
        {
            uint32  enableConservativeRasterization :  1; ///< Set to true to enable conservative rasterization
            uint32  enable1xMsaaSampleLocations     :  1; ///< Set to true to enable 1xMSAA quad sample pattern
            uint32  disableAlphaToCoverageDither    :  1; ///< Disables coverage dithering.
            uint32  enableLineStipple               :  1; ///< Set to true to enable line stippling
            uint32  reserved                        : 28; ///<  Reserved for future use

        };

        uint32  u32All;
    } flags;
};

/**
 ***********************************************************************************************************************
 * @interface IMsaaState
 * @brief     Dynamic state object controlling fixed function MSAA state.
 *
 * Configures sample counts of various portions of the pipeline, specifies sample positions, etc.  The full range of
 * EQAA hardware features are exposed.
 *
 * @see IDevice::CreateMsaaState
 ***********************************************************************************************************************
 */
class IMsaaState : public IDestroyable
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
    IMsaaState() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IMsaaState() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
