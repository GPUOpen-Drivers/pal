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

#include "core/layers/interfaceLogger/interfaceLoggerLogContext.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
void LogContext::Enum(
    AtomicOp value)
{
    const char*const StringTable[] =
    {
        "AddInt32",  // 0x00,
        "SubInt32",  // 0x01,
        "MinUint32", // 0x02,
        "MaxUint32", // 0x03,
        "MinSint32", // 0x04,
        "MaxSint32", // 0x05,
        "AndInt32",  // 0x06,
        "OrInt32",   // 0x07,
        "XorInt32",  // 0x08,
        "IncUint32", // 0x09,
        "DecUint32", // 0x0A,
        "AddInt64",  // 0x0B,
        "SubInt64",  // 0x0C,
        "MinUint64", // 0x0D,
        "MaxUint64", // 0x0E,
        "MinSint64", // 0x0F,
        "MaxSint64", // 0x10,
        "AndInt64",  // 0x11,
        "OrInt64",   // 0x12,
        "XorInt64",  // 0x13,
        "IncUint64", // 0x14,
        "DecUint64", // 0x15,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(AtomicOp::Count),
                  "The Blend string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(AtomicOp::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    Developer::BarrierReason value)
{
    const char* pStr = nullptr;
    if (value == Developer::BarrierReasonInvalid)
    {
        pStr = "BarrierReasonInvalid";
    }
    else if (value == Developer::BarrierReasonUnknown)
    {
        pStr = "BarrierReasonUnknown";
    }
    else if ((value >= Developer::BarrierReasonFirst) && (value < Developer::BarrierReasonInternalLastDefined))
    {
        const char*const StringTable[] =
        {
            "BarrierReasonPreComputeColorClear",
            "BarrierReasonPostComputeColorClear",
            "BarrierReasonPreComputeDepthStencilClear",
            "BarrierReasonPostComputeDepthStencilClear",
            "BarrierReasonMlaaResolveEdgeSync",
            "BarrierReasonAqlWaitForParentKernel",
            "BarrierReasonAqlWaitForChildrenKernels",
            "BarrierReasonP2PBlitSync",
            "BarrierReasonTimeGraphGrid",
            "BarrierReasonTimeGraphGpuLine",
            "BarrierReasonDebugOverlayText",
            "BarrierReasonDebugOverlayGraph",
            "BarrierReasonDevDriverOverlay",
            "BarrierReasonDmaImgScanlineCopySync",
            "BarrierReasonPostSqttTrace",
            "BarrierReasonPrePerfDataCopy",
            "BarrierReasonFlushL2CachedData",
        };
        static_assert((Developer::BarrierReasonInternalLastDefined - Developer::BarrierReasonFirst)
                       == ArrayLen(StringTable),
                      "Barrier reason strings need to be updated!");
        pStr = StringTable[value - Developer::BarrierReasonFirst];
    }

    if (pStr != nullptr)
    {
        Value(pStr);
    }
    else
    {
        // We don't have a string for this reason (eg. client-defined)
        Value(value);
    }
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
void LogContext::Enum(
    DispatchInterleaveSize value)
{
    const char*const StringTable[] =
    {
        "Default", // 0x0,
        "Disable", // 0x1,
        "128",     // 0x2,
        "256",     // 0x3,
        "512",     // 0x4,
    };

    static_assert(ArrayLen32(StringTable) == static_cast<uint32>(DispatchInterleaveSize::Count),
                  "The DispatchInterleaveSize string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(DispatchInterleaveSize::Count));

    Value(StringTable[idx]);
}
#endif

// =====================================================================================================================
void LogContext::Enum(
    BinningOverride value)
{
    const char*const StringTable[] =
    {
        "Default", // 0x0,
        "Disable", // 0x1,
        "Enable",  // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(BinningOverride::Count),
        "The BinningOverride string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(BinningOverride::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    Blend value)
{
    const char*const StringTable[] =
    {
        "Zero",                  // 0x00,
        "One",                   // 0x01,
        "SrcColor",              // 0x02,
        "OneMinusSrcColor",      // 0x03,
        "DstColor",              // 0x04,
        "OneMinusDstColor",      // 0x05,
        "SrcAlpha",              // 0x06,
        "OneMinusSrcAlpha",      // 0x07,
        "DstAlpha",              // 0x08,
        "OneMinusDstAlpha",      // 0x09,
        "ConstantColor",         // 0x0A,
        "OneMinusConstantColor", // 0x0B,
        "ConstantAlpha",         // 0x0C,
        "OneMinusConstantAlpha", // 0x0D,
        "SrcAlphaSaturate",      // 0x0E,
        "Src1Color",             // 0x0F,
        "OneMinusSrc1Color",     // 0x10,
        "Src1Alpha",             // 0x11,
        "OneMinusSrc1Alpha",     // 0x12,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(Blend::Count),
                  "The Blend string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(Blend::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    BlendFunc value)
{
    const char*const StringTable[] =
    {
        "Add",             // 0x0,
        "Subtract",        // 0x1,
        "ReverseSubtract", // 0x2,
        "Min",             // 0x3,
        "Max",             // 0x4,
        "ScaledMin",       // 0x5,
        "ScaledMax",       // 0x6,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(BlendFunc::Count),
                  "The BlendFunc string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(BlendFunc::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    BorderColorType value)
{
    const char*const StringTable[] =
    {
        "White",            // 0x0,
        "TransparentBlack", // 0x1,
        "OpaqueBlack",      // 0x2,
        "PaletteIndex",     // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(BorderColorType::Count),
                  "The BorderColorType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(BorderColorType::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ChannelSwizzle value)
{
    const char*const StringTable[] =
    {
        "Zero", // 0x0,
        "One",  // 0x1,
        "X",    // 0x2,
        "Y",    // 0x3,
        "Z",    // 0x4,
        "W",    // 0x5,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ChannelSwizzle::Count),
                  "The ChannelSwizzle string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ChannelSwizzle::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ChNumFormat value)
{
    const char*const StringTable[] =
    {
        "Undefined",                // 0x0,
        "X1_Unorm",                 // 0x1,
        "X1_Uscaled",               // 0x2,
        "X4Y4_Unorm",               // 0x3,
        "X4Y4_Uscaled",             // 0x4,
        "L4A4_Unorm",               // 0x5,
        "X4Y4Z4W4_Unorm",           // 0x6,
        "X4Y4Z4W4_Uscaled",         // 0x7,
        "X5Y6Z5_Unorm",             // 0x8,
        "X5Y6Z5_Uscaled",           // 0x9,
        "X5Y5Z5W1_Unorm",           // 0xA,
        "X5Y5Z5W1_Uscaled",         // 0xB,
        "X1Y5Z5W5_Unorm",           // 0xC,
        "X1Y5Z5W5_Uscaled",         // 0xD,
        "X8_Unorm",                 // 0xE,
        "X8_Snorm",                 // 0xF,
        "X8_Uscaled",               // 0x10,
        "X8_Sscaled",               // 0x11,
        "X8_Uint",                  // 0x12,
        "X8_Sint",                  // 0x13,
        "X8_Srgb",                  // 0x14,
        "A8_Unorm",                 // 0x15,
        "L8_Unorm",                 // 0x16,
        "P8_Unorm",                 // 0x17,
        "X8Y8_Unorm",               // 0x18,
        "X8Y8_Snorm",               // 0x19,
        "X8Y8_Uscaled",             // 0x1A,
        "X8Y8_Sscaled",             // 0x1B,
        "X8Y8_Uint",                // 0x1C,
        "X8Y8_Sint",                // 0x1D,
        "X8Y8_Srgb",                // 0x1E,
        "L8A8_Unorm",               // 0x1F,
        "X8Y8Z8W8_Unorm",           // 0x20,
        "X8Y8Z8W8_Snorm",           // 0x21,
        "X8Y8Z8W8_Uscaled",         // 0x22,
        "X8Y8Z8W8_Sscaled",         // 0x23,
        "X8Y8Z8W8_Uint",            // 0x24,
        "X8Y8Z8W8_Sint",            // 0x25,
        "X8Y8Z8W8_Srgb",            // 0x26,
        "U8V8_Snorm_L8W8_Unorm",    // 0x27,
        "X10Y11Z11_Float",          // 0x28,
        "X11Y11Z10_Float",          // 0x29,
        "X10Y10Z10W2_Unorm",        // 0x2A,
        "X10Y10Z10W2_Snorm",        // 0x2B,
        "X10Y10Z10W2_Uscaled",      // 0x2C,
        "X10Y10Z10W2_Sscaled",      // 0x2D,
        "X10Y10Z10W2_Uint",         // 0x2E,
        "X10Y10Z10W2_Sint",         // 0x2F,
        "X10Y10Z10W2Bias_Unorm",    // 0x30,
        "U10V10W10_Snorm_A2_Unorm", // 0x31,
        "X16_Unorm",                // 0x32,
        "X16_Snorm",                // 0x33,
        "X16_Uscaled",              // 0x34,
        "X16_Sscaled",              // 0x35,
        "X16_Uint",                 // 0x36,
        "X16_Sint",                 // 0x37,
        "X16_Float",                // 0x38,
        "L16_Unorm",                // 0x39,
        "X16Y16_Unorm",             // 0x3A,
        "X16Y16_Snorm",             // 0x3B,
        "X16Y16_Uscaled",           // 0x3C,
        "X16Y16_Sscaled",           // 0x3D,
        "X16Y16_Uint",              // 0x3E,
        "X16Y16_Sint",              // 0x3F,
        "X16Y16_Float",             // 0x40,
        "X16Y16Z16W16_Unorm",       // 0x41,
        "X16Y16Z16W16_Snorm",       // 0x42,
        "X16Y16Z16W16_Uscaled",     // 0x43,
        "X16Y16Z16W16_Sscaled",     // 0x44,
        "X16Y16Z16W16_Uint",        // 0x45,
        "X16Y16Z16W16_Sint",        // 0x46,
        "X16Y16Z16W16_Float",       // 0x47,
        "X32_Uint",                 // 0x48,
        "X32_Sint",                 // 0x49,
        "X32_Float",                // 0x4A,
        "X32Y32_Uint",              // 0x4B,
        "X32Y32_Sint",              // 0x4C,
        "X32Y32_Float",             // 0x4D,
        "X32Y32Z32_Uint",           // 0x4E,
        "X32Y32Z32_Sint",           // 0x4F,
        "X32Y32Z32_Float",          // 0x50,
        "X32Y32Z32W32_Uint",        // 0x51,
        "X32Y32Z32W32_Sint",        // 0x52,
        "X32Y32Z32W32_Float",       // 0x53,
        "D16_Unorm_S8_Uint",        // 0x54,
        "D32_Float_S8_Uint",        // 0x55,
        "X9Y9Z9E5_Float",           // 0x56,
        "Bc1_Unorm",                // 0x57,
        "Bc1_Srgb",                 // 0x58,
        "Bc2_Unorm",                // 0x59,
        "Bc2_Srgb",                 // 0x5A,
        "Bc3_Unorm",                // 0x5B,
        "Bc3_Srgb",                 // 0x5C,
        "Bc4_Unorm",                // 0x5D,
        "Bc4_Snorm",                // 0x5E,
        "Bc5_Unorm",                // 0x5F,
        "Bc5_Snorm",                // 0x60,
        "Bc6_Ufloat",               // 0x61,
        "Bc6_Sfloat",               // 0x62,
        "Bc7_Unorm",                // 0x63,
        "Bc7_Srgb",                 // 0x64,
        "Etc2X8Y8Z8_Unorm",         // 0x65,
        "Etc2X8Y8Z8_Srgb",          // 0x66,
        "Etc2X8Y8Z8W1_Unorm",       // 0x67,
        "Etc2X8Y8Z8W1_Srgb",        // 0x68,
        "Etc2X8Y8Z8W8_Unorm",       // 0x69,
        "Etc2X8Y8Z8W8_Srgb",        // 0x6A,
        "Etc2X11_Unorm",            // 0x6B,
        "Etc2X11_Snorm",            // 0x6C,
        "Etc2X11Y11_Unorm",         // 0x6D,
        "Etc2X11Y11_Snorm",         // 0x6E,
        "AstcLdr4x4_Unorm",         // 0x6F,
        "AstcLdr4x4_Srgb",          // 0x70,
        "AstcLdr5x4_Unorm",         // 0x71,
        "AstcLdr5x4_Srgb",          // 0x72,
        "AstcLdr5x5_Unorm",         // 0x73,
        "AstcLdr5x5_Srgb",          // 0x74,
        "AstcLdr6x5_Unorm",         // 0x75,
        "AstcLdr6x5_Srgb",          // 0x76,
        "AstcLdr6x6_Unorm",         // 0x77,
        "AstcLdr6x6_Srgb",          // 0x78,
        "AstcLdr8x5_Unorm",         // 0x79,
        "AstcLdr8x5_Srgb",          // 0x7A,
        "AstcLdr8x6_Unorm",         // 0x7B,
        "AstcLdr8x6_Srgb",          // 0x7C,
        "AstcLdr8x8_Unorm",         // 0x7D,
        "AstcLdr8x8_Srgb",          // 0x7E,
        "AstcLdr10x5_Unorm",        // 0x7F,
        "AstcLdr10x5_Srgb",         // 0x80,
        "AstcLdr10x6_Unorm",        // 0x81,
        "AstcLdr10x6_Srgb",         // 0x82,
        "AstcLdr10x8_Unorm",        // 0x83,
        "AstcLdr10x8_Srgb",         // 0x84,
        "AstcLdr10x10_Unorm",       // 0x85,
        "AstcLdr10x10_Srgb",        // 0x86,
        "AstcLdr12x10_Unorm",       // 0x87,
        "AstcLdr12x10_Srgb",        // 0x88,
        "AstcLdr12x12_Unorm",       // 0x89,
        "AstcLdr12x12_Srgb",        // 0x8A,
        "AstcHdr4x4_Float",         // 0x8B,
        "AstcHdr5x4_Float",         // 0x8C,
        "AstcHdr5x5_Float",         // 0x8D,
        "AstcHdr6x5_Float",         // 0x8E,
        "AstcHdr6x6_Float",         // 0x8F,
        "AstcHdr8x5_Float",         // 0x90,
        "AstcHdr8x6_Float",         // 0x91,
        "AstcHdr8x8_Float",         // 0x92,
        "AstcHdr10x5_Float",        // 0x93,
        "AstcHdr10x6_Float",        // 0x94,
        "AstcHdr10x8_Float",        // 0x95,
        "AstcHdr10x10_Float",       // 0x96,
        "AstcHdr12x10_Float",       // 0x97,
        "AstcHdr12x12_Float",       // 0x98,
        "X8Y8_Z8Y8_Unorm",          // 0x99,
        "X8Y8_Z8Y8_Uscaled",        // 0x9A,
        "Y8X8_Y8Z8_Unorm",          // 0x9B,
        "Y8X8_Y8Z8_Uscaled",        // 0x9C,
        "AYUV",                     // 0x9D,
        "UYVY",                     // 0x9E,
        "VYUY",                     // 0x9F,
        "YUY2",                     // 0xA0,
        "YVY2",                     // 0xA1,
        "YV12",                     // 0xA2,
        "NV11",                     // 0xA3,
        "NV12",                     // 0xA4,
        "NV21",                     // 0xA5,
        "P016",                     // 0xA6,
        "P010",                     // 0xA7,
        "P210",                     // 0xA8,
        "X8_MM_Unorm",              // 0xA9,
        "X8_MM_Uint",               // 0xAA,
        "X8Y8_MM_Unorm",            // 0xAB,
        "X8Y8_MM_Uint",             // 0xAC,
        "X16_MM10_Unorm",           // 0xAD,
        "X16_MM10_Uint",            // 0xAE,
        "X16Y16_MM10_Unorm",        // 0xAF,
        "X16Y16_MM10_Uint",         // 0xB0,
        "P208",                     // 0xB1,
        "X16_MM12_Unorm",
        "X16_MM12_Uint",
        "X16Y16_MM12_Unorm",
        "X16Y16_MM12_Uint",
        "P012",
        "P212",
        "P412",
        "X10Y10Z10W2_Float",
        "Y216",
        "Y210",
        "Y416",
        "Y410",
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ChNumFormat::Count),
                  "The ChNumFormat string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ChNumFormat::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ClearColorType value)
{
    const char*const StringTable[] =
    {
        "Uint",  // 0x0,
        "Sint",  // 0x1,
        "Float", // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    CompareFunc value)
{
    const char*const StringTable[] =
    {
        "Never",        // 0x0,
        "Less",         // 0x1,
        "Equal",        // 0x2,
        "LessEqual",    // 0x3,
        "Greater",      // 0x4,
        "NotEqual",     // 0x5,
        "GreaterEqual", // 0x6,
        "Always",       // 0x7,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(CompareFunc::Count),
                  "The CompareFunc string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(CompareFunc::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    CullMode value)
{
    const char*const StringTable[] =
    {
        "None",          // 0x0,
        "Front",         // 0x1,
        "Back",          // 0x2,
        "FrontAndBack",  // 0x3,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    DepthRange value)
{
    const char*const StringTable[] =
    {
        "ZeroToOne",        // 0x0,
        "NegativeOneToOne", // 0x1,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    DepthClampMode value)
{
    const char* const StringTable[] =
    {
        "Viewport",  // 0x0,
        "None",      // 0x1,
        "ZeroToOne", // 0x2
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    DeviceClockMode value)
{
    const char*const StringTable[] =
    {
        "Default",        // 0x0,
        "Query",          // 0x1,
        "Profiling",      // 0x2,
        "MinimumMemory",  // 0x3,
        "MinimumEngine",  // 0x4,
        "Peak",           // 0x5,
        "QueryProfiling", // 0x6,
        "QueryPeak",      // 0x7,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(DeviceClockMode::Count),
                  "The DeviceClockMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    EngineType value)
{
    Value(GetEngineName(value));
}

// =====================================================================================================================
void LogContext::Enum(
    FaceOrientation value)
{
    const char*const StringTable[] =
    {
        "Ccw", // 0x0,
        "Cw",  // 0x1,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    FillMode value)
{
    const char*const StringTable[] =
    {
        "Points",    // 0x0,
        "Wireframe", // 0x1,
        "Solid",     // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    FlglSupport value)
{
    const char*const StringTable[] =
    {
        "NotAvailable",  // 0x0,
        "NotConnected",  // 0x1,
        "Available",     // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(FlglSupport::Count),
                  "The FlglSupport string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(FlglSupport::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    GpuHeap value)
{
    const char*const StringTable[] =
    {
        "GpuHeapLocal",         // GpuHeapLocal         = 0x0,
        "GpuHeapInvisible",     // GpuHeapInvisible     = 0x1,
        "GpuHeapGartUswc",      // GpuHeapGartUswc      = 0x2,
        "GpuHeapGartCacheable", // GpuHeapGartCacheable = 0x3,
    };

    static_assert(ArrayLen(StringTable) == GpuHeapCount,
                  "The GpuHeap string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < GpuHeapCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    GpuHeapAccess value)
{
    const char* const StringTable[] =
    {
        "GpuHeapAccessExplicit",       // 0x0
        "GpuHeapAccessCpuNoAccess",    // 0x1
        "GpuHeapAccessGpuMostly",      // 0x2
        "GpuHeapAccessCpuReadMostly",  // 0x3
        "GpuHeapAccessCpuWriteMostly", // 0x4
        "GpuHeapAccessCpuMostly",      // 0x5
    };

    static_assert(ArrayLen(StringTable) == GpuHeapAccessCount,
                  "The GpuHeapAccess string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < GpuHeapAccessCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    GpuMemPriority value)
{
    const char*const StringTable[] =
    {
        "Unused",   // Unused    = 0x0,
        "VeryLow",  // VeryLow   = 0x1,
        "Low",      // Low       = 0x2,
        "Normal",   // Normal    = 0x3,
        "High",     // High      = 0x4,
        "VeryHigh", // VeryHigh  = 0x5,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(GpuMemPriority::Count),
                  "The GpuMemPriority string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(GpuMemPriority::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    GpuMemPriorityOffset value)
{
    const char*const StringTable[] =
    {
        "Offset0", // Offset0  = 0x0,
        "Offset1", // Offset1  = 0x1,
        "Offset2", // Offset2  = 0x2,
        "Offset3", // Offset3  = 0x3,
        "Offset4", // Offset4  = 0x4,
        "Offset5", // Offset5  = 0x5,
        "Offset6", // Offset6  = 0x6,
        "Offset7", // Offset7  = 0x7,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(GpuMemPriorityOffset::Count),
                  "The GpuMemPriorityOffset string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(GpuMemPriorityOffset::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    HwPipePoint value)
{
    const char*const StringTable[] =
    {
        "HwPipeTop",              // 0x0,
        "HwPipePostPrefetch",     // 0x1,
        "HwPipePreRasterization", // 0x2,
        "HwPipePostPs",           // 0x3,
        "HwPipePreColorTarget",   // 0x4,
        "HwPipePostCs",           // 0x5,
        "HwPipePostBlt",          // 0x6,
        "HwPipeBottom",           // 0x7,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageRotation value)
{
    const char*const StringTable[] =
    {
        "Ccw0",   // 0x0,
        "Ccw90",  // 0x1,
        "Ccw180", // 0x2,
        "Ccw270", // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageRotation::Count),
                  "The ImageRotation string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageRotation::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageTexOptLevel value)
{
    const char*const StringTable[] =
    {
        "Default",      // 0x0
        "Disabled",     // 0x1
        "Enabled",      // 0x2
        "Maximum",      // 0x3
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageTexOptLevel::Count),
                  "The ImageTexOptLevel string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageTexOptLevel::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageTiling value)
{
    const char*const StringTable[] =
    {
        "Linear",       // 0x0,
        "Optimal",      // 0x1,
        "Standard64Kb", // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageTiling::Count),
                  "The ImageTiling string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageTiling::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageTilingPattern value)
{
    const char*const StringTable[] =
    {
        "Default",     // 0x0,
        "Standard",    // 0x1,
        "XMajor",      // 0x2,
        "YMajor",      // 0x3,
        "Interleaved", // 0x4,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageTilingPattern::Count),
                  "The ImageTilingPattern string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageTilingPattern::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageType value)
{
    const char*const StringTable[] =
    {
        "Tex1d", // 0x0,
        "Tex2d", // 0x1,
        "Tex3d", // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageType::Count),
                  "The ImageType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageType::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImageViewType value)
{
    const char*const StringTable[] =
    {
        "Tex1d",    // 0x0,
        "Tex2d",    // 0x1,
        "Tex3d",    // 0x2,
        "TexCube",  // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImageViewType::Count),
                  "The ImageViewType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImageViewType::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    IndexType value)
{
    const char*const StringTable[] =
    {
        "Idx8",  // 0x0,
        "Idx16", // 0x1,
        "Idx32", // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(IndexType::Count),
                  "The IndexType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(IndexType::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    IndirectParamType value)
{
    const char*const StringTable[] =
    {
        "Draw",           // 0x0
        "DrawIndexed",    // 0x1
        "Dispatch",       // 0x2
        "DispatchMesh",   // 0x3
        "BindIndexData",  // 0x4
        "BindVertexData", // 0x5
        "SetUserData",    // 0x6
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    LogicOp value)
{
    const char*const StringTable[] =
    {
        "Copy",         // 0x0,
        "Clear",        // 0x1,
        "And",          // 0x2,
        "AndReverse",   // 0x3,
        "AndInverted",  // 0x4,
        "Noop",         // 0x5,
        "Xor",          // 0x6,
        "Or",           // 0x7,
        "Nor",          // 0x8,
        "Equiv",        // 0x9,
        "Invert",       // 0xA,
        "OrReverse",    // 0xB,
        "CopyInverted", // 0xC,
        "OrInverted",   // 0xD,
        "Nand",         // 0xE,
        "Set",          // 0xF,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    MetadataMode value)
{
    const char*const StringTable[] =
    {
        "Default",
        "OptForTexFetchPerf",
        "Disabled",
        "Count",
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    MgpuMode value)
{
    const char*const StringTable[] =
    {
        "MgpuModeOff",  // 0x0,
        "MgpuModeSw",   // 0x1,
        "MgpuModeDvo",  // 0x2,
        "MgpuModeXdma", // 0x3,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    MipFilter value)
{
    const char*const StringTable[] =
    {
        "MipFilterNone",   // 0x0,
        "MipFilterPoint",  // 0x1,
        "MipFilterLinear", // 0x2,
    };

    static_assert(ArrayLen(StringTable) == MipFilterCount,
                  "The MipFilter string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < MipFilterCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    NullGpuId value)
{
    const char* const StringTable[] =
    {
        "Polaris10",
        "Polaris11",
        "Polaris12",
        nullptr,

        "Vega10",
        "Raven",
        "Vega12",
        "Vega20",
        "Raven2",
        "Renoir",

        "Navi10",
        "Navi12",
        nullptr,
        "Navi14",
        nullptr,
        "Navi21",
        "Navi22",
        "Navi23",
        "Navi24",
        nullptr,
        "Rembrandt",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
#if PAL_BUILD_NAVI31
        "Navi31",
#else
        nullptr,
#endif
        nullptr,
        nullptr,
        nullptr,
        "Raphael",
        nullptr,
        nullptr,
        nullptr,
        "Max",
        "All",
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(NullGpuId::All) + 1,
                  "The NullGpuId string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT((idx <= static_cast<uint32>(NullGpuId::All)) && (StringTable[idx] != nullptr));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PipelineBindPoint value)
{
    const char*const StringTable[] =
    {
        "Compute",     // 0x0,
        "Graphics",    // 0x1,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(PipelineBindPoint::Count),
                  "The PipelineBindPoint string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(PipelineBindPoint::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PointOrigin value)
{
    const char*const StringTable[] =
    {
        "UpperLeft", // 0x0,
        "LowerLeft", // 0x1,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PowerProfile value)
{
    const char*const StringTable[] =
    {
        "Default",   // 0x0,
        "VrCustom",  // 0x1,
        "VrDefault", // 0x2,
        "Idle",      // 0x3,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PredicateType value)
{
    const char*const StringTable[] =
    {
        "",          // 0x0,
        "Zpass",     // 0x1,
        "PrimCount", // 0x2,
        "Boolean",   // 0x3,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PresentMode value)
{
    const char*const StringTable[] =
    {
        "Unknown",   // 0x0,
        "Windowed",  // 0x1,
        "Fullscreen" // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(PresentMode::Count),
                  "The PresentMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(PresentMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrimitiveType value)
{
    const char*const StringTable[] =
    {
        "Point",    // 0x0,
        "Line",     // 0x1,
        "Triangle", // 0x2,
        "Rect",     // 0x3,
        "Quad",     // 0x4,
        "Patch",    // 0x5,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrimitiveTopology value)
{
    const char*const StringTable[] =
    {
        "PointList",        // 0x0,
        "LineList",         // 0x1,
        "LineStrip",        // 0x2,
        "TriangleList",     // 0x3,
        "TriangleStrip",    // 0x4,
        "RectList",         // 0x5,
        "QuadList",         // 0x6,
        "QuadStrip",        // 0x7,
        "LineListAdj",      // 0x8,
        "LineStripAdj",     // 0x9,
        "TriangleListAdj",  // 0xA,
        "TriangleStripAdj", // 0xB,
        "Patch",            // 0xC,
        "TriangleFan",      // 0xD,
        "LineLoop",         // 0xE,
        "Polygon",          // 0xF,
        "TwoDRectList",     // 0x10,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrivateDisplayColorDepth value)
{
    const char*const StringTable[] =
    {
        "ColorDepth666",    // 0x0,
        "ColorDepth888",    // 0x1,
        "ColorDepth101010", // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrivateDisplayPixelEncoding value)
{
    const char*const StringTable[] =
    {
        "Rgb",      // 0x0,
        "YcbCr422", // 0x1,
        "YcbCr444", // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrivateDisplayPowerState value)
{
    const char*const StringTable[] =
    {
        "",         // 0x0,
        "PowerOn",  // 0x1,
        "PowerOff", // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrivateScreenType value)
{
    const char*const StringTable[] =
    {
        "Permanent", // 0x0,
        "Temporary", // 0x1,
        "Emulated",  // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ProvokingVertex value)
{
    const char*const StringTable[] =
    {
        "First", // 0x0,
        "Last",  // 0x1,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    QueryPoolType value)
{
    const char*const StringTable[] =
    {
        "Occlusion",        // 0x0,
        "PipelineStats",    // 0x1,
        "StreamoutStats",   // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(QueryPoolType::Count),
                  "The QueryPoolType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(QueryPoolType::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    QueryType value)
{
    const char*const StringTable[] =
    {
        "Occlusion",        // 0x0,
        "BinaryOcclusion",  // 0x1,
        "PipelineStats",    // 0x2,
        "StreamoutStats",   // 0x3,
        "StreamoutStats1",  // 0x4,
        "StreamoutStats2",  // 0x5,
        "StreamoutStats3",  // 0x6,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    QueuePriority value)
{
    const char*const StringTable[] =
    {
        "Low",    // 0x0,
        "Medium", // 0x1,
        "High",   // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    QueueType value)
{
    const char*const StringTable[] =
    {
        "QueueTypeUniversal",   // 0x0,
        "QueueTypeCompute",     // 0x1,
        "QueueTypeDma",         // 0x2,
        "QueueTypeTimer",       // 0x3,
    };

    static_assert(ArrayLen(StringTable) == QueueTypeCount,
                  "The QueueType string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < QueueTypeCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ReclaimResult value)
{
    const char*const StringTable[] =
    {
        "Ok",           // 0
        "Discarded",    // 1
        "NotCommitted", // 2
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ReclaimResult::Count),
                  "The ReclaimResult string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ReclaimResult::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ResolveMode value)
{
    const char*const StringTable[] =
    {
        "Average",      // 0x0,
        "Minimum",      // 0x1,
        "Maximum",      // 0x2,
        "Decompress",   // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ResolveMode::Count),
                  "The ResolveMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ResolveMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    Result value)
{
    if (Util::IsErrorResult(value))
    {
        // The error-codes start at -1 and decrease. There are gaps in this table from when we retired old errors.
        const char*const StringTable[] =
        {
            "ErrorUnknown",                           // -(0x00000001),
            "ErrorUnavailable",                       // -(0x00000002),
            "ErrorInitializationFailed",              // -(0x00000003),
            "ErrorOutOfMemory",                       // -(0x00000004),
            "ErrorOutOfGpuMemory",                    // -(0x00000005),
            "",
            "ErrorDeviceLost",                        // -(0x00000007),
            "ErrorInvalidPointer",                    // -(0x00000008),
            "ErrorInvalidValue",                      // -(0x00000009),
            "ErrorInvalidOrdinal",                    // -(0x0000000A),
            "ErrorInvalidMemorySize",                 // -(0x0000000B),
            "ErrorInvalidFlags",                      // -(0x0000000C),
            "ErrorInvalidAlignment",                  // -(0x0000000D),
            "ErrorInvalidFormat",                     // -(0x0000000E),
            "ErrorInvalidImage",                      // -(0x0000000F),
            "ErrorInvalidDescriptorSetData",          // -(0x00000010),
            "ErrorInvalidQueueType",                  // -(0x00000011),
            "ErrorInvalidObjectType",                 // -(0x00000012),
            "ErrorUnsupportedShaderIlVersion",        // -(0x00000013),
            "ErrorBadShaderCode",                     // -(0x00000014),
            "ErrorBadPipelineData",                   // -(0x00000015),
            "ErrorTooManyMemoryReferences",           // -(0x00000016),
            "ErrorNotMappable",                       // -(0x00000017),
            "ErrorGpuMemoryMapFailed",                // -(0x00000018),
            "ErrorGpuMemoryUnmapFailed",              // -(0x00000019),
            "ErrorIncompatibleDevice",                // -(0x0000001A),
            "ErrorIncompatibleLibrary",               // -(0x0000001B),
            "ErrorIncompleteCommandBuffer",           // -(0x0000001C),
            "ErrorBuildingCommandBuffer",             // -(0x0000001D),
            "ErrorGpuMemoryNotBound",                 // -(0x0000001E),
            "ErrorIncompatibleQueue",                 // -(0x0000001F),
            "ErrorNotShareable",                      // -(0x00000020),
            "ErrorFullscreenUnavailable",             // -(0x00000021),
            "ErrorScreenRemoved",                     // -(0x00000022),
            "ErrorIncompatibleScreenMode",            // -(0x00000023),
            "ErrorMultiDevicePresentFailed",          // -(0x00000024),
            "ErrorWindowedPresentUnavailable",        // -(0x00000025),
            "ErrorInvalidResolution",                 // -(0x00000026),
            "ErrorThreadGroupTooBig",                 // -(0x00000027),
            "ErrorInvalidImageTargetUsage",           // -(0x00000028),
            "ErrorInvalidColorTargetType",            // -(0x00000029),
            "ErrorInvalidDepthTargetType",            // -(0x0000002A),
            "ErrorMissingDepthStencilUsage",          // -(0x0000002B),
            "ErrorInvalidMsaaMipLevels",              // -(0x0000002C),
            "ErrorInvalidMsaaFormat",                 // -(0x0000002D),
            "ErrorInvalidMsaaType",                   // -(0x0000002E),
            "ErrorInvalidSampleCount",                // -(0x0000002F),
            "ErrorInvalidCompressedImageType",        // -(0x00000030),
            "",
            "ErrorInvalidUsageForFormat",             // -(0x00000032),
            "ErrorInvalidImageArraySize",             // -(0x00000033),
            "ErrorInvalid3dImageArraySize",           // -(0x00000034),
            "ErrorInvalidImageWidth",                 // -(0x00000035),
            "ErrorInvalidImageHeight",                // -(0x00000036),
            "ErrorInvalidImageDepth",                 // -(0x00000037),
            "ErrorInvalidMipCount",                   // -(0x00000038),
            "ErrorFormatIncompatibleWithImageUsage",  // -(0x00000039),
            "ErrorImagePlaneUnavailable",             // -(0x0000003A),
            "ErrorFormatIncompatibleWithImageFormat", // -(0x0000003B),
            "ErrorFormatIncompatibleWithImagePlane",  // -(0x0000003C),
            "ErrorImageNotShaderAccessible",          // -(0x0000003D),
            "ErrorInvalidFormatSwizzle",              // -(0x0000003E),
            "ErrorInvalidBaseMipLevel",               // -(0x0000003F),
            "ErrorInvalidViewArraySize",              // -(0x00000040),
            "ErrorInvalidViewBaseSlice",              // -(0x00000041),
            "ErrorViewTypeIncompatibleWithImageType", // -(0x00000042),
            "ErrorInsufficientImageArraySize",        // -(0x00000043),
            "ErrorCubemapIncompatibleWithMsaa",       // -(0x00000044),
            "ErrorCubemapNonSquareFaceSize",          // -(0x00000045),
            "ErrorImageFmaskUnavailable",             // -(0x00000046),
            "ErrorPrivateScreenRemoved",              // -(0x00000047),
            "ErrorPrivateScreenUsed",                 // -(0x00000048),
            "ErrorTooManyPrivateDisplayImages",       // -(0x00000049),
            "ErrorPrivateScreenNotEnabled",           // -(0x0000004A),
            "ErrorTooManyPrivateScreens",             // -(0x0000004B),
            "ErrorMismatchedImageRowPitch",           // -(0x0000004C),
            "ErrorMismatchedImageDepthPitch",         // -(0x0000004D),
            "ErrorTooManyPresentableImages",          // -(0x0000004E),
            "ErrorFenceNeverSubmitted",               // -(0x0000004F),
            "ErrorPrivateScreenInvalidFormat",        // -(0x00000050),
            "ErrorPrivateScreenInvalidTiming",        // -(0x00000051),
            "ErrorPrivateScreenInvalidResolution",    // -(0x00000052),
            "ErrorPrivateScreenInvalidScaling",       // -(0x00000053),
            "ErrorInvalidYuvImageType",               // -(0x00000054),
            "ErrorShaderCacheHashCollision",          // -(0x00000055),
            "ErrorShaderCacheFull",                   // -(0x00000056),
            "ErrorGpuPageFaultDetected",              // -(0x00000057),
            "ErrorUnsupportedPipelineElfAbiVersion",  // -(0x00000058),
            "ErrorInvalidPipelineElf",                // -(0x00000059),
            "ErrorIncompleteResults",                 // -(0x00000060),
        };

        const uint32 idx = static_cast<uint32>(-(1 + static_cast<int32>(value)));
        PAL_ASSERT(idx < ArrayLen(StringTable));

        Value(StringTable[idx]);
    }
    else
    {
        // The non-error-codes start at zero and increase.
        const char*const StringTable[] =
        {
            "Success",                     // 0x00000000,
            "Unsupported",                 // 0x00000001,
            "NotReady",                    // 0x00000002,
            "Timeout",                     // 0x00000003,
            "EventSet",                    // 0x00000004,
            "EventReset",                  // 0x00000005,
            "TooManyFlippableAllocations", // 0x00000006,
            "PresentOccluded",             // 0x00000007,
            "AlreadyExists",               // 0x00000008,
            "OutOfSpec",                   // 0x00000009,
            "NotFound",                    // 0x0000000A,
            "Eof",                         // 0x0000000B,
        };

        const uint32 idx = static_cast<uint32>(value);
        PAL_ASSERT(idx < ArrayLen(StringTable));

        Value(StringTable[idx]);
    }
}

// =====================================================================================================================
void LogContext::Enum(
    ShadeMode value)
{
    const char*const StringTable[] =
    {
        "Gouraud", // 0x0,
        "Flat",    // 0x1,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    StencilOp value)
{
    const char*const StringTable[] =
    {
        "Keep",     // 0x0,
        "Zero",     // 0x1,
        "Replace",  // 0x2,
        "IncClamp", // 0x3,
        "DecClamp", // 0x4,
        "Invert",   // 0x5,
        "IncWrap",  // 0x6,
        "DecWrap",  // 0x7,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(StencilOp::Count),
                  "The StencilOp string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(StencilOp::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    SubmitOptMode value)
{
    const char*const StringTable[] =
    {
        "Default",           // 0x0,
        "Disabled",          // 0x1,
        "MinKernelSubmits",  // 0x2,
        "MinGpuCmdOverhead", // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(SubmitOptMode::Count),
                  "The SubmitOptMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(SubmitOptMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    SurfaceTransformFlags value)
{
    // Note that these are flag enums but you can only specify one at a time in the interface.
    PAL_ASSERT(IsPowerOfTwo(value));

    const char*const StringTable[] =
    {
        "SurfaceTransformNone",          // 0x00000001,
        "SurfaceTransformRot90",         // 0x00000002,
        "SurfaceTransformRot180",        // 0x00000004,
        "SurfaceTransformRot270",        // 0x00000008,
        "SurfaceTransformHMirror",       // 0x00000010,
        "SurfaceTransformHMirrorRot90",  // 0x00000020,
        "SurfaceTransformHMirrorRot180", // 0x00000040,
        "SurfaceTransformHMirrorRot270", // 0x00000080,
        "SurfaceTransformInherit",       // 0x00000100,
    };

    const uint32 idx = Log2(static_cast<uint32>(value));
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    SwapChainMode value)
{
    const char*const StringTable[] =
    {
        "Immediate",   // 0x0,
        "Mailbox",     // 0x1,
        "Fifo",        // 0x2,
        "FifoRelaxed", // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(SwapChainMode::Count),
                  "The SwapChainMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(SwapChainMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    TexAddressMode value)
{
    const char*const StringTable[] =
    {
        "Wrap",                  // 0x0,
        "Mirror",                // 0x1,
        "Clamp",                 // 0x2,
        "MirrorOnce",            // 0x3,
        "ClampBorder",           // 0x4,
        "MirrorClampHalfBorder", // 0x5,
        "ClampHalfBorder",       // 0x6,
        "MirrorClampBorder",     // 0x7,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(TexAddressMode::Count),
                  "The TexAddressMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(TexAddressMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    TexFilterMode value)
{
    const char*const StringTable[] =
    {
        "Blend", // 0x0,
        "Min",   // 0x1,
        "Max",   // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    TilingOptMode value)
{
    const char*const StringTable[] =
    {
        "Balanced",       // 0x0,
        "OptForSpace",    // 0x1,
        "OptForSpeed",    // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(TilingOptMode::Count),
                  "The TilingOptMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(TilingOptMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    VaRange value)
{
    const char*const StringTable[] =
    {
        "Default",               // 0x0,
        "DescriptorTable",       // 0x1,
        "ShadowDescriptorTable", // 0x2,
        "Svm",                   // 0x3,
        "CaptureReplay",         // 0x4,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(VaRange::Count),
                  "The VaRange string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(VaRange::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrtPlusResolveType value)
{
    const char*const StringTable[] =
    {
        "Decode",  // 0x0,
        "Encode",  // 0x1,
    };

    static_assert(Util::ArrayLen(StringTable) == static_cast<uint32>(PrtPlusResolveType::Count),
                  "The PRT resolve type table needs to be updated");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrtMapAccessType value)
{
    const char*const StringTable[] =
    {
        "Raw",                 // 0x0,
        "Read",                // 0x1,
        "WriteMin",            // 0x2,
        "WriteMax",            // 0x3,
        "WriteSamplingStatus", // 0x4,
    };

    static_assert(Util::ArrayLen(StringTable) == static_cast<uint32>(PrtMapAccessType::Count),
                  "The PRT map access table needs to be updated");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    PrtMapType value)
{
    const char*const StringTable[] =
    {
        "None",               // 0x0,
        "Residency",          // 0x1,
        "SamplingStatus",     // 0x2,
    };

    static_assert(Util::ArrayLen(StringTable) == static_cast<uint32>(PrtMapType::Count),
                  "The PRT map type access table needs to be updated");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    VrsShadingRate value)
{
    const char*const StringTable[] =
    {
        "16xSsaa", // 0x0,
        "8xSsaa",  // 0x1,
        "4xSsaa",  // 0x2,
        "2xSsaa",  // 0x3,
        "1x1",     // 0x4,
        "1x2",     // 0x5,
        "2x1",     // 0x6,
        "2x2",     // 0x7,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    VrsCombiner value)
{
    const char*const StringTable[] =
    {
        "Passthrough", // 0
        "Override",    // 1
        "Min",         // 2
        "Max",         // 3
        "Sum",         // 4
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    VirtualGpuMemAccessMode value)
{
    const char*const StringTable[] =
    {
        "Undefined", // 0x0,
        "NoAccess",  // 0x1,
        "ReadZero",  // 0x2,
    };

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    WsiPlatform value)
{
    // Note that these are flag enums but you can only specify one at a time in the interface.
    PAL_ASSERT(IsPowerOfTwo(value));

    const char*const StringTable[] =
    {
        "Win32",   // 0x00000001,
        "Xcb",     // 0x00000002,
        "Xlib",    // 0x00000004,
        "Wayland", // 0x00000008,
        "Mir",     // 0x00000010,
    };

    const uint32 idx = Log2(static_cast<uint32>(value));
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    XyFilter value)
{
    const char*const StringTable[] =
    {
        "XyFilterPoint",             // 0x0,
        "XyFilterLinear",            // 0x1,
        "XyFilterAnisotropicPoint",  // 0x2,
        "XyFilterAnisotropicLinear", // 0x3,
    };

    static_assert(ArrayLen(StringTable) == XyFilterCount,
                  "The XyFilter string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < XyFilterCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ZFilter value)
{
    const char*const StringTable[] =
    {
        "ZFilterNone",   // 0x0,
        "ZFilterPoint",  // 0x1,
        "ZFilterLinear", // 0x2,
    };

    static_assert(ArrayLen(StringTable) == ZFilterCount,
                  "The ZFilter string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ZFilterCount);

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    VirtualDisplayVSyncMode value)
{
    const char*const StringTable[] =
    {
        "Default",      // 0x0,
        "Immediate",    // 0x1,
        "HMD",          // 0x2,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(VirtualDisplayVSyncMode::Count),
        "The VirtualDisplayVSyncMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(VirtualDisplayVSyncMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    ImmediateDataWidth value)
{
    const char*const StringTable[] =
    {
        "ImmediateData32Bit",    // 0x0,
        "ImmediateData64Bit",    // 0x1,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(ImmediateDataWidth::Count),
        "The ImmediateDataWidth string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(ImmediateDataWidth::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    TurboSyncControlMode value)
{
    const char*const StringTable[] =
    {
        "Disable",            // 0x0,
        "Enable",             // 0x1,
        "UpdateAllocations",  // 0x2,
        "Register",           // 0x3,
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(TurboSyncControlMode::Count),
        "The TurboSyncControlMode string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(TurboSyncControlMode::Count));

    Value(StringTable[idx]);
}

// =====================================================================================================================
void LogContext::Enum(
    BoxSortHeuristic value)
{
    const char* const StringTable[] =
    {
        "ClosestFirst",     // 0x0,
        "LargestFirst",     // 0x1,
        "ClosestMidPoint",  // 0x2,
        "Disabled",         // 0x3,
    };

    static_assert(Util::ArrayLen(StringTable) == static_cast<uint32>(BoxSortHeuristic::Count),
        "The BoxSortHeuristic type table needs to be updated");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < ArrayLen(StringTable));

    Value(StringTable[idx]);
}

} // InterfaceLogger
} // Pal

#endif
