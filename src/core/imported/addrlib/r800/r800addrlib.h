/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
****************************************************************************************************
* @file  r800addrlib.h
* @brief Contains the R800Lib class definition.
****************************************************************************************************
*/

#ifndef __R800_ADDR_LIB_H__
#define __R800_ADDR_LIB_H__

#include "egbaddrlib.h"

namespace Addr
{
namespace V1
{

/**
****************************************************************************************************
* @brief R800 specific settings structure.
* @note We don't need this since we are safe
****************************************************************************************************
*/
struct R800ChipSettings
{
    // Asic identification flags.
    struct
    {
        UINT_32 isEvergreen      : 1;  ///< Member of Evergreen family (8xx)
        UINT_32 isCypress        : 1;  ///< Cypress
        UINT_32 isJuniper        : 1;  ///< Juniper
        UINT_32 isRedwood        : 1;  ///< Redwood
        UINT_32 isCedar          : 1;  ///< Cedar

        /// Treat all 8xx fusion the same
        UINT_32 isSumo           : 1;  ///< Member of Sumo (8xx fusion)

        UINT_32 isNorthernIsland : 1;  ///< Member of NorthernIslands family (9xx)
        UINT_32 isCayman         : 1;  ///< Cayman
        UINT_32 isBarts          : 1;  ///< Barts
        UINT_32 isTurks          : 1;  ///< Turks
        UINT_32 isCaicos         : 1;  ///< Caicos

        UINT_32 isTrinity        : 1;  ///< Trinity

        ///@todo Remove Kauai when NI palladium testing is completed
        UINT_32 isKauai          : 1;  ///< Kauai
    };
};

/**
****************************************************************************************************
* @brief This class is the R800 specific address library sfunction set.
* @note  Including Evergreen and Northern Islands
****************************************************************************************************
*/
class R800Lib : public EgBasedLib
{
public:
    /// Creates R800Lib object
    static Addr::Lib* CreateObj(const Client* pClient)
    {
        VOID* pMem = Object::ClientAlloc(sizeof(R800Lib), pClient);
        return (pMem != NULL) ? new (pMem) R800Lib(pClient) : NULL;
    }

protected:
    R800Lib(const Client* pClient);
    virtual ~R800Lib();

