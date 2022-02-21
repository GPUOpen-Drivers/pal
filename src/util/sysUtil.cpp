/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palSysUtil.h"

namespace Util
{
/// Defines for AMD

/// Defines for Processor Signature
static constexpr uint32 AmdProcessorReserved2        = 0xf0000000;    ///< Bits 31 - 28 of processor signature
static constexpr uint32 AmdProcessorExtendedFamily   = 0x0ff00000;    ///< Bits 27 - 20 of processor signature
static constexpr uint32 AmdProcessorExtendedModel    = 0x000f0000;    ///< Bits 16 - 19 of processor signature
static constexpr uint32 AmdProcessorReserved1        = 0x0000f000;    ///< Bits 15 - 12 of processor signature
static constexpr uint32 AmdProcessorFamily           = 0x00000f00;    ///< Bits 11 -  8 of processor signature
static constexpr uint32 AmdProcessorModel            = 0x000000f0;    ///< Bits  7 -  4 of processor signature
static constexpr uint32 AmdProcessorStepping         = 0x0000000f;    ///< Bits  3 -  0 of processor signature

/// Defines for Instruction Family
static constexpr uint32 AmdInstructionFamily5        = 0x5;           ///< Instruction family 5
static constexpr uint32 AmdInstructionFamily6        = 0x6;           ///< Instruction family 6
static constexpr uint32 AmdInstructionFamilyF        = 0xF;           ///< Instruction family F

/// Defines for Intel

/// Defines for Processor Signature
static constexpr uint32 IntelProcessorExtendedFamily = 0x0ff00000;    ///< Bits 27 - 20 of processor signature
static constexpr uint32 IntelProcessorType           = 0x00003000;    ///< Bits 13 - 12 of processor signature
static constexpr uint32 IntelProcessorFamily         = 0x00000f00;    ///< Bits 11 -  8 of processor signature
static constexpr uint32 IntelProcessorModel          = 0x000000f0;    ///< Bits  7 -  4 of processor signature
static constexpr uint32 IntelProcessorStepping       = 0x0000000f;    ///< Bits  3 -  0 of processor signature

/// Defines for Family
static constexpr uint32 IntelPentiumFamily           = 0x5;           ///< Any Pentium
static constexpr uint32 IntelP6ArchitectureFamily    = 0x6;           ///< P-III, and some Celeron's
static constexpr uint32 IntelPentium4Family          = 0xF;           ///< Pentium4, Pentium4-M, and some Celeron's

// =====================================================================================================================
// Query cpu type for AMD processor
void QueryAMDCpuType(
    SystemInfo* pSystemInfo)
{
#if PAL_HAS_CPUID
    uint32 reg[4] = {};

    CpuId(reg, 1);

    uint32 model  = (reg[0] & AmdProcessorModel) >> 0;
    uint32 family = (reg[0] & AmdProcessorFamily) >> 8;
    uint32 extendedFamily = (reg[0] & AmdProcessorExtendedFamily) >> 20;

    switch (family)
    {
        case AmdInstructionFamily5:
            if (model <= 3)
            {
                pSystemInfo->cpuType = CpuType::AmdK5;
            }
            else if (model <= 7)
            {
                pSystemInfo->cpuType = CpuType::AmdK6;
            }
            else if (model == 8)
            {
                pSystemInfo->cpuType = CpuType::AmdK6_2;
            }
            else if (model <= 15)
            {
                pSystemInfo->cpuType = CpuType::AmdK6_3;
            }
            else
            {
                pSystemInfo->cpuType = CpuType::Unknown;
            }
            break;
        case AmdInstructionFamily6: // Athlon
            // All Athlons and Durons fall into this group
            pSystemInfo->cpuType = CpuType::AmdK7;
            break;
        case AmdInstructionFamilyF: // For family F, we must check extended family
            switch (extendedFamily)
            {
                case 0:
                    // All Athlon64s and Opterons fall into this group
                    pSystemInfo->cpuType = CpuType::AmdK8;
                    break;
                case 1:
                    // 'Family 10h' aka K10 aka Barcelona, Phenom, Greyhound
                    pSystemInfo->cpuType = CpuType::AmdK10;
                    break;
                case 3:
                    // Family 12h - Llano
                    pSystemInfo->cpuType = CpuType::AmdFamily12h;
                    break;
                case 5:
                    // ExtFamilyID of '5' for Bobcat as read from ASIC
                    pSystemInfo->cpuType = CpuType::AmdBobcat;
                    break;
                case 6:
                    // Family 15h - Orochi, Trinity, Komodo, Kaveri, Basilisk
                    pSystemInfo->cpuType = CpuType::AmdFamily15h;
                    break;
                case 7:
                    // Family 16h - Kabini
                    pSystemInfo->cpuType = CpuType::AmdFamily16h;
                    break;
                case 8:
                case 10:
                    // Family 17h and Family 19h - Ryzen
                    pSystemInfo->cpuType = CpuType::AmdRyzen;
                    break;
                default:
                    pSystemInfo->cpuType = CpuType::Unknown;
                    break;
            }
            break;
        default:
            pSystemInfo->cpuType = CpuType::Unknown;
            break;
    }
#else
    pSystemInfo->cpuType = CpuType::Unknown;
#endif
}

// =====================================================================================================================
// Query cpu type for Intel processor
void QueryIntelCpuType(
    SystemInfo* pSystemInfo)
{
#if PAL_HAS_CPUID
    uint32 reg[4] = {};

    CpuId(reg, 1);

    uint32 model  = (reg[0] & IntelProcessorModel) >> 0;
    uint32 family = (reg[0] & IntelProcessorFamily) >> 8;
    uint32 extendedFamily = (reg[0] & IntelProcessorExtendedFamily) >> 20;

    switch (family)
    {
        case IntelPentiumFamily:
            pSystemInfo->cpuType = CpuType::IntelOld;
            break;
        case IntelP6ArchitectureFamily:
            {
                switch (model)
                {
                    case 1:
                    case 3:
                    case 5:
                    case 6:
                        pSystemInfo->cpuType = CpuType::IntelOld;
                        break;
                    case 7:
                    case 8:
                    case 11:
                        pSystemInfo->cpuType = CpuType::IntelP3;
                        break;
                    case 9:
                        pSystemInfo->cpuType = CpuType::IntelPMModel9;
                        break;
                    case 13:
                        pSystemInfo->cpuType = CpuType::IntelPMModelD;
                        break;
                    case 14:
                        pSystemInfo->cpuType = CpuType::IntelPMModelE;
                        break;
                    case 15:
                        pSystemInfo->cpuType = CpuType::IntelCoreModelF;
                        break;
                    default:
                        pSystemInfo->cpuType = CpuType::Unknown;
                        break;
                }
            }
            break;
        case IntelPentium4Family:
            // When the family is IntelPentium4, must check the extended family
            switch (extendedFamily)
            {
                case 0:
                    pSystemInfo->cpuType = CpuType::IntelP4;
                    break;
                default:
                    pSystemInfo->cpuType = CpuType::Unknown;
                    break;
            }
            break;
        default:
            pSystemInfo->cpuType = CpuType::Unknown;
            break;
    }
#else
    pSystemInfo->cpuType = CpuType::Unknown;
#endif
}

} // Util
