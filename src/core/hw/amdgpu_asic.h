/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef _AMDGPU_ASIC_H
#define _AMDGPU_ASIC_H

#define ATI_VENDOR_ID         0x1002
#define AMD_VENDOR_ID         0x1022

// AMDGPU_VENDOR_IS_AMD(vendorId)
#define AMDGPU_VENDOR_IS_AMD(v) ((v == ATI_VENDOR_ID) || (v == AMD_VENDOR_ID))

#define FAMILY_UNKNOWN 0x00
#define FAMILY_TN      0x69
#define FAMILY_SI      0x6E
#define FAMILY_CI      0x78
#define FAMILY_KV      0x7D
#define FAMILY_VI      0x82
#define FAMILY_POLARIS 0x82
#define FAMILY_CZ      0x87
#if PAL_BUILD_GFX9
#define FAMILY_AI      0x8D
#endif
#define FAMILY_RV      0x8E

// AMDGPU_FAMILY_IS(familyId, familyName)
#define FAMILY_IS(f, fn)     (f == FAMILY_##fn)
#define FAMILY_IS_TN(f)      FAMILY_IS(f, TN)
#define FAMILY_IS_SI(f)      FAMILY_IS(f, SI)
#define FAMILY_IS_CI(f)      FAMILY_IS(f, CI)
#define FAMILY_IS_KV(f)      FAMILY_IS(f, KV)
#define FAMILY_IS_VI(f)      FAMILY_IS(f, VI)
#define FAMILY_IS_POLARIS(f) FAMILY_IS(f, POLARIS)
#define FAMILY_IS_CZ(f)      FAMILY_IS(f, CZ)
#if PAL_BUILD_GFX9
#define FAMILY_IS_AI(f)      FAMILY_IS(f, AI)
#endif
#define FAMILY_IS_RV(f)      FAMILY_IS(f, RV)

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

#if PAL_BUILD_GFX9
#define AMDGPU_VEGA10_RANGE     0x01, 0x14
#define AMDGPU_VEGA12_RANGE     0x14, 0x28
#endif

#define AMDGPU_RAVEN_RANGE      0x01, 0x81

#define AMDGPU_EXPAND_FIX(x) x
#define AMDGPU_RANGE_HELPER(val, min, max) ((val >= min) && (val < max))
#define AMDGPU_IN_RANGE(val, ...)   AMDGPU_EXPAND_FIX(AMDGPU_RANGE_HELPER(val, __VA_ARGS__))

// ASICREV_IS(eRevisionId, revisionName)
#define ASICREV_IS(r, rn)              AMDGPU_IN_RANGE(r, AMDGPU_##rn##_RANGE)
#define ASICREV_IS_TAHITI_P(r)         ASICREV_IS(r, TAHITI)
#define ASICREV_IS_PITCAIRN_PM(r)      ASICREV_IS(r, PITCAIRN)
#define ASICREV_IS_CAPEVERDE_M(r)      ASICREV_IS(r, CAPEVERDE)
#define ASICREV_IS_OLAND_M(r)          ASICREV_IS(r, OLAND)
#define ASICREV_IS_HAINAN_V(r)         ASICREV_IS(r, HAINAN)

#define ASICREV_IS_BONAIRE_M(r)        ASICREV_IS(r, BONAIRE)
#define ASICREV_IS_HAWAII_P(r)         ASICREV_IS(r, HAWAII)

#define ASICREV_IS_SPECTRE(r)          ASICREV_IS(r, SPECTRE)
#define ASICREV_IS_SPOOKY(r)           ASICREV_IS(r, SPOOKY)
#define ASICREV_IS_KALINDI(r)          ASICREV_IS(r, KALINDI)
#define ASICREV_IS_KALINDI_GODAVARI(r) ASICREV_IS(r, GODAVARI)

#define ASICREV_IS_ICELAND_M(r)        ASICREV_IS(r, ICELAND)
#define ASICREV_IS_TONGA_P(r)          ASICREV_IS(r, TONGA)
#define ASICREV_IS_FIJI_P(r)           ASICREV_IS(r, FIJI)

