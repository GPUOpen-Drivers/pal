/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFormat.h"
#include "palFormatInfo.h"
#include "directDrawSurface.h"

// Including DXGI_FORMAT is optional. If the file can be included, the functions in this file will support DDS for
// more pixel formats.  There are two paths to inclusion here.
// The first uses a __has_include to search for the file in the include path.  This works when the compiler
// supports __has_include (C++17 and up).
// The second path includes the file for WIN32 platforms. On such platforms, we know the file will exist.
#if defined __has_include
#if __has_include(<dxgiformat.h>)
#include  <dxgiformat.h>
#endif
#endif

using namespace DirectX;

namespace Util
{

// =====================================================================================================================
// Determines the DDS format to use from the input pal format
// Also determine the required dxgi format when the pixel format is DDSPF_DX10
Result GetDdsPixelFormat(
    DDS_PIXELFORMAT*    pDdspf,         // [out] Pixel format for the input pal format.
    uint32_t*           pDxgiFormat,    // [out] DXGI format for the input pal format.  This is only necessary when
                                        //       the pixel format is a fourcc of "DX10"
    Pal::SwizzledFormat palFormat)
{
    Result result = Result::Success;

    if ((pDdspf == nullptr) ||
        (pDxgiFormat == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
#if DXGI_FORMAT_DEFINED
        // Typed pointer to the input DXGI format for easy assignment
        static_assert(sizeof(DXGI_FORMAT) == sizeof(uint32_t), "DXGI_FORMAT and uint32_t size mismatch");
        DXGI_FORMAT* pDxgiFormatInternal = reinterpret_cast<DXGI_FORMAT*>(pDxgiFormat);
#endif

        switch (palFormat.format)
        {
            case Pal::ChNumFormat::X1_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X1_Uscaled:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X4Y4_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X4Y4_Uscaled:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::L4A4_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X4Y4Z4W4_Unorm:
            {
                *pDdspf = DDSPF_A4R4G4B4;
                break;
            }
            case Pal::ChNumFormat::X4Y4Z4W4_Uscaled:
            {
                *pDdspf = DDSPF_A4R4G4B4;
                break;
            }
            case Pal::ChNumFormat::X5Y6Z5_Unorm:
            {
                *pDdspf = DDSPF_R5G6B5;
                break;
            }
            case Pal::ChNumFormat::X5Y6Z5_Uscaled:
            {
                *pDdspf = DDSPF_R5G6B5;
                break;
            }
            case Pal::ChNumFormat::X5Y5Z5W1_Unorm:
            {
                *pDdspf = DDSPF_A1R5G5B5;
                break;
            }
            case Pal::ChNumFormat::X5Y5Z5W1_Uscaled:
            {
                *pDdspf = DDSPF_A1R5G5B5;
                break;
            }
            case Pal::ChNumFormat::X1Y5Z5W5_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X1Y5Z5W5_Uscaled:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X8_Unorm:
            {
                *pDdspf = DDSPF_L8;
                break;
            }
            case Pal::ChNumFormat::X8_Snorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X8_Uscaled:
            {
                *pDdspf = DDSPF_L8;
                break;
            }
            case Pal::ChNumFormat::X8_Sscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X8_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::A8_Unorm:
            {
                *pDdspf = DDSPF_A8;
                break;
            }
            case Pal::ChNumFormat::L8_Unorm:
            {
                *pDdspf = DDSPF_L8;
                break;
            }
            case Pal::ChNumFormat::P8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Unorm:
            {
                *pDdspf = DDSPF_A8L8;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Snorm:
            {
                *pDdspf = DDSPF_V8U8;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Uscaled:
            {
                *pDdspf = DDSPF_A8L8;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Sscaled:
            {
                *pDdspf = DDSPF_V8U8;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::L8A8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Unorm:
            {
                *pDdspf = DDSPF_A8B8G8R8;
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Snorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8B8A8_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Uscaled:
            {
                *pDdspf = DDSPF_A8B8G8R8;
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Sscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8B8A8_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8B8A8_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X8Y8Z8W8_Srgb:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
#endif
                break;
            }
            case Pal::ChNumFormat::U8V8_Snorm_L8W8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X10Y11Z11_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X11Y11Z10_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R11G11B10_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Unorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Snorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Uscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Sscaled:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R10G10B10A2_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Sint:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X10Y10Z10W2Bias_Unorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::U10V10W10_Snorm_A2_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X16_Unorm:
            {
                *pDdspf = DDSPF_L16;
                break;
            }
            case Pal::ChNumFormat::X16_Snorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16_Uscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16_Sscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::L16_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X16Y16_Unorm:
            {
                *pDdspf = DDSPF_G16R16;
                break;
            }
            case Pal::ChNumFormat::X16Y16_Snorm:
            {
                *pDdspf = DDSPF_V16U16;
                break;
            }
            case Pal::ChNumFormat::X16Y16_Uscaled:
            {
                *pDdspf = DDSPF_G16R16;
                break;
            }
            case Pal::ChNumFormat::X16Y16_Sscaled:
            {
                *pDdspf = DDSPF_V16U16;
                break;
            }
            case Pal::ChNumFormat::X16Y16_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Unorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Snorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Uscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Sscaled:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_SNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X16Y16Z16W16_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R16G16B16A16_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32_FLOAT;
#endif
            }
            case Pal::ChNumFormat::X32Y32Z32_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32Z32_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32Z32_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32Z32W32_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32A32_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32Z32W32_Sint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32A32_SINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X32Y32Z32W32_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R32G32B32A32_FLOAT;
#endif
                break;
            }
            case Pal::ChNumFormat::D16_Unorm_S8_Uint:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::D32_Float_S8_Uint:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
#endif
                break;
            }
            case Pal::ChNumFormat::X9Y9Z9E5_Float:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc1_Unorm:
            {
                *pDdspf = DDSPF_DXT1;
                break;
            }
            case Pal::ChNumFormat::Bc1_Srgb:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC1_UNORM_SRGB;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc2_Unorm:
            {
                *pDdspf = DDSPF_DXT3;
                break;
            }
            case Pal::ChNumFormat::Bc2_Srgb:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC2_UNORM_SRGB;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc3_Unorm:
            {
                *pDdspf = DDSPF_DXT5;
                break;
            }
            case Pal::ChNumFormat::Bc3_Srgb:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC3_UNORM_SRGB;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc4_Unorm:
            {
                *pDdspf = DDSPF_BC4_UNORM;
                break;
            }
            case Pal::ChNumFormat::Bc4_Snorm:
            {
                *pDdspf = DDSPF_BC4_SNORM;
                break;
            }
            case Pal::ChNumFormat::Bc5_Unorm:
            {
                *pDdspf = DDSPF_BC5_UNORM;
                break;
            }
            case Pal::ChNumFormat::Bc5_Snorm:
            {
                *pDdspf = DDSPF_BC5_SNORM;
                break;
            }
            case Pal::ChNumFormat::Bc6_Ufloat:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC6H_UF16;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc6_Sfloat:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC6H_SF16;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc7_Unorm:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC7_UNORM;
#endif
                break;
            }
            case Pal::ChNumFormat::Bc7_Srgb:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_BC7_UNORM_SRGB;
#endif
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8W1_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8W1_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8W8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X8Y8Z8W8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X11_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X11_Snorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X11Y11_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Etc2X11Y11_Snorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr4x4_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr4x4_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr5x4_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr5x4_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr5x5_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr5x5_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr6x5_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr6x5_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr6x6_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr6x6_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x5_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x5_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x6_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x6_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr8x8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x5_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x5_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x6_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x6_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x8_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x8_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x10_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr10x10_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr12x10_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr12x10_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr12x12_Unorm:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcLdr12x12_Srgb:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr4x4_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr5x4_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr5x5_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr6x5_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr6x6_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr8x5_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr8x6_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr8x8_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr10x5_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr10x6_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr10x8_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr10x10_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr12x10_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::AstcHdr12x12_Float:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Z8Y8_Unorm:
            {
                *pDdspf = DDSPF_R8G8_B8G8;
                break;
            }
            case Pal::ChNumFormat::X8Y8_Z8Y8_Uscaled:
            {
                *pDdspf = DDSPF_R8G8_B8G8;
                break;
            }
            case Pal::ChNumFormat::Y8X8_Y8Z8_Unorm:
            {
                *pDdspf = DDSPF_G8R8_G8B8;
                break;
            }
            case Pal::ChNumFormat::Y8X8_Y8Z8_Uscaled:
            {
                *pDdspf = DDSPF_G8R8_G8B8;
                break;
            }
            case Pal::ChNumFormat::AYUV:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_AYUV;
#endif
                break;
            }
            case Pal::ChNumFormat::UYVY:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::VYUY:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::YUY2:
            {
                *pDdspf = DDSPF_YUY2;
                break;
            }
            case Pal::ChNumFormat::YVY2:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::YV12:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::NV11:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_NV11;
#endif
                break;
            }
            case Pal::ChNumFormat::NV12:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_NV12;
#endif
                break;
            }
            case Pal::ChNumFormat::NV21:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::P016:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_P016;
#endif
                break;
            }
            case Pal::ChNumFormat::P010:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_P010;
#endif
                break;
            }
            case Pal::ChNumFormat::P210:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::X8_MM_Unorm:
            case Pal::ChNumFormat::X8_MM_Uint:
            case Pal::ChNumFormat::X8Y8_MM_Unorm:
            case Pal::ChNumFormat::X8Y8_MM_Uint:
            case Pal::ChNumFormat::X16_MM10_Unorm:
            case Pal::ChNumFormat::X16_MM10_Uint:
            case Pal::ChNumFormat::X16Y16_MM10_Unorm:
            case Pal::ChNumFormat::X16Y16_MM10_Uint:
            case Pal::ChNumFormat::X16_MM12_Unorm:
            case Pal::ChNumFormat::X16_MM12_Uint:
            case Pal::ChNumFormat::X16Y16_MM12_Unorm:
            case Pal::ChNumFormat::X16Y16_MM12_Uint:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::P208:
            {
                *pDdspf = DDSPF_DX10;
#if DXGI_FORMAT_DEFINED
                *pDxgiFormatInternal = DXGI_FORMAT_P208;
#endif
                break;
            }
            case Pal::ChNumFormat::P012:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::P212:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::P412:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Y216:
            {
                result = Result::Unsupported;
                break;
            }
            case  Pal::ChNumFormat::Y210:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Y416:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Y410:
            {
                result = Result::Unsupported;
                break;
            }
            case Pal::ChNumFormat::Count:
            {
                result = Result::Unsupported;
                break;
            }
            default:
            {
                result = Result::ErrorInvalidFormat;
                break;
            }
        }

        static_assert(static_cast<uint32>(Pal::ChNumFormat::Count) == 0xBE,
                      "Format table needs updating!");

