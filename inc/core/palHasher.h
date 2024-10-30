/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palImage.h"

namespace Pal
{
/**
 ***********************************************************************************************************************
* @brief  A wrapper class which has a Metrohash64 or MetroHash128 member. It provides Hash() methods for all the
*          necessary struct or enum types to be called by hash calculation. Ultimately the metrohash library's
*          default Update method which just treats every object like a raw byte array will be removed.
*          Each time a new struct for hash caluclation is added or an existed struct for hash caluclation is changed,
*           a Hash method need to be added or changed
*
***********************************************************************************************************************
*/
template<typename HashType >
class Hasher final
{
    static_assert((std::is_same<HashType, Util::MetroHash64>::value ||
                   std::is_same<HashType, Util::MetroHash128>::value),
                  "Hash type HashType must be either MetroHash64 or MetroHash128.");
public:
    Hasher() {}
    ~Hasher() {}

    template<typename Type> inline
    typename std::enable_if<std::is_enum<Type>::value, void>::type Hash(Type value)
    {
        m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value));
    }

    template<typename Type> inline
    typename std::enable_if<std::is_arithmetic<Type>::value, void>::type Hash(Type value)
    {
        m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value));
    }

    void Hash(const ImageUsageFlags& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const SwizzledFormat& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const Extent3d& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const Rational& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const ClearColor& info)
    {
        Hash(info.type);
        Hash(info.disabledChannelMask);
        m_hasher.Update(reinterpret_cast<const uint8*>(&(info.u32Color[0])), 4*sizeof(uint32));
    }

    void Hash(const ImageCreateInfo& info)
    {
        // Note that one client is not able to guarantee that they consistently set the perSubresInit flag
        // for all images that must be identical so we need to skip over the ImageCreateFlags.
        Hash(info.usageFlags);
        Hash(info.imageType);
        Hash(info.swizzledFormat);
        Hash(info.extent);
        Hash(info.mipLevels);
        Hash(info.arraySize);
        Hash(info.samples);
        Hash(info.fragments);
        Hash(info.tiling);
        Hash(info.tilingPreference);
        Hash(info.tilingOptMode);
        Hash(info.tileSwizzle);
        Hash(info.metadataMode);
        Hash(info.metadataTcCompatMode);
        Hash(info.maxBaseAlign);
        Hash(info.imageMemoryBudget);
        Hash(info.prtPlus.mapType);
        Hash(info.prtPlus.lodRegion);
        Hash(info.rowPitch);
        Hash(info.depthPitch);
        Hash(info.refreshRate);
        if ((info.pViewFormats != nullptr) && (info.viewFormatCount > 0))
        {
            m_hasher.Update(reinterpret_cast<const uint8*>(&(info.pViewFormats[0])), info.viewFormatCount*sizeof(SwizzledFormat));
        }
    }

    void Hash(const PipelineCreateFlags& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const Util::MetroHash::Hash& value) { m_hasher.Update(reinterpret_cast<const uint8*>(&value), sizeof(value)); }
    void Hash(const RasterizerState& info)
    {
        Hash(info.pointCoordOrigin);
        Hash(info.expandLineWidth);
        Hash(info.shadeMode);
        Hash(info.rasterizeLastLinePixel);
        Hash(info.outOfOrderPrimsEnable);
        Hash(info.perpLineEndCapsEnable);
        Hash(info.binningOverride);
        Hash(info.depthClampMode);
        Hash(info.clipDistMask);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 869
        Hash(info.forcedShadingRate);
#endif
        Hash(info.dx10DiamondTestDisable);
    }
    void Hash(const ViewportInfo& info)
    {
        Hash(info.depthClipNearEnable);
        Hash(info.depthClipFarEnable);
        Hash(info.depthRange);
    }
    void Hash(const ColorTargetInfo& info)
    {
        Hash(info.swizzledFormat);
        Hash(info.channelWriteMask);
        Hash(info.forceAlphaToOne);
    }

    void Hash(const GraphicsPipelineCreateInfo& info)
    {
        Hash(info.flags);
        Hash(info.useLateAllocVsLimit);

        if (info.useLateAllocVsLimit)
        {
            Hash(info.lateAllocVsLimit);
        }

        Hash(info.useLateAllocGsLimit);

        if (info.useLateAllocGsLimit)
        {
            Hash(info.lateAllocGsLimit);
        }

        m_hasher.Update(reinterpret_cast<const uint8*>(&(info.iaState)), sizeof(info.iaState));
        Hash(info.rsState);
        Hash(info.cbState.alphaToCoverageEnable);
        Hash(info.cbState.dualSourceBlendEnable);
        Hash(info.cbState.logicOp);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 904
        Hash(info.cbState.uavExportSingleDraw);
#endif
        Hash(info.viewportInfo);
        for (uint32_t i = 0; i < Pal::MaxColorTargets; ++i)
        {
            if (info.cbState.target[i].swizzledFormat.format != Pal::ChNumFormat::Undefined)
            {
                Hash(info.cbState.target[i]);
            }
        }

        if (info.viewInstancingDesc.viewInstanceCount > 0)
        {
            const Pal::ViewInstancingDescriptor& desc = info.viewInstancingDesc;

            Hash(reinterpret_cast<const uint8_t*>(desc.viewId),
                sizeof(desc.viewId[0]) * desc.viewInstanceCount);

            Hash(reinterpret_cast<const uint8_t*>(desc.renderTargetArrayIdx),
                sizeof(desc.renderTargetArrayIdx[0]) * desc.viewInstanceCount);

            Hash(reinterpret_cast<const uint8_t*>(desc.viewportArrayIdx),
                sizeof(desc.viewportArrayIdx[0]) * desc.viewInstanceCount);

            Hash(desc.enableMasking);
        }

        if (info.coverageOutDesc.flags.enable == 1)
        {
            m_hasher.Update(reinterpret_cast<const uint8*>(&(info.coverageOutDesc)), sizeof(info.coverageOutDesc));
        }
    }

    inline void Hash(const void* buffer, const uint64 length)
    {
        m_hasher.Update(reinterpret_cast<const uint8*>(buffer), length);
    }
    inline void Finalize(void* const hash) { m_hasher.Finalize(reinterpret_cast<uint8* const>(hash)); }

private:
    HashType m_hasher;
    PAL_DISALLOW_COPY_AND_ASSIGN(Hasher);
};

typedef Hasher<Util::MetroHash64> Hasher64;
typedef Hasher<Util::MetroHash128> Hasher128;

}
