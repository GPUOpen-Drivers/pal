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
 * @file  pal.h
 * @brief Common include for the Platform Abstraction Library (PAL) interface.  Defines common types, enums, etc.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"

// Forward declarations of global types (must be done outside of Pal namespace).

/// Library-wide namespace encapsulating all PAL entities.
namespace Pal
{

typedef Util::int8    int8;     ///< 8-bit integer.
typedef Util::int16   int16;    ///< 16-bit integer.
typedef Util::int32   int32;    ///< 32-bit integer.
typedef Util::int64   int64;    ///< 64-bit integer.
typedef Util::uint8   uint8;    ///< Unsigned 8-bit integer.
typedef Util::uint16  uint16;   ///< Unsigned 16-bit integer.
typedef Util::uint32  uint32;   ///< Unsigned 32-bit integer.
typedef Util::uint64  uint64;   ///< Unsigned 64-bit integer.
typedef Util::gpusize gpusize;  ///< Used to specify GPU addresses and sizes of GPU allocations.  This differs from
                                ///  size_t since the GPU still uses 64-bit addresses on a 32-bit OS.
typedef Util::Result  Result;   ///< The PAL core and utility companion share the same result codes for convenience.

typedef Util::Rational Rational; ///< A ratio of two unsigned integers.

typedef void*       OsDisplayHandle;  ///< The Display Handle for Linux except X11 platform
typedef uint32      OsExternalHandle; ///< OsExternalHandle corresponds to a generic handle on linux

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 392
/// OsWindowHandle corresponds to a window on X-Windows or surface on Wayland.
union OsWindowHandle
{
    void*  pSurface;  ///< Native surface handle in wayland is a pointer.
    uint32 win;       ///< Native window handle in X is a 32-bit integer.
};
constexpr OsWindowHandle NullWindowHandle = {nullptr}; ///< Value representing a null or invalid window handle.
#else
typedef uint32 OsWindowHandle; ///< OsWindowHandle corresponds to a Window on X-Windows
constexpr OsWindowHandle NullWindowHandle = 0u; ///< Value representing a null or invalid window handle.
#endif

constexpr uint32 InvalidVidPnSourceId     = ~0u; ///< In cases where PAL cannot abstract a Windows VidPnSourceId, this
                                                 ///  represents an invalid value. (Note: zero is a valid value.)

constexpr uint32 MaxColorTargets          = 8;   ///< Maximum number of color targets.
constexpr uint32 MaxStreamOutTargets      = 4;   ///< Maximum number of stream output target buffers.
constexpr uint32 MaxDescriptorSets        = 2;   ///< Maximum number of descriptor sets.
constexpr uint32 MaxMsaaRasterizerSamples = 16;  ///< Maximum number of MSAA samples supported by the rasterizer.
constexpr uint32 MaxAvailableEngines      = 8;   ///< Maximum number of engines for a particular engine type.

/// Specifies a category of GPU engine.  Each category corresponds directly to a hardware engine. There may be multiple
/// engines available for a given type; the available engines on a particular GPU can be queried via
/// Device::GetProperties, returned in DeviceProperties.engineProperties[].
enum EngineType : uint32
{
    /// Corresponds to the graphics hardware engine (a.k.a. graphcis ring a.k.a 3D).
    EngineTypeUniversal        = 0x0,

    /// Corresponds to asynchronous compute engines (ACE).
    EngineTypeCompute          = 0x1,

    /// Corresponds to asynchronous compute engines (ACE) which are exclusively owned by one client at a time.
    EngineTypeExclusiveCompute = 0x2,

    /// Corresponds to SDMA engines.
    EngineTypeDma              = 0x3,

    /// Virtual engine that only supports inserting sleeps, used for implementing frame-pacing.
    EngineTypeTimer            = 0x4,

        /// Corresponds to a hw engine that supports all operations (graphics and compute)
        EngineTypeHighPriorityUniversal = 0x5,

        /// Corresponds to a hw engine that supports only graphics operations
        EngineTypeHighPriorityGraphics  = 0x6,
    /// Number of engine types.
    EngineTypeCount,
};

/// Specifies a category of GPU work.  Each queue type only supports specific types of work. Determining which
/// QueueTypes are supported on which engines can be queried via IDevice::GetProperties, returned in
/// DeviceProperties.engineProperties[].
enum QueueType : uint32
{
    /// Supports graphics commands (draws), compute commands (dispatches), and copy commands.
    QueueTypeUniversal   = 0x0,

    /// Supports compute commands (dispatches), and copy commands.
    QueueTypeCompute     = 0x1,

    /// Supports copy commands.
    QueueTypeDma         = 0x2,

    /// Virtual engine that only supports inserting sleeps, used for implementing frame pacing.  This is a software-only
    /// queue.
    QueueTypeTimer       = 0x3,

    /// Number of queue types.
    QueueTypeCount,
};

/// Defines flags for describing which queues are supported.
enum QueueTypeSupport : uint32
{
    SupportQueueTypeUniversal   = (1 << static_cast<uint32>(QueueTypeUniversal)),
    SupportQueueTypeCompute     = (1 << static_cast<uint32>(QueueTypeCompute)),
    SupportQueueTypeDma         = (1 << static_cast<uint32>(QueueTypeDma)),
    SupportQueueTypeTimer       = (1 << static_cast<uint32>(QueueTypeTimer)),
};

/// Selects one of a few possible memory heaps accessible by a GPU.
enum GpuHeap : uint32
{
    GpuHeapLocal         = 0x0,  ///< Local heap visible to the CPU.
    GpuHeapInvisible     = 0x1,  ///< Local heap not visible to the CPU.
    GpuHeapGartUswc      = 0x2,  ///< GPU-accessible uncached system memory.
    GpuHeapGartCacheable = 0x3,  ///< GPU-accessible cached system memory.
    GpuHeapCount
};

/// Comparison function determines how a pass/fail condition is determined between two values.  For depth/stencil
/// comparison, the first value comes from source data and the second value comes from destination data.
enum class CompareFunc : uint32
{
    Never        = 0x0,
    Less         = 0x1,
    Equal        = 0x2,
    LessEqual    = 0x3,
    Greater      = 0x4,
    NotEqual     = 0x5,
    GreaterEqual = 0x6,
    _Always      = 0x7,