#define ASICREV_IS_POLARIS10_P(r)      ASICREV_IS(r, POLARIS10)
#define ASICREV_IS_POLARIS11_M(r)      ASICREV_IS(r, POLARIS11)
#define ASICREV_IS_POLARIS12_V(r)      ASICREV_IS(r, POLARIS12)

#define ASICREV_IS_CARRIZO(r)          ASICREV_IS(r, CARRIZO)
#define ASICREV_IS_CARRIZO_BRISTOL(r)  ASICREV_IS(r, BRISTOL)
#define ASICREV_IS_STONEY(r)           ASICREV_IS(r, STONEY)

#if PAL_BUILD_GFX9
#define ASICREV_IS_VEGA10_M(r)         ASICREV_IS(r, VEGA10)
#define ASICREV_IS_VEGA10_P(r)         ASICREV_IS(r, VEGA10)
#define ASICREV_IS_VEGA12_P(r)         ASICREV_IS(r, VEGA12)
#define ASICREV_IS_VEGA12_p(r)         ASICREV_IS(r, VEGA12)
#endif

#define ASICREV_IS_RAVEN(r)            ASICREV_IS(r, RAVEN)

// AMDGPU_IS(familyId, eRevisionId, familyName, revisionName)
#define AMDGPU_IS(f, r, fn, rn)    (FAMILY_IS(f, fn) && ASICREV_IS(r, rn))
#define AMDGPU_IS_TAHITI(f, r)     AMDGPU_IS(f, r, SI, TAHITI)
#define AMDGPU_IS_PITCAIRN(f, r)   AMDGPU_IS(f, r, SI, PITCAIRN)
#define AMDGPU_IS_CAPEVERDE(f, r)  AMDGPU_IS(f, r, SI, CAPEVERDE)
#define AMDGPU_IS_OLAND(f, r)      AMDGPU_IS(f, r, SI, OLAND)
#define AMDGPU_IS_HAINAN(f, r)     AMDGPU_IS(f, r, SI, HAINAN)

#define AMDGPU_IS_BONAIRE(f, r)    AMDGPU_IS(f, r, CI, BONAIRE)
#define AMDGPU_IS_HAWAII(f, r)     AMDGPU_IS(f, r, CI, HAWAII)

#define AMDGPU_IS_SPECTRE(f, r)    AMDGPU_IS(f, r, KV, SPECTRE)
#define AMDGPU_IS_SPOOKY(f, r)     AMDGPU_IS(f, r, KV, SPOOKY)
#define AMDGPU_IS_KALINDI(f, r)    AMDGPU_IS(f, r, KV, KALINDI)
#define AMDGPU_IS_GODAVARI(f, r)   AMDGPU_IS(f, r, KV, GODAVARI)

#define AMDGPU_IS_ICELAND(f, r)    AMDGPU_IS(f, r, VI, ICELAND)
#define AMDGPU_IS_TONGA(f, r)      AMDGPU_IS(f, r, VI, TONGA)
#define AMDGPU_IS_FIJI(f, r)       AMDGPU_IS(f, r, VI, FIJI)

#define AMDGPU_IS_POLARIS10(f, r)  AMDGPU_IS(f, r, POLARIS, POLARIS10)
#define AMDGPU_IS_POLARIS11(f, r)  AMDGPU_IS(f, r, POLARIS, POLARIS11)
#define AMDGPU_IS_POLARIS12(f, r)  AMDGPU_IS(f, r, POLARIS, POLARIS12)

#define AMDGPU_IS_CARRIZO(f, r)    AMDGPU_IS(f, r, CZ, CARRIZO)
#define AMDGPU_IS_BRISTOL(f, r)    AMDGPU_IS(f, r, CZ, BRISTOL)
#define AMDGPU_IS_STONEY(f, r)     AMDGPU_IS(f, r, CZ, STONEY)

#if PAL_BUILD_GFX9
#define AMDGPU_IS_VEGA10(f, r)     AMDGPU_IS(f, r, AI, VEGA10)
#define AMDGPU_IS_VEGA12(f, r)     AMDGPU_IS(f, r, AI, VEGA12)
#endif

#define AMDGPU_IS_RAVEN(f, r)      AMDGPU_IS(f, r, RV, RAVEN)