    // Hwl interface - defined in AddrLib
    virtual ADDR_E_RETURNCODE HwlComputeSurfaceInfo(
        const ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn,
        ADDR_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    virtual ADDR_E_RETURNCODE HwlComputeSurfaceAddrFromCoord(
        const ADDR_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,
        ADDR_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT* pOut) const;

    virtual ADDR_E_RETURNCODE HwlComputeSurfaceCoordFromAddr(
        const ADDR_COMPUTE_SURFACE_COORDFROMADDR_INPUT* pIn,
        ADDR_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT* pOut) const;

    virtual BOOL_32 HwlComputeMipLevel(
        ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn) const;

    virtual BOOL_32 HwlInitGlobalParams(
        const ADDR_CREATE_INPUT* pCreateIn);

    virtual ChipFamily HwlConvertChipFamily(UINT_32 uChipFamily, UINT_32 uChipRevision);

    virtual UINT_32 ComputePipeFromCoord(
        UINT_32 x, UINT_32 y, UINT_32 slice, AddrTileMode tileMode,
        UINT_32 pipeSwizzle, BOOL_32 ignoreSE, ADDR_TILEINFO* pTileInfo) const;

    virtual VOID HwlComputeSurfaceCoord2DFromBankPipe(
        AddrTileMode tileMode, UINT_32* pX, UINT_32* pY, UINT_32 slice,
        UINT_32 bank, UINT_32 pipe,
        UINT_32 bankSwizzle, UINT_32 pipeSwizzle, UINT_32 tileSlices,
        BOOL_32 ignoreSE,
        ADDR_TILEINFO* pTileInfo) const;

    virtual UINT_32 HwlComputeXmaskCoordYFrom8Pipe(
        UINT_32 pipe, UINT_32 x) const;

    virtual UINT_64 HwlComputeHtileBytes(
        UINT_32 pitch, UINT_32 height, UINT_32 bpp,
        BOOL_32 isLinear, UINT_32 numSlices, UINT_64* pSliceBytes, UINT_32 baseAlign) const;

    /// Gets maximum alignments
    virtual ADDR_E_RETURNCODE HwlGetMaxAlignments(ADDR_GET_MAX_ALINGMENTS_OUTPUT* pOut) const
    {
        if (pOut != NULL)
        {
            // Max tile size is 8-sample and 16 byte-per-pixel
            const UINT_64 tileSize = Min(m_rowSize, MicroTilePixels * 8 * 16);

            // With the largest tile size, the bank_width and bank_height can be treated as 1.
            pOut->baseAlign = tileSize * m_banks * m_pipes;
        }

        return ADDR_OK;
    }

private:

    // Sub-hwl interface - defined in EgBasedLib

    /// Initialization
    virtual VOID HwlSetupTileInfo(
        AddrTileMode tileMode, ADDR_SURFACE_FLAGS flags,
        UINT_32 bpp, UINT_32 pitch, UINT_32 height, UINT_32 numSamples,
        ADDR_TILEINFO* inputTileInfo, ADDR_TILEINFO* outputTileInfo,
        AddrTileType inTileType, ADDR_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    virtual UINT_32 HwlGetPitchAlignmentLinear(UINT_32 bpp, ADDR_SURFACE_FLAGS flags) const;

    virtual UINT_64 HwlGetSizeAdjustmentLinear(
        AddrTileMode tileMode,
        UINT_32 bpp, UINT_32 numSamples, UINT_32 baseAlign, UINT_32 pitchAlign,
        UINT_32 *pPitch, UINT_32 *pHeight, UINT_32 *pHeightAlign) const;

    virtual BOOL_32 HwlSanityCheckMacroTiled(
        ADDR_TILEINFO* pTileInfo) const;

    virtual VOID HwlCheckLastMacroTiledLvl(
        const ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn,
        ADDR_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    /// FMASK helper
    virtual UINT_32 HwlComputeFmaskBits(
        const ADDR_COMPUTE_FMASK_INFO_INPUT* pIn,
        UINT_32* pNumSamples) const;

    /// Adjusts bank before bank is modified by rotation
    virtual UINT_32 HwlPreAdjustBank(
        UINT_32         tileX,
        UINT_32         bank,
        ADDR_TILEINFO*  pTileInfo) const
    {
        return bank;
    }

    BOOL_32 ComputeSurfaceInfoPowerSave(
        const ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn,
        ADDR_COMPUTE_SURFACE_INFO_OUTPUT* pOut) const;

    BOOL_32 ComputeSurfaceAlignmentsPowerSave(
        UINT_32 bpp, ADDR_SURFACE_FLAGS flags,
        UINT_32* pBaseAlign, UINT_32* pPitchAlign, UINT_32* pHeightAlign) const;

    UINT_64 ComputeSurfaceAddrFromCoordPowerSave(
        UINT_32 x, UINT_32 y, UINT_32 slice, UINT_32 bpp,
        UINT_32 pitch, UINT_32 height,ADDR_TILEINFO* pTileInfo,
        UINT_32* pBitPosition) const;

    VOID    ComputeSurfaceCoordFromAddrPowerSave(
        UINT_64 addr, UINT_32 bitPosition, UINT_32 bpp,
        UINT_32 pitch, UINT_32 height, ADDR_TILEINFO* pTileInfo,
        UINT_32* pX, UINT_32* pY) const;

    UINT_32 ComputePixelIndexWithinPowerSave(
        UINT_32 x, UINT_32 y, UINT_32 z, UINT_32 bpp) const;

    VOID    ComputePixelCoordFromOffsetPowerSave(
        UINT_32 offset, UINT_32 bpp, UINT_32* pX, UINT_32* pY) const;

    BOOL_32 SanityCheckPowerSave(
        UINT_32 bpp, UINT_32 numSamples, UINT_32 mipLevel, UINT_32 numSlices) const;

    UINT_32 ComputeDefaultBank(
        AddrTileMode tileMode, UINT_32 bpp,
        ADDR_SURFACE_FLAGS flags, UINT_32 numSamples,
        UINT_32 pitch, UINT_32 height, UINT_32 tileSplitBytes) const;

private:
    UINT_32 m_shaderEngines;        ///< Number of shader engines
    UINT_32 m_shaderEngineTileSize; ///< Tile size for each shader engine
    UINT_32 m_lowerPipes;           ///< Number of pipes to interleave for power save tiling

    R800ChipSettings m_settings;    ///< Chip settings
};

} // V1
} // Addr

#endif