#if !DXGI_FORMAT_DEFINED
        if (pDdspf->fourCC == MAKEFOURCC('D','X','1','0'))
        {
            result = Result::Unsupported;
        }
#endif
    }

    return result;
}

// =====================================================================================================================
// Determine the header and header size for a DDS image of the input surface.
// This can fail if any of the input pointers are nullptr or if the input format is not supported by DDS.
Result GetDdsHeader(
    DdsHeaderFull*      pHeader,            // [out] DDS header for the input surface
    size_t*             pActualHeaderSize,  // [out] Size of the DDS header. This can vary depending on the format of
                                            //       the input surface.
    Pal::ImageType      imageType,
    Pal::SwizzledFormat palFormat,
    uint32              arraySize,
    Pal::SubresLayout*  pSubresLayout)
{
    Result result = Result::Success;

    if ((pHeader            == nullptr) ||
        (pActualHeaderSize  == nullptr) ||
        (pSubresLayout      == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        uint32_t dxgiFormat;
        result = GetDdsPixelFormat(&(pHeader->headerBase.ddspf), &dxgiFormat, palFormat);

        if (result == Result::Success)
        {
            // Write Header magic and base
            *pActualHeaderSize = sizeof(DDS_MAGIC) + sizeof(DDS_HEADER);

            pHeader->ddsMagic                   = DDS_MAGIC;

            pHeader->headerBase.flags           = DDS_HEADER_FLAGS_TEXTURE;
            pHeader->headerBase.size            = sizeof(DDS_HEADER);
            pHeader->headerBase.mipMapCount     = 1;
            pHeader->headerBase.depth           = 1;
            pHeader->headerBase.caps            = DDS_SURFACE_FLAGS_TEXTURE;
            pHeader->headerBase.width           = static_cast<uint32>(pSubresLayout->rowPitch);
            pHeader->headerBase.width           /= Pal::Formats::BytesPerPixel(palFormat.format);
            pHeader->headerBase.height          =
                static_cast<uint32>(pSubresLayout->depthPitch / pSubresLayout->rowPitch);

            if (Pal::Formats::IsBlockCompressed(palFormat.format) == true)
            {
                pHeader->headerBase.flags |= DDS_HEADER_FLAGS_LINEARSIZE;
                pHeader->headerBase.pitchOrLinearSize = static_cast<uint32>(pSubresLayout->depthPitch);
            }
            else
            {
                pHeader->headerBase.flags |= DDS_HEADER_FLAGS_PITCH;
                pHeader->headerBase.pitchOrLinearSize = static_cast<uint32>(pSubresLayout->rowPitch);
            }

            // Write Ext Header if required
            if (pHeader->headerBase.ddspf.fourCC == MAKEFOURCC('D','X','1','0'))
            {
                *pActualHeaderSize += sizeof(DDS_HEADER_DXT10);

                pHeader->headerExt.dxgiFormat = dxgiFormat;

                // Determine resource dimension
                if (imageType == Pal::ImageType::Tex1d)
                {
                    pHeader->headerExt.resourceDimension = DDS_DIMENSION_TEXTURE1D;
                }
                else if (imageType == Pal::ImageType::Tex2d)
                {
                    pHeader->headerExt.resourceDimension = DDS_DIMENSION_TEXTURE2D;
                }
                else if (imageType == Pal::ImageType::Tex3d)
                {
                    pHeader->headerExt.resourceDimension = DDS_DIMENSION_TEXTURE3D;
                }

                pHeader->headerExt.arraySize = arraySize;
            }
        }
    }

    return result;
}

} // Util

#endif
