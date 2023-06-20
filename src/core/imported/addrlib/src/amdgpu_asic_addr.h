/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef _AMDGPU_ASIC_ADDR_H
#define _AMDGPU_ASIC_ADDR_H

#define ATI_VENDOR_ID         0x1002
#define AMD_VENDOR_ID         0x1022

// AMDGPU_VENDOR_IS_AMD(vendorId)
#define AMDGPU_VENDOR_IS_AMD(v) ((v == ATI_VENDOR_ID) || (v == AMD_VENDOR_ID))

#define FAMILY_UNKNOWN 0x00
#define FAMILY_TN      0x69
#define FAMILY_POLARIS 0x82
#if ADDR_GFX9_BUILD
#define FAMILY_AI      0x8D
#endif
#if ADDR_RAVEN1_BUILD
#define FAMILY_RV      0x8E
#endif
#define FAMILY_NV      0x8F
#if ADDR_GFX11_BUILD
#define FAMILY_NV3     0x91
#endif
#define FAMILY_RMB     0x92
#define FAMILY_RPL     0x95
#define FAMILY_MDN     0x97

// AMDGPU_FAMILY_IS(familyId, familyName)
#define FAMILY_IS(f, fn)     (f == FAMILY_##fn)
#define FAMILY_IS_TN(f)      FAMILY_IS(f, TN)
#define FAMILY_IS_POLARIS(f) FAMILY_IS(f, POLARIS)
#if ADDR_GFX9_BUILD
#define FAMILY_IS_AI(f)      FAMILY_IS(f, AI)
#endif
#if ADDR_RAVEN1_BUILD
#define FAMILY_IS_RV(f)      FAMILY_IS(f, RV)
#endif
#define FAMILY_IS_NV(f)      FAMILY_IS(f, NV)
#if ADDR_GFX11_BUILD
#define FAMILY_IS_NV3(f)     FAMILY_IS(f, NV3)
#endif
#define FAMILY_IS_RMB(f)     FAMILY_IS(f, RMB)

#define AMDGPU_UNKNOWN          0xFF

#define AMDGPU_TAHITI_RANGE     0x05, 0x14
#define AMDGPU_PITCAIRN_RANGE   0x15, 0x28
#define AMDGPU_CAPEVERDE_RANGE  0x29, 0x3C
#define AMDGPU_OLAND_RANGE      0x3C, 0x46
#define AMDGPU_HAINAN_RANGE     0x46, 0xFF

#define AMDGPU_BONAIRE_RANGE    0x14, 0x28
#define AMDGPU_HAWAII_RANGE     0x28, 0x3C

#define AMDGPU_SPECTRE_RANGE    0x01, 0x41
#define AMDGPU_SPOOKY_RANGE     0x41, 0x81
#define AMDGPU_KALINDI_RANGE    0x81, 0xA1
#define AMDGPU_GODAVARI_RANGE   0xA1, 0xFF

#define AMDGPU_ICELAND_RANGE    0x01, 0x14
#define AMDGPU_TONGA_RANGE      0x14, 0x28
#define AMDGPU_FIJI_RANGE       0x3C, 0x50

#define AMDGPU_POLARIS10_RANGE  0x50, 0x5A
#define AMDGPU_POLARIS11_RANGE  0x5A, 0x64
#define AMDGPU_POLARIS12_RANGE  0x64, 0x6E

#define AMDGPU_CARRIZO_RANGE    0x01, 0x21
#define AMDGPU_BRISTOL_RANGE    0x10, 0x21
#define AMDGPU_STONEY_RANGE     0x61, 0xFF

#if ADDR_GFX9_BUILD
#define AMDGPU_VEGA10_RANGE     0x01, 0x14
#endif
#if ADDR_VEGA12_BUILD
#define AMDGPU_VEGA12_RANGE     0x14, 0x28
#endif
#define AMDGPU_VEGA20_RANGE     0x28, 0xFF

#if ADDR_RAVEN1_BUILD
#define AMDGPU_RAVEN_RANGE      0x01, 0x81
#endif
#define AMDGPU_RAVEN2_RANGE     0x81, 0x90
#define AMDGPU_RENOIR_RANGE     0x91, 0xFF

#define AMDGPU_NAVI10_RANGE     0x01, 0x0A

#define AMDGPU_NAVI12_RANGE     0x0A, 0x14

#define AMDGPU_NAVI14_RANGE     0x14, 0x28

#define AMDGPU_NAVI21_RANGE     0x28, 0x32