    // Unfortunately for Linux clients, X.h includes a "#define Always 2" macro.  Clients have their choice of either
    // undefing Always before including this header or using _Always when dealing with PAL.
#ifndef Always
    Always       = _Always,
#endif

    Count
};

/// Specifies the format for an image or GPU memory view.
///
/// This defines the bit layout of the channels and how the value in each channel is interpreted.
///
/// Channels are listed in DX10+-style order, where the least significant channels are listed first.  For example, for
/// a uint32 val with an X8Y8Z8W8 value: X = val & 0xFF, Y = (val >> 8) & 0xFF, Z = (val >> 16) & 0xFF,
/// W = (val >> 24) & 0xFF.  Enums without a more detailed explanation can be decoded in this manner.  Multimedia,
/// or "YUV" formats are all exceptions to this rule.  Each of those formats explicitly describes how their channels are
/// organized.
///
/// Many of the multimedia (i.e., "YUV") formats are encoded such that the chrominance (chroma, CbCr, UV) samples are
/// stored at a lower resolution than the luminance (luma, Y) samples as a form of compression.  The ratio of the
/// subsampling is often referred to using an A:B:C notation, where the trio of numbers A,B,C are used to describe the
/// number of luma and chroma samples in a hypothetical region which is A pixels wide and 2 pixels high.  The three
/// numbers usually refer to the following quantities by convention:
///  A --> Width of the conceptual region of pixels, and is usually 4.
///  B --> Number of chroma samples in the first of two rows of A pixels.
///  C --> Number of changes of chroma samples between the first and second rows of A pixels.
///
/// Common examples of this notation are described below:
///  4:4:4 --> No chroma subsampling because luma and chroma both have 4 samples per row, and the number of chroma and
///            luma rows is the same.
///  4:1:1 --> Each row of 4 pixels has 1 chroma sample, and the number of chroma and luma rows is the same.
///  4:2:0 --> Each row of 4 pixels has 2 chroma samples, and there is only 1 chroma row for every 2 luma rows.
///  4:2:2 --> Each row of 4 pixels has 2 chroma samples, and the number of chroma and luma rows is the same.
///
/// Because of the subsampling ratios for multimedia formats, there are some restrictions on what dimensions can be used
/// when creating Images of these formats. 4:1:1 formats must have widths specified as a multiple of 4. 4:2:0 formats
/// must have widths and heights specified as multiples of 2. 4:2:2 formats must have widths specified as a multiple of
/// 2. 4:4:4 formats have no dimensional restrictions.
///
/// Additionally, the YUV formats are broadly grouped into two categories: packed and planar formats.  Packed formats
/// interleave the luma and chroma samples in each row of pixels.  Planar formats are organized so that all of the luma
/// samples are together, followed by all of the chroma samples.  Some planar formats interleave the U and V chroma
/// data, while some choose to have separate U and V planes.  Both packed and planar formats can have any subsampling
/// ratio between the luma and chroma data.
enum class ChNumFormat : uint32
{
    Undefined                = 0x0,     ///< Used in situations where no format is needed, like raw memory views, or to
                                        ///  indicate no color/depth target will be attached when creating a graphics
                                        ///  pipeline.
    X1_Unorm                 = 0x1,     ///< _Untested._
    X1_Uscaled               = 0x2,     ///< _Untested._
    X4Y4_Unorm               = 0x3,
    X4Y4_Uscaled             = 0x4,
    L4A4_Unorm               = 0x5,
    X4Y4Z4W4_Unorm           = 0x6,
    X4Y4Z4W4_Uscaled         = 0x7,
    X5Y6Z5_Unorm             = 0x8,
    X5Y6Z5_Uscaled           = 0x9,
    X5Y5Z5W1_Unorm           = 0xA,
    X5Y5Z5W1_Uscaled         = 0xB,
    X1Y5Z5W5_Unorm           = 0xC,
    X1Y5Z5W5_Uscaled         = 0xD,
    X8_Unorm                 = 0xE,
    X8_Snorm                 = 0xF,
    X8_Uscaled               = 0x10,
    X8_Sscaled               = 0x11,
    X8_Uint                  = 0x12,
    X8_Sint                  = 0x13,
    X8_Srgb                  = 0x14,
    A8_Unorm                 = 0x15,
    L8_Unorm                 = 0x16,
    P8_Uint                  = 0x17,
    X8Y8_Unorm               = 0x18,
    X8Y8_Snorm               = 0x19,
    X8Y8_Uscaled             = 0x1A,
    X8Y8_Sscaled             = 0x1B,
    X8Y8_Uint                = 0x1C,
    X8Y8_Sint                = 0x1D,
    X8Y8_Srgb                = 0x1E,
    L8A8_Unorm               = 0x1F,
    X8Y8Z8W8_Unorm           = 0x20,
    X8Y8Z8W8_Snorm           = 0x21,
    X8Y8Z8W8_Uscaled         = 0x22,
    X8Y8Z8W8_Sscaled         = 0x23,
    X8Y8Z8W8_Uint            = 0x24,
    X8Y8Z8W8_Sint            = 0x25,
    X8Y8Z8W8_Srgb            = 0x26,
    U8V8_Snorm_L8W8_Unorm    = 0x27,    ///< Mixed signed/unsigned format. Valid Image and Color-Target View formats
                                        ///  are X8Y8Z8W8_Snorm (to target U8V8_Snorm) and X8Y8Z8W8_Unorm (to target
                                        ///  L8W8_Unorm).
    X10Y11Z11_Float          = 0x28,
    X11Y11Z10_Float          = 0x29,
    X10Y10Z10W2_Unorm        = 0x2A,
    X10Y10Z10W2_Snorm        = 0x2B,
    X10Y10Z10W2_Uscaled      = 0x2C,
    X10Y10Z10W2_Sscaled      = 0x2D,
    X10Y10Z10W2_Uint         = 0x2E,
    X10Y10Z10W2_Sint         = 0x2F,
    X10Y10Z10W2Bias_Unorm    = 0x30,    ///< A four-component, 32-bit 2.8-biased fixed-point format that supports 10
                                        ///  bits for each color channel and 2-bit alpha. A shader must be aware of
                                        ///  *Bias* and must perform its own bias and scale on any data that is read
                                        ///  from or written.
    U10V10W10_Snorm_A2_Unorm = 0X31,    ///< Mixed signed/unsigned format. Valid Image and Color-Target View formats
                                        ///  are X10Y10Z10W2_Snorm (to target U10V10W10_Snorm) and X10Y10Z10W2_Unorm
                                        ///  (to target A2_Unorm).
    X16_Unorm                = 0x32,
    X16_Snorm                = 0x33,
    X16_Uscaled              = 0x34,
    X16_Sscaled              = 0x35,
    X16_Uint                 = 0x36,
    X16_Sint                 = 0x37,
    X16_Float                = 0x38,
    L16_Unorm                = 0x39,
    X16Y16_Unorm             = 0x3A,
    X16Y16_Snorm             = 0x3B,
    X16Y16_Uscaled           = 0x3C,
    X16Y16_Sscaled           = 0x3D,
    X16Y16_Uint              = 0x3E,
    X16Y16_Sint              = 0x3F,
    X16Y16_Float             = 0x40,
    X16Y16Z16W16_Unorm       = 0x41,
    X16Y16Z16W16_Snorm       = 0x42,
    X16Y16Z16W16_Uscaled     = 0x43,
    X16Y16Z16W16_Sscaled     = 0x44,
    X16Y16Z16W16_Uint        = 0x45,
    X16Y16Z16W16_Sint        = 0x46,
    X16Y16Z16W16_Float       = 0x47,
    X32_Uint                 = 0x48,
    X32_Sint                 = 0x49,
    X32_Float                = 0x4A,
    X32Y32_Uint              = 0x4B,
    X32Y32_Sint              = 0x4C,
    X32Y32_Float             = 0x4D,
    X32Y32Z32_Uint           = 0x4E,
    X32Y32Z32_Sint           = 0x4F,
    X32Y32Z32_Float          = 0x50,
    X32Y32Z32W32_Uint        = 0x51,
    X32Y32Z32W32_Sint        = 0x52,
    X32Y32Z32W32_Float       = 0x53,
    D16_Unorm_S8_Uint        = 0x54,
    D32_Float_S8_Uint        = 0x55,
    X9Y9Z9E5_Float           = 0x56,    ///< Three partial-precision floating-point numbers encoded into a single 32-bit
                                        ///  value all sharing the same 5-bit exponent (variant of s10e5, which is sign
                                        ///  bit, 10-bit mantissa, and 5-bit biased (15) exponent). There is no sign
                                        ///  bit, and there is a shared 5-bit biased (15) exponent and a 9-bit mantissa
                                        ///  for each channelShared exponent format.
    Bc1_Unorm                = 0x57,    ///< [BC1](http://tinyurl.com/kejao56) compressed texture format.
    Bc1_Srgb                 = 0x58,    ///< [BC1](http://tinyurl.com/kejao56) compressed texture format.
    Bc2_Unorm                = 0x59,    ///< [BC2](http://tinyurl.com/kxtubtj) compressed texture format.
    Bc2_Srgb                 = 0x5A,    ///< [BC2](http://tinyurl.com/kxtubtj) compressed texture format.
    Bc3_Unorm                = 0x5B,    ///< [BC3](http://tinyurl.com/kwa65u3) compressed texture format.
    Bc3_Srgb                 = 0x5C,    ///< [BC3](http://tinyurl.com/kwa65u3) compressed texture format.
    Bc4_Unorm                = 0x5D,    ///< [BC4](http://tinyurl.com/lvouv7q) compressed texture format.
    Bc4_Snorm                = 0x5E,    ///< [BC4](http://tinyurl.com/lvouv7q) compressed texture format.
    Bc5_Unorm                = 0x5F,    ///< [BC5](http://tinyurl.com/l59bu2s) compressed texture format.
    Bc5_Snorm                = 0x60,    ///< [BC5](http://tinyurl.com/l59bu2s) compressed texture format.
    Bc6_Ufloat               = 0x61,    ///< [BC6](http://tinyurl.com/nxxjhlq) unsigned compressed texture format.
    Bc6_Sfloat               = 0x62,    ///< [BC6](http://tinyurl.com/nxxjhlq) signed compressed texture format.
    Bc7_Unorm                = 0x63,    ///< [BC7](http://tinyurl.com/l6qhpgr) compressed texture format.
    Bc7_Srgb                 = 0x64,    ///< [BC7](http://tinyurl.com/l6qhpgr) compressed texture format.
    Etc2X8Y8Z8_Unorm         = 0x65,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X8Y8Z8_Srgb          = 0x66,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X8Y8Z8W1_Unorm       = 0x67,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X8Y8Z8W1_Srgb        = 0x68,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X8Y8Z8W8_Unorm       = 0x69,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X8Y8Z8W8_Srgb        = 0x6A,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X11_Unorm            = 0x6B,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X11_Snorm            = 0x6C,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X11Y11_Unorm         = 0x6D,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    Etc2X11Y11_Snorm         = 0x6E,    ///< _Untested._ [ETC Formats](http://tinyurl.com/qznv7od)
    AstcLdr4x4_Unorm         = 0x6F,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr4x4_Srgb          = 0x70,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr5x4_Unorm         = 0x71,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr5x4_Srgb          = 0x72,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr5x5_Unorm         = 0x73,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr5x5_Srgb          = 0x74,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr6x5_Unorm         = 0x75,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr6x5_Srgb          = 0x76,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr6x6_Unorm         = 0x77,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr6x6_Srgb          = 0x78,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x5_Unorm         = 0x79,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x5_Srgb          = 0x7A,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x6_Unorm         = 0x7B,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x6_Srgb          = 0x7C,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x8_Unorm         = 0x7D,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr8x8_Srgb          = 0x7E,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x5_Unorm        = 0x7F,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x5_Srgb         = 0x80,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x6_Unorm        = 0x81,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x6_Srgb         = 0x82,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x8_Unorm        = 0x83,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x8_Srgb         = 0x84,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x10_Unorm       = 0x85,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr10x10_Srgb        = 0x86,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr12x10_Unorm       = 0x87,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr12x10_Srgb        = 0x88,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr12x12_Unorm       = 0x89,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcLdr12x12_Srgb        = 0x8A,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr4x4_Float         = 0x8B,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr5x4_Float         = 0x8C,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr5x5_Float         = 0x8D,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr6x5_Float         = 0x8E,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr6x6_Float         = 0x8F,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr8x5_Float         = 0x90,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr8x6_Float         = 0x91,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr8x8_Float         = 0x92,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr10x5_Float        = 0x93,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr10x6_Float        = 0x94,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr10x8_Float        = 0x95,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr10x10_Float       = 0x96,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr12x10_Float       = 0x97,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    AstcHdr12x12_Float       = 0x98,    ///< _Untested._ [ASTC Formats](http://tinyurl.com/oysygeq)
    X8Y8_Z8Y8_Unorm          = 0x99,    ///< _Untested._
    X8Y8_Z8Y8_Uscaled        = 0x9A,    ///< _Untested._
    Y8X8_Y8Z8_Unorm          = 0x9B,    ///< _Untested._
    Y8X8_Y8Z8_Uscaled        = 0x9C,    ///< _Untested._
    AYUV                     = 0x9D,    ///< YUV 4:4:4 packed format.  Valid Image and Color-Target view formats are
                                        ///  { X8Y8Z8W8, Unorm } and { X8Y8Z8W8, Uint }.  Each view fully maps the
                                        ///  entire YUV subresource, with the V,U,Y,A channels mapped to the X,Y,Z,W
                                        ///  channels respectively.  Additionally, Image views can use the { X32, Uint }
                                        ///  format where all four channels are packed into a single uint32.
    UYVY                     = 0x9E,    ///< YUV 4:2:2 packed format.  The Image data is subsampled such that each 32bit
                                        ///  element contains two Y samples and one U and V sample.  Valid Image view
                                        ///  formats are { X8Y8Z8W8, Unorm } and { X8Y8Z8W8, Uint }.  Each view fully
                                        ///  maps the entire YUV subresource, with the X,Y,Z,W channels mapped to the
                                        ///  U0,Y0,V0,Y1 channels respectively. Additionally, Image views can use the
                                        ///  { X32, Uint } format where all four channels are packed into a single
                                        ///  uint32. Image views can also use the { X8Y8_Z8Y8, Unorm } format to access
                                        ///  these as well. In this case, the width of the Image view would appear to be
                                        ///  twice as wide as it normally does, and the X0,Y0,Z0,Y1 channels map to the
                                        ///  U0,Y0,V0,Y1 channels respectively.
    VYUY                     = 0x9F,    ///< YUV 4:2:2 packed format.  The image data is encoded just like the
                                        ///  @ref ChNumFormat::UYVY format, except with a different channel ordering.
                                        ///  Image views with X8Y8Z8W8 channel formats map the X,Y,Z,W channels to the
                                        ///  V0,Y0,U0,Y1 channels respectively. Image views with the X8Y8_Z8Y8 channel
                                        ///  format map the X0,Y0,Z0,Y1 channels to the V0,Y0,U0,Y1 channels
                                        ///  respectively.
    YUY2                     = 0xA0,    ///< YUV 4:2:2 packed format.  The image data is encoded just like the
                                        ///  @ref ChNumFormat::UYVY format, except with a different channel ordering.
                                        ///  X8Y8Z8W8 Image view formats map the X,Y,Z,W channels to the Y0,U0,Y1,V0
                                        ///  channels respectively. Image views can use the { Y8X8_Y8Z8, Unorm } format
                                        ///  where the Y0,X0,Y1,Z0 channels are mapped to the Y0,U0,Y1,V0 channels.
    YVY2                     = 0xA1,    ///< YUV 4:2:2 packed format.  The image data is encoded just like the
                                        ///  @ref ChNumFormat::YUY2 format, except with a different channel ordering.
                                        ///  X8Y8Z8W8 Image view formats map the X,Y,Z,W channels to the Y0,V0,Y1,U0
                                        ///  channels respectively. Image views can use the { Y8X8_Y8Z8, Unorm } format
                                        ///  where the Y0,X0,Y1,Z0 channels are mapped to the Y0,V0,Y1,U0 channels.
    YV12                     = 0xA2,    ///< YUV 4:2:0 planar format, with 8 bits per luma and chroma sample.  The Y
                                        ///  plane is first, containg a uint8 per sample.  Next is the U plane and the V
                                        ///  plane, both of which have a uint8 per sample.  Valid Image view formats are
                                        ///  { X8, Unorm } and { X8, Uint }.  Each view only has access to one of the Y,
                                        ///  U, or V planes.
    NV11                     = 0xA3,    ///< YUV 4:1:1 planar format, with 8 bits per luma and chroma sample.  The Y
                                        ///  plane is first, containing a uint8 per sample.  Next is a UV plane which
                                        ///  has interleaved U and V samples, each stored as a uint8.  Valid Image and
                                        ///  Color-Target view formats are { X8, Unorm }, { Y8, Uint }, { X8Y8, Unorm }
                                        ///  and { X8Y8, Uint }.  When using an X8 channel format for the View, the view
                                        ///  only has access to the Y plane.  When using X8Y8, the view only has access
                                        ///  to the UV plane.
    NV12                     = 0xA4,    ///< YUV 4:2:0 planar format, with 8 bits per luma and chroma sample.  The Y
                                        ///  plane is first, containing a uint8 per sample.  Next is a UV plane which
                                        ///  has interleaved U and V samples, each stored as a uint8.  Valid Image and
                                        ///  Color-Target view formats are { X8, Unorm }, { X8, Uint }, { X8Y8, Unorm }
                                        ///  and { X8Y8, Uint }.  When using an X8 channel format for the View, the view
                                        ///  only has access to the Y plane.  When using X8Y8, the view only has access
                                        ///  to the UV plane.
    NV21                     = 0xA5,    ///< YUV 4:2:0 planar format, with 8 bits per luma and chroma sample.  This is
                                        ///  identical to @ref ChNumFormat::NV12, except that the second plane swaps the
                                        ///  ordering of the U and V samples. Image views behave just like with
                                        ///  @ref ChNumFormat::NV12.
    P016                     = 0xA6,    ///< YUV 4:2:0 planar format, with 16 bits per luma and chroma sample.  The
                                        ///  plane ordering is identical to @ref ChNumFormat::NV12.  Instead of uint8
                                        ///  samples, this format uses 8.8 fixed point sample encoding.  Image views
                                        ///  behave just like with @ref ChNumFormat::NV12, except R16 channel formats
                                        ///  are used for the Y plane, and X16Y16 channel formats are used for the UV
                                        ///  plane.
    P010                     = 0xA7,    ///< YUV 4:2:0 planar format, with 10 bits per luma and chroma sample.  This is
                                        ///  identical to @ref ChNumFormat::P016, except that the lowest 6 bits of each
                                        ///  luma and chroma sample are ignored. This allows the source data to be
                                        ///  interpreted as either P016 or P010 interchangably.
    Count
};

/// Specifies which channel of a resource should be mapped to a particular component of an image view.
///
/// @ingroup ResourceBinding
enum class ChannelSwizzle : uint8
{
    Zero = 0x0,  ///< Ignore resource data and always fetch a 0 into this component.
    One  = 0x1,  ///< Ignore resource data and always fetch a 1 into this component.
    X    = 0x2,  ///< Use the X channel from resource for this component.
    Y    = 0x3,  ///< Use the Y channel from resource for this component.
    Z    = 0x4,  ///< Use the Z channel from resource for this component.
    W    = 0x5,  ///< Use the W channel from resource for this component.
    Count
};

/// Specifies a mapping for each component of an image or buffer view to a channel in its associated resource.
///
/// @ingroup ResourceBinding
struct ChannelMapping
{
    union
    {
        struct
        {
            ChannelSwizzle r;          ///< Red component swizzle.
            ChannelSwizzle g;          ///< Green component swizzle.
            ChannelSwizzle b;          ///< Blue component swizzle.
            ChannelSwizzle a;          ///< Alpha component swizzle.
        };
        ChannelSwizzle     swizzle[4]; ///< All four swizzles packed into one array.
    };
};

/// Specifies a pixel format for an image or memory view and its corresponding channel swizzle.
struct SwizzledFormat
{
    ChNumFormat    format;  ///< Pixel format.
    ChannelMapping swizzle; ///< Compatible channel swizzle for the above pixel format.
};

/// Constant for undefined formats.
const SwizzledFormat UndefinedSwizzledFormat =
{
    ChNumFormat::Undefined,
    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
};

/// Defines an offset into a 2D pixel region.
struct Offset2d
{
    int32 x;  ///< X offset.
    int32 y;  ///< Y offset.
};

/// Defines an offset into a 3D pixel region.
struct Offset3d
{
    int32 x;  ///< X offset.
    int32 y;  ///< Y offset.
    int32 z;  ///< Z offset.
};

/// Defines a width and height for a 2D image region. The dimensions could be pixels, blocks, or bytes
/// depending on context, so be sure to check documentation for the PAL interface of interest to be sure you
/// get it right.
struct Extent2d
{
    uint32 width;   ///< Width of region.
    uint32 height;  ///< Height of region.
};

/// Defines a signed width and height, for a 2D image region. The dimensions could be pixels, blocks, or bytes
/// depending on context, so be sure to check documentation for the PAL interface of interest to be sure you
/// get it right.
struct SignedExtent2d
{
    int32 width;    ///< Width of region.
    int32 height;   ///< Height of region.
};

/// Defines a width, height, and depth for a 3D image region. The dimensions could be pixels, blocks, or bytes
/// depending on context, so be sure to check documentation for the PAL interface of interest to be sure you
/// get it right.
struct Extent3d
{
    uint32 width;   ///< Width of region.
    uint32 height;  ///< Height of region.
    uint32 depth;   ///< Depth of region.
};

/// Defines a signed width, height, and depth for a 3D image region. The dimensions could be pixels, blocks, or bytes
/// depending on context, so be sure to check documentation for the PAL interface of interest to be sure you
/// get it right.
struct SignedExtent3d
{
    int32 width;    ///< Width of region.
    int32 height;   ///< Height of region.
    int32 depth;    ///< Depth of region.
};

/// Defines a region in 1D space.
struct Range
{
    int32  offset;  ///< Starting position.
    uint32 extent;  ///< Region size.
};

/// Defines a rectangular region in 2D space.
struct Rect
{
    Offset2d offset;  ///< Top left corner.
    Extent2d extent;  ///< Rectangle width and height.
};

/// Defines a cubic region in 3D space.
struct Box
{
    Offset3d offset;  ///< Top left front corner.
    Extent3d extent;  ///< Box width, height and depth.
};

/// Specifies the Display Output Post-Processing (DOPP) desktop texture information, which are provided by OpenGL via
/// interop.  The DOPP is an OpenGL extension to allow its client to access the desktop texture directly without the
/// need of copying to system memory.  This is only supported on Windows.
struct DoppDesktopInfo
{
    gpusize gpuVirtAddr;    ///< The VA of the dopp desktop texture. Set to 0 for the non-dopp resource.
    uint32  vidPnSourceId;  ///< Display source id of the dopp desktop texture.
};

/// Specifies parameters for opening a shared GPU resource from a non-PAL device or non-local process.
struct ExternalResourceOpenInfo
{
    OsExternalHandle hExternalResource;         ///< External GPU resource from another non-PAL device to open.

