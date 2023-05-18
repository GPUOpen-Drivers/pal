/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the PalTools/codegen/formats directory.
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

/**
***********************************************************************************************************************
* @file  g_mergedFormatInfo.h
* @brief Auto-generated file that describes the channel format properties for PAL's hardware independent layer.
***********************************************************************************************************************
*/

#include "pal.h"
#include "palFormatInfo.h"

namespace Pal
{
namespace Formats
{

// Lookup table for intrinsic properties describing each channel format. Callers should access the members of this
// table via BitsPerPixel() and related functions.
const FormatInfo FormatInfoTable[] =
{

    // Undefined
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Undefined,
    },
    // X1_Unorm
    {
        1,   1,                             // 1 bpp,  1 component
        {  1,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X1_Uscaled
    {
        1,   1,                             // 1 bpp,  1 component
        {  1,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X4Y4_Unorm
    {
        8,   2,                             // 8 bpp,  2 components
        {  4,  4,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X4Y4_Uscaled
    {
        8,   2,                             // 8 bpp,  2 components
        {  4,  4,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // L4A4_Unorm
    {
        8,   2,                             // 8 bpp,  2 components
        {  4,  4,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X4Y4Z4W4_Unorm
    {
        16,  4,                             // 16 bpp, 4 components
        {  4,  4,  4,  4, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X4Y4Z4W4_Uscaled
    {
        16,  4,                             // 16 bpp, 4 components
        {  4,  4,  4,  4, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X5Y6Z5_Unorm
    {
        16,  3,                             // 16 bpp, 3 components
        {  5,  6,  5,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X5Y6Z5_Uscaled
    {
        16,  3,                             // 16 bpp, 3 components
        {  5,  6,  5,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X5Y5Z5W1_Unorm
    {
        16,  4,                             // 16 bpp, 4 components
        {  5,  5,  5,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X5Y5Z5W1_Uscaled
    {
        16,  4,                             // 16 bpp, 4 components
        {  5,  5,  5,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X1Y5Z5W5_Unorm
    {
        16,  4,                             // 16 bpp, 4 components
        {  1,  5,  5,  5, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X1Y5Z5W5_Uscaled
    {
        16,  4,                             // 16 bpp, 4 components
        {  1,  5,  5,  5, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X8_Unorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8_Snorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X8_Uscaled
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X8_Sscaled
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X8_Uint
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X8_Sint
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X8_Srgb
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // A8_Unorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // L8_Unorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // P8_Unorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8_Unorm
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8_Snorm
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X8Y8_Uscaled
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X8Y8_Sscaled
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X8Y8_Uint
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X8Y8_Sint
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X8Y8_Srgb
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // L8A8_Unorm
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8Z8W8_Unorm
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8Z8W8_Snorm
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X8Y8Z8W8_Uscaled
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X8Y8Z8W8_Sscaled
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X8Y8Z8W8_Uint
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X8Y8Z8W8_Sint
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X8Y8Z8W8_Srgb
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // U8V8_Snorm_L8W8_Unorm
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X10Y11Z11_Float
    {
        32,  3,                             // 32 bpp, 3 components
        { 10, 11, 11,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X11Y11Z10_Float
    {
        32,  3,                             // 32 bpp, 3 components
        { 11, 11, 10,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X10Y10Z10W2_Unorm
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X10Y10Z10W2_Snorm
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X10Y10Z10W2_Uscaled
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X10Y10Z10W2_Sscaled
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X10Y10Z10W2_Uint
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X10Y10Z10W2_Sint
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X10Y10Z10W2Bias_Unorm
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // U10V10W10_Snorm_A2_Unorm
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X16_Unorm
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16_Snorm
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X16_Uscaled
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X16_Sscaled
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X16_Uint
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16_Sint
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X16_Float
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // L16_Unorm
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16Y16_Unorm
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16Y16_Snorm
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X16Y16_Uscaled
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X16Y16_Sscaled
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X16Y16_Uint
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16Y16_Sint
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X16Y16_Float
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X16Y16Z16W16_Unorm
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16Y16Z16W16_Snorm
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // X16Y16Z16W16_Uscaled
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // X16Y16Z16W16_Sscaled
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sscaled,
    },
    // X16Y16Z16W16_Uint
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16Y16Z16W16_Sint
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X16Y16Z16W16_Float
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X32_Uint
    {
        32,  1,                             // 32 bpp, 1 component
        { 32,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X32_Sint
    {
        32,  1,                             // 32 bpp, 1 component
        { 32,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X32_Float
    {
        32,  1,                             // 32 bpp, 1 component
        { 32,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X32Y32_Uint
    {
        64,  2,                             // 64 bpp, 2 components
        { 32, 32,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X32Y32_Sint
    {
        64,  2,                             // 64 bpp, 2 components
        { 32, 32,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X32Y32_Float
    {
        64,  2,                             // 64 bpp, 2 components
        { 32, 32,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X32Y32Z32_Uint
    {
        96,  3,                             // 96 bpp, 3 components
        { 32, 32, 32,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X32Y32Z32_Sint
    {
        96,  3,                             // 96 bpp, 3 components
        { 32, 32, 32,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X32Y32Z32_Float
    {
        96,  3,                             // 96 bpp, 3 components
        { 32, 32, 32,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X32Y32Z32W32_Uint
    {
        128, 4,                             // 128 bpp, 4 components
        { 32, 32, 32, 32, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X32Y32Z32W32_Sint
    {
        128, 4,                             // 128 bpp, 4 components
        { 32, 32, 32, 32, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Sint,
    },
    // X32Y32Z32W32_Float
    {
        128, 4,                             // 128 bpp, 4 components
        { 32, 32, 32, 32, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // D16_Unorm_S8_Uint
    {
        24,  2,                             // 24 bpp, 2 components
        { 16,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::DepthStencil,
    },
    // D32_Float_S8_Uint
    {
        40,  2,                             // 40 bpp, 2 components
        { 32,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::DepthStencil,
    },
    // X9Y9Z9E5_Float
    {
        32,  4,                             // 32 bpp, 4 components
        {  9,  9,  9,  5, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // Bc1_Unorm
    {
        64,  4,                             // 64 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc1_Srgb
    {
        64,  4,                             // 64 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Bc2_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc2_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Bc3_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc3_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Bc4_Unorm
    {
        64,  1,                             // 64 bpp, 1 component
        {  0,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc4_Snorm
    {
        64,  1,                             // 64 bpp, 1 component
        {  0,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // Bc5_Unorm
    {
        128, 2,                             // 128 bpp, 2 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc5_Snorm
    {
        128, 2,                             // 128 bpp, 2 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // Bc6_Ufloat
    {
        128, 3,                             // 128 bpp, 3 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // Bc6_Sfloat
    {
        128, 3,                             // 128 bpp, 3 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // Bc7_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Bc7_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Etc2X8Y8Z8_Unorm
    {
        64,  3,                             // 64 bpp, 3 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Etc2X8Y8Z8_Srgb
    {
        64,  3,                             // 64 bpp, 3 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Etc2X8Y8Z8W1_Unorm
    {
        64,  4,                             // 64 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Etc2X8Y8Z8W1_Srgb
    {
        64,  4,                             // 64 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Etc2X8Y8Z8W8_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Etc2X8Y8Z8W8_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // Etc2X11_Unorm
    {
        64,  1,                             // 64 bpp, 1 component
        {  0,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Etc2X11_Snorm
    {
        64,  1,                             // 64 bpp, 1 component
        {  0,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // Etc2X11Y11_Unorm
    {
        128, 2,                             // 128 bpp, 2 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Etc2X11Y11_Snorm
    {
        128, 2,                             // 128 bpp, 2 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Snorm,
    },
    // AstcLdr4x4_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr4x4_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr5x4_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr5x4_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr5x5_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr5x5_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr6x5_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr6x5_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr6x6_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr6x6_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr8x5_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr8x5_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr8x6_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr8x6_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr8x8_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr8x8_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr10x5_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr10x5_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr10x6_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr10x6_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr10x8_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr10x8_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr10x10_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr10x10_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr12x10_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr12x10_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcLdr12x12_Unorm
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // AstcLdr12x12_Srgb
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Srgb,
    },
    // AstcHdr4x4_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr5x4_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr5x5_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr6x5_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr6x6_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr8x5_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr8x6_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr8x8_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr10x5_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr10x6_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr10x8_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr10x10_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr12x10_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // AstcHdr12x12_Float
    {
        128, 4,                             // 128 bpp, 4 components
        {  0,  0,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (BitCountInaccurate |
         BlockCompressed),                  // Format Properties
        NumericSupportFlags::Float,
    },
    // X8Y8_Z8Y8_Unorm
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8_Z8Y8_Uscaled
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // Y8X8_Y8Z8_Unorm
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Unorm,
    },
    // Y8X8_Y8Z8_Uscaled
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  1, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Uscaled,
    },
    // AYUV
    {
        32,  4,                             // 32 bpp, 4 components
        {  8,  8,  8,  8, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (YuvPacked),                        // Format Properties
        NumericSupportFlags::Yuv,
    },
    // UYVY
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Yuv,
    },
    // VYUY
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Yuv,
    },
    // YUY2
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Yuv,
    },
    // YVY2
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Yuv,
    },
    // YV12
    {
        16,  3,                             // 16 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // NV11
    {
        12,  3,                             // 12 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // NV12
    {
        12,  3,                             // 12 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // NV21
    {
        12,  3,                             // 12 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // P016
    {
        24,  3,                             // 24 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // P010
    {
        24,  3,                             // 24 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // P210
    {
        16,  3,                             // 16 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // X8_MM_Unorm
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8_MM_Uint
    {
        8,   1,                             // 8 bpp,  1 component
        {  8,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X8Y8_MM_Unorm
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X8Y8_MM_Uint
    {
        16,  2,                             // 16 bpp, 2 components
        {  8,  8,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16_MM10_Unorm
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16_MM10_Uint
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16Y16_MM10_Unorm
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16Y16_MM10_Uint
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // P208
    {
        12,  3,                             // 12 bpp, 3 components
        {  8,  8,  8,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // X16_MM12_Unorm
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16_MM12_Uint
    {
        16,  1,                             // 16 bpp, 1 component
        { 16,  0,  0,  0, },
        ChannelFlags::X,                    // Channel Mask: X---
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // X16Y16_MM12_Unorm
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Unorm,
    },
    // X16Y16_MM12_Uint
    {
        32,  2,                             // 32 bpp, 2 components
        { 16, 16,  0,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y),                  // Channel Mask: XY--
        0,                                  // Format Properties
        NumericSupportFlags::Uint,
    },
    // P012
    {
        18,  3,                             // 18 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // P212
    {
        24,  3,                             // 24 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // P412
    {
        36,  3,                             // 36 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPlanar |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // X10Y10Z10W2_Float
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        0,                                  // Format Properties
        NumericSupportFlags::Float,
    },
    // Y216
    {
        32,  3,                             // 32 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked),                 // Format Properties
        NumericSupportFlags::Yuv,
    },
    // Y210
    {
        32,  3,                             // 32 bpp, 3 components
        { 16, 16, 16,  0, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z),                  // Channel Mask: XYZ-
        (YuvPacked |
         MacroPixelPacked |
         BitCountInaccurate),               // Format Properties
        NumericSupportFlags::Yuv,
    },
    // Y416
    {
        64,  4,                             // 64 bpp, 4 components
        { 16, 16, 16, 16, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (YuvPacked),                        // Format Properties
        NumericSupportFlags::Yuv,
    },
    // Y410
    {
        32,  4,                             // 32 bpp, 4 components
        { 10, 10, 10,  2, },
        (ChannelFlags::X |
         ChannelFlags::Y |
         ChannelFlags::Z |
         ChannelFlags::W),                  // Channel Mask: XYZW
        (YuvPacked),                        // Format Properties
        NumericSupportFlags::Yuv,
    },
};

} // Formats
} // Pal
