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
* @file  g_gfx9MergedDataFormats.h
* @brief Auto-generated file that describes the channel format properties for Gfx9.
***********************************************************************************************************************
*/

#include "pal.h"
#include "core/device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"

namespace Pal
{
namespace Formats
{
namespace Gfx9
{

// Handy flag combinations that we can use to build the format support info table.
//
// Translation table:
//   B    = blending
//   C    = color target
//   D    = depth target
//   I    = image, used in conjunction with "r", "w", "a", "fl", and "fmm"
//   M    = multi-sampled
//   S    = stencil
//   T    = memory view in a shader, used in conjunction with "r", "w", and "a"
//   X    = format conversion
//   P    = windowed-mode present
//   r    = read
//   w    = write
//   a    = atomics
//   fl   = filter linear
//   fmm  = filter min max
constexpr FormatFeatureFlags None                     = static_cast<FormatFeatureFlags>(0);
constexpr FormatFeatureFlags Copy                     = static_cast<FormatFeatureFlags>(FormatFeatureCopy);
constexpr FormatFeatureFlags Xd                       = static_cast<FormatFeatureFlags>(FormatFeatureFormatConversionDst);
constexpr FormatFeatureFlags IrXs                     = static_cast<FormatFeatureFlags>(Copy                         |
                                                                                        FormatFeatureImageShaderRead |
                                                                                        FormatFeatureFormatConversionSrc);
constexpr FormatFeatureFlags Iw                       = static_cast<FormatFeatureFlags>(Copy | FormatFeatureImageShaderWrite);
constexpr FormatFeatureFlags IwXd                     = static_cast<FormatFeatureFlags>(Iw | Xd);
constexpr FormatFeatureFlags Ia                       = static_cast<FormatFeatureFlags>(FormatFeatureImageShaderAtomics);
constexpr FormatFeatureFlags Ifl                      = static_cast<FormatFeatureFlags>(FormatFeatureImageFilterLinear);
constexpr FormatFeatureFlags Ifmm                     = static_cast<FormatFeatureFlags>(FormatFeatureImageFilterMinMax);
constexpr FormatFeatureFlags Tr                       = static_cast<FormatFeatureFlags>(FormatFeatureMemoryShaderRead);
constexpr FormatFeatureFlags Tw                       = static_cast<FormatFeatureFlags>(FormatFeatureMemoryShaderWrite);
constexpr FormatFeatureFlags Ta                       = static_cast<FormatFeatureFlags>(FormatFeatureMemoryShaderAtomics);
constexpr FormatFeatureFlags CB                       = static_cast<FormatFeatureFlags>(Copy | FormatFeatureColorTargetWrite |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags CBP                      = static_cast<FormatFeatureFlags>(CB | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags CBX                      = static_cast<FormatFeatureFlags>(CB | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags CBXP                     = static_cast<FormatFeatureFlags>(CBX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIfl                  = static_cast<FormatFeatureFlags>(IrXs | Ifl);
constexpr FormatFeatureFlags IrXsIflIfmm              = static_cast<FormatFeatureFlags>(IrXsIfl | Ifmm);
constexpr FormatFeatureFlags IrXsIw                   = static_cast<FormatFeatureFlags>(IrXs | Iw);
constexpr FormatFeatureFlags IrXsIwIfl                = static_cast<FormatFeatureFlags>(IrXs | Iw | Ifl);
constexpr FormatFeatureFlags IrXsIwIflIfmm            = static_cast<FormatFeatureFlags>(IrXs | Iw | Ifl | Ifmm);
constexpr FormatFeatureFlags IrXsIa                   = static_cast<FormatFeatureFlags>(IrXs | Ia);
constexpr FormatFeatureFlags IrXsIaIfl                = static_cast<FormatFeatureFlags>(IrXs | Ia | Ifl);
constexpr FormatFeatureFlags IrXsIaIflIfmm            = static_cast<FormatFeatureFlags>(IrXsIaIfl | Ifmm);
constexpr FormatFeatureFlags TrTw                     = static_cast<FormatFeatureFlags>(Tr | Tw);
constexpr FormatFeatureFlags TrTa                     = static_cast<FormatFeatureFlags>(Tr | Ta);
constexpr FormatFeatureFlags IrXsTr                   = static_cast<FormatFeatureFlags>(IrXs | Tr);
constexpr FormatFeatureFlags IrXsIflTr                = static_cast<FormatFeatureFlags>(IrXs | Ifl | Tr);
constexpr FormatFeatureFlags IrXsTrTw                 = static_cast<FormatFeatureFlags>(IrXs | TrTw);
constexpr FormatFeatureFlags IrXsIfmmTrTw             = static_cast<FormatFeatureFlags>(IrXsTrTw | Ifmm);
constexpr FormatFeatureFlags IrXsIflTrTa              = static_cast<FormatFeatureFlags>(IrXsIfl | TrTa);
constexpr FormatFeatureFlags IrXsIflTrTw              = static_cast<FormatFeatureFlags>(IrXsIfl | TrTw);
constexpr FormatFeatureFlags IrXsIflIfmmTrTw          = static_cast<FormatFeatureFlags>(IrXsIflTrTw | Ifmm);
constexpr FormatFeatureFlags IrXsIwTrTw               = static_cast<FormatFeatureFlags>(IrXsIw | TrTw);
constexpr FormatFeatureFlags IrXsIwIflTrTw            = static_cast<FormatFeatureFlags>(IrXsIwIfl | TrTw);
constexpr FormatFeatureFlags IrXsIaTrTa               = static_cast<FormatFeatureFlags>(IrXsIa | Tr | Ta);
constexpr FormatFeatureFlags IrXsIaIflTrTa            = static_cast<FormatFeatureFlags>(IrXsIaIfl | Tr | Ta);
constexpr FormatFeatureFlags IrXsIwIaTrTwTa           = static_cast<FormatFeatureFlags>(IrXsIaTrTa | Iw | Tw);
constexpr FormatFeatureFlags IrXsIwIaIfmmTrTwTa       = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa | Ifmm);
constexpr FormatFeatureFlags IrXsIflTrTaM             = static_cast<FormatFeatureFlags>(IrXsIflTrTa | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwIaTrTwTaM          = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwIaIfmmTrTwTaM      = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTaM | Ifmm);
constexpr FormatFeatureFlags IrXsIwIaIflTrTwTa        = static_cast<FormatFeatureFlags>(IrXsIaIflTrTa | Iw | Tw);
constexpr FormatFeatureFlags IrXsIwIaTrTwTaP          = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwIaTrTwTaMP         = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTaP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwTrTwP              = static_cast<FormatFeatureFlags>(IrXsIwTrTw | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwTrTwM              = static_cast<FormatFeatureFlags>(IrXsIwTrTw | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwTrTwMP             = static_cast<FormatFeatureFlags>(IrXsIwTrTwP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsCB                   = static_cast<FormatFeatureFlags>(IrXs | CB);
constexpr FormatFeatureFlags IrXsIflCB                = static_cast<FormatFeatureFlags>(IrXsIfl | CB);
constexpr FormatFeatureFlags IrXsCBX                  = static_cast<FormatFeatureFlags>(IrXsCB | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIflCBX               = static_cast<FormatFeatureFlags>(IrXsIflCB | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsXdIflCBX             = static_cast<FormatFeatureFlags>(IrXsIflCBX | FormatFeatureFormatConversionDst);
constexpr FormatFeatureFlags IrXsXdIflIfmmCBX         = static_cast<FormatFeatureFlags>(IrXsXdIflCBX | Ifmm);
constexpr FormatFeatureFlags IrXsXdIflCBXP            = static_cast<FormatFeatureFlags>(IrXsXdIflCBX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsXdIflIfmmCBXP        = static_cast<FormatFeatureFlags>(IrXsXdIflIfmmCBX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsXdIflIfmmTaCBXP      = static_cast<FormatFeatureFlags>(IrXsXdIflIfmmCBXP | Ta);
constexpr FormatFeatureFlags IrXsCBP                  = static_cast<FormatFeatureFlags>(IrXsCB | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsM                    = static_cast<FormatFeatureFlags>(IrXs | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIflM                 = static_cast<FormatFeatureFlags>(IrXsIfl | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsDM                   = static_cast<FormatFeatureFlags>(IrXsM | FormatFeatureDepthTarget);
constexpr FormatFeatureFlags IrXsTrM                  = static_cast<FormatFeatureFlags>(IrXsTr | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIflTrM               = static_cast<FormatFeatureFlags>(IrXsIflTr | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIaTrTaM              = static_cast<FormatFeatureFlags>(IrXsIaTrTa | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIaIflTrTaM           = static_cast<FormatFeatureFlags>(IrXsIaIflTrTa | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsSM                   = static_cast<FormatFeatureFlags>(IrXs | FormatFeatureStencilTarget |
                                                                                        FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsDSM                  = static_cast<FormatFeatureFlags>(IrXsDM | IrXsSM);
constexpr FormatFeatureFlags IrXsCBM                  = static_cast<FormatFeatureFlags>(IrXsCB | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIflCBM               = static_cast<FormatFeatureFlags>(IrXsIflCB | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsCBMX                 = static_cast<FormatFeatureFlags>(IrXsCBM | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIflCBMX              = static_cast<FormatFeatureFlags>(IrXsIflCBM | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsXdIflCBMX            = static_cast<FormatFeatureFlags>(IrXsIflCBMX | FormatFeatureFormatConversionDst);
constexpr FormatFeatureFlags IrXsXdIflIfmmCBMX        = static_cast<FormatFeatureFlags>(IrXsXdIflCBMX | Ifmm);
constexpr FormatFeatureFlags IrXsXdIflCBMXP           = static_cast<FormatFeatureFlags>(IrXsXdIflCBMX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsXdIflIfmmCBMXP       = static_cast<FormatFeatureFlags>(IrXsXdIflIfmmCBMX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsXdIflIfmmTaCBMXP     = static_cast<FormatFeatureFlags>(IrXsXdIflIfmmCBMXP | Ta);
constexpr FormatFeatureFlags IrXsCBMP                 = static_cast<FormatFeatureFlags>(IrXsCBM | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIaTaCBP              = static_cast<FormatFeatureFlags>(IrXs | Ia | Ta | CBP);
constexpr FormatFeatureFlags IrXsIaIflTaCBP           = static_cast<FormatFeatureFlags>(IrXs | Ia | Ifl | Ta | CBP);
constexpr FormatFeatureFlags IrXsIaTaCBXP             = static_cast<FormatFeatureFlags>(IrXsIaTaCBP | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIaIflTaCBXP          = static_cast<FormatFeatureFlags>(IrXsIaIflTaCBP | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsXdIaIflTaCBXP        = static_cast<FormatFeatureFlags>(IrXsIaIflTaCBXP | FormatFeatureFormatConversionDst);
constexpr FormatFeatureFlags IrXsXdIaIflIfmmTaCBXP    = static_cast<FormatFeatureFlags>(IrXsXdIaIflTaCBXP | Ifmm);
constexpr FormatFeatureFlags IrXsIaTaCBMP             = static_cast<FormatFeatureFlags>(IrXsIaTaCBP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIaIflTaCBMP          = static_cast<FormatFeatureFlags>(IrXsIaIflTaCBP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIaTaCBMXP            = static_cast<FormatFeatureFlags>(IrXsIaTaCBMP | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIaIflTaCBMXP         = static_cast<FormatFeatureFlags>(IrXsIaIflTaCBMP | FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsXdIaIflTaCBMXP       = static_cast<FormatFeatureFlags>(IrXsIaIflTaCBMXP | FormatFeatureFormatConversionDst);
constexpr FormatFeatureFlags IrXsXdIaIflIfmmTaCBMXP   = static_cast<FormatFeatureFlags>(IrXsXdIaIflTaCBMXP | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCBX              = static_cast<FormatFeatureFlags>(IrXsIw | CBX | Xd);
constexpr FormatFeatureFlags IrXsIwXdIflCBX           = static_cast<FormatFeatureFlags>(IrXsIwIfl | CBX | Xd);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmCBX       = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBX | Ifmm);
constexpr FormatFeatureFlags IrXsIwIaTrTwTaBP         = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa                  |
                                                                                        FormatFeatureColorTargetBlend |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwIaTrTwTaBMP        = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTaBP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCX       = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa | Xd           |
                                                                                        FormatFeatureColorTargetWrite |
                                                                                        FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIwXdIaIfmmTrTwTaCX   = static_cast<FormatFeatureFlags>(IrXsIwXdIaTrTwTaCX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCMX      = static_cast<FormatFeatureFlags>(IrXsIwXdIaTrTwTaCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIfmmTrTwTaCMX  = static_cast<FormatFeatureFlags>(IrXsIwXdIaTrTwTaCMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCBX      = static_cast<FormatFeatureFlags>(IrXsIwIaTrTwTa | CBX | Xd);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCBX   = static_cast<FormatFeatureFlags>(IrXsIwIaIflTrTwTa | CBX | Xd);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmTrTwTaCBX = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCBMX     = static_cast<FormatFeatureFlags>(IrXsIwXdIaTrTwTaCBX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCBMX  = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmTrTwTaCBMX  = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCBXP             = static_cast<FormatFeatureFlags>(IrXsIwXdCBX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflCBXP          = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmCBXP      = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBXP | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCBMX             = static_cast<FormatFeatureFlags>(IrXsIwXdCBX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIflCBMX          = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmCBMX      = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCBMXP            = static_cast<FormatFeatureFlags>(IrXsIwXdCBXP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIflCBMXP         = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBXP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmCBMXP     = static_cast<FormatFeatureFlags>(IrXsIwXdIflCBMXP | Ifmm);
constexpr FormatFeatureFlags IrXsIwTrTwBP             = static_cast<FormatFeatureFlags>(IrXsIwTrTw | FormatFeatureColorTargetBlend |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwTrTwBMP            = static_cast<FormatFeatureFlags>(IrXsIwTrTwBP | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdTrTwCX           = static_cast<FormatFeatureFlags>(IrXsIwTrTw | Xd               |
                                                                                        FormatFeatureColorTargetWrite |
                                                                                        FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCX       = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCX               = static_cast<FormatFeatureFlags>(IrXsIw | Xd | FormatFeatureColorTargetWrite |
                                                                                        FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIwXdIfmmCX           = static_cast<FormatFeatureFlags>(IrXsIwXdCX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdCMX              = static_cast<FormatFeatureFlags>(IrXsIwXdCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaCMX            = static_cast<FormatFeatureFlags>(IrXsIwXdCMX | Ia);
constexpr FormatFeatureFlags IrXsIwXdIaCX             = static_cast<FormatFeatureFlags>(IrXsIwXdCX | Ia);
constexpr FormatFeatureFlags IrXsIwXdIfmmCMX          = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIfmmCX         = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmCX | Ia);
constexpr FormatFeatureFlags IrXsIwXdIaIfmmCMX        = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmCMX | Ia);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCX        = static_cast<FormatFeatureFlags>(IrXsIwIflTrTw | Xd            |
                                                                                        FormatFeatureColorTargetWrite |
                                                                                        FormatFeatureFormatConversion);
constexpr FormatFeatureFlags IrXsIwXdTrTwCXP          = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCXP       = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCX | FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCXP      = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCX    = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCX | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCMX   = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCXP   = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdTrTwCMX          = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCMX      = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdTrTwCSMX         = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCMX |
                                                                                        FormatFeatureStencilTarget);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCSMX     = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCSMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCMX       = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCX | FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdTrTwCMXP         = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCMXP      = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCMXP     = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCMXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCMXP  = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCMXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdTrTwCBX          = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCBX       = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCBX      = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmTrTwCX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwCBX   = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwTaCBX = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwCBX | Ta);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCBXP     = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmTrTwCBX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwCBXP  = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwCBX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdTrTwCBXP         = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCBX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCBXP     = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCBXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCBXP      = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCBXP  = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwTaCBXP = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwTaCBX |
                                                                                         FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwTaCBMXP = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwTaCBXP |
                                                                                         FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmTrTwTaCBXP = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBXP | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdTrTwCBMX         = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCMX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdTrTwCBDMX        = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCBMX |
                                                                                        FormatFeatureDepthTarget);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCBMX      = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCMX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCBMX     = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmTrTwCMX |
                                                                                        FormatFeatureColorTargetBlend);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCBMXP    = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmTrTwCBMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwCBMX  = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwCBMXP = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwCBMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwTaCBMX = static_cast<FormatFeatureFlags>(IrXsIwXdIflIfmmTrTwCBMX | Ta);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCBDMX     = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBMX |
                                                                                        FormatFeatureDepthTarget);
constexpr FormatFeatureFlags IrXsIwXdIfmmTrTwCBDMX    = static_cast<FormatFeatureFlags>(IrXsIwXdIfmmTrTwCBMX |
                                                                                        FormatFeatureDepthTarget);
constexpr FormatFeatureFlags IrXsIwXdIflIfmmTrTwCBDMX = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBDMX | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdTrTwCBMXP        = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCBMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaTrTwTaCBMXP    = static_cast<FormatFeatureFlags>(IrXsIwXdTrTwCBMXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIflTrTwCBMXP     = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBMX |
                                                                                        FormatFeatureWindowedPresent);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCBMXP = static_cast<FormatFeatureFlags>(IrXsIwXdIflTrTwCBMXP | Ia | Ta);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmTrTwTaCBMXP = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBMXP | Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIaIflTrTwTaCBDMX = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBMXP |
                                                                                        FormatFeatureDepthTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmCBX     = static_cast<FormatFeatureFlags>(IrXsIwXdCBX |
                                                                                        Ia |
                                                                                        Ifl |
                                                                                        Ifmm);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmCBMX    = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflIfmmCBX |
                                                                                        FormatFeatureMsaaTarget);
constexpr FormatFeatureFlags IrXsIwXdIaIflIfmmTrTwTaCBDMX = static_cast<FormatFeatureFlags>(IrXsIwXdIaIflTrTwTaCBDMX | Ifmm);

// Lookup table for GPU access capabilities for each format/tiling-type pairing in Gfx9.
constexpr MergedFormatPropertiesTable Gfx9MergedFormatPropertiesTable =
{
    {
        // Note: Feature capabilities are listed in (linear, optimal) order.
        { None,                           None                           }, // ChNumFormat::Undefined
        { None,                           None                           }, // ChNumFormat::X1_Unorm
        { None,                           None                           }, // ChNumFormat::X1_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X4Y4_Unorm
        { IrXsIfl,                        IrXsIfl                        }, // ChNumFormat::X4Y4_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::L4A4_Unorm
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X4Y4Z4W4_Unorm
        { IrXsIfl,                        IrXsIflM                       }, // ChNumFormat::X4Y4Z4W4_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y6Z5_Unorm
        { IrXsIfl,                        IrXsIflM                       }, // ChNumFormat::X5Y6Z5_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y5Z5W1_Unorm
        { IrXsIfl,                        IrXsIflM                       }, // ChNumFormat::X5Y5Z5W1_Uscaled
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X1Y5Z5W5_Unorm
        { IrXsIfl,                        IrXsIflM                       }, // ChNumFormat::X1Y5Z5W5_Uscaled
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCSMX           }, // ChNumFormat::X8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::P8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8Y8_Srgb
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8A8_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X8Y8Z8W8_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X8Y8Z8W8_Snorm
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X8Y8Z8W8_Uscaled
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X8Y8Z8W8_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Sint
        { IrXsXdIflIfmmTaCBXP,            IrXsXdIflIfmmTaCBMXP           }, // ChNumFormat::X8Y8Z8W8_Srgb
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        { IrXsIwXdIaIflIfmmTrTwTaCBX,     IrXsIwXdIaIflIfmmTrTwTaCBMX    }, // ChNumFormat::X10Y11Z11_Float
        { IrXsIwXdIaIflIfmmTrTwTaCBXP,    IrXsIwXdIaIflIfmmTrTwTaCBMXP   }, // ChNumFormat::X11Y11Z10_Float
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X10Y10Z10W2_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBX,       IrXsIwXdIflIfmmTrTwTaCBMX      }, // ChNumFormat::X10Y10Z10W2_Snorm
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X10Y10Z10W2_Uscaled
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X10Y10Z10W2_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X10Y10Z10W2_Uint
        { IrXsIwIaIfmmTrTwTa,             IrXsIwIaIfmmTrTwTaM            }, // ChNumFormat::X10Y10Z10W2_Sint
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X10Y10Z10W2Bias_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Float
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L16_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBX,       IrXsIwXdIflIfmmTrTwTaCBMX      }, // ChNumFormat::X16Y16_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBX,       IrXsIwXdIflIfmmTrTwTaCBMX      }, // ChNumFormat::X16Y16_Snorm
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X16Y16_Uscaled
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X16Y16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Sint
        { IrXsIwXdIaIflIfmmTrTwTaCBX,     IrXsIwXdIaIflIfmmTrTwTaCBMX    }, // ChNumFormat::X16Y16_Float
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X16Y16Z16W16_Unorm
        { IrXsIwXdIflIfmmTrTwTaCBXP,      IrXsIwXdIflIfmmTrTwTaCBMXP     }, // ChNumFormat::X16Y16Z16W16_Snorm
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X16Y16Z16W16_Uscaled
        { IrXsIflTrTa,                    IrXsIflTrTaM                   }, // ChNumFormat::X16Y16Z16W16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Sint
        { IrXsIwXdIaIflIfmmTrTwTaCBXP,    IrXsIwXdIaIflIfmmTrTwTaCBMXP   }, // ChNumFormat::X16Y16Z16W16_Float
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Sint
        { IrXsIwXdIaIflIfmmTrTwTaCBX,     IrXsIwXdIaIflIfmmTrTwTaCBDMX   }, // ChNumFormat::X32_Float
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32Y32_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32Y32_Sint
        { IrXsIwXdIaIflIfmmTrTwTaCBX,     IrXsIwXdIaIflIfmmTrTwTaCBMX    }, // ChNumFormat::X32Y32_Float
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Uint
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Sint
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32Z32W32_Float
        { None,                           IrXsDSM                        }, // ChNumFormat::D16_Unorm_S8_Uint
        { None,                           IrXsDSM                        }, // ChNumFormat::D32_Float_S8_Uint
        { IrXsIaIflIfmm,                  IrXsIaIflIfmm                  }, // ChNumFormat::X9Y9Z9E5_Float
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Ufloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Sfloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11_Snorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Snorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Srgb
        { None,                           None                           }, // ChNumFormat::AstcHdr4x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x12_Float
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X8Y8_Z8Y8_Unorm
        { IrXsIfl,                        IrXsIfl                        }, // ChNumFormat::X8Y8_Z8Y8_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::Y8X8_Y8Z8_Unorm
        { IrXsIfl,                        IrXsIfl                        }, // ChNumFormat::Y8X8_Y8Z8_Uscaled
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::AYUV
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::UYVY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::VYUY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YUY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YVY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV11
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV21
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P016
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P010
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P210
        { None,                           None                           }, // ChNumFormat::X8_MM_Unorm
        { None,                           None                           }, // ChNumFormat::X8_MM_Uint
        { None,                           None                           }, // ChNumFormat::X8Y8_MM_Unorm
        { None,                           None                           }, // ChNumFormat::X8Y8_MM_Uint
        { None,                           None                           }, // ChNumFormat::X16_MM10_Unorm
        { None,                           None                           }, // ChNumFormat::X16_MM10_Uint
        { None,                           None                           }, // ChNumFormat::X16Y16_MM10_Unorm
        { None,                           None                           }, // ChNumFormat::X16Y16_MM10_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P208
        { None,                           None                           }, // ChNumFormat::X16_MM12_Unorm
        { None,                           None                           }, // ChNumFormat::X16_MM12_Uint
        { None,                           None                           }, // ChNumFormat::X16Y16_MM12_Unorm
        { None,                           None                           }, // ChNumFormat::X16Y16_MM12_Uint
        { None,                           None                           }, // ChNumFormat::P012
        { None,                           None                           }, // ChNumFormat::P212
        { None,                           None                           }, // ChNumFormat::P412
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Float
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y216
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y210
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y416
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y410
    }
};

// Stores a ChannelFmtInfo struct for each available channel format for mapping PAL channel formats to the format values
// for various hardware blocks.
constexpr MergedFmtInfo Gfx9MergedChannelFmtInfoTbl[] =
{
    // ChNumFormat::Undefined
    {
        ChNumFormat::Undefined,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Unorm
    {
        ChNumFormat::X1_Unorm,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Uscaled
    {
        ChNumFormat::X1_Uscaled,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Unorm
    {
        ChNumFormat::X4Y4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_4_4__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Uscaled
    {
        ChNumFormat::X4Y4_Uscaled,         // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_4_4__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L4A4_Unorm
    {
        ChNumFormat::L4A4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_4_4__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Unorm
    {
        ChNumFormat::X4Y4Z4W4_Unorm,       // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_4_4_4_4,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Uscaled
    {
        ChNumFormat::X4Y4Z4W4_Uscaled,     // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_4_4_4_4,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Unorm
    {
        ChNumFormat::X5Y6Z5_Unorm,         // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_5_6_5,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Uscaled
    {
        ChNumFormat::X5Y6Z5_Uscaled,       // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_5_6_5,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Unorm
    {
        ChNumFormat::X5Y5Z5W1_Unorm,       // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_1_5_5_5,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Uscaled
    {
        ChNumFormat::X5Y5Z5W1_Uscaled,     // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_1_5_5_5,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Unorm
    {
        ChNumFormat::X1Y5Z5W5_Unorm,       // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_5_5_5_1,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Uscaled
    {
        ChNumFormat::X1Y5Z5W5_Uscaled,     // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_5_5_5_1,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Unorm
    {
        ChNumFormat::X8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Snorm
    {
        ChNumFormat::X8_Snorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uscaled
    {
        ChNumFormat::X8_Uscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Sscaled
    {
        ChNumFormat::X8_Sscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uint
    {
        ChNumFormat::X8_Uint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X8_Sint
    {
        ChNumFormat::X8_Sint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Srgb
    {
        ChNumFormat::X8_Srgb,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::A8_Unorm
    {
        ChNumFormat::A8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8_Unorm
    {
        ChNumFormat::L8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P8_Unorm
    {
        ChNumFormat::P8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8,           // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8,           // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Unorm
    {
        ChNumFormat::X8Y8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Snorm
    {
        ChNumFormat::X8Y8_Snorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Uscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sscaled
    {
        ChNumFormat::X8Y8_Sscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uint
    {
        ChNumFormat::X8Y8_Uint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sint
    {
        ChNumFormat::X8Y8_Sint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Srgb
    {
        ChNumFormat::X8Y8_Srgb,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8A8_Unorm
    {
        ChNumFormat::L8A8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8,         // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8,         // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Unorm
    {
        ChNumFormat::X8Y8Z8W8_Unorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Snorm
    {
        ChNumFormat::X8Y8Z8W8_Snorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uscaled
    {
        ChNumFormat::X8Y8Z8W8_Uscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sscaled
    {
        ChNumFormat::X8Y8Z8W8_Sscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uint
    {
        ChNumFormat::X8Y8Z8W8_Uint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sint
    {
        ChNumFormat::X8Y8Z8W8_Sint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Srgb
    {
        ChNumFormat::X8Y8Z8W8_Srgb,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_8_8_8_8,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_8_8_8_8,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U8V8_Snorm_L8W8_Unorm
    {
        ChNumFormat::U8V8_Snorm_L8W8_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y11Z11_Float
    {
        ChNumFormat::X10Y11Z11_Float,      // ChNumFormat
        Chip::COLOR_11_11_10,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_11_11_10,    // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_11_11_10,    // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X11Y11Z10_Float
    {
        ChNumFormat::X11Y11Z10_Float,      // ChNumFormat
        Chip::COLOR_10_11_11,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_10_11_11,    // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_10_11_11,    // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Unorm
    {
        ChNumFormat::X10Y10Z10W2_Unorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Snorm
    {
        ChNumFormat::X10Y10Z10W2_Snorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uscaled
    {
        ChNumFormat::X10Y10Z10W2_Uscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sscaled
    {
        ChNumFormat::X10Y10Z10W2_Sscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uint
    {
        ChNumFormat::X10Y10Z10W2_Uint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sint
    {
        ChNumFormat::X10Y10Z10W2_Sint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2Bias_Unorm
    {
        ChNumFormat::X10Y10Z10W2Bias_Unorm, // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_2_10_10_10,  // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U10V10W10_Snorm_A2_Unorm
    {
        ChNumFormat::U10V10W10_Snorm_A2_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Unorm
    {
        ChNumFormat::X16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Snorm
    {
        ChNumFormat::X16_Snorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uscaled
    {
        ChNumFormat::X16_Uscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sscaled
    {
        ChNumFormat::X16_Sscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uint
    {
        ChNumFormat::X16_Uint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sint
    {
        ChNumFormat::X16_Sint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Float
    {
        ChNumFormat::X16_Float,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L16_Unorm
    {
        ChNumFormat::L16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Unorm
    {
        ChNumFormat::X16Y16_Unorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Snorm
    {
        ChNumFormat::X16Y16_Snorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uscaled
    {
        ChNumFormat::X16Y16_Uscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sscaled
    {
        ChNumFormat::X16Y16_Sscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uint
    {
        ChNumFormat::X16Y16_Uint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sint
    {
        ChNumFormat::X16Y16_Sint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Float
    {
        ChNumFormat::X16Y16_Float,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Unorm
    {
        ChNumFormat::X16Y16Z16W16_Unorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Snorm
    {
        ChNumFormat::X16Y16Z16W16_Snorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uscaled
    {
        ChNumFormat::X16Y16Z16W16_Uscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sscaled
    {
        ChNumFormat::X16Y16Z16W16_Sscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SSCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SSCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uint
    {
        ChNumFormat::X16Y16Z16W16_Uint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sint
    {
        ChNumFormat::X16Y16Z16W16_Sint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Float
    {
        ChNumFormat::X16Y16Z16W16_Float,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_16_16_16_16, // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_16_16_16_16, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Uint
    {
        ChNumFormat::X32_Uint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Sint
    {
        ChNumFormat::X32_Sint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Float
    {
        ChNumFormat::X32_Float,            // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32,          // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32,          // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Uint
    {
        ChNumFormat::X32Y32_Uint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Sint
    {
        ChNumFormat::X32Y32_Sint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Float
    {
        ChNumFormat::X32Y32_Float,         // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32,       // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32,       // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Uint
    {
        ChNumFormat::X32Y32Z32_Uint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32,    // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32,    // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Sint
    {
        ChNumFormat::X32Y32Z32_Sint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32,    // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32,    // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Float
    {
        ChNumFormat::X32Y32Z32_Float,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32,    // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32,    // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Uint
    {
        ChNumFormat::X32Y32Z32W32_Uint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32_32, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32_32, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Sint
    {
        ChNumFormat::X32Y32Z32W32_Sint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32_32, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32_32, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Float
    {
        ChNumFormat::X32Y32Z32W32_Float,   // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_32_32_32_32, // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_32_32_32_32, // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::D16_Unorm_S8_Uint
    {
        ChNumFormat::D16_Unorm_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::D32_Float_S8_Uint
    {
        ChNumFormat::D32_Float_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X9Y9Z9E5_Float
    {
        ChNumFormat::X9Y9Z9E5_Float,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_5_9_9_9__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Unorm
    {
        ChNumFormat::Bc1_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC1__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Srgb
    {
        ChNumFormat::Bc1_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC1__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Unorm
    {
        ChNumFormat::Bc2_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC2__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Srgb
    {
        ChNumFormat::Bc2_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC2__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Unorm
    {
        ChNumFormat::Bc3_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC3__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Srgb
    {
        ChNumFormat::Bc3_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC3__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Unorm
    {
        ChNumFormat::Bc4_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC4__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Snorm
    {
        ChNumFormat::Bc4_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC4__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Unorm
    {
        ChNumFormat::Bc5_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC5__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Snorm
    {
        ChNumFormat::Bc5_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC5__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Ufloat
    {
        ChNumFormat::Bc6_Ufloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC6__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Sfloat
    {
        ChNumFormat::Bc6_Sfloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC6__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Unorm
    {
        ChNumFormat::Bc7_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC7__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Srgb
    {
        ChNumFormat::Bc7_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BC7__GFX09,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGBA1__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGBA1__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGBA__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RGBA__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SRGB__GFX09,  // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Unorm
    {
        ChNumFormat::Etc2X11_Unorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_R__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Snorm
    {
        ChNumFormat::Etc2X11_Snorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_R__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Unorm
    {
        ChNumFormat::Etc2X11Y11_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RG__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Snorm
    {
        ChNumFormat::Etc2X11Y11_Snorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ETC2_RG__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_SNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_SNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Unorm
    {
        ChNumFormat::AstcLdr4x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Srgb
    {
        ChNumFormat::AstcLdr4x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Unorm
    {
        ChNumFormat::AstcLdr5x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Srgb
    {
        ChNumFormat::AstcLdr5x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Unorm
    {
        ChNumFormat::AstcLdr5x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Srgb
    {
        ChNumFormat::AstcLdr5x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Unorm
    {
        ChNumFormat::AstcLdr6x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Srgb
    {
        ChNumFormat::AstcLdr6x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Unorm
    {
        ChNumFormat::AstcLdr6x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Srgb
    {
        ChNumFormat::AstcLdr6x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Unorm
    {
        ChNumFormat::AstcLdr8x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Srgb
    {
        ChNumFormat::AstcLdr8x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Unorm
    {
        ChNumFormat::AstcLdr8x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Srgb
    {
        ChNumFormat::AstcLdr8x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Unorm
    {
        ChNumFormat::AstcLdr8x8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Srgb
    {
        ChNumFormat::AstcLdr8x8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Unorm
    {
        ChNumFormat::AstcLdr10x5_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Srgb
    {
        ChNumFormat::AstcLdr10x5_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Unorm
    {
        ChNumFormat::AstcLdr10x6_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Srgb
    {
        ChNumFormat::AstcLdr10x6_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Unorm
    {
        ChNumFormat::AstcLdr10x8_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Srgb
    {
        ChNumFormat::AstcLdr10x8_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Unorm
    {
        ChNumFormat::AstcLdr10x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Srgb
    {
        ChNumFormat::AstcLdr10x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Unorm
    {
        ChNumFormat::AstcLdr12x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Srgb
    {
        ChNumFormat::AstcLdr12x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Unorm
    {
        ChNumFormat::AstcLdr12x12_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Srgb
    {
        ChNumFormat::AstcLdr12x12_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr4x4_Float
    {
        ChNumFormat::AstcHdr4x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x4_Float
    {
        ChNumFormat::AstcHdr5x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x5_Float
    {
        ChNumFormat::AstcHdr5x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x5_Float
    {
        ChNumFormat::AstcHdr6x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x6_Float
    {
        ChNumFormat::AstcHdr6x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x5_Float
    {
        ChNumFormat::AstcHdr8x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x6_Float
    {
        ChNumFormat::AstcHdr8x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x8_Float
    {
        ChNumFormat::AstcHdr8x8_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x5_Float
    {
        ChNumFormat::AstcHdr10x5_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x6_Float
    {
        ChNumFormat::AstcHdr10x6_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x8_Float
    {
        ChNumFormat::AstcHdr10x8_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x10_Float
    {
        ChNumFormat::AstcHdr10x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x10_Float
    {
        ChNumFormat::AstcHdr12x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x12_Float
    {
        ChNumFormat::AstcHdr12x12_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09, // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Unorm
    {
        ChNumFormat::X8Y8_Z8Y8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_GB_GR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Z8Y8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_GB_GR__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Unorm
    {
        ChNumFormat::Y8X8_Y8Z8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BG_RG__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Uscaled
    {
        ChNumFormat::Y8X8_Y8Z8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_DATA_FORMAT_BG_RG__GFX09, // Image Channel Format
        Chip::IMG_NUM_FORMAT_USCALED,      // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_USCALED,      // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AYUV
    {
        ChNumFormat::AYUV,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::UYVY
    {
        ChNumFormat::UYVY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::VYUY
    {
        ChNumFormat::VYUY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YUY2
    {
        ChNumFormat::YUY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YVY2
    {
        ChNumFormat::YVY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YV12
    {
        ChNumFormat::YV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV11
    {
        ChNumFormat::NV11,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV12
    {
        ChNumFormat::NV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV21
    {
        ChNumFormat::NV21,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P016
    {
        ChNumFormat::P016,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P010
    {
        ChNumFormat::P010,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P210
    {
        ChNumFormat::P210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Unorm
    {
        ChNumFormat::X8_MM_Unorm,          // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Uint
    {
        ChNumFormat::X8_MM_Uint,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Unorm
    {
        ChNumFormat::X8Y8_MM_Unorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Uint
    {
        ChNumFormat::X8Y8_MM_Uint,         // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Unorm
    {
        ChNumFormat::X16_MM10_Unorm,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Uint
    {
        ChNumFormat::X16_MM10_Uint,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Unorm
    {
        ChNumFormat::X16Y16_MM10_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Uint
    {
        ChNumFormat::X16Y16_MM10_Uint,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P208
    {
        ChNumFormat::P208,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Unorm
    {
        ChNumFormat::X16_MM12_Unorm,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Uint
    {
        ChNumFormat::X16_MM12_Uint,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Unorm
    {
        ChNumFormat::X16Y16_MM12_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UNORM,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UNORM,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Uint
    {
        ChNumFormat::X16Y16_MM12_Uint,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_UINT,         // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_UINT,         // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P012
    {
        ChNumFormat::P012,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P212
    {
        ChNumFormat::P212,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P412
    {
        ChNumFormat::P412,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Float
    {
        ChNumFormat::X10Y10Z10W2_Float,    // ChNumFormat
        Chip::COLOR_2_10_10_10_6E4,        // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_2_10_10_10,  // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y216
    {
        ChNumFormat::Y216,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y210
    {
        ChNumFormat::Y210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y416
    {
        ChNumFormat::Y416,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y410
    {
        ChNumFormat::Y410,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_DATA_FORMAT_INVALID,     // Image Channel Format
        Chip::IMG_NUM_FORMAT_FLOAT,        // Image Numeric Format
        Chip::BUF_DATA_FORMAT_INVALID,     // Buffer Image Format
        Chip::BUF_NUM_FORMAT_FLOAT,        // Buffer Numeric Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
};

// Stores a MergedImgDataFmtInfo struct for each HW image format up to the last format known to the spreadsheet.
constexpr MergedImgDataFmtInfo Gfx9MergedImgDataFmtTbl[] =
{
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_8,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X8_Unorm,
               ChNumFormat::A8_Unorm,
               ChNumFormat::L8_Unorm,
               ChNumFormat::P8_Unorm,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::X8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_16,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X16_Unorm,
               ChNumFormat::L16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_8_8,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X8Y8_Unorm,
               ChNumFormat::L8A8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X8Y8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X8Y8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X8Y8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X8Y8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X8Y8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::X8Y8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_32,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_16_16,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X16Y16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X16Y16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X16Y16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X16Y16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X16Y16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X16Y16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X16Y16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_10_11_11,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X11Y11Z10_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_11_11_10,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X10Y11Z11_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_2_10_10_10,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X10Y10Z10W2_Unorm,
               ChNumFormat::X10Y10Z10W2Bias_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X10Y10Z10W2_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X10Y10Z10W2_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X10Y10Z10W2_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X10Y10Z10W2_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X10Y10Z10W2_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X10Y10Z10W2_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_8_8_8_8,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X8Y8Z8W8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X8Y8Z8W8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X8Y8Z8W8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X8Y8Z8W8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X8Y8Z8W8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X8Y8Z8W8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::X8Y8Z8W8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_32_32,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X32Y32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X32Y32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X32Y32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_16_16_16_16,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X16Y16Z16W16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::X16Y16Z16W16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X16Y16Z16W16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::X16Y16Z16W16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X16Y16Z16W16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X16Y16Z16W16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X16Y16Z16W16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_32_32_32,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X32Y32Z32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X32Y32Z32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X32Y32Z32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_32_32_32_32,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::X32Y32Z32W32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::X32Y32Z32W32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X32Y32Z32W32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_5_6_5,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X5Y6Z5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X5Y6Z5_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_1_5_5_5,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X5Y5Z5W1_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X5Y5Z5W1_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_5_5_5_1,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X1Y5Z5W5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X1Y5Z5W5_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_4_4_4_4,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X4Y4Z4W4_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X4Y4Z4W4_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ETC2_RGB__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Etc2X8Y8Z8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Etc2X8Y8Z8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ETC2_RGBA__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Etc2X8Y8Z8W8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Etc2X8Y8Z8W8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ETC2_R__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Etc2X11_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Etc2X11_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ETC2_RG__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Etc2X11Y11_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Etc2X11Y11_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ETC2_RGBA1__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Etc2X8Y8Z8W1_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Etc2X8Y8Z8W1_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_GB_GR__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X8Y8_Z8Y8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X8Y8_Z8Y8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BG_RG__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Y8X8_Y8Z8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Y8X8_Y8Z8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_5_9_9_9__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::X9Y9Z9E5_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC1__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc1_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Bc1_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC2__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc2_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Bc2_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC3__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc3_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Bc3_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC4__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc4_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Bc4_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC5__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Bc5_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC6__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc6_Ufloat,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Bc6_Sfloat,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_BC7__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Bc7_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Bc7_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ASTC_2D_LDR__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::AstcLdr4x4_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::AstcLdr5x4_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::AstcLdr5x5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::AstcLdr6x5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::AstcLdr6x6_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::AstcLdr8x5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::AstcLdr8x6_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::AstcLdr8x8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::AstcLdr10x5_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::AstcLdr10x6_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::AstcLdr10x8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::AstcLdr10x10_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::AstcLdr12x10_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::AstcLdr12x12_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ASTC_2D_HDR__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::AstcHdr4x4_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::AstcHdr5x4_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::AstcHdr5x5_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::AstcHdr6x5_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::AstcHdr6x6_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::AstcHdr8x5_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::AstcHdr8x6_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::AstcHdr8x8_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::AstcHdr10x5_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::AstcHdr10x6_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::AstcHdr10x8_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::AstcHdr10x10_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::AstcHdr12x10_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::AstcHdr12x12_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_ASTC_2D_LDR_SRGB__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::AstcLdr4x4_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::AstcLdr5x4_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::AstcLdr5x5_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::AstcLdr6x5_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::AstcLdr6x6_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::AstcLdr8x5_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::AstcLdr8x6_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::AstcLdr8x8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::AstcLdr10x5_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::AstcLdr10x6_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::AstcLdr10x8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::AstcLdr10x10_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::AstcLdr12x10_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::AstcLdr12x12_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_INVALID,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::IMG_DATA_FORMAT_4_4__GFX09,
      {
          { // IMG_NUM_FORMAT_UNORM / IMG_NUM_FORMAT_ASTC_2D_4x4__GFX09
               ChNumFormat::X4Y4_Unorm,
               ChNumFormat::L4A4_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SNORM / IMG_NUM_FORMAT_ASTC_2D_5x4__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_USCALED / IMG_NUM_FORMAT_ASTC_2D_5x5__GFX09
               ChNumFormat::X4Y4_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SSCALED / IMG_NUM_FORMAT_ASTC_2D_6x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_UINT / IMG_NUM_FORMAT_ASTC_2D_6x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SINT / IMG_NUM_FORMAT_ASTC_2D_8x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_8x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_FLOAT / IMG_NUM_FORMAT_ASTC_2D_8x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x5__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_SRGB__GFX09 / IMG_NUM_FORMAT_ASTC_2D_10x6__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x8__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_10x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x10__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // IMG_NUM_FORMAT_ASTC_2D_12x12__GFX09
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
};

constexpr uint32 Gfx9MergedImgDataFmtCount = sizeof(Gfx9MergedImgDataFmtTbl) / sizeof(MergedImgDataFmtInfo);

// Stores a MergedBufDataFmtInfo struct for each HW buffer format up to the last format known to the spreadsheet.
constexpr MergedBufDataFmtInfo Gfx9MergedBufDataFmtTbl[] =
{
    { Chip::BUF_DATA_FORMAT_INVALID,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_8,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X8_Unorm,
               ChNumFormat::A8_Unorm,
               ChNumFormat::L8_Unorm,
               ChNumFormat::P8_Unorm,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_16,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X16_Unorm,
               ChNumFormat::L16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_8_8,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X8Y8_Unorm,
               ChNumFormat::L8A8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X8Y8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X8Y8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X8Y8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X8Y8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X8Y8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X8Y8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_32,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_16_16,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X16Y16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X16Y16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X16Y16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X16Y16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X16Y16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X16Y16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X16Y16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_10_11_11,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X11Y11Z10_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_11_11_10,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X10Y11Z11_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_INVALID,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_2_10_10_10,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X10Y10Z10W2_Unorm,
               ChNumFormat::X10Y10Z10W2Bias_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X10Y10Z10W2_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X10Y10Z10W2_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X10Y10Z10W2_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X10Y10Z10W2_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X10Y10Z10W2_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_8_8_8_8,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X8Y8Z8W8_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X8Y8Z8W8_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X8Y8Z8W8_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X8Y8Z8W8_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X8Y8Z8W8_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X8Y8Z8W8_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X8Y8Z8W8_Srgb,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_32_32,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X32Y32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X32Y32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X32Y32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_16_16_16_16,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::X16Y16Z16W16_Unorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::X16Y16Z16W16_Snorm,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::X16Y16Z16W16_Uscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::X16Y16Z16W16_Sscaled,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X16Y16Z16W16_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X16Y16Z16W16_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X16Y16Z16W16_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_32_32_32,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X32Y32Z32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X32Y32Z32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X32Y32Z32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
    { Chip::BUF_DATA_FORMAT_32_32_32_32,
      {
          { // BUF_NUM_FORMAT_UNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SNORM
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_USCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SSCALED
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_UINT
               ChNumFormat::X32Y32Z32W32_Uint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_SINT
               ChNumFormat::X32Y32Z32W32_Sint,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // Unused
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
          { // BUF_NUM_FORMAT_FLOAT
               ChNumFormat::X32Y32Z32W32_Float,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
               ChNumFormat::Undefined,
          },
      },
    },
};

constexpr uint32 Gfx9MergedBufDataFmtCount = sizeof(Gfx9MergedBufDataFmtTbl) / sizeof(MergedBufDataFmtInfo);

// Lookup table for GPU access capabilities for each format/tiling-type pairing in Gfx10.
constexpr MergedFormatPropertiesTable Gfx10MergedFormatPropertiesTable =
{
    {
        // Note: Feature capabilities are listed in (linear, optimal) order.
        { None,                           None                           }, // ChNumFormat::Undefined
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X1_Unorm
        { None,                           None                           }, // ChNumFormat::X1_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X4Y4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::L4A4_Unorm
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X4Y4Z4W4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4Z4W4_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y6Z5_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y6Z5_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y5Z5W1_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y5Z5W1_Uscaled
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X1Y5Z5W5_Unorm
        { None,                           None                           }, // ChNumFormat::X1Y5Z5W5_Uscaled
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCSMX           }, // ChNumFormat::X8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::P8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8Y8_Srgb
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Sint
        { IrXsXdIflIfmmCBXP,              IrXsXdIflIfmmCBMXP             }, // ChNumFormat::X8Y8Z8W8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y11Z11_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X11Y11Z10_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y10Z10W2_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X10Y10Z10W2_Uint
        { IrXsIwIaIfmmTrTwTa,             IrXsIwIaIfmmTrTwTaM            }, // ChNumFormat::X10Y10Z10W2_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2Bias_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Float
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Float
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32_Float
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Uint
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Sint
        { IrXsIflIfmmTrTw,                None                           }, // ChNumFormat::X32Y32Z32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32Z32W32_Float
        { None,                           IrXsDSM                        }, // ChNumFormat::D16_Unorm_S8_Uint
        { None,                           IrXsDSM                        }, // ChNumFormat::D32_Float_S8_Uint
        { IrXsIaIflIfmm,                  IrXsIaIflIfmm                  }, // ChNumFormat::X9Y9Z9E5_Float
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Ufloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Sfloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11_Snorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Snorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Srgb
        { None,                           None                           }, // ChNumFormat::AstcHdr4x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x12_Float
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X8Y8_Z8Y8_Unorm
        { None,                           None                           }, // ChNumFormat::X8Y8_Z8Y8_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::Y8X8_Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Y8X8_Y8Z8_Uscaled
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::AYUV
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::UYVY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::VYUY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YUY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YVY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV11
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV21
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P016
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P010
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P210
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8Y8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8Y8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16_MM10_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X16_MM10_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16Y16_MM10_Unorm
        { IrXsIwXdIaIfmmCX,               IrXsIwXdIaIfmmCMX              }, // ChNumFormat::X16Y16_MM10_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P208
        { None,                           None                           }, // ChNumFormat::X16_MM12_Unorm
        { None,                           None                           }, // ChNumFormat::X16_MM12_Uint
        { None,                           None                           }, // ChNumFormat::X16Y16_MM12_Unorm
        { None,                           None                           }, // ChNumFormat::X16Y16_MM12_Uint
        { None,                           None                           }, // ChNumFormat::P012
        { None,                           None                           }, // ChNumFormat::P212
        { None,                           None                           }, // ChNumFormat::P412
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Float
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y216
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y210
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y416
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y410
    }
};

// Stores a ChannelFmtInfo struct for each available channel format for mapping PAL channel formats to the format values
// for various hardware blocks.
constexpr MergedFlatFmtInfo Gfx10MergedChannelFmtInfoTbl[] =
{
    // ChNumFormat::Undefined
    {
        ChNumFormat::Undefined,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Unorm
    {
        ChNumFormat::X1_Unorm,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_UNORM__GFX10CORE,  // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Uscaled
    {
        ChNumFormat::X1_Uscaled,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Unorm
    {
        ChNumFormat::X4Y4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Uscaled
    {
        ChNumFormat::X4Y4_Uscaled,         // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L4A4_Unorm
    {
        ChNumFormat::L4A4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Unorm
    {
        ChNumFormat::X4Y4Z4W4_Unorm,       // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Uscaled
    {
        ChNumFormat::X4Y4Z4W4_Uscaled,     // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Unorm
    {
        ChNumFormat::X5Y6Z5_Unorm,         // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_6_5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Uscaled
    {
        ChNumFormat::X5Y6Z5_Uscaled,       // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Unorm
    {
        ChNumFormat::X5Y5Z5W1_Unorm,       // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_5_5_5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Uscaled
    {
        ChNumFormat::X5Y5Z5W1_Uscaled,     // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Unorm
    {
        ChNumFormat::X1Y5Z5W5_Unorm,       // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_5_5_1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Uscaled
    {
        ChNumFormat::X1Y5Z5W5_Uscaled,     // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Unorm
    {
        ChNumFormat::X8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Snorm
    {
        ChNumFormat::X8_Snorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_SNORM,             // Image Channel Format
        Chip::BUF_FMT_8_SNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uscaled
    {
        ChNumFormat::X8_Uscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_USCALED,           // Image Channel Format
        Chip::BUF_FMT_8_USCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Sscaled
    {
        ChNumFormat::X8_Sscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_SSCALED,           // Image Channel Format
        Chip::BUF_FMT_8_SSCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uint
    {
        ChNumFormat::X8_Uint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_UINT,              // Image Channel Format
        Chip::BUF_FMT_8_UINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X8_Sint
    {
        ChNumFormat::X8_Sint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_SINT,              // Image Channel Format
        Chip::BUF_FMT_8_SINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Srgb
    {
        ChNumFormat::X8_Srgb,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_SRGB__GFX10CORE,   // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::A8_Unorm
    {
        ChNumFormat::A8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8_Unorm
    {
        ChNumFormat::L8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P8_Unorm
    {
        ChNumFormat::P8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Unorm
    {
        ChNumFormat::X8Y8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Snorm
    {
        ChNumFormat::X8Y8_Snorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_SNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_SNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Uscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_USCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_USCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sscaled
    {
        ChNumFormat::X8Y8_Sscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_SSCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_SSCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uint
    {
        ChNumFormat::X8Y8_Uint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_UINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_UINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sint
    {
        ChNumFormat::X8Y8_Sint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_SINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Srgb
    {
        ChNumFormat::X8Y8_Srgb,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8A8_Unorm
    {
        ChNumFormat::L8A8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Unorm
    {
        ChNumFormat::X8Y8Z8W8_Unorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Snorm
    {
        ChNumFormat::X8Y8Z8W8_Snorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uscaled
    {
        ChNumFormat::X8Y8Z8W8_Uscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sscaled
    {
        ChNumFormat::X8Y8Z8W8_Sscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uint
    {
        ChNumFormat::X8Y8Z8W8_Uint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sint
    {
        ChNumFormat::X8Y8Z8W8_Sint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Srgb
    {
        ChNumFormat::X8Y8Z8W8_Srgb,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U8V8_Snorm_L8W8_Unorm
    {
        ChNumFormat::U8V8_Snorm_L8W8_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y11Z11_Float
    {
        ChNumFormat::X10Y11Z11_Float,      // ChNumFormat
        Chip::COLOR_11_11_10,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_11_11_10_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_11_11_10_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X11Y11Z10_Float
    {
        ChNumFormat::X11Y11Z10_Float,      // ChNumFormat
        Chip::COLOR_10_11_11,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_10_11_11_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_10_11_11_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Unorm
    {
        ChNumFormat::X10Y10Z10W2_Unorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Snorm
    {
        ChNumFormat::X10Y10Z10W2_Snorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uscaled
    {
        ChNumFormat::X10Y10Z10W2_Uscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sscaled
    {
        ChNumFormat::X10Y10Z10W2_Sscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uint
    {
        ChNumFormat::X10Y10Z10W2_Uint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sint
    {
        ChNumFormat::X10Y10Z10W2_Sint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2Bias_Unorm
    {
        ChNumFormat::X10Y10Z10W2Bias_Unorm, // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U10V10W10_Snorm_A2_Unorm
    {
        ChNumFormat::U10V10W10_Snorm_A2_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Unorm
    {
        ChNumFormat::X16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Snorm
    {
        ChNumFormat::X16_Snorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_SNORM,            // Image Channel Format
        Chip::BUF_FMT_16_SNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uscaled
    {
        ChNumFormat::X16_Uscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_USCALED,          // Image Channel Format
        Chip::BUF_FMT_16_USCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sscaled
    {
        ChNumFormat::X16_Sscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_SSCALED,          // Image Channel Format
        Chip::BUF_FMT_16_SSCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uint
    {
        ChNumFormat::X16_Uint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_UINT,             // Image Channel Format
        Chip::BUF_FMT_16_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sint
    {
        ChNumFormat::X16_Sint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_SINT,             // Image Channel Format
        Chip::BUF_FMT_16_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Float
    {
        ChNumFormat::X16_Float,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_16_FLOAT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L16_Unorm
    {
        ChNumFormat::L16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Unorm
    {
        ChNumFormat::X16Y16_Unorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_UNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_UNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Snorm
    {
        ChNumFormat::X16Y16_Snorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_SNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_SNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uscaled
    {
        ChNumFormat::X16Y16_Uscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_USCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_USCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sscaled
    {
        ChNumFormat::X16Y16_Sscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_SSCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_SSCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uint
    {
        ChNumFormat::X16Y16_Uint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_UINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_UINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sint
    {
        ChNumFormat::X16Y16_Sint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_SINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_SINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Float
    {
        ChNumFormat::X16Y16_Float,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_FLOAT,         // Image Channel Format
        Chip::BUF_FMT_16_16_FLOAT,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Unorm
    {
        ChNumFormat::X16Y16Z16W16_Unorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Snorm
    {
        ChNumFormat::X16Y16Z16W16_Snorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uscaled
    {
        ChNumFormat::X16Y16Z16W16_Uscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sscaled
    {
        ChNumFormat::X16Y16Z16W16_Sscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uint
    {
        ChNumFormat::X16Y16Z16W16_Uint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sint
    {
        ChNumFormat::X16Y16Z16W16_Sint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Float
    {
        ChNumFormat::X16Y16Z16W16_Float,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Uint
    {
        ChNumFormat::X32_Uint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_UINT,             // Image Channel Format
        Chip::BUF_FMT_32_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Sint
    {
        ChNumFormat::X32_Sint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_SINT,             // Image Channel Format
        Chip::BUF_FMT_32_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Float
    {
        ChNumFormat::X32_Float,            // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_32_FLOAT,            // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Uint
    {
        ChNumFormat::X32Y32_Uint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Sint
    {
        ChNumFormat::X32Y32_Sint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Float
    {
        ChNumFormat::X32Y32_Float,         // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Uint
    {
        ChNumFormat::X32Y32Z32_Uint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Sint
    {
        ChNumFormat::X32Y32Z32_Sint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Float
    {
        ChNumFormat::X32Y32Z32_Float,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Uint
    {
        ChNumFormat::X32Y32Z32W32_Uint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Sint
    {
        ChNumFormat::X32Y32Z32W32_Sint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Float
    {
        ChNumFormat::X32Y32Z32W32_Float,   // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::D16_Unorm_S8_Uint
    {
        ChNumFormat::D16_Unorm_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::D32_Float_S8_Uint
    {
        ChNumFormat::D32_Float_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X9Y9Z9E5_Float
    {
        ChNumFormat::X9Y9Z9E5_Float,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_5_9_9_9_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Unorm
    {
        ChNumFormat::Bc1_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Srgb
    {
        ChNumFormat::Bc1_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC1_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Unorm
    {
        ChNumFormat::Bc2_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC2_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Srgb
    {
        ChNumFormat::Bc2_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC2_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Unorm
    {
        ChNumFormat::Bc3_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC3_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Srgb
    {
        ChNumFormat::Bc3_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC3_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Unorm
    {
        ChNumFormat::Bc4_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Snorm
    {
        ChNumFormat::Bc4_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Unorm
    {
        ChNumFormat::Bc5_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Snorm
    {
        ChNumFormat::Bc5_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Ufloat
    {
        ChNumFormat::Bc6_Ufloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_UFLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Sfloat
    {
        ChNumFormat::Bc6_Sfloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_SFLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Unorm
    {
        ChNumFormat::Bc7_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC7_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Srgb
    {
        ChNumFormat::Bc7_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC7_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGB_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGB_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA1_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Unorm
    {
        ChNumFormat::Etc2X11_Unorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_R_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Snorm
    {
        ChNumFormat::Etc2X11_Snorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_R_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Unorm
    {
        ChNumFormat::Etc2X11Y11_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RG_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Snorm
    {
        ChNumFormat::Etc2X11Y11_Snorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RG_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Unorm
    {
        ChNumFormat::AstcLdr4x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Srgb
    {
        ChNumFormat::AstcLdr4x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Unorm
    {
        ChNumFormat::AstcLdr5x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Srgb
    {
        ChNumFormat::AstcLdr5x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Unorm
    {
        ChNumFormat::AstcLdr5x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Srgb
    {
        ChNumFormat::AstcLdr5x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Unorm
    {
        ChNumFormat::AstcLdr6x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Srgb
    {
        ChNumFormat::AstcLdr6x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Unorm
    {
        ChNumFormat::AstcLdr6x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Srgb
    {
        ChNumFormat::AstcLdr6x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Unorm
    {
        ChNumFormat::AstcLdr8x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Srgb
    {
        ChNumFormat::AstcLdr8x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Unorm
    {
        ChNumFormat::AstcLdr8x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Srgb
    {
        ChNumFormat::AstcLdr8x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Unorm
    {
        ChNumFormat::AstcLdr8x8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Srgb
    {
        ChNumFormat::AstcLdr8x8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Unorm
    {
        ChNumFormat::AstcLdr10x5_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Srgb
    {
        ChNumFormat::AstcLdr10x5_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Unorm
    {
        ChNumFormat::AstcLdr10x6_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Srgb
    {
        ChNumFormat::AstcLdr10x6_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Unorm
    {
        ChNumFormat::AstcLdr10x8_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Srgb
    {
        ChNumFormat::AstcLdr10x8_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Unorm
    {
        ChNumFormat::AstcLdr10x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Srgb
    {
        ChNumFormat::AstcLdr10x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Unorm
    {
        ChNumFormat::AstcLdr12x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Srgb
    {
        ChNumFormat::AstcLdr12x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Unorm
    {
        ChNumFormat::AstcLdr12x12_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Srgb
    {
        ChNumFormat::AstcLdr12x12_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr4x4_Float
    {
        ChNumFormat::AstcHdr4x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x4_Float
    {
        ChNumFormat::AstcHdr5x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x5_Float
    {
        ChNumFormat::AstcHdr5x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x5_Float
    {
        ChNumFormat::AstcHdr6x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x6_Float
    {
        ChNumFormat::AstcHdr6x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x5_Float
    {
        ChNumFormat::AstcHdr8x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x6_Float
    {
        ChNumFormat::AstcHdr8x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x8_Float
    {
        ChNumFormat::AstcHdr8x8_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x5_Float
    {
        ChNumFormat::AstcHdr10x5_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x6_Float
    {
        ChNumFormat::AstcHdr10x6_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x8_Float
    {
        ChNumFormat::AstcHdr10x8_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x10_Float
    {
        ChNumFormat::AstcHdr10x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x10_Float
    {
        ChNumFormat::AstcHdr12x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x12_Float
    {
        ChNumFormat::AstcHdr12x12_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Unorm
    {
        ChNumFormat::X8Y8_Z8Y8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_GB_GR_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Z8Y8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Unorm
    {
        ChNumFormat::Y8X8_Y8Z8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BG_RG_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Uscaled
    {
        ChNumFormat::Y8X8_Y8Z8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AYUV
    {
        ChNumFormat::AYUV,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::UYVY
    {
        ChNumFormat::UYVY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::VYUY
    {
        ChNumFormat::VYUY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YUY2
    {
        ChNumFormat::YUY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YVY2
    {
        ChNumFormat::YVY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YV12
    {
        ChNumFormat::YV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV11
    {
        ChNumFormat::NV11,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV12
    {
        ChNumFormat::NV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV21
    {
        ChNumFormat::NV21,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P016
    {
        ChNumFormat::P016,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P010
    {
        ChNumFormat::P010,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P210
    {
        ChNumFormat::P210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Unorm
    {
        ChNumFormat::X8_MM_Unorm,          // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Uint
    {
        ChNumFormat::X8_MM_Uint,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Unorm
    {
        ChNumFormat::X8Y8_MM_Unorm,        // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Uint
    {
        ChNumFormat::X8Y8_MM_Uint,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Unorm
    {
        ChNumFormat::X16_MM10_Unorm,       // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Uint
    {
        ChNumFormat::X16_MM10_Uint,        // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Unorm
    {
        ChNumFormat::X16Y16_MM10_Unorm,    // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Uint
    {
        ChNumFormat::X16Y16_MM10_Uint,     // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P208
    {
        ChNumFormat::P208,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Unorm
    {
        ChNumFormat::X16_MM12_Unorm,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Uint
    {
        ChNumFormat::X16_MM12_Uint,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Unorm
    {
        ChNumFormat::X16Y16_MM12_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Uint
    {
        ChNumFormat::X16Y16_MM12_Uint,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P012
    {
        ChNumFormat::P012,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P212
    {
        ChNumFormat::P212,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P412
    {
        ChNumFormat::P412,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Float
    {
        ChNumFormat::X10Y10Z10W2_Float,    // ChNumFormat
        Chip::COLOR_2_10_10_10_6E4,        // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y216
    {
        ChNumFormat::Y216,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y210
    {
        ChNumFormat::Y210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y416
    {
        ChNumFormat::Y416,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y410
    {
        ChNumFormat::Y410,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
};

// Stores a ChNumFormat struct for each HW image format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx10MergedImgDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_Unorm,              // IMG_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // IMG_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // IMG_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // IMG_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // IMG_FMT_8_UINT
    ChNumFormat::X8_Sint,               // IMG_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // IMG_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // IMG_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // IMG_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // IMG_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // IMG_FMT_16_UINT
    ChNumFormat::X16_Sint,              // IMG_FMT_16_SINT
    ChNumFormat::X16_Float,             // IMG_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // IMG_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // IMG_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // IMG_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // IMG_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // IMG_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // IMG_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // IMG_FMT_32_UINT
    ChNumFormat::X32_Sint,              // IMG_FMT_32_SINT
    ChNumFormat::X32_Float,             // IMG_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // IMG_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // IMG_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // IMG_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // IMG_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // IMG_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // IMG_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // IMG_FMT_16_16_FLOAT
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X11Y11Z10_Float,       // IMG_FMT_10_11_11_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X10Y11Z11_Float,       // IMG_FMT_11_11_10_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // IMG_FMT_2_10_10_10_UNORM__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Snorm,     // IMG_FMT_2_10_10_10_SNORM__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uscaled,   // IMG_FMT_2_10_10_10_USCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sscaled,   // IMG_FMT_2_10_10_10_SSCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uint,      // IMG_FMT_2_10_10_10_UINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sint,      // IMG_FMT_2_10_10_10_SINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Unorm,        // IMG_FMT_8_8_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Snorm,        // IMG_FMT_8_8_8_8_SNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uscaled,      // IMG_FMT_8_8_8_8_USCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sscaled,      // IMG_FMT_8_8_8_8_SSCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uint,         // IMG_FMT_8_8_8_8_UINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sint,         // IMG_FMT_8_8_8_8_SINT__GFX10CORE
    ChNumFormat::X32Y32_Uint,           // IMG_FMT_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32_Sint,           // IMG_FMT_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32_Float,          // IMG_FMT_32_32_FLOAT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Unorm,    // IMG_FMT_16_16_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Snorm,    // IMG_FMT_16_16_16_16_SNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uscaled,  // IMG_FMT_16_16_16_16_USCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sscaled,  // IMG_FMT_16_16_16_16_SSCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uint,     // IMG_FMT_16_16_16_16_UINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sint,     // IMG_FMT_16_16_16_16_SINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Float,    // IMG_FMT_16_16_16_16_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32_Uint,        // IMG_FMT_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Sint,        // IMG_FMT_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Float,       // IMG_FMT_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Uint,     // IMG_FMT_32_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Sint,     // IMG_FMT_32_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Float,    // IMG_FMT_32_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_Srgb,               // IMG_FMT_8_SRGB__GFX10CORE
    ChNumFormat::X8Y8_Srgb,             // IMG_FMT_8_8_SRGB__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Srgb,         // IMG_FMT_8_8_8_8_SRGB__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X9Y9Z9E5_Float,        // IMG_FMT_5_9_9_9_FLOAT__GFX10CORE
    ChNumFormat::X5Y6Z5_Unorm,          // IMG_FMT_5_6_5_UNORM__GFX10CORE
    ChNumFormat::X5Y5Z5W1_Unorm,        // IMG_FMT_1_5_5_5_UNORM__GFX10CORE
    ChNumFormat::X1Y5Z5W5_Unorm,        // IMG_FMT_5_5_5_1_UNORM__GFX10CORE
    ChNumFormat::X4Y4Z4W4_Unorm,        // IMG_FMT_4_4_4_4_UNORM__GFX10CORE
    ChNumFormat::X4Y4_Unorm,            // IMG_FMT_4_4_UNORM__GFX10CORE
    ChNumFormat::X1_Unorm,              // IMG_FMT_1_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8Y8_Z8Y8_Unorm,       // IMG_FMT_GB_GR_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Y8X8_Y8Z8_Unorm,       // IMG_FMT_BG_RG_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Bc1_Unorm,             // IMG_FMT_BC1_UNORM__GFX10CORE
    ChNumFormat::Bc1_Srgb,              // IMG_FMT_BC1_SRGB__GFX10CORE
    ChNumFormat::Bc2_Unorm,             // IMG_FMT_BC2_UNORM__GFX10CORE
    ChNumFormat::Bc2_Srgb,              // IMG_FMT_BC2_SRGB__GFX10CORE
    ChNumFormat::Bc3_Unorm,             // IMG_FMT_BC3_UNORM__GFX10CORE
    ChNumFormat::Bc3_Srgb,              // IMG_FMT_BC3_SRGB__GFX10CORE
    ChNumFormat::Bc4_Unorm,             // IMG_FMT_BC4_UNORM__GFX10CORE
    ChNumFormat::Bc4_Snorm,             // IMG_FMT_BC4_SNORM__GFX10CORE
    ChNumFormat::Bc5_Unorm,             // IMG_FMT_BC5_UNORM__GFX10CORE
    ChNumFormat::Bc5_Snorm,             // IMG_FMT_BC5_SNORM__GFX10CORE
    ChNumFormat::Bc6_Ufloat,            // IMG_FMT_BC6_UFLOAT__GFX10CORE
    ChNumFormat::Bc6_Sfloat,            // IMG_FMT_BC6_SFLOAT__GFX10CORE
    ChNumFormat::Bc7_Unorm,             // IMG_FMT_BC7_UNORM__GFX10CORE
    ChNumFormat::Bc7_Srgb,              // IMG_FMT_BC7_SRGB__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8_Unorm,      // IMG_FMT_ETC2_RGB_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8_Srgb,       // IMG_FMT_ETC2_RGB_SRGB__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W8_Unorm,    // IMG_FMT_ETC2_RGBA_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W8_Srgb,     // IMG_FMT_ETC2_RGBA_SRGB__GFX10CORE
    ChNumFormat::Etc2X11_Unorm,         // IMG_FMT_ETC2_R_UNORM__GFX10CORE
    ChNumFormat::Etc2X11_Snorm,         // IMG_FMT_ETC2_R_SNORM__GFX10CORE
    ChNumFormat::Etc2X11Y11_Unorm,      // IMG_FMT_ETC2_RG_UNORM__GFX10CORE
    ChNumFormat::Etc2X11Y11_Snorm,      // IMG_FMT_ETC2_RG_SNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W1_Unorm,    // IMG_FMT_ETC2_RGBA1_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W1_Srgb,     // IMG_FMT_ETC2_RGBA1_SRGB__GFX10CORE
    ChNumFormat::AstcLdr4x4_Unorm,      // IMG_FMT_ASTC_2D_LDR_4X4__GFX10CORE
    ChNumFormat::AstcLdr5x4_Unorm,      // IMG_FMT_ASTC_2D_LDR_5X4__GFX10CORE
    ChNumFormat::AstcLdr5x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_5X5__GFX10CORE
    ChNumFormat::AstcLdr6x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_6X5__GFX10CORE
    ChNumFormat::AstcLdr6x6_Unorm,      // IMG_FMT_ASTC_2D_LDR_6X6__GFX10CORE
    ChNumFormat::AstcLdr8x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X5__GFX10CORE
    ChNumFormat::AstcLdr8x6_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X6__GFX10CORE
    ChNumFormat::AstcLdr8x8_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X8__GFX10CORE
    ChNumFormat::AstcLdr10x5_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X5__GFX10CORE
    ChNumFormat::AstcLdr10x6_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X6__GFX10CORE
    ChNumFormat::AstcLdr10x8_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X8__GFX10CORE
    ChNumFormat::AstcLdr10x10_Unorm,    // IMG_FMT_ASTC_2D_LDR_10X10__GFX10CORE
    ChNumFormat::AstcLdr12x10_Unorm,    // IMG_FMT_ASTC_2D_LDR_12X10__GFX10CORE
    ChNumFormat::AstcLdr12x12_Unorm,    // IMG_FMT_ASTC_2D_LDR_12X12__GFX10CORE
    ChNumFormat::AstcHdr4x4_Float,      // IMG_FMT_ASTC_2D_HDR_4X4__GFX10CORE
    ChNumFormat::AstcHdr5x4_Float,      // IMG_FMT_ASTC_2D_HDR_5X4__GFX10CORE
    ChNumFormat::AstcHdr5x5_Float,      // IMG_FMT_ASTC_2D_HDR_5X5__GFX10CORE
    ChNumFormat::AstcHdr6x5_Float,      // IMG_FMT_ASTC_2D_HDR_6X5__GFX10CORE
    ChNumFormat::AstcHdr6x6_Float,      // IMG_FMT_ASTC_2D_HDR_6X6__GFX10CORE
    ChNumFormat::AstcHdr8x5_Float,      // IMG_FMT_ASTC_2D_HDR_8X5__GFX10CORE
    ChNumFormat::AstcHdr8x6_Float,      // IMG_FMT_ASTC_2D_HDR_8X6__GFX10CORE
    ChNumFormat::AstcHdr8x8_Float,      // IMG_FMT_ASTC_2D_HDR_8X8__GFX10CORE
    ChNumFormat::AstcHdr10x5_Float,     // IMG_FMT_ASTC_2D_HDR_10X5__GFX10CORE
    ChNumFormat::AstcHdr10x6_Float,     // IMG_FMT_ASTC_2D_HDR_10X6__GFX10CORE
    ChNumFormat::AstcHdr10x8_Float,     // IMG_FMT_ASTC_2D_HDR_10X8__GFX10CORE
    ChNumFormat::AstcHdr10x10_Float,    // IMG_FMT_ASTC_2D_HDR_10X10__GFX10CORE
    ChNumFormat::AstcHdr12x10_Float,    // IMG_FMT_ASTC_2D_HDR_12X10__GFX10CORE
    ChNumFormat::AstcHdr12x12_Float,    // IMG_FMT_ASTC_2D_HDR_12X12__GFX10CORE
    ChNumFormat::AstcLdr4x4_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_4X4__GFX10CORE
    ChNumFormat::AstcLdr5x4_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_5X4__GFX10CORE
    ChNumFormat::AstcLdr5x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_5X5__GFX10CORE
    ChNumFormat::AstcLdr6x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_6X5__GFX10CORE
    ChNumFormat::AstcLdr6x6_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_6X6__GFX10CORE
    ChNumFormat::AstcLdr8x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X5__GFX10CORE
    ChNumFormat::AstcLdr8x6_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X6__GFX10CORE
    ChNumFormat::AstcLdr8x8_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X8__GFX10CORE
    ChNumFormat::AstcLdr10x5_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X5__GFX10CORE
    ChNumFormat::AstcLdr10x6_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X6__GFX10CORE
    ChNumFormat::AstcLdr10x8_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X8__GFX10CORE
    ChNumFormat::AstcLdr10x10_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_10X10__GFX10CORE
    ChNumFormat::AstcLdr12x10_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_12X10__GFX10CORE
    ChNumFormat::AstcLdr12x12_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_12X12__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_MM_Unorm,           // IMG_FMT_MM_8_UNORM__GFX10CORE
    ChNumFormat::X8_MM_Uint,            // IMG_FMT_MM_8_UINT__GFX10CORE
    ChNumFormat::X8Y8_MM_Unorm,         // IMG_FMT_MM_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8_MM_Uint,          // IMG_FMT_MM_8_8_UINT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X16_MM10_Unorm,        // IMG_FMT_MM_10_IN_16_UNORM__GFX10CORE
    ChNumFormat::X16_MM10_Uint,         // IMG_FMT_MM_10_IN_16_UINT__GFX10CORE
    ChNumFormat::X16Y16_MM10_Unorm,     // IMG_FMT_MM_10_IN_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16_MM10_Uint,      // IMG_FMT_MM_10_IN_16_16_UINT__GFX10CORE
};

constexpr uint32 Gfx10MergedImgDataFmtCount = sizeof(Gfx10MergedImgDataFmtTbl) / sizeof(ChNumFormat);

// Stores a ChNumFormat struct for each HW buffer format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx10MergedBufDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X8_Unorm,              // BUF_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // BUF_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // BUF_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // BUF_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // BUF_FMT_8_UINT
    ChNumFormat::X8_Sint,               // BUF_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // BUF_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // BUF_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // BUF_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // BUF_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // BUF_FMT_16_UINT
    ChNumFormat::X16_Sint,              // BUF_FMT_16_SINT
    ChNumFormat::X16_Float,             // BUF_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // BUF_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // BUF_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // BUF_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // BUF_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // BUF_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // BUF_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // BUF_FMT_32_UINT
    ChNumFormat::X32_Sint,              // BUF_FMT_32_SINT
    ChNumFormat::X32_Float,             // BUF_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // BUF_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // BUF_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // BUF_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // BUF_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // BUF_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // BUF_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // BUF_FMT_16_16_FLOAT
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X11Y11Z10_Float,       // BUF_FMT_10_11_11_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X10Y11Z11_Float,       // BUF_FMT_11_11_10_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // BUF_FMT_2_10_10_10_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sint,         // BUF_FMT_8_8_8_8_SINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uscaled,   // BUF_FMT_2_10_10_10_USCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sscaled,   // BUF_FMT_2_10_10_10_SSCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uint,      // BUF_FMT_2_10_10_10_UINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sint,      // BUF_FMT_2_10_10_10_SINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Unorm,        // BUF_FMT_8_8_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Snorm,        // BUF_FMT_8_8_8_8_SNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uscaled,      // BUF_FMT_8_8_8_8_USCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sscaled,      // BUF_FMT_8_8_8_8_SSCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uint,         // BUF_FMT_8_8_8_8_UINT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X32Y32_Uint,           // BUF_FMT_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32_Sint,           // BUF_FMT_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32_Float,          // BUF_FMT_32_32_FLOAT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Unorm,    // BUF_FMT_16_16_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Snorm,    // BUF_FMT_16_16_16_16_SNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uscaled,  // BUF_FMT_16_16_16_16_USCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sscaled,  // BUF_FMT_16_16_16_16_SSCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uint,     // BUF_FMT_16_16_16_16_UINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sint,     // BUF_FMT_16_16_16_16_SINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Float,    // BUF_FMT_16_16_16_16_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32_Uint,        // BUF_FMT_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Sint,        // BUF_FMT_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Float,       // BUF_FMT_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Uint,     // BUF_FMT_32_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Sint,     // BUF_FMT_32_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Float,    // BUF_FMT_32_32_32_32_FLOAT__GFX10CORE
};

constexpr uint32 Gfx10MergedBufDataFmtCount = sizeof(Gfx10MergedBufDataFmtTbl) / sizeof(ChNumFormat);

// Lookup table for GPU access capabilities for each format/tiling-type pairing in Gfx10_3.
constexpr MergedFormatPropertiesTable Gfx10_3MergedFormatPropertiesTable =
{
    {
        // Note: Feature capabilities are listed in (linear, optimal) order.
        { None,                           None                           }, // ChNumFormat::Undefined
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X1_Unorm
        { None,                           None                           }, // ChNumFormat::X1_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X4Y4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::L4A4_Unorm
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X4Y4Z4W4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4Z4W4_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y6Z5_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y6Z5_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y5Z5W1_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y5Z5W1_Uscaled
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X1Y5Z5W5_Unorm
        { None,                           None                           }, // ChNumFormat::X1Y5Z5W5_Uscaled
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCSMX           }, // ChNumFormat::X8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::P8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8Y8_Srgb
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Sint
        { IrXsXdIflIfmmCBXP,              IrXsXdIflIfmmCBMXP             }, // ChNumFormat::X8Y8Z8W8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y11Z11_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X11Y11Z10_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y10Z10W2_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X10Y10Z10W2_Uint
        { IrXsIwIaIfmmTrTwTa,             IrXsIwIaIfmmTrTwTaM            }, // ChNumFormat::X10Y10Z10W2_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2Bias_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Float
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Float
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32_Float
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Uint
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Sint
        { IrXsIflIfmmTrTw,                None                           }, // ChNumFormat::X32Y32Z32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32Z32W32_Float
        { None,                           IrXsDSM                        }, // ChNumFormat::D16_Unorm_S8_Uint
        { None,                           IrXsDSM                        }, // ChNumFormat::D32_Float_S8_Uint
        { IrXsIwXdIaIflIfmmCBX,           IrXsIwXdIaIflIfmmCBMX          }, // ChNumFormat::X9Y9Z9E5_Float
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Ufloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Sfloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11_Snorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Snorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Srgb
        { None,                           None                           }, // ChNumFormat::AstcHdr4x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x12_Float
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X8Y8_Z8Y8_Unorm
        { None,                           None                           }, // ChNumFormat::X8Y8_Z8Y8_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::Y8X8_Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Y8X8_Y8Z8_Uscaled
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::AYUV
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::UYVY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::VYUY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YUY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YVY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV11
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV21
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P016
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P010
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P210
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8Y8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8Y8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16_MM10_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X16_MM10_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16Y16_MM10_Unorm
        { IrXsIwXdIaIfmmCX,               IrXsIwXdIaIfmmCMX              }, // ChNumFormat::X16Y16_MM10_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P208
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16_MM12_Unorm
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16_MM12_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16Y16_MM12_Unorm
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16Y16_MM12_Uint
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P012
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P212
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P412
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Float
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y216
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y210
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y416
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y410
    }
};

// Stores a ChannelFmtInfo struct for each available channel format for mapping PAL channel formats to the format values
// for various hardware blocks.
constexpr MergedFlatFmtInfo Gfx10_3MergedChannelFmtInfoTbl[] =
{
    // ChNumFormat::Undefined
    {
        ChNumFormat::Undefined,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Unorm
    {
        ChNumFormat::X1_Unorm,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_UNORM__GFX10CORE,  // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Uscaled
    {
        ChNumFormat::X1_Uscaled,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Unorm
    {
        ChNumFormat::X4Y4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Uscaled
    {
        ChNumFormat::X4Y4_Uscaled,         // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L4A4_Unorm
    {
        ChNumFormat::L4A4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Unorm
    {
        ChNumFormat::X4Y4Z4W4_Unorm,       // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_4_4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Uscaled
    {
        ChNumFormat::X4Y4Z4W4_Uscaled,     // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Unorm
    {
        ChNumFormat::X5Y6Z5_Unorm,         // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_6_5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Uscaled
    {
        ChNumFormat::X5Y6Z5_Uscaled,       // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Unorm
    {
        ChNumFormat::X5Y5Z5W1_Unorm,       // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_5_5_5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Uscaled
    {
        ChNumFormat::X5Y5Z5W1_Uscaled,     // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Unorm
    {
        ChNumFormat::X1Y5Z5W5_Unorm,       // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_5_5_1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Uscaled
    {
        ChNumFormat::X1Y5Z5W5_Uscaled,     // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Unorm
    {
        ChNumFormat::X8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Snorm
    {
        ChNumFormat::X8_Snorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_SNORM,             // Image Channel Format
        Chip::BUF_FMT_8_SNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uscaled
    {
        ChNumFormat::X8_Uscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_USCALED,           // Image Channel Format
        Chip::BUF_FMT_8_USCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Sscaled
    {
        ChNumFormat::X8_Sscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_SSCALED,           // Image Channel Format
        Chip::BUF_FMT_8_SSCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uint
    {
        ChNumFormat::X8_Uint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_UINT,              // Image Channel Format
        Chip::BUF_FMT_8_UINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X8_Sint
    {
        ChNumFormat::X8_Sint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_SINT,              // Image Channel Format
        Chip::BUF_FMT_8_SINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Srgb
    {
        ChNumFormat::X8_Srgb,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_SRGB__GFX10CORE,   // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::A8_Unorm
    {
        ChNumFormat::A8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8_Unorm
    {
        ChNumFormat::L8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P8_Unorm
    {
        ChNumFormat::P8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Unorm
    {
        ChNumFormat::X8Y8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Snorm
    {
        ChNumFormat::X8Y8_Snorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_SNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_SNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Uscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_USCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_USCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sscaled
    {
        ChNumFormat::X8Y8_Sscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_SSCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_SSCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uint
    {
        ChNumFormat::X8Y8_Uint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_UINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_UINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sint
    {
        ChNumFormat::X8Y8_Sint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_SINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Srgb
    {
        ChNumFormat::X8Y8_Srgb,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8A8_Unorm
    {
        ChNumFormat::L8A8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Unorm
    {
        ChNumFormat::X8Y8Z8W8_Unorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Snorm
    {
        ChNumFormat::X8Y8Z8W8_Snorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uscaled
    {
        ChNumFormat::X8Y8Z8W8_Uscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sscaled
    {
        ChNumFormat::X8Y8Z8W8_Sscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uint
    {
        ChNumFormat::X8Y8Z8W8_Uint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sint
    {
        ChNumFormat::X8Y8Z8W8_Sint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Srgb
    {
        ChNumFormat::X8Y8Z8W8_Srgb,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U8V8_Snorm_L8W8_Unorm
    {
        ChNumFormat::U8V8_Snorm_L8W8_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y11Z11_Float
    {
        ChNumFormat::X10Y11Z11_Float,      // ChNumFormat
        Chip::COLOR_11_11_10,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_11_11_10_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_11_11_10_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X11Y11Z10_Float
    {
        ChNumFormat::X11Y11Z10_Float,      // ChNumFormat
        Chip::COLOR_10_11_11,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_10_11_11_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_10_11_11_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Unorm
    {
        ChNumFormat::X10Y10Z10W2_Unorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Snorm
    {
        ChNumFormat::X10Y10Z10W2_Snorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uscaled
    {
        ChNumFormat::X10Y10Z10W2_Uscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sscaled
    {
        ChNumFormat::X10Y10Z10W2_Sscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uint
    {
        ChNumFormat::X10Y10Z10W2_Uint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sint
    {
        ChNumFormat::X10Y10Z10W2_Sint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2Bias_Unorm
    {
        ChNumFormat::X10Y10Z10W2Bias_Unorm, // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U10V10W10_Snorm_A2_Unorm
    {
        ChNumFormat::U10V10W10_Snorm_A2_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Unorm
    {
        ChNumFormat::X16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Snorm
    {
        ChNumFormat::X16_Snorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_SNORM,            // Image Channel Format
        Chip::BUF_FMT_16_SNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uscaled
    {
        ChNumFormat::X16_Uscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_USCALED,          // Image Channel Format
        Chip::BUF_FMT_16_USCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sscaled
    {
        ChNumFormat::X16_Sscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_SSCALED,          // Image Channel Format
        Chip::BUF_FMT_16_SSCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uint
    {
        ChNumFormat::X16_Uint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_UINT,             // Image Channel Format
        Chip::BUF_FMT_16_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sint
    {
        ChNumFormat::X16_Sint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_SINT,             // Image Channel Format
        Chip::BUF_FMT_16_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Float
    {
        ChNumFormat::X16_Float,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_16_FLOAT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L16_Unorm
    {
        ChNumFormat::L16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Unorm
    {
        ChNumFormat::X16Y16_Unorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_UNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_UNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Snorm
    {
        ChNumFormat::X16Y16_Snorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_SNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_SNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uscaled
    {
        ChNumFormat::X16Y16_Uscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_USCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_USCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sscaled
    {
        ChNumFormat::X16Y16_Sscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_SSCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_SSCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uint
    {
        ChNumFormat::X16Y16_Uint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_UINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_UINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sint
    {
        ChNumFormat::X16Y16_Sint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_SINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_SINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Float
    {
        ChNumFormat::X16Y16_Float,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_FLOAT,         // Image Channel Format
        Chip::BUF_FMT_16_16_FLOAT,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Unorm
    {
        ChNumFormat::X16Y16Z16W16_Unorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Snorm
    {
        ChNumFormat::X16Y16Z16W16_Snorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SNORM__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uscaled
    {
        ChNumFormat::X16Y16Z16W16_Uscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_USCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_USCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sscaled
    {
        ChNumFormat::X16Y16Z16W16_Sscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SSCALED__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SSCALED__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uint
    {
        ChNumFormat::X16Y16Z16W16_Uint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sint
    {
        ChNumFormat::X16Y16Z16W16_Sint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Float
    {
        ChNumFormat::X16Y16Z16W16_Float,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Uint
    {
        ChNumFormat::X32_Uint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_UINT,             // Image Channel Format
        Chip::BUF_FMT_32_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Sint
    {
        ChNumFormat::X32_Sint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_SINT,             // Image Channel Format
        Chip::BUF_FMT_32_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Float
    {
        ChNumFormat::X32_Float,            // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_32_FLOAT,            // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Uint
    {
        ChNumFormat::X32Y32_Uint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Sint
    {
        ChNumFormat::X32Y32_Sint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Float
    {
        ChNumFormat::X32Y32_Float,         // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Uint
    {
        ChNumFormat::X32Y32Z32_Uint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Sint
    {
        ChNumFormat::X32Y32Z32_Sint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Float
    {
        ChNumFormat::X32Y32Z32_Float,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Uint
    {
        ChNumFormat::X32Y32Z32W32_Uint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_UINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Sint
    {
        ChNumFormat::X32Y32Z32W32_Sint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_SINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_SINT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Float
    {
        ChNumFormat::X32Y32Z32W32_Float,   // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_FLOAT__GFX10CORE, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::D16_Unorm_S8_Uint
    {
        ChNumFormat::D16_Unorm_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::D32_Float_S8_Uint
    {
        ChNumFormat::D32_Float_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X9Y9Z9E5_Float
    {
        ChNumFormat::X9Y9Z9E5_Float,       // ChNumFormat
        Chip::COLOR_5_9_9_9__GFX103PLUSEXCLUSIVE, // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_5_9_9_9_FLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Unorm
    {
        ChNumFormat::Bc1_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Srgb
    {
        ChNumFormat::Bc1_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC1_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Unorm
    {
        ChNumFormat::Bc2_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC2_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Srgb
    {
        ChNumFormat::Bc2_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC2_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Unorm
    {
        ChNumFormat::Bc3_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC3_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Srgb
    {
        ChNumFormat::Bc3_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC3_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Unorm
    {
        ChNumFormat::Bc4_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Snorm
    {
        ChNumFormat::Bc4_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Unorm
    {
        ChNumFormat::Bc5_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Snorm
    {
        ChNumFormat::Bc5_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Ufloat
    {
        ChNumFormat::Bc6_Ufloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_UFLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Sfloat
    {
        ChNumFormat::Bc6_Sfloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_SFLOAT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Unorm
    {
        ChNumFormat::Bc7_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC7_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Srgb
    {
        ChNumFormat::Bc7_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC7_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGB_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGB_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA1_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA1_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_ETC2_RGBA_SRGB__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Unorm
    {
        ChNumFormat::Etc2X11_Unorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_R_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Snorm
    {
        ChNumFormat::Etc2X11_Snorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_R_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Unorm
    {
        ChNumFormat::Etc2X11Y11_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RG_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Snorm
    {
        ChNumFormat::Etc2X11Y11_Snorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_ETC2_RG_SNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Unorm
    {
        ChNumFormat::AstcLdr4x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Srgb
    {
        ChNumFormat::AstcLdr4x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Unorm
    {
        ChNumFormat::AstcLdr5x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Srgb
    {
        ChNumFormat::AstcLdr5x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Unorm
    {
        ChNumFormat::AstcLdr5x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Srgb
    {
        ChNumFormat::AstcLdr5x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Unorm
    {
        ChNumFormat::AstcLdr6x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Srgb
    {
        ChNumFormat::AstcLdr6x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Unorm
    {
        ChNumFormat::AstcLdr6x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Srgb
    {
        ChNumFormat::AstcLdr6x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Unorm
    {
        ChNumFormat::AstcLdr8x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Srgb
    {
        ChNumFormat::AstcLdr8x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Unorm
    {
        ChNumFormat::AstcLdr8x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Srgb
    {
        ChNumFormat::AstcLdr8x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Unorm
    {
        ChNumFormat::AstcLdr8x8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Srgb
    {
        ChNumFormat::AstcLdr8x8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Unorm
    {
        ChNumFormat::AstcLdr10x5_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Srgb
    {
        ChNumFormat::AstcLdr10x5_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Unorm
    {
        ChNumFormat::AstcLdr10x6_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Srgb
    {
        ChNumFormat::AstcLdr10x6_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Unorm
    {
        ChNumFormat::AstcLdr10x8_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Srgb
    {
        ChNumFormat::AstcLdr10x8_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Unorm
    {
        ChNumFormat::AstcLdr10x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Srgb
    {
        ChNumFormat::AstcLdr10x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Unorm
    {
        ChNumFormat::AstcLdr12x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Srgb
    {
        ChNumFormat::AstcLdr12x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Unorm
    {
        ChNumFormat::AstcLdr12x12_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Srgb
    {
        ChNumFormat::AstcLdr12x12_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_LDR_SRGB_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr4x4_Float
    {
        ChNumFormat::AstcHdr4x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_4X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x4_Float
    {
        ChNumFormat::AstcHdr5x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_5X4__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x5_Float
    {
        ChNumFormat::AstcHdr5x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_5X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x5_Float
    {
        ChNumFormat::AstcHdr6x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_6X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x6_Float
    {
        ChNumFormat::AstcHdr6x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_6X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x5_Float
    {
        ChNumFormat::AstcHdr8x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x6_Float
    {
        ChNumFormat::AstcHdr8x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x8_Float
    {
        ChNumFormat::AstcHdr8x8_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_8X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x5_Float
    {
        ChNumFormat::AstcHdr10x5_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X5__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x6_Float
    {
        ChNumFormat::AstcHdr10x6_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X6__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x8_Float
    {
        ChNumFormat::AstcHdr10x8_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X8__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x10_Float
    {
        ChNumFormat::AstcHdr10x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_10X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x10_Float
    {
        ChNumFormat::AstcHdr12x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_12X10__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x12_Float
    {
        ChNumFormat::AstcHdr12x12_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_ASTC_2D_HDR_12X12__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Unorm
    {
        ChNumFormat::X8Y8_Z8Y8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_GB_GR_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Z8Y8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Unorm
    {
        ChNumFormat::Y8X8_Y8Z8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BG_RG_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Uscaled
    {
        ChNumFormat::Y8X8_Y8Z8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AYUV
    {
        ChNumFormat::AYUV,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::UYVY
    {
        ChNumFormat::UYVY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::VYUY
    {
        ChNumFormat::VYUY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YUY2
    {
        ChNumFormat::YUY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YVY2
    {
        ChNumFormat::YVY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YV12
    {
        ChNumFormat::YV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV11
    {
        ChNumFormat::NV11,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV12
    {
        ChNumFormat::NV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV21
    {
        ChNumFormat::NV21,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P016
    {
        ChNumFormat::P016,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P010
    {
        ChNumFormat::P010,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P210
    {
        ChNumFormat::P210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Unorm
    {
        ChNumFormat::X8_MM_Unorm,          // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Uint
    {
        ChNumFormat::X8_MM_Uint,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Unorm
    {
        ChNumFormat::X8Y8_MM_Unorm,        // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Uint
    {
        ChNumFormat::X8Y8_MM_Uint,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Unorm
    {
        ChNumFormat::X16_MM10_Unorm,       // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Uint
    {
        ChNumFormat::X16_MM10_Uint,        // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Unorm
    {
        ChNumFormat::X16Y16_MM10_Unorm,    // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UNORM__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Uint
    {
        ChNumFormat::X16Y16_MM10_Uint,     // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UINT__GFX10CORE, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P208
    {
        ChNumFormat::P208,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Unorm
    {
        ChNumFormat::X16_MM12_Unorm,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Uint
    {
        ChNumFormat::X16_MM12_Uint,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Unorm
    {
        ChNumFormat::X16Y16_MM12_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Uint
    {
        ChNumFormat::X16Y16_MM12_Uint,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P012
    {
        ChNumFormat::P012,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P212
    {
        ChNumFormat::P212,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P412
    {
        ChNumFormat::P412,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Float
    {
        ChNumFormat::X10Y10Z10W2_Float,    // ChNumFormat
        Chip::COLOR_2_10_10_10_6E4,        // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y216
    {
        ChNumFormat::Y216,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y210
    {
        ChNumFormat::Y210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y416
    {
        ChNumFormat::Y416,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y410
    {
        ChNumFormat::Y410,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
};

// Stores a ChNumFormat struct for each HW image format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx10_3MergedImgDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_Unorm,              // IMG_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // IMG_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // IMG_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // IMG_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // IMG_FMT_8_UINT
    ChNumFormat::X8_Sint,               // IMG_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // IMG_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // IMG_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // IMG_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // IMG_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // IMG_FMT_16_UINT
    ChNumFormat::X16_Sint,              // IMG_FMT_16_SINT
    ChNumFormat::X16_Float,             // IMG_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // IMG_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // IMG_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // IMG_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // IMG_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // IMG_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // IMG_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // IMG_FMT_32_UINT
    ChNumFormat::X32_Sint,              // IMG_FMT_32_SINT
    ChNumFormat::X32_Float,             // IMG_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // IMG_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // IMG_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // IMG_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // IMG_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // IMG_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // IMG_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // IMG_FMT_16_16_FLOAT
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X11Y11Z10_Float,       // IMG_FMT_10_11_11_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X10Y11Z11_Float,       // IMG_FMT_11_11_10_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // IMG_FMT_2_10_10_10_UNORM__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Snorm,     // IMG_FMT_2_10_10_10_SNORM__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uscaled,   // IMG_FMT_2_10_10_10_USCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sscaled,   // IMG_FMT_2_10_10_10_SSCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uint,      // IMG_FMT_2_10_10_10_UINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sint,      // IMG_FMT_2_10_10_10_SINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Unorm,        // IMG_FMT_8_8_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Snorm,        // IMG_FMT_8_8_8_8_SNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uscaled,      // IMG_FMT_8_8_8_8_USCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sscaled,      // IMG_FMT_8_8_8_8_SSCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uint,         // IMG_FMT_8_8_8_8_UINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sint,         // IMG_FMT_8_8_8_8_SINT__GFX10CORE
    ChNumFormat::X32Y32_Uint,           // IMG_FMT_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32_Sint,           // IMG_FMT_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32_Float,          // IMG_FMT_32_32_FLOAT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Unorm,    // IMG_FMT_16_16_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Snorm,    // IMG_FMT_16_16_16_16_SNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uscaled,  // IMG_FMT_16_16_16_16_USCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sscaled,  // IMG_FMT_16_16_16_16_SSCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uint,     // IMG_FMT_16_16_16_16_UINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sint,     // IMG_FMT_16_16_16_16_SINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Float,    // IMG_FMT_16_16_16_16_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32_Uint,        // IMG_FMT_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Sint,        // IMG_FMT_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Float,       // IMG_FMT_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Uint,     // IMG_FMT_32_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Sint,     // IMG_FMT_32_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Float,    // IMG_FMT_32_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_Srgb,               // IMG_FMT_8_SRGB__GFX10CORE
    ChNumFormat::X8Y8_Srgb,             // IMG_FMT_8_8_SRGB__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Srgb,         // IMG_FMT_8_8_8_8_SRGB__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X9Y9Z9E5_Float,        // IMG_FMT_5_9_9_9_FLOAT__GFX10CORE
    ChNumFormat::X5Y6Z5_Unorm,          // IMG_FMT_5_6_5_UNORM__GFX10CORE
    ChNumFormat::X5Y5Z5W1_Unorm,        // IMG_FMT_1_5_5_5_UNORM__GFX10CORE
    ChNumFormat::X1Y5Z5W5_Unorm,        // IMG_FMT_5_5_5_1_UNORM__GFX10CORE
    ChNumFormat::X4Y4Z4W4_Unorm,        // IMG_FMT_4_4_4_4_UNORM__GFX10CORE
    ChNumFormat::X4Y4_Unorm,            // IMG_FMT_4_4_UNORM__GFX10CORE
    ChNumFormat::X1_Unorm,              // IMG_FMT_1_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8Y8_Z8Y8_Unorm,       // IMG_FMT_GB_GR_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Y8X8_Y8Z8_Unorm,       // IMG_FMT_BG_RG_UNORM__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Bc1_Unorm,             // IMG_FMT_BC1_UNORM__GFX10CORE
    ChNumFormat::Bc1_Srgb,              // IMG_FMT_BC1_SRGB__GFX10CORE
    ChNumFormat::Bc2_Unorm,             // IMG_FMT_BC2_UNORM__GFX10CORE
    ChNumFormat::Bc2_Srgb,              // IMG_FMT_BC2_SRGB__GFX10CORE
    ChNumFormat::Bc3_Unorm,             // IMG_FMT_BC3_UNORM__GFX10CORE
    ChNumFormat::Bc3_Srgb,              // IMG_FMT_BC3_SRGB__GFX10CORE
    ChNumFormat::Bc4_Unorm,             // IMG_FMT_BC4_UNORM__GFX10CORE
    ChNumFormat::Bc4_Snorm,             // IMG_FMT_BC4_SNORM__GFX10CORE
    ChNumFormat::Bc5_Unorm,             // IMG_FMT_BC5_UNORM__GFX10CORE
    ChNumFormat::Bc5_Snorm,             // IMG_FMT_BC5_SNORM__GFX10CORE
    ChNumFormat::Bc6_Ufloat,            // IMG_FMT_BC6_UFLOAT__GFX10CORE
    ChNumFormat::Bc6_Sfloat,            // IMG_FMT_BC6_SFLOAT__GFX10CORE
    ChNumFormat::Bc7_Unorm,             // IMG_FMT_BC7_UNORM__GFX10CORE
    ChNumFormat::Bc7_Srgb,              // IMG_FMT_BC7_SRGB__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8_Unorm,      // IMG_FMT_ETC2_RGB_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8_Srgb,       // IMG_FMT_ETC2_RGB_SRGB__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W8_Unorm,    // IMG_FMT_ETC2_RGBA_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W8_Srgb,     // IMG_FMT_ETC2_RGBA_SRGB__GFX10CORE
    ChNumFormat::Etc2X11_Unorm,         // IMG_FMT_ETC2_R_UNORM__GFX10CORE
    ChNumFormat::Etc2X11_Snorm,         // IMG_FMT_ETC2_R_SNORM__GFX10CORE
    ChNumFormat::Etc2X11Y11_Unorm,      // IMG_FMT_ETC2_RG_UNORM__GFX10CORE
    ChNumFormat::Etc2X11Y11_Snorm,      // IMG_FMT_ETC2_RG_SNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W1_Unorm,    // IMG_FMT_ETC2_RGBA1_UNORM__GFX10CORE
    ChNumFormat::Etc2X8Y8Z8W1_Srgb,     // IMG_FMT_ETC2_RGBA1_SRGB__GFX10CORE
    ChNumFormat::AstcLdr4x4_Unorm,      // IMG_FMT_ASTC_2D_LDR_4X4__GFX10CORE
    ChNumFormat::AstcLdr5x4_Unorm,      // IMG_FMT_ASTC_2D_LDR_5X4__GFX10CORE
    ChNumFormat::AstcLdr5x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_5X5__GFX10CORE
    ChNumFormat::AstcLdr6x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_6X5__GFX10CORE
    ChNumFormat::AstcLdr6x6_Unorm,      // IMG_FMT_ASTC_2D_LDR_6X6__GFX10CORE
    ChNumFormat::AstcLdr8x5_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X5__GFX10CORE
    ChNumFormat::AstcLdr8x6_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X6__GFX10CORE
    ChNumFormat::AstcLdr8x8_Unorm,      // IMG_FMT_ASTC_2D_LDR_8X8__GFX10CORE
    ChNumFormat::AstcLdr10x5_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X5__GFX10CORE
    ChNumFormat::AstcLdr10x6_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X6__GFX10CORE
    ChNumFormat::AstcLdr10x8_Unorm,     // IMG_FMT_ASTC_2D_LDR_10X8__GFX10CORE
    ChNumFormat::AstcLdr10x10_Unorm,    // IMG_FMT_ASTC_2D_LDR_10X10__GFX10CORE
    ChNumFormat::AstcLdr12x10_Unorm,    // IMG_FMT_ASTC_2D_LDR_12X10__GFX10CORE
    ChNumFormat::AstcLdr12x12_Unorm,    // IMG_FMT_ASTC_2D_LDR_12X12__GFX10CORE
    ChNumFormat::AstcHdr4x4_Float,      // IMG_FMT_ASTC_2D_HDR_4X4__GFX10CORE
    ChNumFormat::AstcHdr5x4_Float,      // IMG_FMT_ASTC_2D_HDR_5X4__GFX10CORE
    ChNumFormat::AstcHdr5x5_Float,      // IMG_FMT_ASTC_2D_HDR_5X5__GFX10CORE
    ChNumFormat::AstcHdr6x5_Float,      // IMG_FMT_ASTC_2D_HDR_6X5__GFX10CORE
    ChNumFormat::AstcHdr6x6_Float,      // IMG_FMT_ASTC_2D_HDR_6X6__GFX10CORE
    ChNumFormat::AstcHdr8x5_Float,      // IMG_FMT_ASTC_2D_HDR_8X5__GFX10CORE
    ChNumFormat::AstcHdr8x6_Float,      // IMG_FMT_ASTC_2D_HDR_8X6__GFX10CORE
    ChNumFormat::AstcHdr8x8_Float,      // IMG_FMT_ASTC_2D_HDR_8X8__GFX10CORE
    ChNumFormat::AstcHdr10x5_Float,     // IMG_FMT_ASTC_2D_HDR_10X5__GFX10CORE
    ChNumFormat::AstcHdr10x6_Float,     // IMG_FMT_ASTC_2D_HDR_10X6__GFX10CORE
    ChNumFormat::AstcHdr10x8_Float,     // IMG_FMT_ASTC_2D_HDR_10X8__GFX10CORE
    ChNumFormat::AstcHdr10x10_Float,    // IMG_FMT_ASTC_2D_HDR_10X10__GFX10CORE
    ChNumFormat::AstcHdr12x10_Float,    // IMG_FMT_ASTC_2D_HDR_12X10__GFX10CORE
    ChNumFormat::AstcHdr12x12_Float,    // IMG_FMT_ASTC_2D_HDR_12X12__GFX10CORE
    ChNumFormat::AstcLdr4x4_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_4X4__GFX10CORE
    ChNumFormat::AstcLdr5x4_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_5X4__GFX10CORE
    ChNumFormat::AstcLdr5x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_5X5__GFX10CORE
    ChNumFormat::AstcLdr6x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_6X5__GFX10CORE
    ChNumFormat::AstcLdr6x6_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_6X6__GFX10CORE
    ChNumFormat::AstcLdr8x5_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X5__GFX10CORE
    ChNumFormat::AstcLdr8x6_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X6__GFX10CORE
    ChNumFormat::AstcLdr8x8_Srgb,       // IMG_FMT_ASTC_2D_LDR_SRGB_8X8__GFX10CORE
    ChNumFormat::AstcLdr10x5_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X5__GFX10CORE
    ChNumFormat::AstcLdr10x6_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X6__GFX10CORE
    ChNumFormat::AstcLdr10x8_Srgb,      // IMG_FMT_ASTC_2D_LDR_SRGB_10X8__GFX10CORE
    ChNumFormat::AstcLdr10x10_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_10X10__GFX10CORE
    ChNumFormat::AstcLdr12x10_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_12X10__GFX10CORE
    ChNumFormat::AstcLdr12x12_Srgb,     // IMG_FMT_ASTC_2D_LDR_SRGB_12X12__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_MM_Unorm,           // IMG_FMT_MM_8_UNORM__GFX10CORE
    ChNumFormat::X8_MM_Uint,            // IMG_FMT_MM_8_UINT__GFX10CORE
    ChNumFormat::X8Y8_MM_Unorm,         // IMG_FMT_MM_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8_MM_Uint,          // IMG_FMT_MM_8_8_UINT__GFX10CORE
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X16_MM10_Unorm,        // IMG_FMT_MM_10_IN_16_UNORM__GFX10CORE
    ChNumFormat::X16_MM10_Uint,         // IMG_FMT_MM_10_IN_16_UINT__GFX10CORE
    ChNumFormat::X16Y16_MM10_Unorm,     // IMG_FMT_MM_10_IN_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16_MM10_Uint,      // IMG_FMT_MM_10_IN_16_16_UINT__GFX10CORE
};

constexpr uint32 Gfx10_3MergedImgDataFmtCount = sizeof(Gfx10_3MergedImgDataFmtTbl) / sizeof(ChNumFormat);

// Stores a ChNumFormat struct for each HW buffer format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx10_3MergedBufDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X8_Unorm,              // BUF_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // BUF_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // BUF_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // BUF_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // BUF_FMT_8_UINT
    ChNumFormat::X8_Sint,               // BUF_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // BUF_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // BUF_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // BUF_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // BUF_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // BUF_FMT_16_UINT
    ChNumFormat::X16_Sint,              // BUF_FMT_16_SINT
    ChNumFormat::X16_Float,             // BUF_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // BUF_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // BUF_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // BUF_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // BUF_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // BUF_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // BUF_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // BUF_FMT_32_UINT
    ChNumFormat::X32_Sint,              // BUF_FMT_32_SINT
    ChNumFormat::X32_Float,             // BUF_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // BUF_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // BUF_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // BUF_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // BUF_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // BUF_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // BUF_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // BUF_FMT_16_16_FLOAT
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X11Y11Z10_Float,       // BUF_FMT_10_11_11_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X10Y11Z11_Float,       // BUF_FMT_11_11_10_FLOAT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // BUF_FMT_2_10_10_10_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sint,         // BUF_FMT_8_8_8_8_SINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uscaled,   // BUF_FMT_2_10_10_10_USCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sscaled,   // BUF_FMT_2_10_10_10_SSCALED__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Uint,      // BUF_FMT_2_10_10_10_UINT__GFX10CORE
    ChNumFormat::X10Y10Z10W2_Sint,      // BUF_FMT_2_10_10_10_SINT__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Unorm,        // BUF_FMT_8_8_8_8_UNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Snorm,        // BUF_FMT_8_8_8_8_SNORM__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uscaled,      // BUF_FMT_8_8_8_8_USCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Sscaled,      // BUF_FMT_8_8_8_8_SSCALED__GFX10CORE
    ChNumFormat::X8Y8Z8W8_Uint,         // BUF_FMT_8_8_8_8_UINT__GFX10CORE
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X32Y32_Uint,           // BUF_FMT_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32_Sint,           // BUF_FMT_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32_Float,          // BUF_FMT_32_32_FLOAT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Unorm,    // BUF_FMT_16_16_16_16_UNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Snorm,    // BUF_FMT_16_16_16_16_SNORM__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uscaled,  // BUF_FMT_16_16_16_16_USCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sscaled,  // BUF_FMT_16_16_16_16_SSCALED__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Uint,     // BUF_FMT_16_16_16_16_UINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Sint,     // BUF_FMT_16_16_16_16_SINT__GFX10CORE
    ChNumFormat::X16Y16Z16W16_Float,    // BUF_FMT_16_16_16_16_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32_Uint,        // BUF_FMT_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Sint,        // BUF_FMT_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32_Float,       // BUF_FMT_32_32_32_FLOAT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Uint,     // BUF_FMT_32_32_32_32_UINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Sint,     // BUF_FMT_32_32_32_32_SINT__GFX10CORE
    ChNumFormat::X32Y32Z32W32_Float,    // BUF_FMT_32_32_32_32_FLOAT__GFX10CORE
};

constexpr uint32 Gfx10_3MergedBufDataFmtCount = sizeof(Gfx10_3MergedBufDataFmtTbl) / sizeof(ChNumFormat);

#if PAL_BUILD_GFX11

// Lookup table for GPU access capabilities for each format/tiling-type pairing in Gfx11.
constexpr MergedFormatPropertiesTable Gfx11MergedFormatPropertiesTable =
{
    {
        // Note: Feature capabilities are listed in (linear, optimal) order.
        { None,                           None                           }, // ChNumFormat::Undefined
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X1_Unorm
        { None,                           None                           }, // ChNumFormat::X1_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X4Y4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::L4A4_Unorm
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X4Y4Z4W4_Unorm
        { None,                           None                           }, // ChNumFormat::X4Y4Z4W4_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y6Z5_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y6Z5_Uscaled
        { IrXsIwXdIflIfmmCBXP,            IrXsIwXdIflIfmmCBMXP           }, // ChNumFormat::X5Y5Z5W1_Unorm
        { None,                           None                           }, // ChNumFormat::X5Y5Z5W1_Uscaled
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X1Y5Z5W5_Unorm
        { None,                           None                           }, // ChNumFormat::X1Y5Z5W5_Uscaled
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCSMX           }, // ChNumFormat::X8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::P8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X8Y8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X8Y8_Sint
        { IrXsXdIflIfmmCBX,               IrXsXdIflIfmmCBMX              }, // ChNumFormat::X8Y8_Srgb
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L8A8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X8Y8Z8W8_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X8Y8Z8W8_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X8Y8Z8W8_Sint
        { IrXsXdIflIfmmCBXP,              IrXsXdIflIfmmCBMXP             }, // ChNumFormat::X8Y8Z8W8_Srgb
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y11Z11_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X11Y11Z10_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X10Y10Z10W2_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X10Y10Z10W2_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X10Y10Z10W2_Uint
        { IrXsIwIaIfmmTrTwTa,             IrXsIwIaIfmmTrTwTaM            }, // ChNumFormat::X10Y10Z10W2_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2Bias_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16_Sscaled
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16_Float
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::L16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Unorm
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X16Y16_Float
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Unorm
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Snorm
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Uscaled
        { IrXsIflTr,                      IrXsIflTrM                     }, // ChNumFormat::X16Y16Z16W16_Sscaled
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X16Y16Z16W16_Sint
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X16Y16Z16W16_Float
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Uint
        { IrXsIwXdIaIfmmTrTwTaCX,         IrXsIwXdIaIfmmTrTwTaCMX        }, // ChNumFormat::X32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBDMX       }, // ChNumFormat::X32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32_Float
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Uint
        { IrXsIfmmTrTw,                   None                           }, // ChNumFormat::X32Y32Z32_Sint
        { IrXsIflIfmmTrTw,                None                           }, // ChNumFormat::X32Y32Z32_Float
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Uint
        { IrXsIwXdIfmmTrTwCX,             IrXsIwXdIfmmTrTwCMX            }, // ChNumFormat::X32Y32Z32W32_Sint
        { IrXsIwXdIflIfmmTrTwCBX,         IrXsIwXdIflIfmmTrTwCBMX        }, // ChNumFormat::X32Y32Z32W32_Float
        { None,                           IrXsDSM                        }, // ChNumFormat::D16_Unorm_S8_Uint
        { None,                           IrXsDSM                        }, // ChNumFormat::D32_Float_S8_Uint
        { IrXsIwXdIaIflIfmmCBX,           IrXsIwXdIaIflIfmmCBMX          }, // ChNumFormat::X9Y9Z9E5_Float
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc1_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc2_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc3_Srgb
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc4_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc5_Snorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Ufloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc6_Sfloat
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Unorm
        { Copy,                           IrXsIfl                        }, // ChNumFormat::Bc7_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        { None,                           None                           }, // ChNumFormat::Etc2X11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11_Snorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Unorm
        { None,                           None                           }, // ChNumFormat::Etc2X11Y11_Snorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr4x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x4_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr5x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr6x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr8x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x5_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x6_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x8_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr10x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x10_Srgb
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Unorm
        { None,                           None                           }, // ChNumFormat::AstcLdr12x12_Srgb
        { None,                           None                           }, // ChNumFormat::AstcHdr4x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x4_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr5x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr6x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr8x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x5_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x6_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x8_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr10x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x10_Float
        { None,                           None                           }, // ChNumFormat::AstcHdr12x12_Float
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::X8Y8_Z8Y8_Unorm
        { None,                           None                           }, // ChNumFormat::X8Y8_Z8Y8_Uscaled
        { IrXsIflIfmm,                    IrXsIflIfmm                    }, // ChNumFormat::Y8X8_Y8Z8_Unorm
        { None,                           None                           }, // ChNumFormat::Y8X8_Y8Z8_Uscaled
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::AYUV
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::UYVY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::VYUY
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YUY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YVY2
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::YV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV11
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV12
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::NV21
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P016
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P010
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P210
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X8Y8_MM_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X8Y8_MM_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16_MM10_Unorm
        { IrXsIwXdIfmmCX,                 IrXsIwXdIfmmCMX                }, // ChNumFormat::X16_MM10_Uint
        { IrXsIwXdIflIfmmCBX,             IrXsIwXdIflIfmmCBMX            }, // ChNumFormat::X16Y16_MM10_Unorm
        { IrXsIwXdIaIfmmCX,               IrXsIwXdIaIfmmCMX              }, // ChNumFormat::X16Y16_MM10_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::P208
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16_MM12_Unorm
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16_MM12_Uint
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16Y16_MM12_Unorm
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::X16Y16_MM12_Uint
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P012
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P212
        { IrXsIw,                         IrXsIw                         }, // ChNumFormat::P412
        { IrXsIwXdIflIfmmTrTwCBXP,        IrXsIwXdIflIfmmTrTwCBMXP       }, // ChNumFormat::X10Y10Z10W2_Float
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y216
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y210
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y416
        { IrXsIwXdCX,                     IrXsIwXdCX                     }, // ChNumFormat::Y410
    }
};

// Stores a ChannelFmtInfo struct for each available channel format for mapping PAL channel formats to the format values
// for various hardware blocks.
constexpr MergedFlatFmtInfo Gfx11MergedChannelFmtInfoTbl[] =
{
    // ChNumFormat::Undefined
    {
        ChNumFormat::Undefined,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Unorm
    {
        ChNumFormat::X1_Unorm,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1_Uscaled
    {
        ChNumFormat::X1_Uscaled,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Unorm
    {
        ChNumFormat::X4Y4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4_Uscaled
    {
        ChNumFormat::X4Y4_Uscaled,         // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L4A4_Unorm
    {
        ChNumFormat::L4A4_Unorm,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Unorm
    {
        ChNumFormat::X4Y4Z4W4_Unorm,       // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_4_4_4_4_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X4Y4Z4W4_Uscaled
    {
        ChNumFormat::X4Y4Z4W4_Uscaled,     // ChNumFormat
        Chip::COLOR_4_4_4_4,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Unorm
    {
        ChNumFormat::X5Y6Z5_Unorm,         // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_6_5_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y6Z5_Uscaled
    {
        ChNumFormat::X5Y6Z5_Uscaled,       // ChNumFormat
        Chip::COLOR_5_6_5,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Unorm
    {
        ChNumFormat::X5Y5Z5W1_Unorm,       // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_1_5_5_5_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X5Y5Z5W1_Uscaled
    {
        ChNumFormat::X5Y5Z5W1_Uscaled,     // ChNumFormat
        Chip::COLOR_1_5_5_5,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Unorm
    {
        ChNumFormat::X1Y5Z5W5_Unorm,       // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_5_5_5_1_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X1Y5Z5W5_Uscaled
    {
        ChNumFormat::X1Y5Z5W5_Uscaled,     // ChNumFormat
        Chip::COLOR_5_5_5_1,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Unorm
    {
        ChNumFormat::X8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Snorm
    {
        ChNumFormat::X8_Snorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_SNORM,             // Image Channel Format
        Chip::BUF_FMT_8_SNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uscaled
    {
        ChNumFormat::X8_Uscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_USCALED,           // Image Channel Format
        Chip::BUF_FMT_8_USCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Sscaled
    {
        ChNumFormat::X8_Sscaled,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_SSCALED,           // Image Channel Format
        Chip::BUF_FMT_8_SSCALED,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Uint
    {
        ChNumFormat::X8_Uint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_UINT,              // Image Channel Format
        Chip::BUF_FMT_8_UINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X8_Sint
    {
        ChNumFormat::X8_Sint,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_SINT,              // Image Channel Format
        Chip::BUF_FMT_8_SINT,              // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_Srgb
    {
        ChNumFormat::X8_Srgb,              // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_SRGB__GFX104PLUS,  // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::A8_Unorm
    {
        ChNumFormat::A8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8_Unorm
    {
        ChNumFormat::L8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P8_Unorm
    {
        ChNumFormat::P8_Unorm,             // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_UNORM,             // Image Channel Format
        Chip::BUF_FMT_8_UNORM,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Unorm
    {
        ChNumFormat::X8Y8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Snorm
    {
        ChNumFormat::X8Y8_Snorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_SNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_SNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Uscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_USCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_USCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sscaled
    {
        ChNumFormat::X8Y8_Sscaled,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_SSCALED,         // Image Channel Format
        Chip::BUF_FMT_8_8_SSCALED,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Uint
    {
        ChNumFormat::X8Y8_Uint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_UINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_UINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Sint
    {
        ChNumFormat::X8Y8_Sint,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SINT,            // Image Channel Format
        Chip::BUF_FMT_8_8_SINT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Srgb
    {
        ChNumFormat::X8Y8_Srgb,            // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_SRGB__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L8A8_Unorm
    {
        ChNumFormat::L8A8_Unorm,           // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_UNORM,           // Image Channel Format
        Chip::BUF_FMT_8_8_UNORM,           // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Unorm
    {
        ChNumFormat::X8Y8Z8W8_Unorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Snorm
    {
        ChNumFormat::X8Y8Z8W8_Snorm,       // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uscaled
    {
        ChNumFormat::X8Y8Z8W8_Uscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_USCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_USCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sscaled
    {
        ChNumFormat::X8Y8Z8W8_Sscaled,     // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SSCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SSCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Uint
    {
        ChNumFormat::X8Y8Z8W8_Uint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Sint
    {
        ChNumFormat::X8Y8Z8W8_Sint,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_8_8_8_8_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8Z8W8_Srgb
    {
        ChNumFormat::X8Y8Z8W8_Srgb,        // ChNumFormat
        Chip::COLOR_8_8_8_8,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_8_8_8_8_SRGB__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U8V8_Snorm_L8W8_Unorm
    {
        ChNumFormat::U8V8_Snorm_L8W8_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y11Z11_Float
    {
        ChNumFormat::X10Y11Z11_Float,      // ChNumFormat
        Chip::COLOR_11_11_10,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_11_11_10_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_11_11_10_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X11Y11Z10_Float
    {
        ChNumFormat::X11Y11Z10_Float,      // ChNumFormat
        Chip::COLOR_10_11_11,              // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_10_11_11_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_10_11_11_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Unorm
    {
        ChNumFormat::X10Y10Z10W2_Unorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Snorm
    {
        ChNumFormat::X10Y10Z10W2_Snorm,    // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uscaled
    {
        ChNumFormat::X10Y10Z10W2_Uscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_USCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_USCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sscaled
    {
        ChNumFormat::X10Y10Z10W2_Sscaled,  // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SSCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SSCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Uint
    {
        ChNumFormat::X10Y10Z10W2_Uint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Sint
    {
        ChNumFormat::X10Y10Z10W2_Sint,     // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2Bias_Unorm
    {
        ChNumFormat::X10Y10Z10W2Bias_Unorm, // ChNumFormat
        Chip::COLOR_2_10_10_10,            // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_2_10_10_10_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_2_10_10_10_UNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::U10V10W10_Snorm_A2_Unorm
    {
        ChNumFormat::U10V10W10_Snorm_A2_Unorm, // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Unorm
    {
        ChNumFormat::X16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Snorm
    {
        ChNumFormat::X16_Snorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_SNORM,            // Image Channel Format
        Chip::BUF_FMT_16_SNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uscaled
    {
        ChNumFormat::X16_Uscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_USCALED,          // Image Channel Format
        Chip::BUF_FMT_16_USCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sscaled
    {
        ChNumFormat::X16_Sscaled,          // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_SSCALED,          // Image Channel Format
        Chip::BUF_FMT_16_SSCALED,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Uint
    {
        ChNumFormat::X16_Uint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_UINT,             // Image Channel Format
        Chip::BUF_FMT_16_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Sint
    {
        ChNumFormat::X16_Sint,             // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_SINT,             // Image Channel Format
        Chip::BUF_FMT_16_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_Float
    {
        ChNumFormat::X16_Float,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_16_FLOAT,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::L16_Unorm
    {
        ChNumFormat::L16_Unorm,            // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_UNORM,            // Image Channel Format
        Chip::BUF_FMT_16_UNORM,            // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Unorm
    {
        ChNumFormat::X16Y16_Unorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_UNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_UNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Snorm
    {
        ChNumFormat::X16Y16_Snorm,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_SNORM,         // Image Channel Format
        Chip::BUF_FMT_16_16_SNORM,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uscaled
    {
        ChNumFormat::X16Y16_Uscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_USCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_USCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sscaled
    {
        ChNumFormat::X16Y16_Sscaled,       // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_SSCALED,       // Image Channel Format
        Chip::BUF_FMT_16_16_SSCALED,       // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Uint
    {
        ChNumFormat::X16Y16_Uint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_UINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_UINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Sint
    {
        ChNumFormat::X16Y16_Sint,          // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_SINT,          // Image Channel Format
        Chip::BUF_FMT_16_16_SINT,          // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_Float
    {
        ChNumFormat::X16Y16_Float,         // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_FLOAT,         // Image Channel Format
        Chip::BUF_FMT_16_16_FLOAT,         // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Unorm
    {
        ChNumFormat::X16Y16Z16W16_Unorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Snorm
    {
        ChNumFormat::X16Y16Z16W16_Snorm,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SNORM__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uscaled
    {
        ChNumFormat::X16Y16Z16W16_Uscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_USCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_USCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sscaled
    {
        ChNumFormat::X16Y16Z16W16_Sscaled, // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SSCALED,              // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SSCALED__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SSCALED__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Uint
    {
        ChNumFormat::X16Y16Z16W16_Uint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Sint
    {
        ChNumFormat::X16Y16Z16W16_Sint,    // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16Z16W16_Float
    {
        ChNumFormat::X16Y16Z16W16_Float,   // ChNumFormat
        Chip::COLOR_16_16_16_16,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_16_16_16_16_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_16_16_16_16_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Uint
    {
        ChNumFormat::X32_Uint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_UINT,             // Image Channel Format
        Chip::BUF_FMT_32_UINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Sint
    {
        ChNumFormat::X32_Sint,             // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_SINT,             // Image Channel Format
        Chip::BUF_FMT_32_SINT,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32_Float
    {
        ChNumFormat::X32_Float,            // ChNumFormat
        Chip::COLOR_32,                    // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_FLOAT,            // Image Channel Format
        Chip::BUF_FMT_32_FLOAT,            // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Uint
    {
        ChNumFormat::X32Y32_Uint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Sint
    {
        ChNumFormat::X32Y32_Sint,          // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32_Float
    {
        ChNumFormat::X32Y32_Float,         // ChNumFormat
        Chip::COLOR_32_32,                 // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Uint
    {
        ChNumFormat::X32Y32Z32_Uint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Sint
    {
        ChNumFormat::X32Y32Z32_Sint,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32_Float
    {
        ChNumFormat::X32Y32Z32_Float,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Uint
    {
        ChNumFormat::X32Y32Z32W32_Uint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_UINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Sint
    {
        ChNumFormat::X32Y32Z32W32_Sint,    // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_SINT,                 // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_SINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_SINT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X32Y32Z32W32_Float
    {
        ChNumFormat::X32Y32Z32W32_Float,   // ChNumFormat
        Chip::COLOR_32_32_32_32,           // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_32_32_32_32_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_32_32_32_32_FLOAT__GFX104PLUS, // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::D16_Unorm_S8_Uint
    {
        ChNumFormat::D16_Unorm_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_16,                        // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::D32_Float_S8_Uint
    {
        ChNumFormat::D32_Float_S8_Uint,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_32_FLOAT,                  // ZFormat
        Chip::STENCIL_8,                   // StencilFormat
    },
    // ChNumFormat::X9Y9Z9E5_Float
    {
        ChNumFormat::X9Y9Z9E5_Float,       // ChNumFormat
        Chip::COLOR_5_9_9_9__GFX103PLUSEXCLUSIVE, // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_5_9_9_9_FLOAT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Unorm
    {
        ChNumFormat::Bc1_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC1_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc1_Srgb
    {
        ChNumFormat::Bc1_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC1_SRGB__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Unorm
    {
        ChNumFormat::Bc2_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC2_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc2_Srgb
    {
        ChNumFormat::Bc2_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC2_SRGB__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Unorm
    {
        ChNumFormat::Bc3_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC3_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc3_Srgb
    {
        ChNumFormat::Bc3_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC3_SRGB__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Unorm
    {
        ChNumFormat::Bc4_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_UNORM__GFX11,    // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc4_Snorm
    {
        ChNumFormat::Bc4_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC4_SNORM__GFX11,    // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Unorm
    {
        ChNumFormat::Bc5_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_UNORM__GFX11,    // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc5_Snorm
    {
        ChNumFormat::Bc5_Snorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC5_SNORM__GFX11,    // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Ufloat
    {
        ChNumFormat::Bc6_Ufloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_UFLOAT__GFX11,   // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc6_Sfloat
    {
        ChNumFormat::Bc6_Sfloat,           // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_BC6_SFLOAT__GFX11,   // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Unorm
    {
        ChNumFormat::Bc7_Unorm,            // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BC7_UNORM__GFX11,    // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Bc7_Srgb
    {
        ChNumFormat::Bc7_Srgb,             // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_BC7_SRGB__GFX11,     // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W1_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Unorm
    {
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X8Y8Z8W8_Srgb
    {
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SRGB,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Unorm
    {
        ChNumFormat::Etc2X11_Unorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11_Snorm
    {
        ChNumFormat::Etc2X11_Snorm,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Unorm
    {
        ChNumFormat::Etc2X11Y11_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Etc2X11Y11_Snorm
    {
        ChNumFormat::Etc2X11Y11_Snorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_SNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Unorm
    {
        ChNumFormat::AstcLdr4x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr4x4_Srgb
    {
        ChNumFormat::AstcLdr4x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Unorm
    {
        ChNumFormat::AstcLdr5x4_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x4_Srgb
    {
        ChNumFormat::AstcLdr5x4_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Unorm
    {
        ChNumFormat::AstcLdr5x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr5x5_Srgb
    {
        ChNumFormat::AstcLdr5x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Unorm
    {
        ChNumFormat::AstcLdr6x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x5_Srgb
    {
        ChNumFormat::AstcLdr6x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Unorm
    {
        ChNumFormat::AstcLdr6x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr6x6_Srgb
    {
        ChNumFormat::AstcLdr6x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Unorm
    {
        ChNumFormat::AstcLdr8x5_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x5_Srgb
    {
        ChNumFormat::AstcLdr8x5_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Unorm
    {
        ChNumFormat::AstcLdr8x6_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x6_Srgb
    {
        ChNumFormat::AstcLdr8x6_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Unorm
    {
        ChNumFormat::AstcLdr8x8_Unorm,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr8x8_Srgb
    {
        ChNumFormat::AstcLdr8x8_Srgb,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Unorm
    {
        ChNumFormat::AstcLdr10x5_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x5_Srgb
    {
        ChNumFormat::AstcLdr10x5_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Unorm
    {
        ChNumFormat::AstcLdr10x6_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x6_Srgb
    {
        ChNumFormat::AstcLdr10x6_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Unorm
    {
        ChNumFormat::AstcLdr10x8_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x8_Srgb
    {
        ChNumFormat::AstcLdr10x8_Srgb,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Unorm
    {
        ChNumFormat::AstcLdr10x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr10x10_Srgb
    {
        ChNumFormat::AstcLdr10x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Unorm
    {
        ChNumFormat::AstcLdr12x10_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x10_Srgb
    {
        ChNumFormat::AstcLdr12x10_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Unorm
    {
        ChNumFormat::AstcLdr12x12_Unorm,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcLdr12x12_Srgb
    {
        ChNumFormat::AstcLdr12x12_Srgb,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr4x4_Float
    {
        ChNumFormat::AstcHdr4x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x4_Float
    {
        ChNumFormat::AstcHdr5x4_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr5x5_Float
    {
        ChNumFormat::AstcHdr5x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x5_Float
    {
        ChNumFormat::AstcHdr6x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr6x6_Float
    {
        ChNumFormat::AstcHdr6x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x5_Float
    {
        ChNumFormat::AstcHdr8x5_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x6_Float
    {
        ChNumFormat::AstcHdr8x6_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr8x8_Float
    {
        ChNumFormat::AstcHdr8x8_Float,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x5_Float
    {
        ChNumFormat::AstcHdr10x5_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x6_Float
    {
        ChNumFormat::AstcHdr10x6_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x8_Float
    {
        ChNumFormat::AstcHdr10x8_Float,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr10x10_Float
    {
        ChNumFormat::AstcHdr10x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x10_Float
    {
        ChNumFormat::AstcHdr12x10_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AstcHdr12x12_Float
    {
        ChNumFormat::AstcHdr12x12_Float,   // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Unorm
    {
        ChNumFormat::X8Y8_Z8Y8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_GB_GR_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_Z8Y8_Uscaled
    {
        ChNumFormat::X8Y8_Z8Y8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Unorm
    {
        ChNumFormat::Y8X8_Y8Z8_Unorm,      // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_BG_RG_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y8X8_Y8Z8_Uscaled
    {
        ChNumFormat::Y8X8_Y8Z8_Uscaled,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_USCALED,              // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::AYUV
    {
        ChNumFormat::AYUV,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::UYVY
    {
        ChNumFormat::UYVY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::VYUY
    {
        ChNumFormat::VYUY,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YUY2
    {
        ChNumFormat::YUY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YVY2
    {
        ChNumFormat::YVY2,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::YV12
    {
        ChNumFormat::YV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV11
    {
        ChNumFormat::NV11,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV12
    {
        ChNumFormat::NV12,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::NV21
    {
        ChNumFormat::NV21,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P016
    {
        ChNumFormat::P016,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P010
    {
        ChNumFormat::P010,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P210
    {
        ChNumFormat::P210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Unorm
    {
        ChNumFormat::X8_MM_Unorm,          // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8_MM_Uint
    {
        ChNumFormat::X8_MM_Uint,           // ChNumFormat
        Chip::COLOR_8,                     // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Unorm
    {
        ChNumFormat::X8Y8_MM_Unorm,        // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UNORM__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X8Y8_MM_Uint
    {
        ChNumFormat::X8Y8_MM_Uint,         // ChNumFormat
        Chip::COLOR_8_8,                   // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_8_8_UINT__GFX104PLUS, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Unorm
    {
        ChNumFormat::X16_MM10_Unorm,       // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UNORM__GFX11, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM10_Uint
    {
        ChNumFormat::X16_MM10_Uint,        // ChNumFormat
        Chip::COLOR_16,                    // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_UINT__GFX11, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Unorm
    {
        ChNumFormat::X16Y16_MM10_Unorm,    // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UNORM__GFX11, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM10_Uint
    {
        ChNumFormat::X16Y16_MM10_Uint,     // ChNumFormat
        Chip::COLOR_16_16,                 // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_MM_10_IN_16_16_UINT__GFX11, // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P208
    {
        ChNumFormat::P208,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Unorm
    {
        ChNumFormat::X16_MM12_Unorm,       // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16_MM12_Uint
    {
        ChNumFormat::X16_MM12_Uint,        // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Unorm
    {
        ChNumFormat::X16Y16_MM12_Unorm,    // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UNORM,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X16Y16_MM12_Uint
    {
        ChNumFormat::X16Y16_MM12_Uint,     // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_UINT,                 // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P012
    {
        ChNumFormat::P012,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P212
    {
        ChNumFormat::P212,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::P412
    {
        ChNumFormat::P412,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::X10Y10Z10W2_Float
    {
        ChNumFormat::X10Y10Z10W2_Float,    // ChNumFormat
        Chip::COLOR_2_10_10_10_6E4,        // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y216
    {
        ChNumFormat::Y216,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y210
    {
        ChNumFormat::Y210,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y416
    {
        ChNumFormat::Y416,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
    // ChNumFormat::Y410
    {
        ChNumFormat::Y410,                 // ChNumFormat
        Chip::COLOR_INVALID,               // CB Channel Format
        Chip::NUMBER_FLOAT,                // CB Numeric Format
        Chip::IMG_FMT_INVALID,             // Image Channel Format
        Chip::BUF_FMT_INVALID,             // Buffer Image Format
        Chip::Z_INVALID,                   // ZFormat
        Chip::STENCIL_INVALID,             // StencilFormat
    },
};

// Stores a ChNumFormat struct for each HW image format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx11MergedImgDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_Unorm,              // IMG_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // IMG_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // IMG_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // IMG_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // IMG_FMT_8_UINT
    ChNumFormat::X8_Sint,               // IMG_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // IMG_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // IMG_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // IMG_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // IMG_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // IMG_FMT_16_UINT
    ChNumFormat::X16_Sint,              // IMG_FMT_16_SINT
    ChNumFormat::X16_Float,             // IMG_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // IMG_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // IMG_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // IMG_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // IMG_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // IMG_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // IMG_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // IMG_FMT_32_UINT
    ChNumFormat::X32_Sint,              // IMG_FMT_32_SINT
    ChNumFormat::X32_Float,             // IMG_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // IMG_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // IMG_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // IMG_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // IMG_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // IMG_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // IMG_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // IMG_FMT_16_16_FLOAT
    ChNumFormat::X11Y11Z10_Float,       // IMG_FMT_10_11_11_FLOAT__GFX104PLUS
    ChNumFormat::X10Y11Z11_Float,       // IMG_FMT_11_11_10_FLOAT__GFX104PLUS
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // IMG_FMT_2_10_10_10_UNORM__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Snorm,     // IMG_FMT_2_10_10_10_SNORM__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Uscaled,   // IMG_FMT_2_10_10_10_USCALED__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Sscaled,   // IMG_FMT_2_10_10_10_SSCALED__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Uint,      // IMG_FMT_2_10_10_10_UINT__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Sint,      // IMG_FMT_2_10_10_10_SINT__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Unorm,        // IMG_FMT_8_8_8_8_UNORM__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Snorm,        // IMG_FMT_8_8_8_8_SNORM__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Uscaled,      // IMG_FMT_8_8_8_8_USCALED__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Sscaled,      // IMG_FMT_8_8_8_8_SSCALED__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Uint,         // IMG_FMT_8_8_8_8_UINT__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Sint,         // IMG_FMT_8_8_8_8_SINT__GFX104PLUS
    ChNumFormat::X32Y32_Uint,           // IMG_FMT_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32_Sint,           // IMG_FMT_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32_Float,          // IMG_FMT_32_32_FLOAT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Unorm,    // IMG_FMT_16_16_16_16_UNORM__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Snorm,    // IMG_FMT_16_16_16_16_SNORM__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Uscaled,  // IMG_FMT_16_16_16_16_USCALED__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Sscaled,  // IMG_FMT_16_16_16_16_SSCALED__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Uint,     // IMG_FMT_16_16_16_16_UINT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Sint,     // IMG_FMT_16_16_16_16_SINT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Float,    // IMG_FMT_16_16_16_16_FLOAT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Uint,        // IMG_FMT_32_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Sint,        // IMG_FMT_32_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Float,       // IMG_FMT_32_32_32_FLOAT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Uint,     // IMG_FMT_32_32_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Sint,     // IMG_FMT_32_32_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Float,    // IMG_FMT_32_32_32_32_FLOAT__GFX104PLUS
    ChNumFormat::X8_Srgb,               // IMG_FMT_8_SRGB__GFX104PLUS
    ChNumFormat::X8Y8_Srgb,             // IMG_FMT_8_8_SRGB__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Srgb,         // IMG_FMT_8_8_8_8_SRGB__GFX104PLUS
    ChNumFormat::X9Y9Z9E5_Float,        // IMG_FMT_5_9_9_9_FLOAT__GFX104PLUS
    ChNumFormat::X5Y6Z5_Unorm,          // IMG_FMT_5_6_5_UNORM__GFX104PLUS
    ChNumFormat::X5Y5Z5W1_Unorm,        // IMG_FMT_1_5_5_5_UNORM__GFX104PLUS
    ChNumFormat::X1Y5Z5W5_Unorm,        // IMG_FMT_5_5_5_1_UNORM__GFX104PLUS
    ChNumFormat::X4Y4Z4W4_Unorm,        // IMG_FMT_4_4_4_4_UNORM__GFX104PLUS
    ChNumFormat::X4Y4_Unorm,            // IMG_FMT_4_4_UNORM__GFX104PLUS
    ChNumFormat::X1_Unorm,              // IMG_FMT_1_UNORM__GFX104PLUS
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8Y8_Z8Y8_Unorm,       // IMG_FMT_GB_GR_UNORM__GFX104PLUS
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Y8X8_Y8Z8_Unorm,       // IMG_FMT_BG_RG_UNORM__GFX104PLUS
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X16_MM10_Unorm,        // IMG_FMT_MM_10_IN_16_UNORM__GFX11
    ChNumFormat::X16_MM10_Uint,         // IMG_FMT_MM_10_IN_16_UINT__GFX11
    ChNumFormat::X16Y16_MM10_Unorm,     // IMG_FMT_MM_10_IN_16_16_UNORM__GFX11
    ChNumFormat::X16Y16_MM10_Uint,      // IMG_FMT_MM_10_IN_16_16_UINT__GFX11
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Bc1_Unorm,             // IMG_FMT_BC1_UNORM__GFX104PLUS
    ChNumFormat::Bc1_Srgb,              // IMG_FMT_BC1_SRGB__GFX104PLUS
    ChNumFormat::Bc2_Unorm,             // IMG_FMT_BC2_UNORM__GFX104PLUS
    ChNumFormat::Bc2_Srgb,              // IMG_FMT_BC2_SRGB__GFX104PLUS
    ChNumFormat::Bc3_Unorm,             // IMG_FMT_BC3_UNORM__GFX104PLUS
    ChNumFormat::Bc3_Srgb,              // IMG_FMT_BC3_SRGB__GFX104PLUS
    ChNumFormat::Bc4_Unorm,             // IMG_FMT_BC4_UNORM__GFX11
    ChNumFormat::Bc4_Snorm,             // IMG_FMT_BC4_SNORM__GFX11
    ChNumFormat::Bc5_Unorm,             // IMG_FMT_BC5_UNORM__GFX11
    ChNumFormat::Bc5_Snorm,             // IMG_FMT_BC5_SNORM__GFX11
    ChNumFormat::Bc6_Ufloat,            // IMG_FMT_BC6_UFLOAT__GFX11
    ChNumFormat::Bc6_Sfloat,            // IMG_FMT_BC6_SFLOAT__GFX11
    ChNumFormat::Bc7_Unorm,             // IMG_FMT_BC7_UNORM__GFX11
    ChNumFormat::Bc7_Srgb,              // IMG_FMT_BC7_SRGB__GFX11
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::Undefined,             // IMG_FMT_INVALID
    ChNumFormat::X8_MM_Unorm,           // IMG_FMT_MM_8_UNORM__GFX104PLUS
    ChNumFormat::X8_MM_Uint,            // IMG_FMT_MM_8_UINT__GFX104PLUS
    ChNumFormat::X8Y8_MM_Unorm,         // IMG_FMT_MM_8_8_UNORM__GFX104PLUS
    ChNumFormat::X8Y8_MM_Uint,          // IMG_FMT_MM_8_8_UINT__GFX104PLUS
};

constexpr uint32 Gfx11MergedImgDataFmtCount = sizeof(Gfx11MergedImgDataFmtTbl) / sizeof(ChNumFormat);

// Stores a ChNumFormat struct for each HW buffer format up to the last format known to the spreadsheet.
constexpr ChNumFormat Gfx11MergedBufDataFmtTbl[] =
{
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X8_Unorm,              // BUF_FMT_8_UNORM
    ChNumFormat::X8_Snorm,              // BUF_FMT_8_SNORM
    ChNumFormat::X8_Uscaled,            // BUF_FMT_8_USCALED
    ChNumFormat::X8_Sscaled,            // BUF_FMT_8_SSCALED
    ChNumFormat::X8_Uint,               // BUF_FMT_8_UINT
    ChNumFormat::X8_Sint,               // BUF_FMT_8_SINT
    ChNumFormat::X16_Unorm,             // BUF_FMT_16_UNORM
    ChNumFormat::X16_Snorm,             // BUF_FMT_16_SNORM
    ChNumFormat::X16_Uscaled,           // BUF_FMT_16_USCALED
    ChNumFormat::X16_Sscaled,           // BUF_FMT_16_SSCALED
    ChNumFormat::X16_Uint,              // BUF_FMT_16_UINT
    ChNumFormat::X16_Sint,              // BUF_FMT_16_SINT
    ChNumFormat::X16_Float,             // BUF_FMT_16_FLOAT
    ChNumFormat::X8Y8_Unorm,            // BUF_FMT_8_8_UNORM
    ChNumFormat::X8Y8_Snorm,            // BUF_FMT_8_8_SNORM
    ChNumFormat::X8Y8_Uscaled,          // BUF_FMT_8_8_USCALED
    ChNumFormat::X8Y8_Sscaled,          // BUF_FMT_8_8_SSCALED
    ChNumFormat::X8Y8_Uint,             // BUF_FMT_8_8_UINT
    ChNumFormat::X8Y8_Sint,             // BUF_FMT_8_8_SINT
    ChNumFormat::X32_Uint,              // BUF_FMT_32_UINT
    ChNumFormat::X32_Sint,              // BUF_FMT_32_SINT
    ChNumFormat::X32_Float,             // BUF_FMT_32_FLOAT
    ChNumFormat::X16Y16_Unorm,          // BUF_FMT_16_16_UNORM
    ChNumFormat::X16Y16_Snorm,          // BUF_FMT_16_16_SNORM
    ChNumFormat::X16Y16_Uscaled,        // BUF_FMT_16_16_USCALED
    ChNumFormat::X16Y16_Sscaled,        // BUF_FMT_16_16_SSCALED
    ChNumFormat::X16Y16_Uint,           // BUF_FMT_16_16_UINT
    ChNumFormat::X16Y16_Sint,           // BUF_FMT_16_16_SINT
    ChNumFormat::X16Y16_Float,          // BUF_FMT_16_16_FLOAT
    ChNumFormat::X11Y11Z10_Float,       // BUF_FMT_10_11_11_FLOAT__GFX104PLUS
    ChNumFormat::X10Y11Z11_Float,       // BUF_FMT_11_11_10_FLOAT__GFX104PLUS
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::Undefined,             // BUF_FMT_INVALID
    ChNumFormat::X10Y10Z10W2_Unorm,     // BUF_FMT_2_10_10_10_UNORM__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Snorm,     // BUF_FMT_2_10_10_10_SNORM__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Uscaled,   // BUF_FMT_2_10_10_10_USCALED__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Sscaled,   // BUF_FMT_2_10_10_10_SSCALED__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Uint,      // BUF_FMT_2_10_10_10_UINT__GFX104PLUS
    ChNumFormat::X10Y10Z10W2_Sint,      // BUF_FMT_2_10_10_10_SINT__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Unorm,        // BUF_FMT_8_8_8_8_UNORM__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Snorm,        // BUF_FMT_8_8_8_8_SNORM__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Uscaled,      // BUF_FMT_8_8_8_8_USCALED__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Sscaled,      // BUF_FMT_8_8_8_8_SSCALED__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Uint,         // BUF_FMT_8_8_8_8_UINT__GFX104PLUS
    ChNumFormat::X8Y8Z8W8_Sint,         // BUF_FMT_8_8_8_8_SINT__GFX104PLUS
    ChNumFormat::X32Y32_Uint,           // BUF_FMT_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32_Sint,           // BUF_FMT_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32_Float,          // BUF_FMT_32_32_FLOAT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Unorm,    // BUF_FMT_16_16_16_16_UNORM__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Snorm,    // BUF_FMT_16_16_16_16_SNORM__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Uscaled,  // BUF_FMT_16_16_16_16_USCALED__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Sscaled,  // BUF_FMT_16_16_16_16_SSCALED__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Uint,     // BUF_FMT_16_16_16_16_UINT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Sint,     // BUF_FMT_16_16_16_16_SINT__GFX104PLUS
    ChNumFormat::X16Y16Z16W16_Float,    // BUF_FMT_16_16_16_16_FLOAT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Uint,        // BUF_FMT_32_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Sint,        // BUF_FMT_32_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32Z32_Float,       // BUF_FMT_32_32_32_FLOAT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Uint,     // BUF_FMT_32_32_32_32_UINT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Sint,     // BUF_FMT_32_32_32_32_SINT__GFX104PLUS
    ChNumFormat::X32Y32Z32W32_Float,    // BUF_FMT_32_32_32_32_FLOAT__GFX104PLUS
};

constexpr uint32 Gfx11MergedBufDataFmtCount = sizeof(Gfx11MergedBufDataFmtTbl) / sizeof(ChNumFormat);
#endif

} // Gfx9
} // Formats
} // Pal