    union
    {
        struct
        {
            uint32 ntHandle :  1; ///< The provided hExternalResource is an NT handle instead of a default KMT handle.
            uint32 reserved : 31; ///< Reserved for future use.
        };
        uint32 u32All;            ///< Flags packed as 32-bit uint.
    } flags;                      ///< External resource open flags.

    DoppDesktopInfo  doppDesktopInfo;           ///< The information of dopp desktop texture.
};

/// Packed pixel display enumeration.
///
/// In the medical imaging market space, there are several 10-bit per component color and grayscale displays
/// available.In addition to being high precision, these displays tend to be very high resolution.For grayscale
/// displays,one method of getting high pixel resolution in 10b precision is a proprietary method called
/// "packed pixel".Each of these packed pixel formats packs two/three 10-bit luminance values into a single
/// R8G8B8 pixel.
///
/// Example Displays:
///
///     EIZO GS510
///     NEC MD21GS
///     TOTOKU ME55Xi2
///     FIMI 3/5MP
///
///
///   The enumerations are named in a way to describe the format of the packed pixels. Names for
///   formats with two or three pixels packed into a single word (corresponding to a simple RGB pixel)
///   follow this convention:
///
///       LLLLLL_RRRRRR (L=left pixel, R=right pixel) or
///       LLL_MMM_RRR (L=left pixel, M=middle pixel, R=right pixel)
///
///   The bit order for a pixel follows this convention:
///
///       (ColorBand)MSB(ColorBand)LSB
///
///   For example: G70B54 means that the MSBs are in 7-0 of the green channel, and the LSBs
///   are stored in bits 5-4.
///
enum class PackedPixelType : uint32
{
    NotPacked = 0,          ///< Pixels not packed, for standard color RGB8 monitor
    SplitG70B54_R70B10,     ///< 10-bit mono, split screen
    SplitB70G10_R70G76,     ///< 10-bit mono, split screen
    G70B54_R70B10,          ///< 10-bit mono, 2 adjacent pixels
    B70R32_G70R76,          ///< 10-bit mono, 2 adjacent pixels
    B70R30_G70R74,          ///< 12-bit mono, 2 adjacent pixels
    B70_G70_R70,            ///< 8-bit mono, 3 adjacent pixels
    R70G76,                 ///< 10-bit mono, single pixel
    G70B54,                 ///< 10-bit mono, single pixel
    Native,                 ///< 10-bit color, without packing
};

/// Enumerates the logging priority levels supported by PAL.
enum class LogLevel : uint32
{
    Debug = 0, ///< Debug messages
    Verbose,   ///< High frequency messages
    Info,      ///< Low frequency messages
    Alert,     ///< Warnings
    Error,     ///< Critical issues
    Always     ///< All messages
};

/// Enumerates all log categories explicitly defined by PAL
enum class LogCategory : uint64
{
    Correctness = 0, ///< Application correctness
    Performance,     ///< Application performance
    Internal,        ///< Internal logging
    Display,         ///< Display Info
    Count
};

/// Static string table used to register log categories
static const char* LogCategoryTable[] =
{
    "Correctness",
    "Performance",
    "Internal",
    "Display"
};

/// Typedef for log category masks.
typedef uint64 LogCategoryMask;

/// Log category mask for messages related to application correctness
static constexpr LogCategoryMask LogCategoryMaskCorrectness = (1 << static_cast<uint32>(LogCategory::Correctness));

/// Log category mask for messages related to application performance
static constexpr LogCategoryMask LogCategoryMaskPerformance = (1 << static_cast<uint32>(LogCategory::Performance));

/// Log category mask for messages related to internal messages
static constexpr LogCategoryMask LogCategoryMaskInternal    = (1 << static_cast<uint32>(LogCategory::Internal));

/// Log category mask for messages related to display information (e.g. HDR format)
static constexpr LogCategoryMask LogCategoryMaskDisplay = (1 << static_cast<uint32>(LogCategory::Display));

/// Defines the modes that the GPU Profiling layer can be enabled with. If the GpuProfilerMode is
/// GpuProfilerThreadTraceView or GpuProfilerRgp, then the GpuProfilerTraceModeMask is examined to configure
/// the trace type (spm, sqtt or both) requested.
enum GpuProfilerMode : uint32
{
    GpuProfilerDisabled            = 0, ///< Gpu Profiler is disabled.
    GpuProfilerSqttOff             = 1, ///< Traces are disabled but perf counter and timing operations are enabled.
    GpuProfilerSqttThreadTraceView = 2, ///< Traces are output in format (.csv, .out) for Thread trace viewer.
    GpuProfilerSqttRgp             = 3, ///< Trace data is output as .rgp file for Radeon Gpu Profiler.
};

/// Defines the modes that the GPU Profiling layer can be enabled with.
/**
 ***********************************************************************************************************************
 * @mainpage
 *
 * Introduction
 * ------------
 * The Platform Abstraction Library (PAL) provides hardware and OS abstractions for Radeon (GCN+) user-mode 3D graphics
 * drivers.  The level of abstraction is chosen to support performant driver implementations of several APIs while
 * hiding the client from hardware and operating system details.
 *
 * PAL client drivers will have no HW-specific code; their responsibility is to translate API/DDI commands into PAL
 * commands as efficiently as possible.  This means that the client should be unaware of hardware registers, PM4
 * commands, SP3 shaders, etc.  However, PAL is an abstraction of AMD hardware only, so many things in the PAL interface
 * have an obvious correlation to hardware features.
 *
 * PAL client drivers should have little OS-specific code.  PAL and its companion utility collection provide
 * OS abstractions for almost everything a client might need, but there are some cases where this is unavoidable:
 *
 * + Handling dynamic library infrastructure.  I.e., the client has to implement DllMain() on Windows, etc.
 * + OS-specific APIs or extensions.  DX may have Windows-specific functionality in the core API, and Vulkan/Mantle may
 *   export certain OS-specific features as extensions (like for presenting contents to the screen).
 * + Single OS clients (e.g., DX) may choose to make OS-specific calls directly simply out of convenience with no down
 *   side.
 *
 *
 * The following diagram illustrates the software stack when running a 3D application with a PAL-based UMD.  Non-AMD
 * components are in gray, UMD client code is blue, AMD static libs linked into the UMD are green, and the AMD KMD
 * is in red.
 *
 * @image html swStack.png
 *
 * PAL is a relatively _thick_ abstraction layer, typically accounting for the majority of code (excluding SC) in any
 * particular UMD built on PAL.  The level of abstraction tends to be higher in areas where client APIs are similar,
 * and lower (closer to hardware) in areas where client APIs diverge significantly.  The overall philosophy is to share
 * as much code as possible without impacting client driver performance.  Our committed goal is that CPU-limited
 * performance should be within 5% of what a native solution could achieve, and GPU-limited performance should be within
 * 2%.
 *
 * PAL uses a C++ interface.  The public interface is defined in .../pal/inc, and client must _only_ include headers
 * from that directory.  The interface is spread over many header files - typically one per class - in order to clarify
 * dependencies and reduce build times.  There are two sub-directories in .../pal/inc:
 *
 * + <b>.../pal/inc/core</b>    - Defines the PAL Core (see @ref Overview).
 * + <b>.../pal/inc/gpuUtil</b> - Defines the PAL GPU Utility Collection (see @ref GpuUtilOverview).
 * + <b>.../pal/inc/util</b>    - Defines the PAL Utility Collection (see @ref UtilOverview).
 *
 *
 * @copydoc VersionHistory
 *
 * Next: @ref Build
 ***********************************************************************************************************************
 */

/**
 ***********************************************************************************************************************
 * @page Overview PAL Core Overview
 *
 * ### Introduction
 * PAL's core interface is defined in the @ref Pal namespace, and defines an object-oriented model for interacting with
 * the GPU and OS.  The interface closely resembles the Mantle, Vulkan, and DX12 APIs.  Some common features of these
 * APIs that are central to the PAL interface:
 *
 * - All shader stages, and some additional "shader adjacent" state, are glommed together into a monolithic pipeline
 *   object.
 * - Explicit, free-threaded command buffer generation.
 * - Support for multiple, asynchronous engines for executing GPU work (graphics, compute, DMA).
 * - Explicit system and GPU memory management.
 * - Flexible shader resource binding model.
 * - Explicit management of stalls, cache flushes, and compression state changes.
 *
 * However, as a common component supporting multiple APIs, the PAL interface tends to be lower level in places where
 * client APIs diverge.
 *
 * ### Settings
 * The PAL library has a number of configuration settings available for the client to modify either programmatically
 * or via external settings.  PAL also includes infrastructure for building/loading client-specific settings.
 * See @ref Settings for a detailed description of this support.
 *
 * ### Initialization
 * The first step to interacting with the PAL core is creating an IPlatform object and enumerating IDevice objects
 * representing GPUs attached to the system and, optionally, IScreen objects representing displays attached to the
 * system.  See @ref LibInit for a detailed description.
 *
 * ### System Memory Allocation
 * Clients have a lot of control over PAL's system memory allocations.  Most PAL objects require the client to provide
 * system memory; the client first calls a GetSize() method and then passes a pointer to PAL on the actual create call.
 * Further, when PAL needs to make an internal allocation, it will optionally call a client callback, which can be
 * specified on platform creation.  This callback will specify a category for the allocation, which may imply an
 * expected lifetime.
 *
 * ### Interface Classes
 * The following diagram illustrates the relationship of some key PAL interfaces and how they interact to render a
 * typical frame in a modern game.  Below that is a listing of all of PAL's interface classes, and a very brief
 * description of their purpose.  Follow the link for each interface to see detailed reference documentation.
 *
 * @image html scheduling.png
 *
 * - __OS Abstractions__
 *   + _IPlatform_: Root-level object created by clients that interact with PAL.  Mostly responsible for enumerating
 *                  devices and screens attached to the system and returning any system-wide properties.<br><br>
 *   + _IDevice_: Configurable context for querying properties of a particular GPU and interacting with it.  Acts as a
 *                factory for almost all other PAL objects.<br><br>
 *   + _IQueue_: A device has one or more _engines_ which are able to issue certain types of work.  Tahiti, for example,
 *               has 1 universal engine (supports graphics, compute, or copy commands), 2 compute engines (support
 *               compute or copy commands), and 2 DMA engines (support only copy commands).  An IQueue object is a
 *               context for submitting work on a particular engine.  This mainly takes the form of submitting command
 *               buffers and presenting images to the screen.  Work performed in a queue will be started in order, but
 *               work executed on different queues (even if the queues reference the same engine) is not guaranteed
 *               to be ordered without explicit synchronization.<br><br>
 *   + _IQueueSemaphore_: Queue semaphores can be signaled and waited on from an IQueue in order to control execution
 *                        order between queues.<br><br>
 *   + _IFence_: Used for coarse-grain CPU/GPU synchronization.  Fences can be signalled from the GPU as part of a
 *               command buffer submission on a queue, then waited on from the CPU.<br><br>
 *   + _IGpuMemory_: Represents a GPU-accessible memory allocation.  Can either be virtual (only VA allocation which
 *                   must be explicitly mapped via an IQueue operation) or physical.  Residency of physical allocations
 *                   must be managed by the client either globally for a device (IDevice::AddGpuMemoryReferences) or by
 *                   specifying allocations referenced by command buffers at submit.<br><br>
 *   + _ICmdAllocator_: GPU memory allocation pool used for backing an ICmdBuffer.  The client is free to create one
 *                      allocator per device, or one per thread to remove thread contention.<br><br>
 *   + _IScreen_: Represents a display attached to the system.  Mostly used for managing full-screen flip
 *                presents.<br><br>
 *   + _IPrivateScreen_: Represents a display that is not otherwise visible to the OS, typically a VR head mounted
 *                       display.<br><br>
 * - __Hardware IP Abstractions__
 *    + __All IP__
 *      - _ICmdBuffer_: Clients build command buffers to execute the desired work on the GPU, and submit them on a
 *                      corresponding queue.  Different types of work can be executed depending on the _queueType_ of
 *                      the command buffer (graphics work, compute work, DMA work).<br><br>
 *      - _IImage_: Images are a 1D, 2D, or 3D collection of pixels (i.e., _texture_) that can be accessed by the
 *                  GPU in various ways: texture sampling, BLT source/destination, UAV, etc.<br><br>
 *    + __GFXIP-only__
 *      - _IShader_: Container for shader byte code used as an input to pipeline creation.  No compilation occurs
 *                   until an IPipeline is created.  Currently, AMDIL is the only supported input language.<br><br>
 *      - _IPipeline_: Comprised of all shader stages (CS for compute, VS/HS/DS/GS/PS for graphics), resource mappings
 *                     describing how user data entries are to be used by the shaders, and some other fixed-function
 *                     state like depth/color formats, blend enable, MSAA enable, etc.<br><br>
 *      - _IColorTargetView_: IImage view allowing the image to be bound as a color target (i.e., RTV.).<br><br>
 *      - _IDepthStencilView_: IImage view allowing the image to be bound as a depth/stencil target (i.e., DSV).<br><br>
 *      - _IGpuEvent_: Used for fine-grained (intra-command buffer) synchronization between the CPU and GPU.  GPU
 *                     events can be set/reset from either the CPU or GPU and waited on from either.<br><br>
 *      - _IQueryPool_: Collection of query slots for tracking occlusion or pipeline stats query results.<br><br>
 *      - __Dynamic State Objects__: _IColorBlendState_, _IDepthStencilState_, _IMsaaState_, _IScissorState_,
 *                                   and _IViewportState_ define logical collections of related fixed function graphics
 *                                   state, similar to DX11.<br><br>
 *      - _IPerfExperiment_: Used for gathering performance counter and thread trace data.<br><br>
 *      - _IBorderColorPalette_: Provides a collection of indexable colors for use by samplers that clamp to an
 *                               arbitrary border color.<br><br>
 * - __Common Base Classes__
 *   + _IDestroyable_: Defines a _Destroy()_ method for the PAL interface.  Calling _Destroy()_ will release any
 *                     internally allocated resources for the object, but the client is still responsible for freeing
 *                     the system memory provided for the object.<br><br>
 *   + _IGpuMemoryBindable_: Defines a set of methods for binding GPU memory to the object.  Interfaces that inherit
 *                           _IGpuMemoryBindable_ require GPU memory in order to be used by the GPU.  The client
 *                           must query the requirements (e.g., alignment, size, heaps) and allocate/bind GPU memory
 *                           for the object.  _IGpuMemoryBindable_ inherits from _IDestroyable_.<br><br>
 *
 * ### %Format Info
 * Several helper methods are available for dealing with image formats in the @ref Formats namespace.
 *
 * ### Graphics/Compute Execution Model
 * Most graphics/compute work is defined by first binding a set of states then issuing a draw or dispatch command to
 * kick off the work.  The complete set of graphics states available in PAL is illustrated below; compute is a subset
 * of this that only includes the pipeline, user data entries, and border color palette.
 *
 * @image html stateBreakdown.jpg
 *
 * Most of these correspond directly to a PAL interface object above, and these items are bound by calling a
 * corresponding _CmdBind...()_ method in the ICmdBuffer interface.  The states marked in yellow and orange, however,
 * are _immediate_ states for which there is no object, you just specify the required state values in the corresponding
 * _CmdSet...()_ method in the ICmdBuffer interface.
 *
 * User data entries are the way that input resources are specified for the pipeline on an upcoming draw/dispatch.  This
 * mapping is complicated, and is described fully in @ref ResourceBinding.
 *
 * A final complication worth noting is that PAL provides no implicit surface synchronization.  The client is
 * respondible for explicitly inserting barriers to resolve data hazards, flush/invalidate caches, and ensure images
 * are in the proper compression state.  For more detail, see ICmdBuffer::CmdBarrier, BarrierInfo, and
 * BarrierTransition.
 *
 ***********************************************************************************************************************
 */

} // Pal
