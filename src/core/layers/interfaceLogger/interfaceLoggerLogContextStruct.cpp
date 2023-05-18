/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "palAssert.h"
#include "core/layers/interfaceLogger/interfaceLoggerLogContext.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
void LogContext::Struct(
    const AcquireNextImageInfo& value)
{
    BeginMap(false);
    KeyAndValue("timeout", value.timeout);
    KeyAndObject("semaphore", value.pSemaphore);
    KeyAndObject("fence", value.pFence);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BarrierInfo& value)
{
    BeginMap(false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 751
    KeyAndBeginList("flags", true);

    if (value.flags.splitBarrierEarlyPhase)
    {
        Value("splitBarrierEarlyPhase");
    }

    if (value.flags.splitBarrierLatePhase)
    {
        Value("splitBarrierLatePhase");
    }

    EndList();
    KeyAndObject("pSplitBarrierGpuEvent", value.pSplitBarrierGpuEvent);
#endif

    KeyAndEnum("waitPoint", value.waitPoint);
    KeyAndBeginList("pipePoints", false);

    for (uint32 idx = 0; idx < value.pipePointWaitCount; ++idx)
    {
        Enum(value.pPipePoints[idx]);
    }

    EndList();
    KeyAndBeginList("gpuEvents", false);

    for (uint32 idx = 0; idx < value.gpuEventWaitCount; ++idx)
    {
        Object(value.ppGpuEvents[idx]);
    }

    EndList();
    KeyAndBeginList("targets", false);

    for (uint32 idx = 0; idx < value.rangeCheckedTargetWaitCount; ++idx)
    {
        Object(value.ppTargets[idx]);
    }

    EndList();

    KeyAndCacheCoherencyUsageFlags("globalSrcCacheMask", value.globalSrcCacheMask);
    KeyAndCacheCoherencyUsageFlags("globalDstCacheMask", value.globalDstCacheMask);

    KeyAndBeginList("transitions", false);

    for (uint32 idx = 0; idx < value.transitionCount; ++idx)
    {
        const auto& transition = value.pTransitions[idx];

        BeginMap(false);
        KeyAndCacheCoherencyUsageFlags("srcCacheMask", transition.srcCacheMask);
        KeyAndCacheCoherencyUsageFlags("dstCacheMask", transition.dstCacheMask);
        KeyAndObject("pImage", transition.imageInfo.pImage);

        if (transition.imageInfo.pImage != nullptr)
        {
            KeyAndStruct("subresRange", transition.imageInfo.subresRange);
            KeyAndStruct("oldLayout", transition.imageInfo.oldLayout);
            KeyAndStruct("newLayout", transition.imageInfo.newLayout);

            Key("pQuadSamplePattern");
            if (transition.imageInfo.pQuadSamplePattern != nullptr)
            {
                Struct(*transition.imageInfo.pQuadSamplePattern);
            }
            else
            {
                NullValue();
            }
        }

        EndMap();
    }
    EndList();

    KeyAndEnum("reason", static_cast<Developer::BarrierReason>(value.reason));

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrimeGpuCacheRange& value)
{
    BeginMap(false);
    KeyAndValue("gpuVirtAddr", value.gpuVirtAddr);
    KeyAndValue("size", value.size);
    KeyAndValue("usageMask", value.usageMask);
    KeyAndValue("addrTranslationOnly", value.addrTranslationOnly);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const AcquireReleaseInfo& value)
{
    BeginMap(false);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    KeyAndPipelineStageFlags("srcGlobalStageMask", value.srcGlobalStageMask);
    KeyAndPipelineStageFlags("dstGlobalStageMask", value.dstGlobalStageMask);
#else
    KeyAndPipelineStageFlags("srcStageMask", value.srcStageMask);
    KeyAndPipelineStageFlags("dstStageMask", value.dstStageMask);
#endif
    KeyAndCacheCoherencyUsageFlags("srcGlobalAccessMask", value.srcGlobalAccessMask);
    KeyAndCacheCoherencyUsageFlags("dstGlobalAccessMask", value.dstGlobalAccessMask);

    KeyAndBeginList("memoryBarriers", false);

    for (uint32 idx = 0; idx < value.memoryBarrierCount; ++idx)
    {
        const auto& memoryBarrier = value.pMemoryBarriers[idx];

        BeginMap(false);

        KeyAndBeginList("flags", true);

        if (memoryBarrier.flags.globallyAvailable)
        {
            Value("GloballyAvailable");
        }

        EndList();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 731
        KeyAndObject("pGpuMemory", memoryBarrier.memory.pGpuMemory);
#endif
        KeyAndValue("address",     memoryBarrier.memory.address);
        KeyAndValue("offset",      memoryBarrier.memory.offset);
        KeyAndValue("size",        memoryBarrier.memory.size);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        KeyAndPipelineStageFlags("srcStageMask", memoryBarrier.srcStageMask);
        KeyAndPipelineStageFlags("dstStageMask", memoryBarrier.dstStageMask);
#endif

        KeyAndCacheCoherencyUsageFlags("srcAccessMask", memoryBarrier.srcAccessMask);
        KeyAndCacheCoherencyUsageFlags("dstAccessMask", memoryBarrier.dstAccessMask);

        EndMap();
    }

    EndList();
    KeyAndBeginList("imageBarriers", false);

    for (uint32 idx = 0; idx < value.imageBarrierCount; ++idx)
    {
        const auto& imageBarrier = value.pImageBarriers[idx];

        BeginMap(false);

        KeyAndObject("pImage", imageBarrier.pImage);

        if (imageBarrier.pImage != nullptr)
        {
            KeyAndStruct("subresRange", imageBarrier.subresRange);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
            KeyAndPipelineStageFlags("srcStageMask", imageBarrier.srcStageMask);
            KeyAndPipelineStageFlags("dstStageMask", imageBarrier.dstStageMask);
#endif
            KeyAndCacheCoherencyUsageFlags("srcAccessMask", imageBarrier.srcAccessMask);
            KeyAndCacheCoherencyUsageFlags("dstAccessMask", imageBarrier.dstAccessMask);
            KeyAndStruct("box", imageBarrier.box);
            KeyAndStruct("oldLayout", imageBarrier.oldLayout);
            KeyAndStruct("newLayout", imageBarrier.newLayout);

            Key("pQuadSamplePattern");
            if (imageBarrier.pQuadSamplePattern != nullptr)
            {
                Struct(*imageBarrier.pQuadSamplePattern);
            }
            else
            {
                NullValue();
            }
        }

        EndMap();
    }
    EndList();

    KeyAndEnum("reason", static_cast<Developer::BarrierReason>(value.reason));

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PipelineBindParams& value)
{
    BeginMap(false);
    KeyAndEnum("pipelineBindPoint", value.pipelineBindPoint);
    KeyAndObject("pipeline", value.pPipeline);

    if (value.pipelineBindPoint == PipelineBindPoint::Compute)
    {
        KeyAndStruct("cs", value.cs);
    }
    else
    {
        KeyAndStruct("graphics", value.graphics);
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BindStreamOutTargetParams& value)
{
    BeginMap(false);
    KeyAndBeginList("targets", false);

    for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
    {
        BeginMap(false);
        KeyAndValue("gpuVirtAddr", value.target[idx].gpuVirtAddr);
        KeyAndValue("size", value.target[idx].size);
        EndMap();
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BindTargetParams& value)
{
    BeginMap(false);
    KeyAndBeginList("colorTargets", false);

    for (uint32 idx = 0; idx < value.colorTargetCount; ++idx)
    {
        BeginMap(false);
        KeyAndObject("colorTargetView", value.colorTargets[idx].pColorTargetView);
        KeyAndStruct("imageLayout", value.colorTargets[idx].imageLayout);
        EndMap();
    }

    EndList();

    if (value.depthTarget.pDepthStencilView != nullptr)
    {
        KeyAndBeginMap("depthTarget", false);
        KeyAndObject("depthStencilView", value.depthTarget.pDepthStencilView);
        KeyAndStruct("depthLayout", value.depthTarget.depthLayout);
        KeyAndStruct("stencilLayout", value.depthTarget.stencilLayout);
        EndMap();
    }
    else
    {
        KeyAndNullValue("depthTarget");
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BlendConstParams& value)
{
    BeginMap(false);
    KeyAndBeginList("blendConst", false);

    constexpr uint32 Count = sizeof(value.blendConst) / sizeof(value.blendConst[0]);
    for (uint32 idx = 0; idx < Count; ++idx)
    {
        Value(value.blendConst[idx]);
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BorderColorPaletteCreateInfo& value)
{
    BeginMap(false);
    KeyAndValue("paletteSize", value.paletteSize);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BoundColorTarget& value)
{
    BeginMap(false);
    KeyAndValue("targetIndex", value.targetIndex);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndValue("samples", value.samples);
    KeyAndStruct("clearValue", value.clearValue);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Box value)
{
    BeginMap(false);
    KeyAndStruct("offset", value.offset);
    KeyAndStruct("extent", value.extent);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BufferViewInfo& value)
{
    BeginMap(false);
    KeyAndValue("gpuAddr", value.gpuAddr);
    KeyAndValue("range", value.range);
    KeyAndValue("stride", value.stride);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    ChannelMapping value)
{
    BeginMap(false);
    KeyAndEnum("r", value.r);
    KeyAndEnum("g", value.g);
    KeyAndEnum("b", value.b);
    KeyAndEnum("a", value.a);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ClearBoundTargetRegion& value)
{
    BeginMap(false);
    KeyAndStruct("rect", value.rect);
    KeyAndValue("startSlice", value.startSlice);
    KeyAndValue("numSlices", value.numSlices);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ClearColor& value)
{
    BeginMap(false);
    KeyAndEnum("type", value.type);
    KeyAndValue("disabledChannelMask", value.disabledChannelMask);
    KeyAndBeginList("color", true);

    if (value.type == ClearColorType::Float)
    {
        constexpr uint32 Count = sizeof(value.f32Color) / sizeof(value.f32Color[0]);
        for (uint32 idx = 0; idx < Count; ++idx)
        {
            Value(value.f32Color[idx]);
        }
    }
    else
    {
        constexpr uint32 Count = sizeof(value.u32Color) / sizeof(value.u32Color[0]);
        for (uint32 idx = 0; idx < Count; ++idx)
        {
            Value(value.u32Color[idx]);
        }
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorBlendStateCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("targets", false);

    for (uint32 idx = 0; idx < MaxColorTargets; ++idx)
    {
        BeginMap(false);
        KeyAndValue("blendEnable", value.targets[idx].blendEnable);
        KeyAndEnum("srcBlendColor", value.targets[idx].srcBlendColor);
        KeyAndEnum("dstBlendColor", value.targets[idx].dstBlendColor);
        KeyAndEnum("blendFuncColor", value.targets[idx].blendFuncColor);
        KeyAndEnum("srcBlendAlpha", value.targets[idx].srcBlendAlpha);
        KeyAndEnum("dstBlendAlpha", value.targets[idx].dstBlendAlpha);
        KeyAndEnum("blendFuncAlpha", value.targets[idx].blendFuncAlpha);
        EndMap();
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorKey& value)
{
    BeginList(true);

    constexpr uint32 Count = sizeof(value.u32Color) / sizeof(value.u32Color[0]);
    for (uint32 idx = 0; idx < Count; ++idx)
    {
        Value(value.u32Color[idx]);
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorSpaceConversionRegion& value)
{
    BeginMap(false);
    KeyAndStruct("srcOffset", value.srcOffset);
    KeyAndStruct("srcExtent", value.srcExtent);
    KeyAndStruct("dstOffset", value.dstOffset);
    KeyAndStruct("dstExtent", value.dstExtent);
    KeyAndStruct("rgbSubres", value.rgbSubres);
    KeyAndValue("yuvStartSlice", value.yuvStartSlice);
    KeyAndValue("sliceCount", value.sliceCount);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorSpaceConversionTable& value)
{
    BeginList(false);

    for (uint32 rowIdx = 0; rowIdx < 3; ++rowIdx)
    {
        BeginList(true);

        for (uint32 columnIdx = 0; columnIdx < 4; ++columnIdx)
        {
            Value(value.table[rowIdx][columnIdx]);
        }

        EndList();
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorTargetViewCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.isBufferView)
    {
        Value("isBufferView");
    }

    if (value.flags.imageVaLocked)
    {
        Value("imageVaLocked");
    }

    if (value.flags.zRangeValid)
    {
        Value("zRangeValid");
    }

    EndList();
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("zRange", value.zRange);

    if (value.flags.isBufferView)
    {
        KeyAndBeginMap("bufferInfo", false);
        KeyAndObject("gpuMemory", value.bufferInfo.pGpuMemory);
        KeyAndValue("offset", value.bufferInfo.offset);
        KeyAndValue("extent", value.bufferInfo.extent);
        EndMap();
    }
    else
    {
        KeyAndBeginMap("imageInfo", false);
        KeyAndObject("image", value.imageInfo.pImage);
        KeyAndStruct("baseSubRes", value.imageInfo.baseSubRes);
        KeyAndValue("arraySize", value.imageInfo.arraySize);
        EndMap();
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ColorTransform& value)
{
    BeginList(false);

    constexpr uint32 NumFloats = sizeof(value.matrix) / sizeof(value.matrix[0]);
    for (uint32 idx = 0; idx < NumFloats; ++idx)
    {
        Value(value.matrix[idx]);
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const ComputePipelineCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    KeyAndValue("maxFunctionCallDepth", value.maxFunctionCallDepth);
    KeyAndValue("disablePartialDispatchPreemption", value.disablePartialDispatchPreemption);
#if PAL_BUILD_GFX11
    KeyAndEnum("interleaveSize", value.interleaveSize);
#endif
    KeyAndStruct("threadsPerGroup", value.threadsPerGroup);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const CmdAllocatorCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.threadSafe)
    {
        Value("threadSafe");
    }

    if (value.flags.autoMemoryReuse)
    {
        Value("autoMemoryReuse");
    }

    if (value.flags.disableBusyChunkTracking)
    {
        Value("disableBusyChunkTracking");
    }

    if (value.flags.autoTrimMemory)
    {
        Value("autoTrimMemory");
    }

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 28), "Update interfaceLogger!");

    EndList();
    KeyAndBeginMap("allocInfo", false);

    for (uint32 idx = 0; idx < CmdAllocatorTypeCount; ++idx)
    {
        const char*const DataAllocNames[] =
        {
            "CommandData",        // CommandDataAlloc  = 0,
            "EmbeddedData",       // EmbeddedDataAlloc = 1,
            "GpuScratchMemAlloc", // GpuScratchMemAlloc
        };

        static_assert(ArrayLen(DataAllocNames) == static_cast<uint32>(CmdAllocatorTypeCount),
                      "The DataAllocNames string table needs to be updated.");

        KeyAndBeginMap(DataAllocNames[idx], false);
        KeyAndEnum("allocHeap", value.allocInfo[idx].allocHeap);
        KeyAndValue("allocSize", value.allocInfo[idx].allocSize);
        KeyAndValue("suballocSize", value.allocInfo[idx].suballocSize);
        KeyAndValue("allocFreeThreshold", value.allocInfo[idx].allocFreeThreshold);
        EndMap();
    }

    EndMap();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const CmdBufferBuildInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", false);

    if (value.flags.optimizeGpuSmallBatch)
    {
        Value("optimizeGpuSmallBatch");
    }

    if (value.flags.optimizeExclusiveSubmit)
    {
        Value("optimizeExclusiveSubmit");
    }

    if (value.flags.optimizeOneTimeSubmit)
    {
        Value("optimizeOneTimeSubmit");
    }

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 755)
    if (value.flags.optimizeTessDistributionFactors)
    {
        Value("optimizeTessDistributionFactors");
    }
#endif

    if (value.flags.prefetchShaders)
    {
        Value("prefetchShaders");
    }

    if (value.flags.prefetchCommands)
    {
        Value("prefetchCommands");
    }

    if (value.flags.usesCeRamCmds)
    {
        Value("usesCeRamCmds");
    }

    if (value.flags.disallowNestedLaunchViaIb2)
    {
        Value("disallowNestedLaunchViaIb2");
    }

    if (value.flags.enableTmz)
    {
        Value("enableTmz");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 757
    if (value.flags.disableQueryInternalOps)
    {
        Value("disableQueryInternalOps");
    }
#endif

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 763)
    if (value.flags.optimizeContextStatesPerBin)
    {
        Value("optimizeContextStatesPerBin");
    }

    if (value.flags.optimizePersistentStatesPerBin)
    {
        Value("optimizePersistentStatesPerBin");
    }
#endif

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 16), "Update interfaceLogger!");

    EndList();

    if (value.pInheritedState != nullptr)
    {
        KeyAndStruct("inheritedState", *value.pInheritedState);
    }
    else
    {
        KeyAndNullValue("inheritedState");
    }

    if (value.pStateInheritCmdBuffer != nullptr)
    {
        KeyAndObject("stateInheritCmdBuffer", value.pStateInheritCmdBuffer);
    }
    else
    {
        KeyAndNullValue("stateInheritCmdBuffer");
    }

#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 755)
    if (value.flags.optimizeTessDistributionFactors)
    {
        KeyAndStruct("clientTessDistributionFactors", value.clientTessDistributionFactors);
    }
#endif

    KeyAndValue("execMarkerClientHandle", value.execMarkerClientHandle);

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const CmdBufferCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.nested)
    {
        Value("nested");
    }

    if (value.flags.realtimeComputeUnits)
    {
        Value("realtimeComputeUnits");
    }

    if (value.flags.realtimeComputeUnits)
    {
        Value("realtimeComputeUnits");
    }

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 29), "Update interfaceLogger!");

    EndList();
    KeyAndObject("cmdAllocator", value.pCmdAllocator);
    KeyAndEnum("queueType", value.queueType);
    KeyAndEnum("engineType", value.engineType);

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const CmdBufInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.isValid)
    {
        Value("isValid");
    }

    if (value.frameBegin)
    {
        Value("frameBegin");
    }

    if (value.frameEnd)
    {
        Value("frameEnd");
    }

    if (value.p2pCmd)
    {
        Value("p2pCmd");
    }

    if (value.captureBegin)
    {
        Value("captureBegin");
    }

    if (value.captureEnd)
    {
        Value("captureEnd");
    }

    if (value.rayTracingExecuted)
    {
        Value("rayTracingExecuted");
    }

    if (value.preflip)
    {
        Value("preflip");
    }

    if (value.postflip)
    {
        Value("postflip");
    }

    if (value.privateFlip)
    {
        Value("privateFlip");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 741
    if (value.disableDccRejected)
    {
        Value("disableDccRejected");
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 795
    if (value.noFlip)
    {
        Value("noFlip");
    }

    if (value.frameGenIndex)
    {
        Value("frameGenIndex");
    }
#endif

    EndList();
    KeyAndObject("primaryMemory", value.pPrimaryMemory);

    if (value.captureBegin || value.captureEnd)
    {
        KeyAndObject("directCaptureMemory", value.pDirectCapMemory);

        if (value.privateFlip)
        {
            KeyAndObject("pPrivFlipMemory", value.pPrivFlipMemory);
        }

        KeyAndValue("frameIndex", value.frameIndex);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 779
    if (value.pEarlyPresentEvent != nullptr)
    {
        KeyAndValue("pEarlyPresentEvent", value.pEarlyPresentEvent);
    }
#endif

    EndMap();
}
// =====================================================================================================================
void LogContext::Struct(
    const CmdPostProcessFrameInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.srcIsTypedBuffer)
    {
        Value("srcIsTypedBuffer");
    }

    EndList();

    if (value.flags.srcIsTypedBuffer)
    {
        KeyAndObject("srcTypedBuffer", value.pSrcTypedBuffer);
    }
    else
    {
        KeyAndObject("srcImage", value.pSrcImage);
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DepthBiasParams& value)
{
    BeginMap(false);
    KeyAndValue("depthBias", value.depthBias);
    KeyAndValue("depthBiasClamp", value.depthBiasClamp);
    KeyAndValue("slopeScaledDepthBias", value.slopeScaledDepthBias);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DepthBoundsParams& value)
{
    BeginMap(false);
    KeyAndValue("min", value.min);
    KeyAndValue("max", value.max);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DepthStencilSelectFlags& value)
{
    BeginList(true);

    if (value.depth)
    {
        Value("depth");
    }

    if (value.stencil)
    {
        Value("stencil");
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const DepthStencilStateCreateInfo& value)
{
    BeginMap(false);
    KeyAndValue("depthEnable", value.depthEnable);
    KeyAndValue("depthWriteEnable", value.depthWriteEnable);
    KeyAndEnum("depthFunc", value.depthFunc);
    KeyAndValue("depthBoundsEnable", value.depthBoundsEnable);
    KeyAndValue("stencilEnable", value.stencilEnable);
    KeyAndBeginMap("front", false);
    {
        KeyAndEnum("stencilFailOp", value.front.stencilFailOp);
        KeyAndEnum("stencilPassOp", value.front.stencilPassOp);
        KeyAndEnum("stencilDepthFailOp", value.front.stencilDepthFailOp);
        KeyAndEnum("stencilFunc", value.front.stencilFunc);
    }
    EndMap();
    KeyAndBeginMap("back", false);
    {
        KeyAndEnum("stencilFailOp", value.back.stencilFailOp);
        KeyAndEnum("stencilPassOp", value.back.stencilPassOp);
        KeyAndEnum("stencilDepthFailOp", value.back.stencilDepthFailOp);
        KeyAndEnum("stencilFunc", value.back.stencilFunc);
    }
    EndMap();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DepthStencilViewCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.readOnlyDepth)
    {
        Value("readOnlyDepth");
    }

    if (value.flags.readOnlyStencil)
    {
        Value("readOnlyStencil");
    }

    if (value.flags.imageVaLocked)
    {
        Value("imageVaLocked");
    }

    if (value.flags.absoluteDepthBias)
    {
        Value("absoluteDepthBias");
    }
    if (value.flags.bypassMall)
    {
        Value("bypassMall");
    }

    if (value.flags.depthOnlyView)
    {
        Value("depthOnlyView");
    }

    if (value.flags.stencilOnlyView)
    {
        Value("stencilOnlyView");
    }

    if (value.flags.resummarizeHiZ)
    {
        Value("resummarizeHiZ");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 730
    if (value.flags.lowZplanePolyOffsetBits)
    {
        Value("lowZplanePolyOffsetBits");
    }
#endif

    EndList();
    KeyAndObject("image", value.pImage);
    KeyAndValue("mipLevel", value.mipLevel);
    KeyAndValue("baseArraySlice", value.baseArraySlice);
    KeyAndValue("arraySize", value.arraySize);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DeviceFinalizeInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.supportPrivateScreens)
    {
        Value("supportPrivateScreens");
    }

    if (value.flags.requireFlipStatus)
    {
        Value("requireFlipStatus");
    }

    if (value.flags.requireFrameMetadata)
    {
        Value("requireFrameMetadata");
    }

    if (value.flags.internalGpuMemAutoPriority)
    {
        Value("internalGpuMemAutoPriority");
    }

    EndList();

    KeyAndBeginMap("requestedEngineCounts", false);
    for (uint32 idx = 0; idx < EngineTypeCount; ++idx)
    {
        const char*const pEngineName = LogContext::GetEngineName(static_cast<EngineType>(idx));
        KeyAndValue(pEngineName, value.requestedEngineCounts[idx].engines);
    }
    EndMap();

    KeyAndBeginMap("ceRamSizeUsed", false);
    for (uint32 idx = 0; idx < EngineTypeCount; ++idx)
    {
        const char*const pEngineName = LogContext::GetEngineName(static_cast<EngineType>(idx));
        KeyAndValue(pEngineName, value.ceRamSizeUsed[idx]);
    }
    EndMap();

    KeyAndStruct("supportedFullScreenFrameMetadata", value.supportedFullScreenFrameMetadata);
    KeyAndEnum("internalTexOptLevel", value.internalTexOptLevel);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DirectCaptureInfo& value)
{
    BeginMap(false);
    KeyAndValue("vidPnSourceId", value.vidPnSourceId);

    KeyAndBeginList("usageFlags", true);
    if (value.usageFlags.preflip)
    {
        Value("preflip");
    }

    if (value.usageFlags.postflip)
    {
        Value("postflip");
    }

    if (value.usageFlags.accessDesktop)
    {
        Value("accessDesktop");
    }

    if (value.usageFlags.shared)
    {
        Value("shared");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 795
    if (value.usageFlags.frameGenRatio)
    {
        Value("frameGenRatio");
    }

    if (value.usageFlags.paceGeneratedFrame)
    {
        Value("paceGeneratedFrame");
    }
#endif

    static_assert(CheckReservedBits<decltype(value.usageFlags)>(32, 23), "Update interfaceLogger!");
    EndList();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 795
    KeyAndValue("hPreFlipEvent", value.hPreFlipEvent);
#else
    KeyAndValue("hNewFrameEvent", value.hNewFrameEvent);

    KeyAndValue("hFatalErrorEvent", value.hFatalErrorEvent);
#endif

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    DispatchDims value)
{
    BeginMap(false);
    KeyAndValue("x", value.x);
    KeyAndValue("y", value.y);
    KeyAndValue("z", value.z);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DoppDesktopInfo& value)
{
    BeginMap(false);
    KeyAndValue("gpuVirtAddr", value.gpuVirtAddr);
    KeyAndValue("vidPnSourceId", value.vidPnSourceId);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DoppRef& value)
{
    BeginMap(false);
    KeyAndObject("gpuMemory", value.pGpuMemory);
    KeyAndBeginList("flags", true);

    if (value.flags.pfpa)
    {
        Value("pfpa");
    }

    if (value.flags.lastPfpaCmd)
    {
        Value("lastPfpaCmd");
    }

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 30), "Update interfaceLogger!");

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DynamicComputeShaderInfo& value)
{
    BeginMap(false);
    KeyAndValue("maxWavesPerCu", value.maxWavesPerCu);
    KeyAndValue("maxThreadGroupsPerCu", value.maxThreadGroupsPerCu);
    KeyAndValue("tgScheduleCountPerCu", value.tgScheduleCountPerCu);
    KeyAndValue("ldsBytesPerTg", value.ldsBytesPerTg);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DynamicGraphicsShaderInfo& value)
{
    BeginMap(false);
    KeyAndValue("maxWavesPerCu", value.maxWavesPerCu);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 789
    KeyAndValue("cuEnableMask", value.cuEnableMask);
#endif
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DynamicGraphicsShaderInfos& value)
{
    BeginMap(false);
    KeyAndStruct("vs", value.vs);
    KeyAndStruct("hs", value.hs);
    KeyAndStruct("ds", value.ds);
    KeyAndStruct("gs", value.gs);
    KeyAndStruct("ts", value.ts);
    KeyAndStruct("ms", value.ms);
    KeyAndStruct("ps", value.ps);
    KeyAndStruct("dynamicState", value.dynamicState);
    KeyAndBeginList("flags", true);

    EndList();

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 24), "Update interfaceLogger!");

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const DynamicGraphicsState& value)
{
    BeginMap(false);
    KeyAndEnum("depthClampMode",           value.depthClampMode);
    KeyAndEnum("depthRange",               value.depthRange);
    KeyAndEnum("logicOp",                  value.logicOp);
    KeyAndValue("colorWriteMask",          value.colorWriteMask);
    KeyAndValue("switchWinding",           value.switchWinding);
    KeyAndValue("depthClipNearEnable",     value.depthClipNearEnable);
    KeyAndValue("depthClipFarEnable",      value.depthClipFarEnable);
    KeyAndValue("alphaToCoverageEnable",   value.alphaToCoverageEnable);
    KeyAndValue("perpLineEndCapsEnable",   value.perpLineEndCapsEnable);
    KeyAndValue("rasterizerDiscardEnable", value.rasterizerDiscardEnable);
    KeyAndValue("dualSourceBlendEnable",   value.dualSourceBlendEnable);
    KeyAndValue("vertexBufferCount",       value.vertexBufferCount);

    KeyAndBeginList("enable", true);
    if (value.enable.depthClampMode)
    {
        Value("depthClampMode");
    }
    if (value.enable.depthRange)
    {
        Value("depthRange");
    }
    if (value.enable.logicOp)
    {
        Value("logicOp");
    }
    if (value.enable.colorWriteMask)
    {
        Value("colorWriteMask");
    }
    if (value.enable.switchWinding)
    {
        Value("switchWinding");
    }
    if (value.enable.depthClipMode)
    {
        Value("depthClipMode");
    }
    if (value.enable.alphaToCoverageEnable)
    {
        Value("alphaToCoverageEnable");
    }
    if (value.enable.perpLineEndCapsEnable)
    {
        Value("perpLineEndCapsEnable");
    }
    if (value.enable.rasterizerDiscardEnable)
    {
        Value("rasterizerDiscardEnable");
    }

    if (value.enable.dualSourceBlendEnable)
    {
        Value("rasterizerDiscardEnable");
    }
    if (value.enable.vertexBufferCount)
    {
        Value("vertexBufferCount");
    }
    EndList();
    static_assert(sizeof(DynamicGraphicsState) == 24, "Update interfaceLogger!");
    static_assert(CheckReservedBits<DynamicGraphicsState>(192, 19), "Update interfaceLogger!");
    static_assert(CheckReservedBits<decltype(value.enable)>(32, 21), "Update interfaceLogger!");
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ExternalGpuMemoryOpenInfo& value)
{
    BeginMap(false);
    KeyAndStruct("resourceInfo", value.resourceInfo);
    if (value.flags.typedBuffer)
    {
        KeyAndStruct("typedBufferInfo", value.typedBufferInfo);
    }
    static_assert(CheckReservedBits<decltype(value.flags)>(32, 31), "Update interfaceLogger!");
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ExternalImageOpenInfo& value)
{
    BeginMap(false);
    KeyAndStruct("resourceInfo", value.resourceInfo);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("flags", value.flags);
    KeyAndStruct("usageFlags", value.usage);
    KeyAndObject("screen", value.pScreen);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ExternalQueueSemaphoreOpenInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.crossProcess)
    {
        Value("crossProcess");
    }

    if (value.flags.timeline)
    {
        Value("timeline");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ExternalResourceOpenInfo& value)
{
    BeginMap(false);
    if (value.flags.isDopp)
    {
        KeyAndStruct("doppDesktopInfo", value.doppDesktopInfo);
    }
    else if (value.flags.isDirectCapture)
    {
        KeyAndStruct("directCaptureInfo", value.directCaptureInfo);
    }

    KeyAndBeginList("flags", true);

    if (value.flags.globalGpuVa)
    {
        Value("globalGpuVa");
    }

    EndList();
    static_assert(CheckReservedBits<decltype(value.flags)>(32, 27), "Update interfaceLogger!");
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const FullScreenFrameMetadataControlFlags& value)
{
    BeginList(false);

    if (value.timerNodeSubmission)
    {
        Value("timerNodeSubmission");
    }

    if (value.frameBeginFlag)
    {
        Value("frameBeginFlag");
    }

    if (value.frameEndFlag)
    {
        Value("frameEndFlag");
    }

    if (value.primaryHandle)
    {
        Value("primaryHandle");
    }

    if (value.p2pCmdFlag)
    {
        Value("p2pCmdFlag");
    }

    if (value.forceSwCfMode)
    {
        Value("forceSwCfMode");
    }

    if (value.postFrameTimerSubmission)
    {
        Value("postFrameTimerSubmission");
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    Extent2d value)
{
    BeginMap(true);
    KeyAndValue("width", value.width);
    KeyAndValue("height", value.height);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Extent3d value)
{
    BeginMap(true);
    KeyAndValue("width", value.width);
    KeyAndValue("height", value.height);
    KeyAndValue("depth", value.depth);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const FlglState& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);
    if (value.genLockEnabled)
    {
        Value("genLockEnabled");
    }

    if (value.frameLockEnabled)
    {
        Value("frameLockEnabled");
    }

    if (value.isTimingMaster)
    {
        Value("isTimingMaster");
    }
    EndList();

    KeyAndValue("firmwareVersion", value.firmwareVersion);
    KeyAndEnum("support", value.support);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GlSyncConfig& value)
{
    BeginMap(false);
    KeyAndValue("validMask", value.validMask);
    KeyAndValue("syncDelay", value.syncDelay);
    KeyAndValue("framelockCntlVector", value.framelockCntlVector);
    KeyAndValue("signalSource", value.signalSource);
    KeyAndValue("sampleRate", value.sampleRate);
    KeyAndValue("syncField", value.syncField);
    KeyAndValue("triggerEdge", value.triggerEdge);
    KeyAndValue("scanRateCoeff", value.scanRateCoeff);
    KeyAndValue("sigGenFrequency", value.sigGenFrequency);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const FmaskViewInfo& value)
{
    BeginMap(false);
    KeyAndObject("image", value.pImage);
    KeyAndValue("baseArraySlice", value.baseArraySlice);
    KeyAndValue("arraySize", value.arraySize);
    KeyAndBeginList("flags", true);

    if (value.flags.shaderWritable)
    {
        Value("shaderWritable");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GammaRamp& value)
{
    BeginMap(false);
    KeyAndStruct("scale", value.scale);
    KeyAndStruct("offset", value.offset);
    KeyAndBeginList("gammaCurve", false);

    for (uint32 idx = 0; idx < MaxGammaRampControlPoints; ++idx)
    {
        Struct(value.gammaCurve[idx]);
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GlobalScissorParams& value)
{
    BeginMap(false);
    KeyAndStruct("scissorRegion", value.scissorRegion);
    EndMap();
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
// =====================================================================================================================
void LogContext::Struct(
    const ColorWriteMaskParams& value)
{
    BeginMap(false);

    KeyAndValue("count", value.count);

    KeyAndValue("colorWriteMask[0]", value.colorWriteMask[0]);
    KeyAndValue("colorWriteMask[1]", value.colorWriteMask[1]);
    KeyAndValue("colorWriteMask[2]", value.colorWriteMask[2]);
    KeyAndValue("colorWriteMask[3]", value.colorWriteMask[3]);
    KeyAndValue("colorWriteMask[4]", value.colorWriteMask[4]);
    KeyAndValue("colorWriteMask[5]", value.colorWriteMask[5]);
    KeyAndValue("colorWriteMask[6]", value.colorWriteMask[6]);
    KeyAndValue("colorWriteMask[7]", value.colorWriteMask[7]);

    EndMap();
}
#endif

// =====================================================================================================================
void LogContext::Struct(
    const GpuEventCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.gpuAccessOnly)
    {
        Value("gpuAccessOnly");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    GpuMemoryCreateFlags value)
{
    BeginList(false);

    if (value.virtualAlloc)
    {
        Value("virtualAlloc");
    }

    if (value.shareable)
    {
        Value("shareable");
    }

    if (value.interprocess)
    {
        Value("interprocess");
    }

    if (value.presentable)
    {
        Value("presentable");
    }

    if (value.flippable)
    {
        Value("flippable");
    }

    if (value.stereo)
    {
        Value("stereo");
    }

    if (value.globallyCoherent)
    {
        Value("globallyCoherent");
    }

    if (value.xdmaBuffer)
    {
        Value("xdmaBuffer");
    }

    if (value.turboSyncSurface)
    {
        Value("turboSyncSurface");
    }

    if (value.typedBuffer)
    {
        Value("typedBuffer");
    }

    if (value.globalGpuVa)
    {
        Value("globalGpuVa");
    }

    if (value.useReservedGpuVa)
    {
        Value("useReservedGpuVa");
    }

    if (value.autoPriority)
    {
        Value("autoPriority");
    }

    if (value.busAddressable)
    {
        Value("busAddressable");
    }

    if (value.sdiExternal)
    {
        Value("sdiExternal");
    }

    if (value.sharedViaNtHandle)
    {
        Value("sharedViaNtHandle");
    }

    if (value.peerWritable)
    {
        Value("peerWritable");
    }

    if (value.tmzProtected)
    {
        Value("tmzProtected");
    }

    if (value.externalOpened)
    {
        Value("externalOpened");
    }

    if (value.restrictedContent)
    {
        Value("restrictedContent");
    }

    if (value.restrictedAccess)
    {
        Value("restrictedAccess");
    }

    if (value.crossAdapter)
    {
        Value("crossAdapter");
    }

    if (value.cpuInvisible)
    {
        Value("cpuInvisible");
    }

    if (value.gl2Uncached)
    {
        Value("gl2Uncached");
    }

    if (value.mallRangeActive)
    {
        Value("mallRangeActive");
    }

    if (value.explicitSync)
    {
        Value("explicitSync");
    }

    if (value.privPrimary)
    {
        Value("privPrimary");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 723
    if (value.privateScreen)
    {
        Value("privateScreen");
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 746
    if (value.kmdShareUmdSysMem)
    {
        Value("kmdShareUmdSysMem");
    }
    if (value.deferCpuVaReservation)
    {
        Value("deferCpuVaReservation");
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 761
    if (value.startVaHintFlag)
    {
        Value("startVaHintFlag");
    }
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 761
    static_assert(CheckReservedBits<GpuMemoryCreateFlags>(64, 31), "Need to update interfaceLogger!");
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 746
    static_assert(CheckReservedBits<GpuMemoryCreateFlags>(64, 32), "Need to update interfaceLogger!");
#endif

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const GpuMemoryCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    KeyAndValue("size", value.size);
    KeyAndValue("alignment", value.alignment);
    KeyAndEnum("vaRange", value.vaRange);

    // These two values are in a union.
    if (value.flags.useReservedGpuVa)
    {
        KeyAndObject("reservedGpuVaOwner", value.pReservedGpuVaOwner);
    }
    else
    {
        KeyAndValue("descrVirtAddr", value.descrVirtAddr);
    }

    KeyAndEnum("priority", value.priority);
    KeyAndEnum("priorityOffset", value.priorityOffset);

    KeyAndEnum("heapAccess", value.heapAccess);

    KeyAndBeginList("heaps", true);
    for (uint32 idx = 0; idx < value.heapCount; ++idx)
    {
        Enum(value.heaps[idx]);
    }
    EndList();

    KeyAndObject("image", value.pImage);

    if(value.flags.typedBuffer)
    {
        KeyAndStruct("typedBufferInfo", value.typedBufferInfo);
    }
    else
    {
        KeyAndNullValue("typedBufferInfo");
    }

    KeyAndEnum("virtualAccessMode", value.virtualAccessMode);

    if (value.flags.sdiExternal)
    {
        KeyAndValue("surfaceBusAddr", value.surfaceBusAddr);
        KeyAndValue("markerBusAddr", value.markerBusAddr);
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GpuMemoryOpenInfo& value)
{
    BeginMap(false);
    KeyAndObject("sharedMem", value.pSharedMem);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GpuMemoryRef& value)
{
    BeginMap(true);
    KeyAndObject("gpuMemory", value.pGpuMemory);
    KeyAndBeginList("flags", true);

    if (value.flags.readOnly)
    {
        Value("readOnly");
    }

    static_assert(CheckReservedBits<decltype(value.flags)>(32, 31), "Update interfaceLogger!");

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GraphicsPipelineCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    KeyAndValue("useLateAllocVsLimit", value.useLateAllocVsLimit);
    KeyAndValue("lateAllocVsLimit", value.lateAllocVsLimit);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 781
    KeyAndValue("useLateAllocGsLimit", value.useLateAllocGsLimit);
    KeyAndValue("lateAllocGsLimit", value.lateAllocGsLimit);
#endif
    KeyAndBeginMap("iaState", false);
    {
        KeyAndBeginMap("topologyInfo", false);
        {
            KeyAndEnum("primitiveType", value.iaState.topologyInfo.primitiveType);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 709
            KeyAndValue("topologyIsPolygon", value.iaState.topologyInfo.topologyIsPolygon);
#endif
            KeyAndValue("patchControlPoints", value.iaState.topologyInfo.patchControlPoints);
        }
        EndMap();
    }
    EndMap();
    KeyAndBeginMap("rsState", false);
    {
        KeyAndEnum("pointCoordOrigin", value.rsState.pointCoordOrigin);
        KeyAndValue("expandLineWidth", value.rsState.expandLineWidth);
        KeyAndEnum("shadeMode", value.rsState.shadeMode);
        KeyAndValue("rasterizeLastLinePixel", value.rsState.rasterizeLastLinePixel);
        KeyAndValue("outOfOrderPrimsEnable", value.rsState.outOfOrderPrimsEnable);
        KeyAndValue("perpLineEndCapsEnable", value.rsState.perpLineEndCapsEnable);
        KeyAndEnum("binningOverride", value.rsState.binningOverride);
        KeyAndEnum("depthClampMode", value.rsState.depthClampMode);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 733
        KeyAndBeginList("flags", true);
        if (value.rsState.flags.clipDistMaskValid != 0)
        {
            Value("clipDistMaskValid");
        }
        if (value.rsState.flags.cullDistMaskValid != 0)
        {
            Value("cullDistMaskValid");
        }
        EndList();
        KeyAndValue("cullDistMask", value.rsState.cullDistMask);
#endif
        KeyAndValue("clipDistMask", value.rsState.clipDistMask);
    }
    EndMap();

    KeyAndBeginMap("cbState", false);
    {
        KeyAndValue("alphaToCoverageEnable", value.cbState.alphaToCoverageEnable);
        KeyAndValue("dualSourceBlendEnable", value.cbState.dualSourceBlendEnable);
        KeyAndEnum("logicOp", value.cbState.logicOp);
        KeyAndBeginList("targets", false);
        for (uint32 idx = 0; idx < MaxColorTargets; ++idx)
        {
            BeginMap(false);
            KeyAndStruct("swizzledFormat", value.cbState.target[idx].swizzledFormat);
            KeyAndValue("channelWriteMask", value.cbState.target[idx].channelWriteMask);
            EndMap();
        }
        EndList();
    }
    EndMap();

    KeyAndBeginMap("viewInstancingDesc", false);
    {
        KeyAndValue("viewInstanceCount", value.viewInstancingDesc.viewInstanceCount);
        KeyAndBeginList("viewId", true);
        for (uint32 idx = 0; idx < MaxViewInstanceCount; ++idx)
        {
            Value(value.viewInstancingDesc.viewId[idx]);
        }
        EndList();
        KeyAndBeginList("renderTargetArrayIdx", true);
        for (uint32 idx = 0; idx < MaxViewInstanceCount; ++idx)
        {
            Value(value.viewInstancingDesc.renderTargetArrayIdx[idx]);
        }
        EndList();
        KeyAndBeginList("viewportArrayIdx", true);
        for (uint32 idx = 0; idx < MaxViewInstanceCount; ++idx)
        {
            Value(value.viewInstancingDesc.viewportArrayIdx[idx]);
        }
        EndList();
        KeyAndValue("enableMasking", value.viewInstancingDesc.enableMasking);
    }
    EndMap();

    KeyAndBeginMap("coverageOutDesc", false);
    {
        KeyAndValue("enable", value.coverageOutDesc.flags.enable);
        KeyAndValue("numSamples", value.coverageOutDesc.flags.numSamples);
        KeyAndValue("mrt", value.coverageOutDesc.flags.mrt);
        KeyAndValue("channel", value.coverageOutDesc.flags.channel);
    }
    EndMap();

    KeyAndBeginMap("viewportInfo", false);
    {
        KeyAndValue("depthClipNearEnable", value.viewportInfo.depthClipNearEnable);
        KeyAndValue("depthClipFarEnable", value.viewportInfo.depthClipFarEnable);

        KeyAndEnum("depthRange", value.viewportInfo.depthRange);
    }
    EndMap();

#if PAL_BUILD_GFX11
    KeyAndEnum("taskInterleaveSize", value.taskInterleaveSize);
#endif

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const CpuVirtAddrAndStride& value)
{
    BeginMap(false);
    KeyAndValue("cpuVirtAddr", value.pCpuVirtAddr);
    KeyAndValue("stride", value.stride);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GpuVirtAddrAndStride& value)
{
    BeginMap(false);
    KeyAndValue("gpuVirtAddr", value.gpuVirtAddr);
    KeyAndValue("stride", value.stride);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const HiSPretests& value)
{
    BeginMap(false);

    KeyAndBeginList("test", false);
    for (uint32 idx = 0; idx < NumHiSPretests; ++idx)
    {
       BeginMap(false);
       KeyAndEnum("compFunc", value.test[idx].func);
       KeyAndValue("compMask", value.test[idx].mask);
       KeyAndValue("compValue", value.test[idx].value);
       KeyAndValue("enable", value.test[idx].isValid);
       EndMap();
     }
    EndList();

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ImageCopyRegion& value)
{
    BeginMap(false);
    KeyAndStruct("srcSubres", value.srcSubres);
    KeyAndStruct("srcOffset", value.srcOffset);
    KeyAndStruct("dstSubres", value.dstSubres);
    KeyAndStruct("dstOffset", value.dstOffset);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("numSlices", value.numSlices);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    ImageCreateFlags value)
{
    BeginList(false);

    if (value.invariant)
    {
        Value("invariant");
    }

    if (value.cloneable)
    {
        Value("cloneable");
    }

    if (value.shareable)
    {
        Value("shareable");
    }

    if (value.presentable)
    {
        Value("presentable");
    }

    if (value.flippable)
    {
        Value("flippable");
    }

    if (value.stereo)
    {
        Value("stereo");
    }

    if (value.cubemap)
    {
        Value("cubemap");
    }

    if (value.prt)
    {
        Value("prt");
    }

    if (value.needSwizzleEqs)
    {
        Value("needSwizzleEqs");
    }

    if (value.perSubresInit)
    {
        Value("perSubresInit");
    }

    if (value.separateDepthPlaneInit)
    {
        Value("separateDepthPlaneInit");
    }

    if (value.repetitiveResolve)
    {
        Value("repetitiveResolve");
    }

    if (value.preferSwizzleEqs)
    {
        Value("preferSwizzleEqs");
    }

    if (value.fixedTileSwizzle)
    {
        Value("fixedTileSwizzle");
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const ImageCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    KeyAndStruct("usageFlags", value.usageFlags);
    KeyAndEnum("imageType", value.imageType);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("mipLevels", value.mipLevels);
    KeyAndValue("arraySize", value.arraySize);
    KeyAndValue("samples", value.samples);
    KeyAndValue("fragments", value.fragments);
    KeyAndEnum("tiling", value.tiling);
    KeyAndEnum("tilingPreference", value.tilingPreference);
    KeyAndEnum("tilingOptMode", value.tilingOptMode);
    KeyAndValue("tileSwizzle", value.tileSwizzle);
    KeyAndEnum("metadataMode", value.metadataMode);
    KeyAndBeginMap("prtPlus", false);
    KeyAndEnum("mapType", value.prtPlus.mapType);
    KeyAndStruct("lodRegion", value.prtPlus.lodRegion);
    EndMap();
    KeyAndValue("maxBaseAlign", value.maxBaseAlign);
    KeyAndValue("rowPitch", value.rowPitch);
    KeyAndValue("depthPitch", value.depthPitch);
    KeyAndStruct("refreshRate", value.refreshRate);
    KeyAndValue("viewFormatCount", value.viewFormatCount);
    KeyAndBeginList("viewFormats", false);
    if (value.viewFormatCount != AllCompatibleFormats)
    {
        for (uint32 idx = 0; idx < value.viewFormatCount; ++idx)
        {
            Struct(value.pViewFormats[idx]);
        }
    }
    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    ImageLayout value)
{
    BeginMap(false);
    KeyAndImageLayoutUsageFlags("usages", value.usages);
    KeyAndImageLayoutEngineFlags("engines", value.engines);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ImageResolveRegion& value)
{
    BeginMap(false);
    KeyAndValue("srcPlane", value.srcPlane);
    KeyAndValue("srcSlice", value.srcSlice);
    KeyAndStruct("srcOffset", value.srcOffset);
    KeyAndValue("dstPlane", value.dstPlane);
    KeyAndValue("dstMipLevel", value.dstMipLevel);
    KeyAndValue("dstSlice", value.dstSlice);
    KeyAndStruct("dstOffset", value.dstOffset);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("numSlices", value.numSlices);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    Key("pQuadSamplePattern");
    if (value.pQuadSamplePattern != nullptr)
    {
        Struct(*value.pQuadSamplePattern);
    }
    else
    {
        NullValue();
    }
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ImageScaledCopyRegion& value)
{
    BeginMap(false);
    KeyAndStruct("srcSubres", value.srcSubres);
    KeyAndStruct("srcOffset", value.srcOffset);
    KeyAndStruct("srcExtent", value.srcExtent);
    KeyAndStruct("dstSubres", value.dstSubres);
    KeyAndStruct("dstOffset", value.dstOffset);
    KeyAndStruct("dstExtent", value.dstExtent);
    KeyAndValue("numSlices", value.numSlices);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    ImageUsageFlags value)
{
    BeginMap(false);
    KeyAndBeginList("flags", false);

    if (value.shaderRead)
    {
        Value("shaderRead");
    }

    if (value.shaderWrite)
    {
        Value("shaderWrite");
    }

    if (value.resolveSrc)
    {
        Value("resolveSrc");
    }

    if (value.resolveDst)
    {
        Value("resolveDst");
    }

    if (value.colorTarget)
    {
        Value("colorTarget");
    }

    if (value.depthStencil)
    {
        Value("depthStencil");
    }

    if (value.noStencilShaderRead)
    {
        Value("noStencilShaderRead");
    }

    if (value.hiZNeverInvalid)
    {
        Value("hiZNeverInvalid");
    }

    if (value.depthAsZ24)
    {
        Value("depthAsZ24");
    }

    if (value.cornerSampling)
    {
        Value("cornerSampling");
    }

    if (value.vrsDepth)
    {
        Value("vrsDepth");
    }

    if (value.disableOptimizedDisplay)
    {
        Value("disableOptimizedDisplay");
    }

    if (value.useLossy)
    {
        Value("useLossy");
    }

    EndList();
    KeyAndValue("firstShaderWritableMip", value.firstShaderWritableMip);

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ImageViewInfo& value)
{
    BeginMap(false);
    KeyAndObject("image", value.pImage);
    KeyAndEnum("viewType", value.viewType);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("subresRange", value.subresRange);
    KeyAndValue("minLod", value.minLod);
    KeyAndValue("samplePatternIdx", value.samplePatternIdx);
    KeyAndStruct("zRange", value.zRange);
    KeyAndEnum("texOptLevel", value.texOptLevel);
    KeyAndStruct("possibleLayouts", value.possibleLayouts);

    KeyAndObject("pPrtParentImg", value.pPrtParentImg);
    KeyAndEnum("mapAccess", value.mapAccess);

    KeyAndBeginList("flags", true);

    if (value.flags.zRangeValid)
    {
        Value("zRangeValid");
    }

    if (value.flags.includePadding)
    {
        Value("includePadding");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const IndirectCmdGeneratorCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("params", false);

    for (uint32 idx = 0; idx < value.paramCount; ++idx)
    {
        BeginMap(false);
        KeyAndEnum("type", value.pParams[idx].type);
        KeyAndValue("sizeInBytes", value.pParams[idx].sizeInBytes);

        if (value.pParams[idx].type == IndirectParamType::SetUserData)
        {
            KeyAndBeginMap("userData", false);
            KeyAndValue("firstEntry", value.pParams[idx].userData.firstEntry);
            KeyAndValue("entryCount", value.pParams[idx].userData.entryCount);
            EndMap();
        }
        else if (value.pParams[idx].type == IndirectParamType::BindVertexData)
        {
            KeyAndBeginMap("vertexData", false);
            KeyAndValue("bufferId", value.pParams[idx].vertexData.bufferId);
            EndMap();
        }

        EndMap();
    }

    EndList();
    KeyAndValue("strideInBytes", value.strideInBytes);
    KeyAndBeginList("indexTypeTokens", true);

    constexpr uint32 TokenCount = sizeof(value.indexTypeTokens) / sizeof(value.indexTypeTokens[0]);
    for (uint32 idx = 0; idx < TokenCount; ++idx)
    {
        Value(value.indexTypeTokens[idx]);
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const InheritedStateParams& value)
{
    BeginMap(false);
    KeyAndBeginList("stateFlags", true);

    if (value.stateFlags.targetViewState)
    {
        Value("targetViewState");
    }

    if (value.stateFlags.occlusionQuery)
    {
        Value("occlusionQuery");
    }

    if (value.stateFlags.predication)
    {
        Value("predication");
    }
    static_assert(CheckReservedBits<decltype(value.stateFlags)>(32, 29), "Update interfaceLogger!");

    EndList();
    KeyAndBeginList("colorTargets", false);

    for (uint32 idx = 0; idx < value.colorTargetCount; ++idx)
    {
        BeginMap(false);
        KeyAndStruct("swizzledFormat", value.colorTargetSwizzledFormats[idx]);
        KeyAndValue("sampleCount", value.sampleCount[idx]);
        EndMap();
    }

    EndList();
    KeyAndBeginList("stateFlags", true);

    if (value.stateFlags.targetViewState)
    {
        Value("targetViewState");
    }

    if (value.stateFlags.occlusionQuery)
    {
        Value("occlusionQuery");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const InputAssemblyStateParams& value)
{
    BeginMap(false);
    KeyAndEnum("topology", value.topology);
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 747)
    KeyAndValue("patchControlPoints", value.patchControlPoints);
#endif
    KeyAndValue("primitiveRestartIndex", value.primitiveRestartIndex);
    KeyAndValue("primitiveRestartEnable", value.primitiveRestartEnable);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 745
    KeyAndValue("primitiveRestartMatchAllBits", value.primitiveRestartMatchAllBits);
#endif
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const MemoryCopyRegion& value)
{
    BeginMap(false);
    KeyAndValue("srcOffset", value.srcOffset);
    KeyAndValue("dstOffset", value.dstOffset);
    KeyAndValue("copySize", value.copySize);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const MemoryImageCopyRegion& value)
{
    BeginMap(false);
    KeyAndStruct("imageSubres", value.imageSubres);
    KeyAndStruct("imageOffset", value.imageOffset);
    KeyAndStruct("imageExtent", value.imageExtent);
    KeyAndValue("numSlices", value.numSlices);
    KeyAndValue("gpuMemoryOffset", value.gpuMemoryOffset);
    KeyAndValue("gpuMemoryRowPitch", value.gpuMemoryRowPitch);
    KeyAndValue("gpuMemoryDepthPitch", value.gpuMemoryDepthPitch);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const MemoryTiledImageCopyRegion& value)
{
    BeginMap(false);
    KeyAndStruct("imageSubres", value.imageSubres);
    KeyAndStruct("imageOffset", value.imageOffset);
    KeyAndStruct("imageExtent", value.imageExtent);
    KeyAndValue("numSlices", value.numSlices);
    KeyAndValue("gpuMemoryOffset", value.gpuMemoryOffset);
    KeyAndValue("gpuMemoryRowPitch", value.gpuMemoryRowPitch);
    KeyAndValue("gpuMemoryDepthPitch", value.gpuMemoryDepthPitch);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const MsaaQuadSamplePattern& value)
{
    BeginList(false);

    for (uint32 idx = 0; idx < MaxMsaaRasterizerSamples; ++idx)
    {
        BeginMap(false);
        KeyAndStruct("topLeft", value.topLeft[idx]);
        KeyAndStruct("topRight", value.topRight[idx]);
        KeyAndStruct("bottomLeft", value.bottomLeft[idx]);
        KeyAndStruct("bottomRight", value.bottomRight[idx]);
        EndMap();
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const MsaaStateCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.enableConservativeRasterization)
    {
        Value("enableConservativeRasterization");
    }
    if (value.flags.disableAlphaToCoverageDither)
    {
        Value("disableAlphaToCoverageDither");
    }

    EndList();
    KeyAndValue("coverageSamples", value.coverageSamples);
    KeyAndValue("exposedSamples", value.exposedSamples);
    KeyAndValue("pixelShaderSamples", value.pixelShaderSamples);
    KeyAndValue("depthStencilSamples", value.depthStencilSamples);
    KeyAndValue("shaderExportMaskSamples", value.shaderExportMaskSamples);
    KeyAndValue("sampleMask", value.sampleMask);
    KeyAndValue("sampleClusters", value.sampleClusters);
    KeyAndValue("alphaToCoverageSamples", value.alphaToCoverageSamples);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Offset2d value)
{
    BeginMap(true);
    KeyAndValue("x", value.x);
    KeyAndValue("y", value.y);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Offset3d value)
{
    BeginMap(true);
    KeyAndValue("x", value.x);
    KeyAndValue("y", value.y);
    KeyAndValue("z", value.z);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PeerGpuMemoryOpenInfo& value)
{
    BeginMap(false);
    KeyAndObject("originalMem", value.pOriginalMem);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PeerImageOpenInfo& value)
{
    BeginMap(false);
    KeyAndObject("originalImage", value.pOriginalImage);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PerSubQueueSubmitInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("cmdBuffers", false);
    for (uint32 i = 0; i < value.cmdBufferCount; i++)
    {
        BeginMap(false);
        KeyAndObject("object", value.ppCmdBuffers[i]);
        if ((value.pCmdBufInfoList != nullptr) && value.pCmdBufInfoList[i].isValid)
        {
            KeyAndStruct("info", value.pCmdBufInfoList[i]);
        }
        else
        {
            KeyAndNullValue("info");
        }
        EndMap();
    }
    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PinnedGpuMemoryCreateInfo& value)
{
    BeginMap(false);
    KeyAndValue("size", value.size);
    KeyAndEnum("vaRange", value.vaRange);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    LibraryCreateFlags value)
{
    BeginMap(false);
    KeyAndValue("clientInternal", value.clientInternal);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    PipelineCreateFlags value)
{
    BeginMap(false);
    KeyAndValue("clientInternal", value.clientInternal);
    KeyAndValue("supportDynamicDispatch", value.supportDynamicDispatch);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PlatformCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.disableGpuTimeout)
    {
        Value("disableGpuTimeout");
    }

    if (value.flags.force32BitVaSpace)
    {
        Value("force32BitVaSpace");
    }

    if (value.flags.createNullDevice)
    {
        Value("createNullDevice");
    }

    if (value.flags.enableSvmMode)
    {
        Value("enableSvmMode");
    }

    EndList();
    KeyAndValue("settingsPath", value.pSettingsPath);
    KeyAndEnum("nullGpuId", value.nullGpuId);
    KeyAndValue("apiMajorVer", value.apiMajorVer);
    KeyAndValue("apiMinorVer", value.apiMinorVer);
    KeyAndValue("maxSvmSize", value.maxSvmSize);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PointLineRasterStateParams& value)
{
    BeginMap(false);
    KeyAndValue("pointSize", value.pointSize);
    KeyAndValue("lineWidth", value.lineWidth);
    KeyAndValue("pointSizeMin", value.pointSizeMin);
    KeyAndValue("pointSizeMax", value.pointSizeMax);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const LineStippleStateParams& value)
{
    BeginMap(false);
    KeyAndValue("lineStippleValue", value.lineStippleValue);
    KeyAndValue("lineStippleScale", value.lineStippleScale);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PowerSwitchInfo& value)
{
    BeginMap(true);
    KeyAndValue("time", value.time);
    KeyAndValue("performance", value.performance);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PresentableImageCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.fullscreen)
    {
        Value("fullscreen");
    }

    if (value.flags.stereo)
    {
        Value("stereo");
    }

    if (value.flags.turbosync)
    {
        Value("turbosync");
    }

    EndList();
    KeyAndStruct("usageFlags", value.usage);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("extent", value.extent);
    KeyAndObject("screen", value.pScreen);
    KeyAndObject("swapChain", value.pSwapChain);

    KeyAndValue("viewFormatCount", value.viewFormatCount);
    KeyAndBeginList("viewFormats", false);
    if (value.viewFormatCount != AllCompatibleFormats)
    {
        for (uint32 idx = 0; idx < value.viewFormatCount; ++idx)
        {
            Struct(value.pViewFormats[idx]);
        }
    }
    EndList();

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PresentDirectInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.fullscreenDoNotWait)
    {
        Value("fullscreenDoNotWait");
    }
    if (value.flags.srcIsTypedBuffer)
    {
        Value("srcIsTypedBuffer");
    }
    if (value.flags.dstIsTypedBuffer)
    {
        Value("dstIsTypedBuffer");
    }

    EndList();
    KeyAndEnum("presentMode", value.presentMode);
    KeyAndValue("presentInterval", value.presentInterval);

    if (value.flags.srcIsTypedBuffer)
    {
        KeyAndObject("srcTypedBuffer", value.pSrcTypedBuffer);
    }
    else
    {
        KeyAndObject("srcImage", value.pSrcImage);
    }
    if (value.flags.dstIsTypedBuffer)
    {
        KeyAndObject("dstTypedBuffer", value.pDstTypedBuffer);
    }
    else
    {
        KeyAndObject("dstImage", value.pDstImage);
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PresentSwapChainInfo& value)
{
    BeginMap(false);
    KeyAndEnum("presentMode", value.presentMode);
    KeyAndObject("srcImage", value.pSrcImage);
    KeyAndObject("swapChain", value.pSwapChain);
    KeyAndValue("imageIndex", value.imageIndex);
    KeyAndBeginList("flags", true);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 794
    if (value.flags.turboSyncEnabled)
    {
        Value("turboSyncEnabled");
    }
#endif

    if (value.flags.notifyOnly)
    {
        Value("notifyOnly");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateDisplayMode& value)
{
    BeginMap(false);
    KeyAndStruct("sourceSize", value.sourceSize);
    KeyAndValue("pixelClockInKhz", value.pixelClockInKhz);
    KeyAndStruct("horizontalTiming", value.horizontalTiming);
    KeyAndStruct("verticalTiming", value.verticalTiming);
    KeyAndEnum("colorDepth", value.colorDepth);
    KeyAndEnum("pixelEncoding", value.pixelEncoding);
    KeyAndValue("scalingEnabled", value.scalingEnabled);
    KeyAndStruct("destinationSize", value.destinationSize);
    KeyAndStruct("offset", value.offset);
    KeyAndBeginList("flags", true);
    if (value.flags.slsTiledLayout)
    {
        Value("slsTiledLayout");
    }
    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateDisplayTiming& value)
{
    BeginMap(false);
    KeyAndValue("active", value.active);
    KeyAndValue("blank", value.blank);
    KeyAndValue("syncOffset", value.syncOffset);
    KeyAndValue("syncWidth", value.syncWidth);
    KeyAndValue("positiveSyncPolarity", value.positiveSyncPolarity);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenCaps& value)
{
    BeginMap(false);
    KeyAndValue("hasAudio", value.hasAudio);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenCreateInfo& value)
{
    BeginMap(false);
    KeyAndValue("index", value.index);
    KeyAndStruct("props", value.props);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenEnableInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.vsyncAlwaysOn)
    {
        Value("vsyncAlwaysOn");
    }

    if (value.flags.disablePowerManagement)
    {
        Value("disablePowerManagement");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenImageCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.invariant)
    {
        Value("invariant");
    }

    EndList();
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("usage", value.usage);
    KeyAndStruct("extent", value.extent);
    KeyAndObject("screen", value.pScreen);

    KeyAndValue("viewFormatCount", value.viewFormatCount);
    KeyAndBeginList("viewFormats", false);
    if (value.viewFormatCount != AllCompatibleFormats)
    {
        for (uint32 idx = 0; idx < value.viewFormatCount; ++idx)
        {
            Struct(value.pViewFormats[idx]);
        }
    }
    EndList();

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenPresentInfo& value)
{
    BeginMap(false);
    KeyAndObject("srcImg", value.pSrcImg);
    KeyAndObject("presentDoneFence", value.pPresentDoneFence);
    KeyAndValue("vsync", value.vsync);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrivateScreenProperties& value)
{
    BeginMap(false);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("targetId", value.targetId);
    KeyAndEnum("type", value.type);
    KeyAndStruct("refreshRate", value.refreshRate);
    KeyAndValue("hash", value.hash);
    KeyAndBeginList("edid", false);

    for (uint32 idx = 0; idx < value.edidSize; ++idx)
    {
        Value(value.edid[idx]);
    }

    EndList();
    KeyAndBeginList("formats", false);

    for (uint32 idx = 0; idx < value.numFormats; ++idx)
    {
        Struct(value.pFormats[idx]);
    }

    EndList();
    KeyAndValue("maxNumPowerSwitches", value.maxNumPowerSwitches);
    KeyAndValue("powerSwitchLatency", value.powerSwitchLatency);
    KeyAndStruct("caps", value.caps);

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const QueryControlFlags& value)
{
    BeginList(true);

    if (value.impreciseData)
    {
        Value("impreciseData");
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const QueryPoolCreateInfo& value)
{
    BeginMap(false);
    KeyAndEnum("queryPoolType", value.queryPoolType);
    KeyAndValue("numSlots", value.numSlots);

    if (value.queryPoolType == QueryPoolType::PipelineStats)
    {
        KeyAndQueryPipelineStatsFlags("enabledStats", value.enabledStats);
    }
    else
    {
        KeyAndValue("enabledStats", value.enabledStats);
    }

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const QueueCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.windowedPriorBlit)
    {
        Value("windowedPriorBlit");
    }

    EndList();
    KeyAndEnum("queueType", value.queueType);
    KeyAndEnum("engineType", value.engineType);
    KeyAndValue("engineIndex", value.engineIndex);
    KeyAndEnum("submitOptMode", value.submitOptMode);
    KeyAndValue("numReservedCu", value.numReservedCu);
    KeyAndValue("persistentCeRamOffset", value.persistentCeRamOffset);
    KeyAndValue("persistentCeRamSize", value.persistentCeRamSize);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const QueueSemaphoreCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.shareable)
    {
        Value("shareable");
    }

    if (value.flags.timeline)
    {
        Value("timeline");
    }

    if (value.flags.sharedViaNtHandle)
    {
        Value("sharedViaNtHandle");
    }

    if (value.flags.externalOpened)
    {
        Value("externalOpened");
    }

    if (value.flags.noSignalOnDeviceLost)
    {
        Value("noSignalOnDeviceLost");
    }

    EndList();
    KeyAndValue("maxCount", value.maxCount);
    KeyAndValue("initialCount", value.initialCount);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const QueueSemaphoreOpenInfo& value)
{
    BeginMap(false);
    KeyAndObject("sharedQueueSemaphore", value.pSharedQueueSemaphore);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Range value)
{
    BeginMap(false);
    KeyAndValue("offset", value.offset);
    KeyAndValue("extent", value.extent);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Rational value)
{
    BeginMap(true);
    KeyAndValue("numerator", value.numerator);
    KeyAndValue("denominator", value.denominator);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    Rect value)
{
    BeginMap(false);
    KeyAndStruct("offset", value.offset);
    KeyAndStruct("extent", value.extent);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    RgbFloat value)
{
    BeginMap(true);
    KeyAndValue("r", value.r);
    KeyAndValue("g", value.g);
    KeyAndValue("b", value.b);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SamplerInfo& value)
{
    BeginMap(false);
    KeyAndEnum("filterMode", value.filterMode);
    KeyAndStruct("filter", value.filter);
    KeyAndEnum("addressU", value.addressU);
    KeyAndEnum("addressV", value.addressV);
    KeyAndEnum("addressW", value.addressW);
    KeyAndValue("mipLodBias", value.mipLodBias);
    KeyAndValue("maxAnisotropy", value.maxAnisotropy);
    KeyAndEnum("compareFunc", value.compareFunc);
    KeyAndValue("minLod", value.minLod);
    KeyAndValue("maxLod", value.maxLod);
    KeyAndEnum("borderColorType", value.borderColorType);
    KeyAndValue("borderColorPaletteIndex", value.borderColorPaletteIndex);
    KeyAndValue("anisoThreshold", value.anisoThreshold);
    KeyAndValue("perfMip", value.perfMip);

    if (value.flags.forResidencyMap != 0)
    {
        KeyAndStruct("uvOffset", value.uvOffset);
        KeyAndStruct("uvSlope", value.uvSlope);
    }

    KeyAndBeginList("flags", true);

    if (value.flags.mgpuIqMatch)
    {
        Value("mgpuIqMatch");
    }

    if (value.flags.forResidencyMap)
    {
        Value("forResidencyMap");
    }

    if (value.flags.preciseAniso)
    {
        Value("preciseAniso");
    }

    if (value.flags.unnormalizedCoords)
    {
        Value("unnormalizedCoords");
    }
    if (value.flags.useAnisoThreshold)
    {
        Value("useAnisoThreshold");
    }

    if (value.flags.truncateCoords)
    {
        Value("truncateCoords");
    }

    if (value.flags.seamlessCubeMapFiltering)
    {
        Value("seamlessCubeMapFiltering");
    }

    if (value.flags.prtBlendZeroMode)
    {
        Value("prtBlendZeroMode");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const BvhInfo& value)
{
    BeginMap(false);
    KeyAndObject("pMemory", value.pMemory);
    KeyAndValue("offset", value.offset);
    KeyAndValue("numNodes", value.numNodes);
    KeyAndValue("boxGrowValue", value.boxGrowValue);

    KeyAndEnum("boxSortHeuristic", value.boxSortHeuristic);

    KeyAndBeginList("flags", true);

    if (value.flags.useZeroOffset)
    {
        Value("useZeroOffset");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SamplePatternPalette& value)
{
    BeginList(false);

    for (uint32 paletteIdx = 0; paletteIdx < MaxSamplePatternPaletteEntries; ++paletteIdx)
    {
        BeginList(false);

        for (uint32 sampleIdx = 0; sampleIdx < MaxMsaaRasterizerSamples; ++sampleIdx)
        {
            Struct(value[paletteIdx][sampleIdx]);
        }

        EndList();
    }

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    SamplePos value)
{
    BeginMap(true);
    KeyAndValue("x", value.x);
    KeyAndValue("y", value.y);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    ScaledCopyFlags value)
{
    BeginList(false);

    if (value.srcColorKey)
    {
        Value("srcColorKey");
    }

    if (value.dstColorKey)
    {
        Value("dstColorKey");
    }

    if (value.srcAlpha)
    {
        Value("srcAlpha");
    }

    if (value.dstAsSrgb)
    {
        Value("dstAsSrgb");
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 742
    if (value.dstAsNorm)
    {
        Value("dstAsNorm");
    }
#endif

    EndList();
}

// =====================================================================================================================
void LogContext::Struct(
    const ScaledCopyInfo& value)
{
    BeginMap(false);
    KeyAndObject("srcImage", value.pSrcImage);
    KeyAndStruct("srcImageLayout", value.srcImageLayout);
    KeyAndObject("dstImage", value.pDstImage);
    KeyAndStruct("dstImageLayout", value.dstImageLayout);
    KeyAndBeginList("regions", false);

    for (uint32 idx = 0; idx < value.regionCount; ++idx)
    {
        Struct(value.pRegions[idx]);
    }

    EndList();
    KeyAndStruct("filter", value.filter);
    KeyAndEnum("rotation", value.rotation);

    if (value.pColorKey != nullptr)
    {
        KeyAndStruct("srcColorKey", *value.pColorKey);
    }
    else
    {
        KeyAndNullValue("srcColorKey");
    }

    if (value.pScissorRect != nullptr)
    {
        KeyAndStruct("scissorRect", *value.pScissorRect);
    }

    KeyAndStruct("flags", value.flags);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GenMipmapsInfo& value)
{
    BeginMap(false);
    KeyAndObject("image", value.pImage);
    KeyAndStruct("baseMipLayout", value.baseMipLayout);
    KeyAndStruct("genMipLayout", value.genMipLayout);
    KeyAndStruct("range", value.range);
    KeyAndStruct("filter", value.filter);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ScissorRectParams& value)
{
    BeginMap(false);
    KeyAndBeginList("scissors", false);

    for (uint32 idx = 0; idx < value.count; ++idx)
    {
        Struct(value.scissors[idx]);
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SetClockModeInput& value)
{
    BeginMap(false);
    KeyAndEnum("clockMode", value.clockMode);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SetClockModeOutput& value)
{
    BeginMap(false);
    KeyAndValue("memoryClockFrequency", value.memoryClockFrequency);
    KeyAndValue("engineClockFrequency", value.engineClockFrequency);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SetMgpuModeInput& value)
{
    BeginMap(false);
    KeyAndValue("vidPnSrcId", value.vidPnSrcId);
    KeyAndEnum("mgpuMode", value.mgpuMode);
    KeyAndValue("isFramePacingEnabled", value.isFramePacingEnabled);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ShaderLibraryCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ShaderLibraryFunctionInfo& value)
{
    BeginMap(true);
    KeyAndValue("symbolName", value.pSymbolName);
    KeyAndValue("gpuVirtAddr", value.gpuVirtAddr);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    SignedExtent2d value)
{
    BeginMap(true);
    KeyAndValue("width", value.width);
    KeyAndValue("height", value.height);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    SignedExtent3d value)
{
    BeginMap(true);
    KeyAndValue("width", value.width);
    KeyAndValue("height", value.height);
    KeyAndValue("depth", value.depth);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const StencilRefMaskParams& value)
{
    BeginMap(false);
    KeyAndValue("frontRef", value.frontRef);
    KeyAndValue("frontReadMask", value.frontReadMask);
    KeyAndValue("frontWriteMask", value.frontWriteMask);
    KeyAndValue("frontOpValue", value.frontOpValue);
    KeyAndValue("backRef", value.backRef);
    KeyAndValue("backReadMask", value.backReadMask);
    KeyAndValue("backWriteMask", value.backWriteMask);
    KeyAndValue("backOpValue", value.backOpValue);
    KeyAndBeginList("flags", false);

    if (value.flags.updateFrontRef)
    {
        Value("updateFrontRef");
    }

    if (value.flags.updateFrontReadMask)
    {
        Value("updateFrontReadMask");
    }

    if (value.flags.updateFrontWriteMask)
    {
        Value("updateFrontWriteMask");
    }

    if (value.flags.updateFrontOpValue)
    {
        Value("updateFrontOpValue");
    }

    if (value.flags.updateBackRef)
    {
        Value("updateBackRef");
    }

    if (value.flags.updateBackReadMask)
    {
        Value("updateBackReadMask");
    }

    if (value.flags.updateBackWriteMask)
    {
        Value("updateBackWriteMask");
    }

    if (value.flags.updateBackOpValue)
    {
        Value("updateBackOpValue");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    SubresId value)
{
    BeginMap(true);
    KeyAndValue("plane", value.plane);
    KeyAndValue("mipLevel", value.mipLevel);
    KeyAndValue("arraySlice", value.arraySlice);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    SubresRange value)
{
    BeginMap(false);
    KeyAndStruct("startSubres", value.startSubres);
    KeyAndValue("numPlanes", value.numPlanes);
    KeyAndValue("numMips", value.numMips);
    KeyAndValue("numSlices", value.numSlices);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SvmGpuMemoryCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("flags", value.flags);
    KeyAndValue("size", value.size);
    KeyAndValue("alignment", value.alignment);
    KeyAndObject("reservedGpuVaOwner", value.pReservedGpuVaOwner);
    KeyAndValue("isUsedForKernel", value.isUsedForKernel);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const SwapChainCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.clipped)
    {
        Value("clipped");
    }

    if (value.flags.canAcquireBeforeSignaling)
    {
        Value("canAcquireBeforeSignaling");
    }

    EndList();
    KeyAndEnum("wsiPlatform", value.wsiPlatform);
    KeyAndValue("imageCount", value.imageCount);
    KeyAndStruct("imageSwizzledFormat", value.imageSwizzledFormat);
    KeyAndStruct("imageExtent", value.imageExtent);
    KeyAndStruct("imageUsageFlags", value.imageUsageFlags);
    KeyAndEnum("preTransform", value.preTransform);
    KeyAndValue("imageArraySize", value.imageArraySize);
    KeyAndEnum("swapChainMode", value.swapChainMode);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    SwizzledFormat value)
{
    BeginMap(false);
    KeyAndEnum("format", value.format);
    KeyAndStruct("swizzle", value.swizzle);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    TessDistributionFactors value)
{
    BeginMap(false);
    KeyAndValue("isoDistributionFactor", value.isoDistributionFactor);
    KeyAndValue("triDistributionFactor", value.triDistributionFactor);
    KeyAndValue("quadDistributionFactor", value.quadDistributionFactor);
    KeyAndValue("donutDistributionFactor", value.donutDistributionFactor);
    KeyAndValue("trapDistributionFactor", value.trapDistributionFactor);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    TexFilter value)
{
    BeginMap(false);
    KeyAndEnum("magnification", static_cast<XyFilter>(value.magnification));
    KeyAndEnum("minification", static_cast<XyFilter>(value.minification));
    KeyAndEnum("zFilter", static_cast<ZFilter>(value.zFilter));
    KeyAndEnum("mipFilter", static_cast<MipFilter>(value.mipFilter));
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const TriangleRasterStateParams& value)
{
    BeginMap(false);
    KeyAndEnum("frontFillMode", value.frontFillMode);
    KeyAndEnum("backFillMode", value.backFillMode);
    KeyAndEnum("cullMode", value.cullMode);
    KeyAndEnum("frontFace", value.frontFace);
    KeyAndEnum("provokingVertex", value.provokingVertex);
    KeyAndBeginList("flags", true);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 721
    if (value.flags.frontDepthBiasEnable)
    {
        Value("frontDepthBiasEnable");
    }
    if (value.flags.backDepthBiasEnable)
    {
        Value("backDepthBiasEnable");
    }
#else
    if (value.flags.depthBiasEnable)
    {
        Value("depthBiasEnable");
    }
#endif

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const TurboSyncControlInput& value)
{
    BeginMap(false);

    KeyAndEnum("mode", value.mode);
    KeyAndValue("vidPnSourceId", value.vidPnSourceId);
    KeyAndBeginList("primaryMemoryArray", false);

    for (uint32 iGpu = 0; iGpu < MaxDevices; iGpu++)
    {
        BeginList(false);

        for (uint32 idx = 0; idx < TurboSyncMaxSurfaces; ++idx)
        {
            Object(value.pPrimaryMemoryArray[iGpu][idx]);
        }

        EndList();
    }

    EndList();

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const TypedBufferCopyRegion& value)
{
    BeginMap(false);
    KeyAndStruct("srcBuffer", value.srcBuffer);
    KeyAndStruct("dstBuffer", value.dstBuffer);
    KeyAndStruct("extent", value.extent);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const TypedBufferCreateInfo& value)
{
    BeginMap(false);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("rowPitch", value.rowPitch);
    KeyAndValue("depthPitch", value.depthPitch);
    KeyAndValue("depthIsSubres", value.depthIsSubres);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const TypedBufferInfo& value)
{
    BeginMap(false);
    KeyAndStruct("swizzledFormat", value.swizzledFormat);
    KeyAndValue("offset", value.offset);
    KeyAndValue("rowPitch", value.rowPitch);
    KeyAndValue("depthPitch", value.depthPitch);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const UserClipPlane& value)
{
    BeginMap(false);
    KeyAndValue("x", value.x);
    KeyAndValue("y", value.y);
    KeyAndValue("z", value.z);
    KeyAndValue("w", value.w);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const ViewportParams& value)
{
    BeginMap(false);
    KeyAndBeginList("viewports", false);

    for (uint32 idx = 0; idx < value.count; ++idx)
    {
        BeginMap(false);
        KeyAndValue("originX", value.viewports[idx].originX);
        KeyAndValue("originY", value.viewports[idx].originY);
        KeyAndValue("width", value.viewports[idx].width);
        KeyAndValue("height", value.viewports[idx].height);
        KeyAndValue("minDepth", value.viewports[idx].minDepth);
        KeyAndValue("maxDepth", value.viewports[idx].maxDepth);
        KeyAndEnum("origin", value.viewports[idx].origin);
        EndMap();
    }

    EndList();
    KeyAndValue("horzDiscardRatio", value.horzDiscardRatio);
    KeyAndValue("vertDiscardRatio", value.vertDiscardRatio);
    KeyAndValue("horzClipRatio", value.horzClipRatio);
    KeyAndValue("vertClipRatio", value.vertClipRatio);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VirtualMemoryCopyPageMappingsRange& value)
{
    BeginMap(false);
    KeyAndObject("srcGpuMem", value.pSrcGpuMem);
    KeyAndObject("dstGpuMem", value.pDstGpuMem);
    KeyAndValue("srcStartOffset", value.srcStartOffset);
    KeyAndValue("dstStartOffset", value.dstStartOffset);
    KeyAndValue("size", value.size);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VirtualMemoryRemapRange& value)
{
    BeginMap(false);
    KeyAndObject("virtualGpuMem", value.pVirtualGpuMem);
    KeyAndObject("realGpuMem", value.pRealGpuMem);
    KeyAndValue("virtualStart", value.virtualStartOffset);
    KeyAndValue("realStartOffset", value.realStartOffset);
    KeyAndValue("size", value.size);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VirtualDisplayInfo& value)
{
    BeginMap(false);
    KeyAndValue("width", value.width);
    KeyAndValue("height", value.height);
    KeyAndObject("privateScreen", value.pPrivateScreen);
    KeyAndStruct("refreshRate", value.refreshRate);
    KeyAndEnum("vsyncMode", value.vsyncMode);
    KeyAndValue("vsyncOffset", value.vsyncOffset);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VirtualDisplayProperties& value)
{
    BeginMap(false);
    KeyAndValue("isVirtualDisplay", value.isVirtualDisplay);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VrsCenterState&  centerState)
{
    BeginMap(false);
    KeyAndBeginMap("centerOffset", false);

    for (uint32 idx = 0; idx < static_cast<uint32>(VrsCenterRates::Max); idx++)
    {
        KeyAndStruct(GetVrsCenterRateName(static_cast<VrsCenterRates>(idx)), centerState.centerOffset[idx]);
    }

    EndMap();
    KeyAndBeginList("flags", true);

    if (centerState.flags.overrideCenterSsaa)
    {
        Value("overrideCenterSsaa");
    }

    if (centerState.flags.overrideCentroidSsaa)
    {
        Value("overrideCentroidSsaa");
    }

    if (centerState.flags.alwaysComputeCentroid)
    {
        Value("alwaysComputeCentroid");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const VrsRateParams&  rateParams)
{
    BeginMap(false);
    KeyAndEnum("shadingRate", rateParams.shadingRate);
    KeyAndBeginMap("combinerState", false);

    for (uint32 idx = 0; idx < static_cast<uint32>(VrsCombinerStage::Max); idx++)
    {
        KeyAndEnum(GetVrsCombinerStageName(static_cast<VrsCombinerStage>(idx)), rateParams.combinerState[idx]);
    }

    EndMap();
    KeyAndBeginList("flags", true);

    if (rateParams.flags.exposeVrsPixelsMask)
    {
        Value("exposeVrsPixelsMask");
    }

    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const PrtPlusImageResolveRegion& value)
{
    BeginMap(false);
    KeyAndStruct("srcOffset", value.srcOffset);
    KeyAndValue("srcMipLevel", value.srcMipLevel);
    KeyAndValue("srcSlice", value.srcSlice);
    KeyAndStruct("dstOffset", value.dstOffset);
    KeyAndValue("dstMipLevel", value.dstMipLevel);
    KeyAndValue("dstSlice", value.dstSlice);
    KeyAndStruct("extent", value.extent);
    KeyAndValue("numSlices", value.numSlices);
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const FenceCreateInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);

    if (value.flags.signaled)
    {
        Value("signaled");
    }

    if (value.flags.eventCanBeInherited)
    {
        Value("eventCanBeInherited");
    }

    EndList();

    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const FenceOpenInfo& value)
{
    BeginMap(false);
    KeyAndBeginList("flags", true);
    if (value.flags.isReference)
    {
        Value("isReference");
    }
    EndList();
    EndMap();
}

// =====================================================================================================================
void LogContext::Struct(
    const GpuMemSubAllocInfo& value)
{
    BeginMap(false);
    KeyAndValue("address", value.address);
    KeyAndValue("offset", value.offset);
    KeyAndValue("size", value.size);
    EndMap();
}

} // InterfaceLogger
} // Pal

#endif
