/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palPipelineAbiUtils.h
 * @brief PAL Pipeline ABI utilities.
 ***********************************************************************************************************************
 */

#pragma once

#include "palPipelineAbi.h"
#include "palInlineFuncs.h"

#include <climits>

namespace Util
{
namespace Abi
{

/// Helper function to get the GFXIP version when given a machine type.
///
/// @param [in] machineType The machine type.
/// @param [out] pGfxIpMajorVer The major version.
/// @param [out] pGfxIpMinorVer The minor version.
/// @param [out] pGfxIpStepping The stepping.
PAL_INLINE void MachineTypeToGfxIpVersion(
    AmdGpuMachineType machineType,
    uint32* pGfxIpMajorVer,
    uint32* pGfxIpMinorVer,
    uint32* pGfxIpStepping)
{
    switch (machineType)
    {
    case AmdGpuMachineType::Gfx600:
        *pGfxIpMajorVer = 6;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx601:
        *pGfxIpMajorVer = 6;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx602:
        *pGfxIpMajorVer = 6;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx700:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx701:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx702:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx703:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 3;
        break;
    case AmdGpuMachineType::Gfx704:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 4;
        break;
    case AmdGpuMachineType::Gfx705:
        *pGfxIpMajorVer = 7;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 5;
        break;
    case AmdGpuMachineType::Gfx800:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx801:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 1;
        break;
    case AmdGpuMachineType::Gfx802:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx803:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 3;
        break;
    case AmdGpuMachineType::Gfx805:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 5;
        break;
    case AmdGpuMachineType::Gfx810:
        *pGfxIpMajorVer = 8;
        *pGfxIpMinorVer = 1;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx900:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx902:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 2;
        break;
    case AmdGpuMachineType::Gfx904:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 4;
        break;
    case AmdGpuMachineType::Gfx906:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 6;
        break;
    case AmdGpuMachineType::Gfx909:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 9;
        break;
    case AmdGpuMachineType::Gfx90C:
        *pGfxIpMajorVer = 9;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 12;
        break;
    case AmdGpuMachineType::Gfx1010:
        *pGfxIpMajorVer = 10;
        *pGfxIpMinorVer = 1;
        *pGfxIpStepping = 0;
        break;
    case AmdGpuMachineType::Gfx1012:
        *pGfxIpMajorVer = 10;
        *pGfxIpMinorVer = 1;
        *pGfxIpStepping = 2;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        *pGfxIpMajorVer = 0;
        *pGfxIpMinorVer = 0;
        *pGfxIpStepping = 0;
        break;
    }
}

/// Helper function to parse the PalMetadata section of a pipeline ELF.
///
/// @param [in] pReader            The message pack reader.
/// @param [in] pDesc              The content of the metadata note section.
/// @param [in] descSize           The length of the note section.
/// @param [out] pMetadataMajorVer The major metadata version.
/// @param [out] pMetadataMinorVer The minor metadata version.
///
/// @returns Success if successful, ErrorInvalidValue, ErrorUnknown or ErrorInvalidPipelineElf if
///          the metadata could not be parsed.
PAL_INLINE Result GetPalMetadataVersion(
    MsgPackReader* pReader,
    const void*    pDesc,
    uint32         descSize,
    uint32*        pMetadataMajorVer,
    uint32*        pMetadataMinorVer)
{
    // We need to retrieve version info from the msgpack blob.
    Result result = pReader->InitFromBuffer(pDesc, descSize);

    if ((result == Result::Success) && (pReader->Type() != CWP_ITEM_MAP))
    {
        result = Result::ErrorInvalidPipelineElf;
    }

    for (uint32 j = pReader->Get().as.map.size; ((result == Result::Success) && (j > 0)); --j)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const auto&  str     = pReader->Get().as.str;
            const uint32 keyHash = HashString(static_cast<const char*>(str.start), str.length);
            if (keyHash == HashLiteralString(PalCodeObjectMetadataKey::Version))
            {
                result = pReader->Next(CWP_ITEM_ARRAY);
                if ((result == Result::Success) && (pReader->Get().as.array.size >= 2))
                {
                    result = pReader->UnpackNext(pMetadataMajorVer);
                }
                if (result == Result::Success)
                {
                    result = pReader->UnpackNext(pMetadataMinorVer);
                }
                break;
            }
            else
            {
                // Ideally, the version is the first field written so we don't reach here.
                result = pReader->Skip(1);
            }
        }
    }
    return result;
}

/// Helper function to parse the PalMetadata section of a pipeline ELF.
///
/// @param [in] pReader          The message pack reader.
/// @param [in] pRawMetadata     The content of the metadata note section.
/// @param [in] metadataSize     The length of the note section.
/// @param [in] metadataMajorVer The major metadata version.
/// @param [in] metadataMinorVer The minor metadata version.
///
/// @returns Success if successful, ErrorInvalidValue, ErrorUnknown or ErrorUnsupportedPipelineElfAbiVersion if
///          the metadata could not be parsed.
PAL_INLINE Result DeserializePalCodeObjectMetadata(
    MsgPackReader*         pReader,
    PalCodeObjectMetadata* pMetadata,
    const void*            pRawMetadata,
    uint32                 metadataSize,
    uint32                 metadataMajorVer,
    uint32                 metadataMinorVer)
{
    Result result = Result::ErrorUnsupportedPipelineElfAbiVersion;
    if (metadataMajorVer == PipelineMetadataMajorVersion)
    {
        result = pReader->InitFromBuffer(pRawMetadata, metadataSize);
        uint32 registersOffset = UINT_MAX;

        if (result == Result::Success)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 580
            result = Metadata::DeserializePalCodeObjectMetadata(pReader, pMetadata, &registersOffset);
#else
            result = Metadata::DeserializePalCodeObjectMetadata(pReader, pMetadata);
#endif
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 580
        if (result == Result::Success)
        {
            result = pReader->Seek(registersOffset);
        }
#endif
    }

    return result;
}

} //Abi
} //Pal
