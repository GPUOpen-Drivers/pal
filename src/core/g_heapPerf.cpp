/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\generate directory
// OR the perf logs in the
// ../../src/core/hw/heapPerf directory.
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////

// =====================================================================================================================
// Contains the implementation for SettingLoader::SetupHeapPerfRatings.

#include "core/settingsLoader.h"
#include "core/device.h"
#include "palInlineFuncs.h"

namespace Pal {

// =====================================================================================================================
// Sets the heap performance ratings based on baked-in values.
void SettingsLoader::SetupHeapPerfRatings(
    PalSettings* pSettings)
{
    if (IsBonaire(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.0092f;
        pSettings->gpuWritePerfForLocal = 57.f;
        pSettings->gpuReadPerfForLocal = 86.f;
        pSettings->gpuWritePerfForInvisible = 57.f;
        pSettings->gpuReadPerfForInvisible = 85.f;
        pSettings->cpuWritePerfForGartUswc = 6.2f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 2.8f;
        pSettings->gpuReadPerfForGartUswc = 9.4f;
        pSettings->cpuWritePerfForGartCacheable = 6.2f;
        pSettings->cpuReadPerfForGartCacheable = 6.1f;
        pSettings->gpuWritePerfForGartCacheable = 2.8f;
        pSettings->gpuReadPerfForGartCacheable = 9.4f;
    }
    else if (IsCapeVerde(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.021f;
        pSettings->gpuWritePerfForLocal = 37.f;
        pSettings->gpuReadPerfForLocal = 63.f;
        pSettings->gpuWritePerfForInvisible = 37.f;
        pSettings->gpuReadPerfForInvisible = 63.f;
        pSettings->cpuWritePerfForGartUswc = 3.4f;
        pSettings->cpuReadPerfForGartUswc = 0.068f;
        pSettings->gpuWritePerfForGartUswc = 3.8f;
        pSettings->gpuReadPerfForGartUswc = 9.4f;
        pSettings->cpuWritePerfForGartCacheable = 5.4f;
        pSettings->cpuReadPerfForGartCacheable = 5.3f;
        pSettings->gpuWritePerfForGartCacheable = 2.6f;
        pSettings->gpuReadPerfForGartCacheable = 9.4f;
    }
    else if (IsCarrizo(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.2f;
        pSettings->cpuReadPerfForLocal = 0.017f;
        pSettings->gpuWritePerfForLocal = 10.f;
        pSettings->gpuReadPerfForLocal = 11.f;
        pSettings->gpuWritePerfForInvisible = 10.f;
        pSettings->gpuReadPerfForInvisible = 11.f;
        pSettings->cpuWritePerfForGartUswc = 4.f;
        pSettings->cpuReadPerfForGartUswc = 0.05f;
        pSettings->gpuWritePerfForGartUswc = 10.f;
        pSettings->gpuReadPerfForGartUswc = 11.f;
        pSettings->cpuWritePerfForGartCacheable = 3.9f;
        pSettings->cpuReadPerfForGartCacheable = 3.9f;
        pSettings->gpuWritePerfForGartCacheable = 10.f;
        pSettings->gpuReadPerfForGartCacheable = 11.f;
    }
    else if (IsFiji(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.019f;
        pSettings->gpuWritePerfForLocal = 430.f;
        pSettings->gpuReadPerfForLocal = 490.f;
        pSettings->gpuWritePerfForInvisible = 450.f;
        pSettings->gpuReadPerfForInvisible = 490.f;
        pSettings->cpuWritePerfForGartUswc = 6.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 6.7f;
        pSettings->gpuReadPerfForGartUswc = 9.7f;
        pSettings->cpuWritePerfForGartCacheable = 7.8f;
        pSettings->cpuReadPerfForGartCacheable = 7.8f;
        pSettings->gpuWritePerfForGartCacheable = 5.9f;
        pSettings->gpuReadPerfForGartCacheable = 9.7f;
    }
    else if (IsGodavari(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 1.8f;
        pSettings->cpuReadPerfForLocal = 0.0083f;
        pSettings->gpuWritePerfForLocal = 7.6f;
        pSettings->gpuReadPerfForLocal = 7.4f;
        pSettings->gpuWritePerfForInvisible = 7.3f;
        pSettings->gpuReadPerfForInvisible = 7.4f;
        pSettings->cpuWritePerfForGartUswc = 1.8f;
        pSettings->cpuReadPerfForGartUswc = 0.03f;
        pSettings->gpuWritePerfForGartUswc = 6.1f;
        pSettings->gpuReadPerfForGartUswc = 7.2f;
        pSettings->cpuWritePerfForGartCacheable = 1.8f;
        pSettings->cpuReadPerfForGartCacheable = 1.8f;
        pSettings->gpuWritePerfForGartCacheable = 3.6f;
        pSettings->gpuReadPerfForGartCacheable = 6.6f;
    }
    else if (IsHainan(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.9f;
        pSettings->cpuReadPerfForLocal = 0.02f;
        pSettings->gpuWritePerfForLocal = 25.f;
        pSettings->gpuReadPerfForLocal = 28.f;
        pSettings->gpuWritePerfForInvisible = 25.f;
        pSettings->gpuReadPerfForInvisible = 28.f;
        pSettings->cpuWritePerfForGartUswc = 3.1f;
        pSettings->cpuReadPerfForGartUswc = 0.071f;
        pSettings->gpuWritePerfForGartUswc = 3.8f;
        pSettings->gpuReadPerfForGartUswc = 4.8f;
        pSettings->cpuWritePerfForGartCacheable = 5.4f;
        pSettings->cpuReadPerfForGartCacheable = 5.3f;
        pSettings->gpuWritePerfForGartCacheable = 3.8f;
        pSettings->gpuReadPerfForGartCacheable = 4.8f;
    }
    else if (IsHawaii(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.0092f;
        pSettings->gpuWritePerfForLocal = 230.f;
        pSettings->gpuReadPerfForLocal = 280.f;
        pSettings->gpuWritePerfForInvisible = 240.f;
        pSettings->gpuReadPerfForInvisible = 280.f;
        pSettings->cpuWritePerfForGartUswc = 3.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 2.5f;
        pSettings->gpuReadPerfForGartUswc = 9.9f;
        pSettings->cpuWritePerfForGartCacheable = 6.2f;
        pSettings->cpuReadPerfForGartCacheable = 6.1f;
        pSettings->gpuWritePerfForGartCacheable = 2.5f;
        pSettings->gpuReadPerfForGartCacheable = 9.8f;
    }
    else if (IsIceland(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 3.8f;
        pSettings->cpuReadPerfForLocal = 0.022f;
        pSettings->gpuWritePerfForLocal = 4.5f;
        pSettings->gpuReadPerfForLocal = 5.6f;
        pSettings->gpuWritePerfForInvisible = 4.4f;
        pSettings->gpuReadPerfForInvisible = 5.6f;
        pSettings->cpuWritePerfForGartUswc = 4.2f;
        pSettings->cpuReadPerfForGartUswc = 0.065f;
        pSettings->gpuWritePerfForGartUswc = 4.4f;
        pSettings->gpuReadPerfForGartUswc = 5.6f;
        pSettings->cpuWritePerfForGartCacheable = 4.2f;
        pSettings->cpuReadPerfForGartCacheable = 4.2f;
        pSettings->gpuWritePerfForGartCacheable = 2.1f;
        pSettings->gpuReadPerfForGartCacheable = 3.9f;
    }
    else if (IsKalindi(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 1.8f;
        pSettings->cpuReadPerfForLocal = 0.0083f;
        pSettings->gpuWritePerfForLocal = 7.6f;
        pSettings->gpuReadPerfForLocal = 7.4f;
        pSettings->gpuWritePerfForInvisible = 7.3f;
        pSettings->gpuReadPerfForInvisible = 7.4f;
        pSettings->cpuWritePerfForGartUswc = 1.8f;
        pSettings->cpuReadPerfForGartUswc = 0.03f;
        pSettings->gpuWritePerfForGartUswc = 6.1f;
        pSettings->gpuReadPerfForGartUswc = 7.2f;
        pSettings->cpuWritePerfForGartCacheable = 1.8f;
        pSettings->cpuReadPerfForGartCacheable = 1.8f;
        pSettings->gpuWritePerfForGartCacheable = 3.6f;
        pSettings->gpuReadPerfForGartCacheable = 6.6f;
    }
    else
    if (IsNavi10(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.019f;
        pSettings->gpuWritePerfForLocal = 430.f;
        pSettings->gpuReadPerfForLocal = 490.f;
        pSettings->gpuWritePerfForInvisible = 450.f;
        pSettings->gpuReadPerfForInvisible = 490.f;
        pSettings->cpuWritePerfForGartUswc = 6.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 6.7f;
        pSettings->gpuReadPerfForGartUswc = 9.7f;
        pSettings->cpuWritePerfForGartCacheable = 7.8f;
        pSettings->cpuReadPerfForGartCacheable = 7.8f;
        pSettings->gpuWritePerfForGartCacheable = 5.9f;
        pSettings->gpuReadPerfForGartCacheable = 9.7f;
    }
    else
    if (IsOland(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.9f;
        pSettings->cpuReadPerfForLocal = 0.02f;
        pSettings->gpuWritePerfForLocal = 25.f;
        pSettings->gpuReadPerfForLocal = 28.f;
        pSettings->gpuWritePerfForInvisible = 25.f;
        pSettings->gpuReadPerfForInvisible = 28.f;
        pSettings->cpuWritePerfForGartUswc = 3.1f;
        pSettings->cpuReadPerfForGartUswc = 0.071f;
        pSettings->gpuWritePerfForGartUswc = 3.8f;
        pSettings->gpuReadPerfForGartUswc = 4.8f;
        pSettings->cpuWritePerfForGartCacheable = 5.4f;
        pSettings->cpuReadPerfForGartCacheable = 5.3f;
        pSettings->gpuWritePerfForGartCacheable = 3.8f;
        pSettings->gpuReadPerfForGartCacheable = 4.8f;
    }
    else if (IsPitcairn(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.02f;
        pSettings->gpuWritePerfForLocal = 85.f;
        pSettings->gpuReadPerfForLocal = 130.f;
        pSettings->gpuWritePerfForInvisible = 84.f;
        pSettings->gpuReadPerfForInvisible = 130.f;
        pSettings->cpuWritePerfForGartUswc = 6.6f;
        pSettings->cpuReadPerfForGartUswc = 0.063f;
        pSettings->gpuWritePerfForGartUswc = 4.f;
        pSettings->gpuReadPerfForGartUswc = 9.4f;
        pSettings->cpuWritePerfForGartCacheable = 6.6f;
        pSettings->cpuReadPerfForGartCacheable = 6.6f;
        pSettings->gpuWritePerfForGartCacheable = 3.9f;
        pSettings->gpuReadPerfForGartCacheable = 9.4f;
    }
    else if (IsPolaris10(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 5.7f;
        pSettings->cpuReadPerfForLocal = 0.016f;
        pSettings->gpuWritePerfForLocal = 190.f;
        pSettings->gpuReadPerfForLocal = 210.f;
        pSettings->gpuWritePerfForInvisible = 190.f;
        pSettings->gpuReadPerfForInvisible = 210.f;
        pSettings->cpuWritePerfForGartUswc = 4.5f;
        pSettings->cpuReadPerfForGartUswc = 0.066f;
        pSettings->gpuWritePerfForGartUswc = 8.1f;
        pSettings->gpuReadPerfForGartUswc = 7.9f;
        pSettings->cpuWritePerfForGartCacheable = 7.5f;
        pSettings->cpuReadPerfForGartCacheable = 7.5f;
        pSettings->gpuWritePerfForGartCacheable = 8.f;
        pSettings->gpuReadPerfForGartCacheable = 7.9f;
    }
    else if (IsPolaris11(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.9f;
        pSettings->cpuReadPerfForLocal = 0.015f;
        pSettings->gpuWritePerfForLocal = 90.f;
        pSettings->gpuReadPerfForLocal = 96.f;
        pSettings->gpuWritePerfForInvisible = 90.f;
        pSettings->gpuReadPerfForInvisible = 96.f;
        pSettings->cpuWritePerfForGartUswc = 10.f;
        pSettings->cpuReadPerfForGartUswc = 0.066f;
        pSettings->gpuWritePerfForGartUswc = 5.2f;
        pSettings->gpuReadPerfForGartUswc = 4.8f;
        pSettings->cpuWritePerfForGartCacheable = 4.5f;
        pSettings->cpuReadPerfForGartCacheable = 4.4f;
        pSettings->gpuWritePerfForGartCacheable = 5.2f;
        pSettings->gpuReadPerfForGartCacheable = 4.8f;
    }
    else if (IsPolaris12(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.9f;
        pSettings->cpuReadPerfForLocal = 0.015f;
        pSettings->gpuWritePerfForLocal = 90.f;
        pSettings->gpuReadPerfForLocal = 96.f;
        pSettings->gpuWritePerfForInvisible = 90.f;
        pSettings->gpuReadPerfForInvisible = 96.f;
        pSettings->cpuWritePerfForGartUswc = 10.f;
        pSettings->cpuReadPerfForGartUswc = 0.066f;
        pSettings->gpuWritePerfForGartUswc = 5.2f;
        pSettings->gpuReadPerfForGartUswc = 4.8f;
        pSettings->cpuWritePerfForGartCacheable = 4.5f;
        pSettings->cpuReadPerfForGartCacheable = 4.4f;
        pSettings->gpuWritePerfForGartCacheable = 5.2f;
        pSettings->gpuReadPerfForGartCacheable = 4.8f;
    }
    else
    if (IsRaven(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.2f;
        pSettings->cpuReadPerfForLocal = 0.017f;
        pSettings->gpuWritePerfForLocal = 10.f;
        pSettings->gpuReadPerfForLocal = 11.f;
        pSettings->gpuWritePerfForInvisible = 10.f;
        pSettings->gpuReadPerfForInvisible = 11.f;
        pSettings->cpuWritePerfForGartUswc = 4.f;
        pSettings->cpuReadPerfForGartUswc = 0.05f;
        pSettings->gpuWritePerfForGartUswc = 10.f;
        pSettings->gpuReadPerfForGartUswc = 11.f;
        pSettings->cpuWritePerfForGartCacheable = 3.9f;
        pSettings->cpuReadPerfForGartCacheable = 3.9f;
        pSettings->gpuWritePerfForGartCacheable = 10.f;
        pSettings->gpuReadPerfForGartCacheable = 11.f;
    }
    else
    if (IsRaven2(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.2f;
        pSettings->cpuReadPerfForLocal = 0.017f;
        pSettings->gpuWritePerfForLocal = 10.f;
        pSettings->gpuReadPerfForLocal = 11.f;
        pSettings->gpuWritePerfForInvisible = 10.f;
        pSettings->gpuReadPerfForInvisible = 11.f;
        pSettings->cpuWritePerfForGartUswc = 4.f;
        pSettings->cpuReadPerfForGartUswc = 0.05f;
        pSettings->gpuWritePerfForGartUswc = 10.f;
        pSettings->gpuReadPerfForGartUswc = 11.f;
        pSettings->cpuWritePerfForGartCacheable = 3.9f;
        pSettings->cpuReadPerfForGartCacheable = 3.9f;
        pSettings->gpuWritePerfForGartCacheable = 10.f;
        pSettings->gpuReadPerfForGartCacheable = 11.f;
    }
    else
    if (IsSpectre(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 3.9f;
        pSettings->cpuReadPerfForLocal = 0.02f;
        pSettings->gpuWritePerfForLocal = 17.f;
        pSettings->gpuReadPerfForLocal = 21.f;
        pSettings->gpuWritePerfForInvisible = 16.f;
        pSettings->gpuReadPerfForInvisible = 21.f;
        pSettings->cpuWritePerfForGartUswc = 4.1f;
        pSettings->cpuReadPerfForGartUswc = 0.065f;
        pSettings->gpuWritePerfForGartUswc = 16.f;
        pSettings->gpuReadPerfForGartUswc = 21.f;
        pSettings->cpuWritePerfForGartCacheable = 4.2f;
        pSettings->cpuReadPerfForGartCacheable = 4.3f;
        pSettings->gpuWritePerfForGartCacheable = 7.8f;
        pSettings->gpuReadPerfForGartCacheable = 15.f;
    }
    else if (IsSpooky(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 3.9f;
        pSettings->cpuReadPerfForLocal = 0.02f;
        pSettings->gpuWritePerfForLocal = 17.f;
        pSettings->gpuReadPerfForLocal = 21.f;
        pSettings->gpuWritePerfForInvisible = 16.f;
        pSettings->gpuReadPerfForInvisible = 21.f;
        pSettings->cpuWritePerfForGartUswc = 4.1f;
        pSettings->cpuReadPerfForGartUswc = 0.065f;
        pSettings->gpuWritePerfForGartUswc = 16.f;
        pSettings->gpuReadPerfForGartUswc = 21.f;
        pSettings->cpuWritePerfForGartCacheable = 4.2f;
        pSettings->cpuReadPerfForGartCacheable = 4.3f;
        pSettings->gpuWritePerfForGartCacheable = 7.8f;
        pSettings->gpuReadPerfForGartCacheable = 15.f;
    }
    else if (IsStoney(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 3.8f;
        pSettings->cpuReadPerfForLocal = 0.011f;
        pSettings->gpuWritePerfForLocal = 8.9f;
        pSettings->gpuReadPerfForLocal = 11.f;
        pSettings->gpuWritePerfForInvisible = 8.9f;
        pSettings->gpuReadPerfForInvisible = 11.f;
        pSettings->cpuWritePerfForGartUswc = 3.8f;
        pSettings->cpuReadPerfForGartUswc = 0.055f;
        pSettings->gpuWritePerfForGartUswc = 8.9f;
        pSettings->gpuReadPerfForGartUswc = 11.f;
        pSettings->cpuWritePerfForGartCacheable = 3.9f;
        pSettings->cpuReadPerfForGartCacheable = 3.7f;
        pSettings->gpuWritePerfForGartCacheable = 8.9f;
        pSettings->gpuReadPerfForGartCacheable = 10.f;
    }
    else if (IsTahiti(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.6f;
        pSettings->cpuReadPerfForLocal = 0.021f;
        pSettings->gpuWritePerfForLocal = 120.f;
        pSettings->gpuReadPerfForLocal = 240.f;
        pSettings->gpuWritePerfForInvisible = 120.f;
        pSettings->gpuReadPerfForInvisible = 240.f;
        pSettings->cpuWritePerfForGartUswc = 3.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 1.6f;
        pSettings->gpuReadPerfForGartUswc = 9.5f;
        pSettings->cpuWritePerfForGartCacheable = 5.7f;
        pSettings->cpuReadPerfForGartCacheable = 5.6f;
        pSettings->gpuWritePerfForGartCacheable = 1.6f;
        pSettings->gpuReadPerfForGartCacheable = 9.6f;
    }
    else if (IsTonga(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.015f;
        pSettings->gpuWritePerfForLocal = 170.f;
        pSettings->gpuReadPerfForLocal = 200.f;
        pSettings->gpuWritePerfForInvisible = 170.f;
        pSettings->gpuReadPerfForInvisible = 210.f;
        pSettings->cpuWritePerfForGartUswc = 3.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 2.1f;
        pSettings->gpuReadPerfForGartUswc = 9.5f;
        pSettings->cpuWritePerfForGartCacheable = 6.6f;
        pSettings->cpuReadPerfForGartCacheable = 6.6f;
        pSettings->gpuWritePerfForGartCacheable = 2.f;
        pSettings->gpuReadPerfForGartCacheable = 9.5f;
    }
    else
    if (IsVega10(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.019f;
        pSettings->gpuWritePerfForLocal = 430.f;
        pSettings->gpuReadPerfForLocal = 490.f;
        pSettings->gpuWritePerfForInvisible = 450.f;
        pSettings->gpuReadPerfForInvisible = 490.f;
        pSettings->cpuWritePerfForGartUswc = 6.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 6.7f;
        pSettings->gpuReadPerfForGartUswc = 9.7f;
        pSettings->cpuWritePerfForGartCacheable = 7.8f;
        pSettings->cpuReadPerfForGartCacheable = 7.8f;
        pSettings->gpuWritePerfForGartCacheable = 5.9f;
        pSettings->gpuReadPerfForGartCacheable = 9.7f;
    }
    else if (IsVega12(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 4.9f;
        pSettings->cpuReadPerfForLocal = 0.015f;
        pSettings->gpuWritePerfForLocal = 90.f;
        pSettings->gpuReadPerfForLocal = 96.f;
        pSettings->gpuWritePerfForInvisible = 90.f;
        pSettings->gpuReadPerfForInvisible = 96.f;
        pSettings->cpuWritePerfForGartUswc = 10.f;
        pSettings->cpuReadPerfForGartUswc = 0.066f;
        pSettings->gpuWritePerfForGartUswc = 5.2f;
        pSettings->gpuReadPerfForGartUswc = 4.8f;
        pSettings->cpuWritePerfForGartCacheable = 4.5f;
        pSettings->cpuReadPerfForGartCacheable = 4.4f;
        pSettings->gpuWritePerfForGartCacheable = 5.2f;
        pSettings->gpuReadPerfForGartCacheable = 4.8f;
    }
    else
    if (IsVega20(*(static_cast<Pal::Device*>(m_pDevice))))
    {
        pSettings->cpuWritePerfForLocal = 6.6f;
        pSettings->cpuReadPerfForLocal = 0.019f;
        pSettings->gpuWritePerfForLocal = 430.f;
        pSettings->gpuReadPerfForLocal = 490.f;
        pSettings->gpuWritePerfForInvisible = 450.f;
        pSettings->gpuReadPerfForInvisible = 490.f;
        pSettings->cpuWritePerfForGartUswc = 6.1f;
        pSettings->cpuReadPerfForGartUswc = 0.07f;
        pSettings->gpuWritePerfForGartUswc = 6.7f;
        pSettings->gpuReadPerfForGartUswc = 9.7f;
        pSettings->cpuWritePerfForGartCacheable = 7.8f;
        pSettings->cpuReadPerfForGartCacheable = 7.8f;
        pSettings->gpuWritePerfForGartCacheable = 5.9f;
        pSettings->gpuReadPerfForGartCacheable = 9.7f;
    }
    else
    {
        PAL_NOT_IMPLEMENTED();
    }
}
} // Pal