// Device IDs
#define DEVICE_ID_SI_TAHITI_P_6780      0x6780
#define DEVICE_ID_SI_PITCAIRN_PM_6818   0x6818
#define DEVICE_ID_SI_CAPEVERDE_M_683D   0x683D
#define DEVICE_ID_SI_OLAND_M_6611       0x6611
#define DEVICE_ID_SI_HAINAN_V_6660      0x6660

#define DEVICE_ID_CI_BONAIRE_M_6640     0x6640
#define DEVICE_ID_CI_HAWAII_P_67BE      0x67BE
#define DEVICE_ID_SPECTRE_DESKTOP_130F  0x130F
#define DEVICE_ID_SPOOKY_DESKTOP_1316   0x1316
#define DEVICE_ID_KALINDI__9830         0x9830
#define DEVICE_ID_KV_GODAVARI__9850     0x9850

#define DEVICE_ID_VI_ICELAND_M_6900     0x6900
#define DEVICE_ID_CZ_CARRIZO_9870       0x9870
#define DEVICE_ID_CZ_BRISTOL_9874       0x9874
#define DEVICE_ID_VI_TONGA_P_6920       0x6920
#define DEVICE_ID_VI_FIJI_P_7300        0x7300
#define DEVICE_ID_VI_POLARIS10_P_67DF   0x67DF
#define DEVICE_ID_VI_POLARIS11_M_67EF   0x67EF
#define DEVICE_ID_VI_POLARIS12_V_699F   0x699F
#define DEVICE_ID_ST_98E4               0x98E4

#if PAL_BUILD_GFX9
#define DEVICE_ID_AI_VEGA10_P_6860      0x6860
#define DEVICE_ID_RV_15DD               0x15DD
#define DEVICE_ID_AI_VEGA12_P_69A0      0x69A0
#endif

// Revision IDs
#define SI_TAHITI_P_A21              5
#define SI_PITCAIRN_PM_A12          21
#define SI_CAPEVERDE_M_A12          41
#define SI_OLAND_M_A0               60
#define SI_HAINAN_V_A0              70

#define CI_BONAIRE_M_A0             20
#define CI_HAWAII_P_A0              40
#define KV_SPECTRE_A0             0x01
#define KV_SPOOKY_A0              0x41
#define KV_KALINDI_A0             0x81
#define KV_GODAVARI_A0            0xa1

#define VI_ICELAND_M_A0              1
#define CZ_CARRIZO_A0                1
#define CZ_BISTROL_A0             0x10
#define VI_TONGA_P_A1               21
#define VI_FIJI_P_A0                60
#define VI_POLARIS10_P_A0           80
#define VI_POLARIS11_M_A0           90
#define VI_POLARIS12_V_A0          100
#define CZ_STONEY_A0              0x61

#if PAL_BUILD_GFX9
#define AI_VEGA10_P_A0               1
#define RAVEN_A0                     1
#define AI_VEGA12_P_A0              20
#endif

// PRIDs
#define PRID_SI_TAHITI              0x00
#define PRID_SI_PITCAIRN            0x00
#define PRID_SI_CAPEVERDE           0x00
#define PRID_SI_OLAND_87            0x87
#define PRID_SI_HAINAN_EXO_81       0x81

#define PRID_CI_BONAIRE_TOBAGO_81   0x81
#define PRID_CI_HAWAII_80           0x80
#define PRID_KV_SPECTRE_GODAVARI_D4 0xD4
#define PRID_KV_SPOOKY              0x00
#define PRID_KV_KALINDI_00          0x00
#define PRID_GODAVARI_MULLINS_01    0x01

#define PRID_VI_ICELAND_MESO_81     0x81
#define PRID_CZ_CARRIZO_C4          0xC4
#define PRID_CZ_BRISTOL_E1          0xE1
#define PRID_VI_TONGA_00            0x00
#define PRID_VI_FIJI_CC             0xCC
#define PRID_VI_POLARIS10_C7        0xC7
#define PRID_VI_POLARIS11_CF        0xCF
#define PRID_VI_POLARIS12_C7        0xC7
#define PRID_ST_80                  0x80

#if PAL_BUILD_GFX9
#define PRID_AI_VEGA10_C3           0xC3
#define PRID_RV_81                  0x81
#define PRID_AI_VEGA12_00           0x00
#endif

#endif // _AMDGPU_ASIC_H