#define AMDGPU_NAVI22_RANGE     0x32, 0x3C

#define AMDGPU_NAVI23_RANGE     0x3C, 0x46

#define AMDGPU_NAVI24_RANGE     0x46, 0x50

#if ADDR_GFX11_BUILD
#if ADDR_NAVI31_BUILD
#define AMDGPU_NAVI31_RANGE     0x01, 0x10
#endif
#if ADDR_NAVI33_BUILD
#define AMDGPU_NAVI33_RANGE     0x10, 0x20
#endif
#endif

#define AMDGPU_REMBRANDT_RANGE  0x01, 0xFF

#define AMDGPU_RAPHAEL_RANGE    0x01, 0xFF

#define AMDGPU_MENDOCINO_RANGE  0x01, 0xFF

#define AMDGPU_EXPAND_FIX(x) x
#define AMDGPU_RANGE_HELPER(val, min, max) ((val >= min) && (val < max))
#define AMDGPU_IN_RANGE(val, ...)   AMDGPU_EXPAND_FIX(AMDGPU_RANGE_HELPER(val, __VA_ARGS__))

// ASICREV_IS(eRevisionId, revisionName)
#define ASICREV_IS(r, rn)              AMDGPU_IN_RANGE(r, AMDGPU_##rn##_RANGE)
#define ASICREV_IS_KALINDI_GODAVARI(r) ASICREV_IS(r, GODAVARI)
#define ASICREV_IS_FIJI_P(r)           ASICREV_IS(r, FIJI)

#define ASICREV_IS_POLARIS10_P(r)      ASICREV_IS(r, POLARIS10)
#define ASICREV_IS_POLARIS11_M(r)      ASICREV_IS(r, POLARIS11)
#define ASICREV_IS_POLARIS12_V(r)      ASICREV_IS(r, POLARIS12)

#define ASICREV_IS_CARRIZO(r)          ASICREV_IS(r, CARRIZO)
#define ASICREV_IS_CARRIZO_BRISTOL(r)  ASICREV_IS(r, BRISTOL)
#define ASICREV_IS_STONEY(r)           ASICREV_IS(r, STONEY)

#if ADDR_GFX9_BUILD
#define ASICREV_IS_VEGA10_M(r)         ASICREV_IS(r, VEGA10)
#define ASICREV_IS_VEGA10_P(r)         ASICREV_IS(r, VEGA10)
#endif
#if ADDR_VEGA12_BUILD
#define ASICREV_IS_VEGA12_P(r)         ASICREV_IS(r, VEGA12)
#define ASICREV_IS_VEGA12_p(r)         ASICREV_IS(r, VEGA12)
#endif
#define ASICREV_IS_VEGA20_P(r)         ASICREV_IS(r, VEGA20)

#if ADDR_RAVEN1_BUILD
#define ASICREV_IS_RAVEN(r)            ASICREV_IS(r, RAVEN)
#endif
#define ASICREV_IS_RAVEN2(r)           ASICREV_IS(r, RAVEN2)
#define ASICREV_IS_RENOIR(r)           ASICREV_IS(r, RENOIR)

#define ASICREV_IS_NAVI10_P(r)         ASICREV_IS(r, NAVI10)

#define ASICREV_IS_NAVI12_P(r)         ASICREV_IS(r, NAVI12)

#define ASICREV_IS_NAVI14_M(r)         ASICREV_IS(r, NAVI14)

#define ASICREV_IS_NAVI21_M(r)         ASICREV_IS(r, NAVI21)

#define ASICREV_IS_NAVI22_P(r)         ASICREV_IS(r, NAVI22)

#define ASICREV_IS_NAVI23_P(r)         ASICREV_IS(r, NAVI23)

#define ASICREV_IS_NAVI24_P(r)         ASICREV_IS(r, NAVI24)

#if ADDR_GFX11_BUILD
#if ADDR_NAVI31_BUILD
#define ASICREV_IS_NAVI31_P(r)         ASICREV_IS(r, NAVI31)
#endif
#if ADDR_NAVI33_BUILD
#define ASICREV_IS_NAVI33_P(r)         ASICREV_IS(r, NAVI33)
#endif
#endif

#define ASICREV_IS_REMBRANDT(r)        ASICREV_IS(r, REMBRANDT)

#define ASICREV_IS_RAPHAEL(r)          ASICREV_IS(r, RAPHAEL)

#define ASICREV_IS_MENDOCINO(r)        ASICREV_IS(r, MENDOCINO)

#endif
