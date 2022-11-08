/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMotionEstimator.h
 * @brief Defines the Platform Abstraction Library (PAL) IMotionEstimator interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palCmdBuffer.h"
#include "palGpuMemoryBindable.h"

namespace Pal
{

/// Defines ME output format support
enum class MeOutputFormat :uint32
{
    None                        = 0x0,                ///< No output support
    MotionVectorLegacy          = 0x1,                ///< The legacy format ouput contains only motion vectors
    MotionVectorTextureFormat   = 0x2,                ///< MV Output per MB in texture format and no padding
    MotionVectorWithMatrix      = 0x4,                ///< Output contains difference matrix along with motion vectors.
    Count
};

/// Defines creation info for an IMotionEstimator object.
struct MotionEstimatorCreateInfo
{
    EngineType              engineType;             ///< Engine type to run ME
    SwizzledFormat          inputFormat;            ///< The format of Input frame and reference frame
    MeBlockSizeType         meBlockSizeType;        ///< Block size type supported by encoder
    MePrecisionType         precision;              ///< Precision mode set by the application.
    MeSizeRange             sizeRange;              ///< Size range set by application.
    MeOutputFormat          outputFormat;           ///< Output format specified by application
    VideoEncodeCodec        codecType;              ///< Codec type H264 or HEVC specified by the application
    uint32                  outputAswBufferType;    ///< Asw Output BufferType

    /// Below paramerers are exclusively for HEVC.
    uint32                  log2MinLumaCodingBlockSizeMinus3; ///< Parameter for Coding Block Size
    uint32                  constrainedIntraPredFlag;         ///< Parameter for Constrained Intra Prediction

    union
    {
        struct
        {
            uint32  protectedSession            : 1; ///< Indicator set by Dx12 Motion Estimator to enable IP TMZ mode
            uint32  ampEnabled                  : 1; ///< Indicator set to Enable Assymetric Partiton (AMP) for Interprediction
            uint32  strongIntraSmoothingEnabled : 1; ///< Indicator set to Enable Strong Intra Smoothing/Deblocking Filter
            uint32  reserved                    : 29;

        };
        uint32  u32All;
    } flags;
};

/**
 *********************************************************************************************************************
 * @interface IMotionEstimator
 * @brief     Object containing motion estimator state. Separate concrete implementations will support various
 *            HW implementations.
 *
 * @see IDevice::CreateMotionEstimator()
 *********************************************************************************************************************
 */
class IMotionEstimator : public IGpuMemoryBindable
{
public:
    /// Queries the GPU memory properties of the motion vector output
    ///
    /// @param [out] pGpuMemReqs Required properties of GPU memory to be bound to this object.  Includes properties like
    ///                          size, alignment, and allowed heaps.
    virtual Result GetMotionVectorGpuMemRequirements(
        GpuMemoryRequirements* pMotionVectorOutputGpuMemReqs) const = 0;

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
    /// @internal Constructor.
    ///
    /// @param [in] createInfo App-specified parameters describing the desired video decoder properties.
    IMotionEstimator() : m_pClientData(nullptr) { }
    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IMotionEstimator() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
