/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  r800addrlib.cpp
* @brief Contains the implementation for the R800Lib class.
****************************************************************************************************
*/

#include "r800addrlib.h"
#include "r800_gb_reg.h"

#include "evergreen_id.h"
#include "northernisland_id.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Addr
{

/**
****************************************************************************************************
*   R800HwlInit
*
*   @brief
*       Creates an R800Lib object.
*
*   @return
*       Returns an R800Lib object pointer.
****************************************************************************************************
*/
Lib* R800HwlInit(const Client* pClient)
{
    return V1::R800Lib::CreateObj(pClient);
}

namespace V1
{

/**
****************************************************************************************************
*   R800Lib::R800Lib
*
*   @brief
*       Constructor
*
*   @note
*       bankInterleave is never enabled and in SI we cannot accces it since it private
*       so we give it a default value 1, which means no bank interleave actually
****************************************************************************************************
*/
R800Lib::R800Lib(const Client* pClient)
    :
    EgBasedLib(pClient),
    m_shaderEngines(0),
    m_shaderEngineTileSize(0),
    m_lowerPipes(0)
{
    m_class = R800_ADDRLIB;
    memset(&m_settings, 0, sizeof(m_settings));
}

/**
****************************************************************************************************
*   R800Lib::~R800Lib
*
*   @brief
*       Destructor
****************************************************************************************************
*/
R800Lib::~R800Lib()
{
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeSurfaceInfo
*   @brief
*       Entry of r800's ComputeSurfaceInfo
*   @return
*       ADDR_E_RETURNCODE
****************************************************************************************************
*/
ADDR_E_RETURNCODE R800Lib::HwlComputeSurfaceInfo(
    const ADDR_COMPUTE_SURFACE_INFO_INPUT*  pIn,    ///< [in] input structure
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT*       pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE retCode;

    if (pIn->tileMode == ADDR_TM_POWER_SAVE)
    {
        retCode = ADDR_INVALIDPARAMS;
        if (SanityCheckPowerSave(pIn->bpp, pIn->numSamples, pIn->mipLevel, pIn->numSlices))
        {
            if (ComputeSurfaceInfoPowerSave(pIn, pOut))
            {
                retCode = ADDR_OK;
            }
        }
    }
    else
    {
        retCode = EgBasedLib::HwlComputeSurfaceInfo(pIn, pOut);
    }

    return retCode;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeSurfaceAddrFromCoord
*   @brief
*       Entry of r800's ComputeSurfaceAddrFromCoord
*   @return
*       ADDR_E_RETURNCODE
****************************************************************************************************
*/
ADDR_E_RETURNCODE R800Lib::HwlComputeSurfaceAddrFromCoord(
    const ADDR_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
    ADDR_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE retCode = ADDR_OK;

    if (pIn->tileMode == ADDR_TM_POWER_SAVE)
    {
        pOut->addr = ComputeSurfaceAddrFromCoordPowerSave(pIn->x, pIn->y, pIn->slice,
                                                          pIn->bpp, pIn->pitch, pIn->height,
                                                          pIn->pTileInfo,
                                                          &pOut->bitPosition);
    }
    else
    {
        retCode = EgBasedLib::HwlComputeSurfaceAddrFromCoord(pIn, pOut);
    }

    return retCode;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeSurfaceCoordFromAddr
*   @brief
*       Entry of r800's ComputeSurfaceCoordFromAddr
*   @return
*       ADDR_E_RETURNCODE
****************************************************************************************************
*/
ADDR_E_RETURNCODE R800Lib::HwlComputeSurfaceCoordFromAddr(
    const ADDR_COMPUTE_SURFACE_COORDFROMADDR_INPUT* pIn,    ///< [in] input structure
    ADDR_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE retCode = ADDR_OK;

    if (pIn->tileMode == ADDR_TM_POWER_SAVE)
    {
        ComputeSurfaceCoordFromAddrPowerSave(pIn->addr, pIn->bitPosition,
                                             pIn->bpp, pIn->pitch, pIn->height, pIn->pTileInfo,
                                             &pOut->x, &pOut->y);
        pOut->slice     = 0;
        pOut->sample    = 0;
    }
    else
    {
        retCode = EgBasedLib::HwlComputeSurfaceCoordFromAddr(pIn, pOut);
    }

    return retCode;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeMipLevel
*   @brief
*       Compute MipLevel info (A special WA for NON-POW-2 BC format)
*   @return
*       TRUE if HWL's handled
****************************************************************************************************
*/
BOOL_32 R800Lib::HwlComputeMipLevel(
    ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn ///< [in,out] Input structure
    ) const
{
    BOOL_32 handled = TRUE;

    // BC format
    if (ElemLib::IsCompressed(pIn->format))
    {
        if (m_chipFamily == ADDR_CHIP_FAMILY_R8XX)
        {
            // Cypress and Juniper A11 have a H/W bug which we have no W/A
            // We have W/A for other Evergreen/Sumo/Evergreen+ asics
            if (m_settings.isEvergreen)
            {
                // A11 revision id are the same for Evergreen and Sumo
                switch (m_chipRevision)
                {
                    case CYPRESS_A11:
                    case JUNIPER_A11: // Cypress/Juniper A11 are buggy
                        handled = FALSE;
                        break;
                    default:
                        handled = TRUE;
                        break;
                }
            }
            else // Sumo and Evergreen+
            {
                handled = TRUE;
            }

            // If this is ECO'd (or RTL fix) asic, the input width/height should be pow2
            if (handled)
            {
                // We may have issues if we pad mip to power-of-two directly
                ADDR_ASSERT(IsPow2(pIn->width) && IsPow2(pIn->height));
            }
        }
        // NI and TN don't have this bug
    }

    return handled;
}

/**
****************************************************************************************************
*   R800Lib::HwlInitGlobalParams
*
*   @brief
*       Initializes global parameters
*
*   @return
*       TRUE if all settings are valid
*
****************************************************************************************************
*/
BOOL_32 R800Lib::HwlInitGlobalParams(
    const ADDR_CREATE_INPUT* pCreateIn) ///< [in] create input
{
    BOOL_32         valid = TRUE;
    GB_ADDR_CONFIG  reg;

    const ADDR_REGISTER_VALUE* pRegValue = &pCreateIn->regValue;

    reg.val = pRegValue->gbAddrConfig;

    switch (reg.f.num_pipes)
    {
        case ADDR_CONFIG_1_PIPE:
            m_pipes = 1;
            break;
        case ADDR_CONFIG_2_PIPE:
            m_pipes = 2;
            break;
        case ADDR_CONFIG_4_PIPE:
            m_pipes = 4;
            break;
        case ADDR_CONFIG_8_PIPE:
            m_pipes = 8;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.pipe_interleave_size)
    {
        case ADDR_CONFIG_PIPE_INTERLEAVE_256B:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_256B;
            break;
        case ADDR_CONFIG_PIPE_INTERLEAVE_512B:
            m_pipeInterleaveBytes = ADDR_PIPEINTERLEAVE_512B;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.row_size)
    {
        case ADDR_CONFIG_1KB_ROW:
            m_rowSize = ADDR_ROWSIZE_1KB;
            break;
        case ADDR_CONFIG_2KB_ROW:
            m_rowSize = ADDR_ROWSIZE_2KB;
            break;
        case ADDR_CONFIG_4KB_ROW:
            m_rowSize = ADDR_ROWSIZE_4KB;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.bank_interleave_size)
    {
        case ADDR_CONFIG_BANK_INTERLEAVE_1:
            m_bankInterleave = ADDR_BANKINTERLEAVE_1;
            break;
        case ADDR_CONFIG_BANK_INTERLEAVE_2:
            m_bankInterleave = ADDR_BANKINTERLEAVE_2;
            break;
        case ADDR_CONFIG_BANK_INTERLEAVE_4:
            m_bankInterleave = ADDR_BANKINTERLEAVE_4;
            break;
        case ADDR_CONFIG_BANK_INTERLEAVE_8:
            m_bankInterleave = ADDR_BANKINTERLEAVE_8;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.num_shader_engines)
    {
        case ADDR_CONFIG_1_SHADER_ENGINE:
            m_shaderEngines = 1;
            break;
        case ADDR_CONFIG_2_SHADER_ENGINE:
            m_shaderEngines = 2;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.shader_engine_tile_size)
    {
        case ADDR_CONFIG_SE_TILE_16:
            m_shaderEngineTileSize = ADDR_SE_TILESIZE_16;
            break;
        case ADDR_CONFIG_SE_TILE_32:
            m_shaderEngineTileSize = ADDR_SE_TILESIZE_32;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (pRegValue->noOfBanks)
    {
        case 0:
            m_banks = 4;
            break;
        case 1:
            m_banks = 8;
            break;
        case 2:
            m_banks = 16;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (pRegValue->noOfRanks)
    {
        case 0:
            m_ranks = 1;
            break;
        case 1:
            m_ranks = 2;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    switch (reg.f.num_lower_pipes)
    {
        case ADDR_CONFIG_1_PIPE:
            m_lowerPipes = 1;
            break;
        case ADDR_CONFIG_2_PIPE:
            m_lowerPipes = 2;
            break;
        default:
            valid = FALSE;
            ADDR_UNHANDLED_CASE();
            break;
    }

    // power-save mode is only valid for 9xx
    if (m_chipFamily == ADDR_CHIP_FAMILY_NI)
    {
        ADDR_ASSERT(m_lowerPipes <= m_pipes);
        m_maxSamples = 16;
    }

    // Tex2D UAV on cypress will fail/hang if tile mode is linear, so we choose to disable linear
    // mode optimization for r800 HWL
    m_configFlags.disableLinearOpt = TRUE;

    m_logicalBanks = m_banks * m_ranks;

    ADDR_ASSERT(m_logicalBanks <= 16);

    return valid;
}

/**
****************************************************************************************************
*   R800Lib::HwlConvertChipFamily
*
*   @brief
*       Convert familyID defined in atiid.h to ChipFamily and set m_chipFamily/m_chipRevision
*   @return
*       ChipFamily
****************************************************************************************************
*/
ChipFamily R800Lib::HwlConvertChipFamily(
    UINT_32 uChipFamily,        ///< [in] chip family defined in atiih.h
    UINT_32 uChipRevision)      ///< [in] chip revision defined in "asic_family"_id.h
{
    ///@note m_settings records what base driver describes for "family"
    ///      while m_chipFamily describes what address library knows about
    ChipFamily family = ADDR_CHIP_FAMILY_R8XX;

    switch (uChipFamily)
    {
        case FAMILY_EVERGREEN:
        case FAMILY_MANHATTAN:
            m_settings.isEvergreen = 1;
            m_settings.isCypress = ASICREV_IS_CYPRESS(uChipRevision);
            m_settings.isJuniper = ASICREV_IS_JUNIPER(uChipRevision);
            m_settings.isRedwood = ASICREV_IS_REDWOOD(uChipRevision);
            m_settings.isCedar   = ASICREV_IS_CEDAR(uChipRevision);
            break;

        case FAMILY_SUMO:
            m_settings.isSumo = 1;
            break;

        case FAMILY_NI:
            m_settings.isNorthernIsland = 1;

            m_settings.isCayman = ASICREV_IS_CAYMAN(uChipRevision);
            m_settings.isKauai  = ASICREV_IS_KAUAI(uChipRevision);
            m_settings.isBarts  = ASICREV_IS_BARTS(uChipRevision);
            m_settings.isTurks  = ASICREV_IS_TURKS(uChipRevision);
            m_settings.isCaicos = ASICREV_IS_CAICOS(uChipRevision);

            /// Only CAYMAN/KAUAI are real NI asics, others are derivatives of 8XX.
            if (m_settings.isCayman || m_settings.isKauai)
            {
                family = ADDR_CHIP_FAMILY_NI;
            }
            break;

        case FAMILY_TN:
            m_settings.isTrinity = 1;
            family = ADDR_CHIP_FAMILY_NI;
            break;

        default:
            ADDR_ASSERT_ALWAYS();
            family = ADDR_CHIP_FAMILY_NI;
            break;
    }

    return family;
}

/**
****************************************************************************************************
*   R800Lib::ComputePipeFromCoord
*
*   @brief
*       Compute pipe number from coordinates
*   @return
*       Pipe number
****************************************************************************************************
*/
UINT_32 R800Lib::ComputePipeFromCoord(
    UINT_32         x,              ///< [in] x coordinate
    UINT_32         y,              ///< [in] y coordinate
    UINT_32         slice,          ///< [in] slice index
    AddrTileMode    tileMode,       ///< [in] tile mode
    UINT_32         pipeSwizzle,    ///< [in] pipe swizzle
    BOOL_32         ignoreSE,       ///< [in] TRUE if shader engines are ignored
    ADDR_TILEINFO*  pTileInfo       ///< [in] Tile info
    ) const
{
    UINT_32 pipe;
    UINT_32 pipeBit0 = 0;
    UINT_32 pipeBit1 = 0;
    UINT_32 pipeBit2 = 0;
    UINT_32 sliceRotation;

    // SI has its implementation, so we use m_pipes, m_shaderEnginers, m_shaderEngineTileSize
    // directly without virtual function calls.
    UINT_32 numPipes = m_pipes;
    UINT_32 shaderEngines = (ignoreSE ? 1 : m_shaderEngines);
    UINT_32 shaderEngineTileSize = m_shaderEngineTileSize;

    UINT_32 tx = x / MicroTileWidth;
    UINT_32 ty = y / MicroTileHeight;
    UINT_32 x3 = _BIT(tx,0);
    UINT_32 x4 = _BIT(tx,1);
    UINT_32 x5 = _BIT(tx,2);
    UINT_32 y3 = _BIT(ty,0);
    UINT_32 y4 = _BIT(ty,1);
    UINT_32 y5 = _BIT(ty,2);

    switch (numPipes)
    {
        case 1:
            break;
        case 2:
            pipeBit0 = x3 ^ y3;
            break;
        case 4:
            pipeBit0 = x4 ^ y3;
            pipeBit1 = x3 ^ y4;
            break;
        case 8:
            if (shaderEngines == 1)
            {
                pipeBit0 = x4 ^ y4 ^ x5;
                pipeBit1 = x3 ^ y5;
                pipeBit2 = x4 ^ y3 ^ y5;
            }
            else if (shaderEngines == 2)
            {
                switch (shaderEngineTileSize)
                {
                    case 16:
                        pipeBit0 = x4 ^ y3 ^ x5;
                        pipeBit1 = x3 ^ y5;
                        pipeBit2 = x4 ^ y4;
                        break;
                    case 32:
                        pipeBit0 = x4 ^ y3 ^ x5;
                        pipeBit1 = x3 ^ y4;
                        pipeBit2 = x5 ^ y5;
                        break;
                    default:
                        ADDR_UNHANDLED_CASE();
                        break;
                }
            }
            else
            {
                ADDR_UNHANDLED_CASE();
            }
            break;

        default:
            ADDR_UNHANDLED_CASE();
            break;
    }
    pipe = pipeBit0 | (pipeBit1 << 1) | (pipeBit2 << 2);

    UINT_32 microTileThickness = Thickness(tileMode);

    //
    // Apply pipe rotation for the slice.
    //
    switch (tileMode)
    {
        case ADDR_TM_3D_TILED_THIN1:    //fall through thin
        case ADDR_TM_3D_TILED_THICK:    //fall through thick
        case ADDR_TM_3D_TILED_XTHICK:
            sliceRotation =
                Max(1, static_cast<INT_32>(numPipes / 2) - 1) * (slice / microTileThickness);
            break;
        default:
            sliceRotation = 0;
            break;
    }
    pipeSwizzle += sliceRotation;
    pipeSwizzle &= (numPipes - 1);

    pipe = pipe ^ pipeSwizzle;

    return pipe;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeSurfaceCoord2DFromBankPipe
*
*   @brief
*       Compute surface x,y coordinates from bank/pipe info
*   @return
*       N/A
****************************************************************************************************
*/
VOID R800Lib::HwlComputeSurfaceCoord2DFromBankPipe(
    AddrTileMode        tileMode,   ///< [in] tile mode
    UINT_32*            pX,         ///< [in,out] x coordinate
    UINT_32*            pY,         ///< [in,out] y coordinate
    UINT_32             slice,      ///< [in] slice index
    UINT_32             bank,       ///< [in] bank number
    UINT_32             pipe,       ///< [in] pipe number
    UINT_32             bankSwizzle,///< [in] bank swizzle
    UINT_32             pipeSwizzle,///< [in] pipe swizzle
    UINT_32             tileSlices, ///< [in] slices in a micro tile
    BOOL_32             ignoreSE,   ///< [in] TRUE if shader engines are ignored
    ADDR_TILEINFO*      pTileInfo   ///< [in] bank structure. **All fields to be valid on entry**
    ) const
{
    UINT_32 xBit;
    UINT_32 yBit;
    UINT_32 yBit3 = 0;
    UINT_32 yBit4 = 0;
    UINT_32 yBit5 = 0;
    UINT_32 yBit6 = 0;

    UINT_32 xBit3 = 0;
    UINT_32 xBit4 = 0;
    UINT_32 xBit5 = 0;

    CoordFromBankPipe xyBits = {0};
    ComputeSurfaceCoord2DFromBankPipe(tileMode, *pX, *pY, slice, bank, pipe,
                                      bankSwizzle, pipeSwizzle, tileSlices, pTileInfo,
                                      &xyBits);
    yBit3 = xyBits.yBit3;
    yBit4 = xyBits.yBit4;
    yBit5 = xyBits.yBit5;
    yBit6 = xyBits.yBit6;

    xBit3 = xyBits.xBit3;
    xBit4 = xyBits.xBit4;
    xBit5 = xyBits.xBit5;

    yBit = Bits2Number(4, yBit6, yBit5, yBit4, yBit3);
    xBit = Bits2Number(3, xBit5, xBit4, xBit3);

    *pY += yBit * pTileInfo->bankHeight * MicroTileHeight;
    *pX += xBit * m_pipes * pTileInfo->bankWidth * MicroTileWidth;

    //calculate the bank and pipe bits in x, y
    UINT_32 shaderEngines = ignoreSE ? 1 : m_shaderEngines;
    UINT_32 xTile; //x in micro tile
    UINT_32 x3 = 0;
    UINT_32 x4 = 0;
    UINT_32 x5 = 0;
    UINT_32 y = *pY;

    switch (m_pipes)
    {
        case 1:
            break;
        case 2:
            x3 = _BIT(pipe,0) ^ _BIT(y,3);
            break;
        case 4:
            x4 = _BIT(pipe,0) ^ _BIT(y,3);
            x3 = _BIT(pipe,1) ^ _BIT(y,4);
            break;
        case 8:
            if (shaderEngines == 1)
            {
                x3 = _BIT(pipe,1) ^ _BIT(y,5);
                x4 = _BIT(pipe,2) ^ _BIT(y,3) ^  _BIT(y,5);
                x5 = _BIT(pipe,0) ^ _BIT(y,4) ^ x4;
            }
            else
            {
                if (m_shaderEngineTileSize == 16)
                {
                    x3 = _BIT(pipe,1) ^ _BIT(y,5);
                    x4 = _BIT(pipe,2) ^ _BIT(y,4);
                    x5 = _BIT(pipe,0) ^ _BIT(y,3) ^ x4;
                }
                else if (m_shaderEngineTileSize == 32)
                {
                    x3 = _BIT(pipe,1) ^ _BIT(y,4);
                    x5 = _BIT(pipe,2) ^ _BIT(y,5);
                    x4 = _BIT(pipe,0) ^ _BIT(y,3) ^ x5;
                }
                else
                {
                    ADDR_UNHANDLED_CASE();
                }
            }
            break;
        default:
            break;
    }

    xTile = Bits2Number(3, x5, x4, x3);

    *pX += xTile << 3;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeXmaskCoordYFrom8Pipe
*
*   @brief
*       Compute the Y coord which will be added to Xmask Y
*       coord.
*   @return
*       Y coord
****************************************************************************************************
*/
UINT_32 R800Lib::HwlComputeXmaskCoordYFrom8Pipe(
    UINT_32         pipe,       ///< [in] pipe id
    UINT_32         x           ///< [in] tile coord x, which is original x coord / 8
    ) const
{
    UINT_32 yBit0 = 0;
    UINT_32 yBit1 = 0;
    UINT_32 yBit2 = 0;

    UINT_32 y;

    ADDR_ASSERT(m_shaderEngines <= 2);

    if (m_shaderEngines == 1)
    {
        yBit2 = _BIT(pipe, 1) ^ _BIT(x, 0);
        yBit1 = _BIT(pipe, 0) ^ _BIT(x, 1) ^ _BIT(x, 2);
        yBit0 = _BIT(pipe, 2) ^ _BIT(x, 1) ^ yBit2;
    }
    else if (m_shaderEngines == 2)
    {
        if (m_shaderEngineTileSize == ADDR_SE_TILESIZE_16)
        {
            yBit0 = _BIT(pipe, 0) ^ _BIT(x, 1) ^ _BIT(x, 2);
            yBit1 = _BIT(pipe, 2) ^ _BIT(x, 1);
            yBit2 = _BIT(pipe, 1) ^ _BIT(x, 0);
        }
        else if (m_shaderEngineTileSize == ADDR_SE_TILESIZE_32)
        {
            yBit0 = _BIT(pipe, 0) ^ _BIT(x, 1)^ _BIT(x, 2);
            yBit1 = _BIT(pipe, 1) ^ _BIT(x, 0);
            yBit2 = _BIT(pipe, 2) ^ _BIT(x, 2);
        }
        else
        {
            ADDR_UNHANDLED_CASE();
        }
    }

    y = Bits2Number(3, yBit2, yBit1, yBit0);

    return y;
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeHtileBytes
*
*   @brief
*       Compute htile size in bytes
*
*   @return
*       Htile size in bytes
****************************************************************************************************
*/
UINT_64 R800Lib::HwlComputeHtileBytes(
    UINT_32     pitch,          ///< [in] pitch
    UINT_32     height,         ///< [in] height
    UINT_32     bpp,            ///< [in] bits per pixel
    BOOL_32     isLinear,       ///< [in] if it is linear mode
    UINT_32     numSlices,      ///< [in] number of slices
    UINT_64*    pSliceBytes,    ///< [out] bytes per slice
    UINT_32     baseAlign       ///< [in] base alignments
    ) const
{
#if ALT_TEST // Temporary disable this for driver
    if (isLinear && (m_chipFamily == ADDR_CHIP_FAMILY_NI))
    {
        const UINT_32 HtileHeightLinearAlign = 64;

        if (height % HtileHeightLinearAlign)
        {
            height = PowTwoAlign(height, HtileHeightLinearAlign);
        }
    }
#endif
    return ComputeHtileBytes(pitch, height, bpp, isLinear, numSlices, pSliceBytes, baseAlign);
}

/**
****************************************************************************************************
*   R800Lib::HwlSetupTileInfo
*
*   @brief
*       Setup default tile info for any fields set to 0
*       Input can be NULL, in which case all fields are calculated.
*       Output may be the same as the input, in which case, the parameters are
*       not changed.
****************************************************************************************************
*/
VOID R800Lib::HwlSetupTileInfo(
    AddrTileMode                        tileMode,       ///< [in] Tile mode
    ADDR_SURFACE_FLAGS                  flags,          ///< [in] Surface type flags
    UINT_32                             bpp,            ///< [in] Bits per pixel
    UINT_32                             pitch,          ///< [in] Pitch in pixels
    UINT_32                             height,         ///< [in] Height in pixels
    UINT_32                             numSamples,     ///< [in] Number of samples
    ADDR_TILEINFO*                      pTileInfoIn,    ///< [in] Tile info input: NULL for default
    ADDR_TILEINFO*                      pTileInfoOut,   ///< [out] Tile info output
    AddrTileType                        inTileType,     ///< [in] Tile type
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT*   pOut            ///< [out] Output
    ) const
{
    const UINT_32 thickness = Thickness(tileMode);

    const ADDR_TILEINFO tileInfoDef   =
    {
        2, 1, 1, 1, 64, ADDR_PIPECFG_P2
    };

    if (!IsLinear(tileMode))
    {
        if (m_chipFamily >= ADDR_CHIP_FAMILY_NI)
        {
            if (bpp >= 128)
            {
                if (flags.dispTileType)
                {
                    inTileType = ADDR_DISPLAYABLE;
                }
                else
                {
                    inTileType = ADDR_NON_DISPLAYABLE;
                }
            }
            else if (thickness > 1)
            {
                inTileType = ADDR_NON_DISPLAYABLE;
            }
        }

        if (flags.depth || flags.stencil)
        {
            inTileType = ADDR_DEPTH_SAMPLE_ORDER;
        }
    }

    ADDR_TILEINFO* pTileInfo = pTileInfoOut;

    if (IsMacroTiled(tileMode))
    {
        UINT_32 bankWidth;
        UINT_32 bankHeight;
        UINT_32 tileSize;

        static const UINT_32 sCompressZTileSplit[] =
        {
            64, 128, 128, 256, 512
        };

        ADDR_TILEINFO tileInfoDepth = {0};

        UINT_32 defaultRatio = 1;

        if ((numSamples > 1) && !(flags.depth || flags.stencil)) // MSAA texture
        {
            // Remove .texture and add .color to avoid different tile_split
            // Reported by DXX who is doing an MSAA color buffer to MSAA texture memcpy
            flags.texture   = 0;
            flags.color     = 1;
        }

        if (flags.stencil)
        {
            ADDR_ASSERT(bpp == 8);

            if (IsTileInfoAllZero(pTileInfoIn))
            {
                ADDR_SURFACE_FLAGS depthFlags = {{0}};

                depthFlags.depth        = 1;
                // We assume depth is compressed if stencil is compressed, which is not always true.
                // If the assumption is false stencil's tileInfo may mismatch corresponding depth's.
                // We still recommend client copying depth's tileInfo as stencil's input.
                depthFlags.compressZ    = flags.compressZ;
                depthFlags.opt4Space    = flags.opt4Space;

                // Compute corresponding depth tileInfo and use it as stencil input
                HwlSetupTileInfo(tileMode,
                                 depthFlags,
                                 32, //24 bit Z will be rounded to 32 as well
                                 pitch,
                                 height,
                                 numSamples,
                                 NULL,
                                 &tileInfoDepth,
                                 inTileType,
                                 pOut);

                pTileInfoIn = &tileInfoDepth;
                // Let address library choose tile_split
                pTileInfoIn->tileSplitBytes = 0;
                // Other fields are ready so copy them
                *pTileInfoOut = *pTileInfoIn;
            }
        }

        if (pTileInfo->tileSplitBytes == 0)
        {
            UINT_32 log2S = QLog2(numSamples);

            if (flags.stencil)
            {
                // Use Z's tile_split table for compressed stencil to reduce bank_height alignment
                if (flags.compressZ)
                {
                    pTileInfo->tileSplitBytes = sCompressZTileSplit[log2S];
                }
                else
                {
                    pTileInfo->tileSplitBytes = m_rowSize;
                }
            }
            else if (flags.color) // non-texture color buffer
            {
                //TILE_SPLIT = Estimated_Fragmentation * bytes/sample * 64
                if (numSamples > 1)
                {
                    pTileInfo->tileSplitBytes =
                        Max(256u, BITS_TO_BYTES(bpp) * NextPow2(log2S) * 64);

                    // For MSAA color buffer we have chance that tileSplitBytes > rowSize
                    if (pTileInfo->tileSplitBytes > m_rowSize)
                    {
                        pTileInfo->tileSplitBytes = m_rowSize;
                    }
                }
                else // no-AA
                {
                    pTileInfo->tileSplitBytes = m_rowSize;
                }
            }
            else if (flags.depth)
            {
                if (flags.compressZ)
                {
                    pTileInfo->tileSplitBytes = sCompressZTileSplit[log2S];
                }
                else
                {
                    pTileInfo->tileSplitBytes = m_rowSize;
                }
            }
            else
            {
                pTileInfo->tileSplitBytes = m_rowSize;
            }

            // Thick mode has no tile split, we try to enlarge tileSplitBytes to avoid
            // sanity check failure and tile-split happening...
            // But tileSplitBytes cannot be larger than rowSize
            UINT_32 microTileSize = BITS_TO_BYTES(64 * thickness * bpp);

            if ((thickness > 1) &&
                (pTileInfo->tileSplitBytes < microTileSize) &&
                (microTileSize <= m_rowSize))
            {
                pTileInfo->tileSplitBytes = microTileSize;
            }
        }

        if (pTileInfo->banks == 0)
        {
            pTileInfo->banks = ComputeDefaultBank(tileMode,
                                                  bpp,
                                                  flags,
                                                  numSamples,
                                                  pitch,
                                                  height,
                                                  pTileInfo->tileSplitBytes);
        }

        UINT_32 testVal;
        UINT_32 testTime;

        tileSize = Min(pTileInfo->tileSplitBytes, BITS_TO_BYTES(64 * thickness * bpp * numSamples));

        if ((pTileInfo->bankWidth == 0) && (pTileInfo->bankHeight == 0))
        {
            UINT_32 defaultBW = 1;
            UINT_32 defaultBH = 1;

            if (!flags.depth && !flags.stencil)
            {
                if (tileSize <= 32)
                {
                    defaultBH = 8;
                }
                else if (tileSize <= 64)
                {
                    defaultBH = 4;
                }
                else if (tileSize <= 128)
                {
                    defaultBH = 2;
                }
            }
            else
            {
                // Test shows single sample has best performance with 2/4
                if (numSamples > 1)
                {
                    // Stencil might require higher alignment in bank_height
                    // Now we have the assumption Z & S have the same tile_split
                    UINT_32 tileSizeStencil = Min(pTileInfo->tileSplitBytes,
                                                  BITS_TO_BYTES(64 * thickness * 8 * numSamples));

                    UINT_32 scaleFactor = m_pipeInterleaveBytes / tileSizeStencil;

                    // Stencil's bank_height_align = pipe_interleave_size / (tile_size * bank_width)
                    // And tile_size will be tile_split for stencil
                    if (scaleFactor > 1)
                    {
                        if (scaleFactor > 4)
                        {
                            defaultBW = 2;
                        }

                        defaultBH = scaleFactor / defaultBW;
                    }
                    // else use default 1/1
                }
                else
                {
                    defaultBW = 2;
                    defaultBH = 4;
                }
            }

            bankWidth  = defaultBW;
            bankHeight = defaultBH;

            testVal = tileSize * bankWidth * bankHeight;

            testTime = 0;

            while (testVal < 256)
            {
                if (!(testTime & 1))
                {
                    bankWidth <<= 1;
                }
                else
                {
                    bankHeight <<= 1;
                }

                testTime++;

                testVal = tileSize * bankWidth * bankHeight;
            }

            testTime = 0;

            if (tileSize <= m_rowSize) // Avoid infinite loop
            {
                while (testVal > static_cast<UINT_32>(m_rowSize))
                {
                    if (!(testTime & 1) && (bankWidth > 1))
                    {
                        bankWidth >>= 1;
                    }
                    else if (bankHeight > 1)
                    {
                        if (flags.depth)
                        {
                            // Early quit bank_height degradation for "64" bit z buffer
                            // since it will not match stencil's alignment
                            // Stencil's bank_height_align =
                            //           pipe_interleave_size / (tile_size * bank_width)
                            if ((bpp >= 64) && (bankHeight <= 4))
                            {
                                // This break will cause emulation assert but it should function
                                // well on real hardware
                                // And only OpenGL will trigger this on low end asic such Sumo
                                ADDR_WARN(0, ("This setting may cause cmodel assertion!"));
                                break;
                            }
                        }
                        else
                        {
                            bankHeight >>= 1;
                        }
                    }

                    testTime++;

                    testVal = tileSize * bankWidth * bankHeight;
                }
            }

            pTileInfo->bankWidth  = bankWidth;
            pTileInfo->bankHeight = bankHeight;
        }
        else if (pTileInfo->bankWidth == 0)
        {
            pTileInfo->bankWidth = pTileInfo->bankHeight;

            testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;

            while (testVal < 256)
            {
                pTileInfo->bankWidth <<= 1;

                testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;
            }

            if (tileSize <= m_rowSize) // Avoid infinite loop
            {
                while (testVal > static_cast<UINT_32>(m_rowSize))
                {
                    pTileInfo->bankWidth >>= 1;

                    testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;
                }
            }
        }
        else if (pTileInfo->bankHeight == 0)
        {
            // Only fmask can have independent bank height
            if (flags.fmask)
            {
                if (numSamples >= 8)
                {
                    pTileInfo->bankHeight = 1;
                }
                else
                {
                    pTileInfo->bankHeight = 4;
                }
            }
            else
            {
                pTileInfo->bankHeight = pTileInfo->bankWidth;
            }

            testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;

            while (testVal < 256)
            {
                pTileInfo->bankHeight <<= 1;

                testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;
            }

            if (tileSize <= m_rowSize) // Avoid infinite loop
            {
                while (testVal > static_cast<UINT_32>(m_rowSize))
                {
                    pTileInfo->bankHeight >>= 1;

                    testVal = tileSize * pTileInfo->bankWidth * pTileInfo->bankHeight;
                }
            }
        }

        // width alignment = 8 * num_pipes * bank_width * macro_aspect_ratio
        // height alignment = (8 * num_banks * bank_height) / macro_aspect_ratio
        //
        // For some memory pressure case, we may want to increase macro_aspect_ratio to decrease
        // height_align. e.g. set this for 2560x1600 8XAA HDR/Z buffer
        defaultRatio = 1;

        if (pTileInfo->macroAspectRatio == 0)
        {
            // Don't adjust macro_aspect_ratio for filpchain/overlay surfaces
            if (flags.opt4Space)
            {
                UINT_32 dwWidthAlign = 8 * m_pipes * pTileInfo->bankWidth;
                UINT_32 dwHeightAlign = 8 * pTileInfo->banks * pTileInfo->bankHeight;

                // Search "optimal" macro_aspect_ratio for saving space
                while (!(pitch & (2*dwWidthAlign-1)) &&
                       (height & (dwHeightAlign-1))  &&
                       (defaultRatio < 4))
                {
                    defaultRatio <<= 1;

                    dwWidthAlign <<= 1;
                    dwHeightAlign >>= 1;
                }

                if ((height & (dwHeightAlign-1)) &&
                    (defaultRatio < 4))
                {
                    UINT_32 dwActualSize;
                    UINT_32 dwNewActualSize;

                    dwActualSize = PowTwoAlign(pitch, dwWidthAlign) *
                        PowTwoAlign(height, dwHeightAlign);

                    // Try increasing macro_aspect_ratio to see if we can save more space
                    dwNewActualSize = PowTwoAlign(pitch, dwWidthAlign*2) *
                        PowTwoAlign(height, dwHeightAlign/2);

                    if (dwNewActualSize < dwActualSize)
                    {
                        defaultRatio <<= 1;
                    }
                }
            }

            // For fmask used as texture, default ratio(1) is not enough when fmask is treated
            // as a 8bit texture. TC seems to expect ratio to be at least 2
            if ((m_pipes <= 2) && (defaultRatio == 1))
            {
                if ((numSamples > 1) && flags.color)
                {
                    // 2-pipe card has no EQAA, so max of numSamples is 8
                    UINT_32 fmaskTileSize = 64 * (numSamples == 8 ? 4 : 1);
                    UINT_32 minRatio = m_pipeInterleaveBytes * m_bankInterleave /
                                       (fmaskTileSize * m_pipes * pTileInfo->bankWidth);
                    if (minRatio > defaultRatio)
                    {
                        defaultRatio = minRatio;
                    }
                }
            }

            pTileInfo->macroAspectRatio = defaultRatio;

            if (flags.texture)
            {
                if (pTileInfo->bankHeight >= 4)
                {
                    pTileInfo->macroAspectRatio = 2;
                }
            }
            else if (flags.color)
            {
                if (pTileInfo->bankHeight == 4)
                {
                    pTileInfo->macroAspectRatio = 2;
                }
            }
            else if (flags.depth)
            {
                if (numSamples == 1)
                {
                    UINT_32 tileSizeStencil = Min(pTileInfo->tileSplitBytes, 64u);
                    UINT_32 macroAspectAlign =
                        Max(1u, m_pipeInterleaveBytes * m_bankInterleave /
                                (tileSizeStencil * m_pipes * pTileInfo->bankWidth));

                    if (macroAspectAlign > pTileInfo->macroAspectRatio)
                    {
                        pTileInfo->macroAspectRatio = macroAspectAlign;
                    }
                }
            }

            while (pTileInfo->banks < pTileInfo->macroAspectRatio)
            {
                pTileInfo->macroAspectRatio >>= 1;
            }
        }
    }
    else
    {
        if (pTileInfoIn != NULL)
        {
            if (pTileInfoIn->banks == 0)
            {
                pTileInfo->banks = tileInfoDef.banks;
            }
            else
            {
                pTileInfo->banks = pTileInfoIn->banks;
            }

            if (pTileInfoIn->bankWidth == 0)
            {
                pTileInfo->bankWidth = tileInfoDef.bankWidth;
            }
            else
            {
                pTileInfo->bankWidth = pTileInfoIn->bankWidth;
            }

            if (pTileInfoIn->bankHeight == 0)
            {
                pTileInfo->bankHeight = tileInfoDef.bankHeight;
            }
            else
            {
                pTileInfo->bankHeight = pTileInfoIn->bankHeight;
            }

            if (pTileInfoIn->macroAspectRatio == 0)
            {
                pTileInfo->macroAspectRatio = tileInfoDef.macroAspectRatio;
            }
            else
            {
                pTileInfo->macroAspectRatio = pTileInfoIn->macroAspectRatio;
            }

            if (pTileInfoIn->tileSplitBytes == 0)
            {
                pTileInfo->tileSplitBytes = tileInfoDef.tileSplitBytes;
            }
            else
            {
                pTileInfo->tileSplitBytes = pTileInfoIn->tileSplitBytes;
            }
        }
        else
        {
            *pTileInfo = tileInfoDef;
        }
    }

    // Pass through tile type
    pOut->tileType = inTileType;
}

/**
****************************************************************************************************
*   R800Lib::HwlGetPitchAlignmentLinear
*
*   @brief
*       Get pitch alignment
*   @return
*       alignment
****************************************************************************************************
*/
UINT_32 R800Lib::HwlGetPitchAlignmentLinear(
    UINT_32             bpp,    ///< [in] bits per pixel
    ADDR_SURFACE_FLAGS  flags   ///< [in] surface flags
    ) const
{
    // The required granularity for pitch is to 64 pixels or the pipe interleave size,
    // whichever is greater.

    UINT_32 pixelsPerPipeInterleave = m_pipeInterleaveBytes / BITS_TO_BYTES(bpp);

    return Max(64u, pixelsPerPipeInterleave);
}

/**
****************************************************************************************************
*   R800Lib::HwlGetSizeAdjustmentLinear
*
*   @brief
*       Adjust linear surface pitch and slice size
*
*   @return
*       Logical slice size in bytes
****************************************************************************************************
*/
UINT_64 R800Lib::HwlGetSizeAdjustmentLinear(
    AddrTileMode        tileMode,       ///< [in] tile mode
    UINT_32             bpp,            ///< [in] bits per pixel
    UINT_32             numSamples,     ///< [in] number of samples
    UINT_32             baseAlign,      ///< [in] base alignment
    UINT_32             pitchAlign,     ///< [in] pitch alignment
    UINT_32*            pPitch,         ///< [in,out] pointer to pitch
    UINT_32*            pHeight,        ///< [in,out] pointer to height
    UINT_32*            pHeightAlign    ///< [in,out] pointer to height align
    ) const
{
    UINT_64 logicalSliceSize;

    // Logical slice: pitch * height * bpp * numSamples (no MSAA but TGL may pass non 1 value).
    logicalSliceSize = BITS_TO_BYTES(static_cast<UINT_64>(*pPitch) * (*pHeight) * bpp * numSamples);

    return logicalSliceSize;
}

/**
****************************************************************************************************
*   R800Lib::SanityCheckMacroTiled
*
*   @brief
*       Check if macro-tiled parameters are valid
*   @return
*       TRUE if valid
****************************************************************************************************
*/
BOOL_32 R800Lib::HwlSanityCheckMacroTiled(
    ADDR_TILEINFO* pTileInfo   ///< [in] macro-tiled parameters
    ) const
{
    BOOL_32 valid       = TRUE;

    if (m_shaderEngines == 2)
    {
        if (m_pipes != 8)
        {
            valid = FALSE;
        }
    }

    return valid;
}

/**
****************************************************************************************************
*   R800Lib::HwlCheckLastMacroTiledLvl
*
*   @brief
*       Sets pOut->last2DLevel to TRUE if it is
*   @note
*       This function is implemented in 800 HWL for debugging purpose
****************************************************************************************************
*/
VOID R800Lib::HwlCheckLastMacroTiledLvl(
    const ADDR_COMPUTE_SURFACE_INFO_INPUT* pIn, ///< [in] Input structure
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT* pOut      ///< [in,out] Output structure (used as input, too)
    ) const
{
    // R800 has MIP_ADDRESS so the first possible padding happens for level 1
    if (pIn->mipLevel > 0)
    {
        ADDR_ASSERT(IsMacroTiled(pIn->tileMode));

        UINT_32 nextPitch;
        UINT_32 nextHeight;
        UINT_32 nextSlices;

        AddrTileMode nextTileMode;

        nextPitch = NextPow2(pIn->width >> 1);
        // nextHeight must be shifted from this level's original height rather than a pow2 padded
        // one, but this requires original height stored somewhere (pOut->height)
        ADDR_ASSERT(pOut->height != 0);

        // next level's height is just current level's >> 1 in pixels
        nextHeight = pOut->height >> 1;
        // Special format such as FMT_1 and FMT_32_32_32 can be linear only so we consider block
        // compressed foramts
        if (ElemLib::IsBlockCompressed(pIn->format))
        {
            nextHeight = (nextHeight + 3) / 4;
        }
        nextHeight = NextPow2(nextHeight);

        // nextSlices may be 0 if this level's is 1
        if (pIn->flags.volume)
        {
            nextSlices = Max(1u, pIn->numSlices >> 1);
        }
        else
        {
            nextSlices = pIn->numSlices;
        }

        nextTileMode = ComputeSurfaceMipLevelTileMode(pIn->tileMode,
                                                      pIn->bpp,
                                                      nextPitch,
                                                      nextHeight,
                                                      nextSlices,
                                                      pIn->numSamples,
                                                      pOut->blockWidth,
                                                      pOut->blockHeight,
                                                      pOut->pTileInfo);

        pOut->last2DLevel = IsMicroTiled(nextTileMode);
    }
}

/**
****************************************************************************************************
*   R800Lib::HwlComputeFmaskBits
*   @brief
*       Computes fmask bits
*   @return
*       Fmask bits
****************************************************************************************************
*/
UINT_32 R800Lib::HwlComputeFmaskBits(
    const ADDR_COMPUTE_FMASK_INFO_INPUT* pIn,
    UINT_32* pNumSamples
    ) const
{
    UINT_32 numSamples = pIn->numSamples;
    UINT_32 numFrags = GetNumFragments(numSamples, pIn->numFrags);
    UINT_32 bpp;

    if (m_chipFamily == ADDR_CHIP_FAMILY_R8XX)
    {
        if (pIn->numSamples == 2)
        {
            numSamples = 4;
        }

        if (!pIn->resolved)
        {
            bpp          = ComputeFmaskNumPlanesFromNumSamples(numSamples);
            numSamples   = numSamples;
        }
        else
        {
            bpp          = ComputeFmaskResolvedBppFromNumSamples(numSamples);
            numSamples   = 1; // 1x sample
        }
    }
    else
    {
        if (numFrags != numSamples) // EQAA
        {
            ADDR_ASSERT(numFrags <= 8);

            if (!pIn->resolved)
            {
                if (numFrags == 1)
                {
                    bpp          = 1;
                    numSamples   = numSamples == 16 ? 16 : 8;
                }
                else if (numFrags == 2)
                {
                    ADDR_ASSERT(numSamples >= 4);

                    bpp          = 2;
                    numSamples   = numSamples;
                }
                else if (numFrags == 4)
                {
                    ADDR_ASSERT(numSamples >= 4);

                    bpp          = 4;
                    numSamples   = numSamples;
                }
                else // numFrags == 8
                {
                    ADDR_ASSERT(numSamples == 16);

                    bpp          = 4;
                    numSamples   = numSamples;
                }
            }
            else
            {
                if (numFrags == 1)
                {
                    bpp          = (numSamples == 16) ? 16 : 8;
                    numSamples   = 1;
                }
                else if (numFrags == 2)
                {
                    ADDR_ASSERT(numSamples >= 4);

                    bpp          = numSamples*2;
                    numSamples   = 1;
                }
                else if (numFrags == 4)
                {
                    ADDR_ASSERT(numSamples >= 4);

                    bpp          = numSamples*4;
                    numSamples   = 1;
                }
                else // numFrags == 8
                {
                    ADDR_ASSERT(numSamples >= 16);

                    bpp          = 16*4;
                    numSamples   = 1;
                }
            }
        }
        else // Normal AA
        {
            if (!pIn->resolved)
            {
                bpp          = ComputeFmaskNumPlanesFromNumSamples(numSamples);
                numSamples   = numSamples == 2 ? 8 : numSamples;
            }
            else
            {
                // The same as 8XX
                bpp          = ComputeFmaskResolvedBppFromNumSamples(numSamples);
                numSamples   = 1; // 1x sample
            }
        }
    }

    SafeAssign(pNumSamples, numSamples);

    return bpp;
}

/**
****************************************************************************************************
*   R800Lib::ComputeSurfaceInfoPowerSave
*
*   @brief
*       Compute power save tiled surface sizes include padded pitch, height, slices, total
*       size in bytes, meanwhile alignments as well. Results are returned through output
*       parameters.
*
*   @return
*       TRUE if no error occurs
****************************************************************************************************
*/
BOOL_32 R800Lib::ComputeSurfaceInfoPowerSave(
    const ADDR_COMPUTE_SURFACE_INFO_INPUT*  pIn,        ///< [in] Input structure
    ADDR_COMPUTE_SURFACE_INFO_OUTPUT*       pOut        ///< [out] Output structure
    ) const
{
    UINT_32 pitch = pIn->width;
    UINT_32 height = pIn->height;

    ComputeSurfaceAlignmentsPowerSave(pIn->bpp,
                                      pIn->flags,
                                      &pOut->baseAlign,
                                      &pOut->pitchAlign,
                                      &pOut->heightAlign);

    //
    // Pad pitch and height to the required granularities.
    //
    pitch = PowTwoAlign(pitch, pOut->pitchAlign);
    height = PowTwoAlign(height, pOut->pitchAlign);

    // pitch_elements * height * bytes_per_pixel must be a multiple of 256 bytes
    // So pad it to base alignment meets the requirement (from HW c code)
    UINT_32 bytesPerSlice = BITS_TO_BYTES(pitch * height * pIn->bpp);

    while (bytesPerSlice % pOut->baseAlign)
    {
        pitch += pOut->pitchAlign;

        bytesPerSlice = BITS_TO_BYTES(pitch * height * pIn->bpp);
    }

    //
    // Use SafeAssign since FMASK functions may call without valid pointer and it looks tidy
    //
    pOut->pitch = pitch;
    pOut->height = height;
    pOut->depth = 1;
    pOut->depthAlign = 1;

    pOut->surfSize = BITS_TO_BYTES(static_cast<UINT_64>(pitch) * height * pIn->bpp);

    pOut->tileMode = pIn->tileMode;

    return TRUE;
}

/**
****************************************************************************************************
*   EgBasedLib::ComputeSurfaceAlignmentsPowerSave
*
*   @brief
*       Compute power save tiled surface alignment, calculation results are returned through output
*       parameters.
*
*   @return
*       TRUE if no error occurs
****************************************************************************************************
*/
BOOL_32 R800Lib::ComputeSurfaceAlignmentsPowerSave(
    UINT_32             bpp,               ///< [in] bits per pixel
    ADDR_SURFACE_FLAGS  flags,             ///< [in] surface flags
    UINT_32*            pBaseAlign,        ///< [out] base address alignment in bytes
    UINT_32*            pPitchAlign,       ///< [out] pitch alignment in pixels
    UINT_32*            pHeightAlign       ///< [out] height alignment in pixels
    ) const
{
    BOOL_32 valid = TRUE;

    //1.    pitch_elements must be padded to a multiple of 8 elements
    //2.    base_address must be aligned to num_pipes * num_banks * row_size
    //3.    num_lower_pipes must less then or equal to num_pipes
    //4.    height_elements must be padded to a multiple of 8 elements

    *pPitchAlign = 8;
    *pHeightAlign = 8;

    //
    // The required alignment for base is num_pipes * num_banks * row_size
    //
    *pBaseAlign = m_pipes * m_banks * m_rowSize;

    AdjustPitchAlignment(flags, pPitchAlign);

    return valid;
}

/**
****************************************************************************************************
*   R800Lib::SanityCheckPowerSave
*
*   @brief
*       Check if power save tiled parameters are valid
*   @return
*       TRUE if valid
****************************************************************************************************
*/
BOOL_32 R800Lib::SanityCheckPowerSave(
    UINT_32   bpp,                ///< [in] Bits per pixel
    UINT_32   numSamples,         ///< [in] Number of samples
    UINT_32   mipLevel,           ///< [in] Current surface mip level
    UINT_32   numSlices           ///< [in] Number of slices
    ) const
{
    BOOL_32 valid = TRUE;

    if (m_chipFamily != ADDR_CHIP_FAMILY_NI)
    {
        valid = FALSE;
    }

    if (valid)
    {
        switch (bpp)
        {
            case 8:  //fall through
            case 16: //fall through
            case 32: //fall through
            case 64:
                break;
            default:
                valid = FALSE;
                break;
        }
    }

    if (valid)
    {
        if ((numSamples > 1) ||
            (numSlices > 1)  ||
            (mipLevel > 0)   ||
            (m_lowerPipes > m_pipes))
        {
            valid = FALSE;
        }
    }

    return valid;
}

/**
****************************************************************************************************
*   R800Lib::ComputeSurfaceAddrFromCoordPowerSave
*
*   @brief
*       Computes the surface address and bit position from a coordinate for power save tilied mode
*
*   @return
*       The byte address
****************************************************************************************************
*/
UINT_64 R800Lib::ComputeSurfaceAddrFromCoordPowerSave(
    UINT_32             x,                      ///< [in] x coordinate
    UINT_32             y,                      ///< [in] y coordinate
    UINT_32             slice,                  ///< [in] slice index
    UINT_32             bpp,                    ///< [in] bits per pixel
    UINT_32             pitch,                  ///< [in] pitch, in pixels
    UINT_32             height,                 ///< [in] height, in pixels
    ADDR_TILEINFO*      pTileInfo,              ///< [in] Tile info
    UINT_32*            pBitPosition            ///< [out] bit position, e.g. FMT_1 will use this
    ) const
{
    ADDR_ASSERT(m_class <= R800_ADDRLIB);
    UINT_64 addr = 0;

    const UINT_32 pipeInterleaveBytes = m_pipeInterleaveBytes;
    const UINT_32 numLowerPipes       = m_lowerPipes;
    const UINT_32 rowSize             = m_rowSize;
    const UINT_32 numPipes            = m_pipes;
    const UINT_32 numBanks            = m_banks;

    UINT_64 colLsb;
    UINT_64 colMsb;
    UINT_64 pipeLsb;
    UINT_64 pipeMsb;
    UINT_64 bank;
    UINT_64 row;

    //
    // Compute tile width, tile height and number of micro tiles per row.
    //
    UINT_32 tileWidth = (bpp > 32) ? 4 : 8;
    UINT_32 tileHeight = (bpp > 32) ? 2 : (64 / bpp);

    //
    // Compute the pixel index within the tile.
    //
    UINT_32 pixelIndex = ComputePixelIndexWithinPowerSave(x, y, slice, bpp);

    // Compute the pixel offset.
    //
    UINT_32 pixelOffset = pixelIndex * bpp / 8;

    //
    // Compute the offset to the tile containing the specified coordinate.
    //
    UINT_64 tileOffset = (static_cast<UINT_64>(y / tileHeight) * (pitch / tileWidth) + (x / tileWidth));

    tileOffset *= PowerSaveTileBytes; // Convert to bytes

    //
    // Extract the pipe, bank, column, and row from the tile_offset
    //
    colLsb  = (tileOffset % pipeInterleaveBytes);
    pipeLsb = (tileOffset / pipeInterleaveBytes) % numLowerPipes;
    colMsb  = (tileOffset / (pipeInterleaveBytes * numLowerPipes)) % (rowSize / pipeInterleaveBytes);
    bank    = (tileOffset / (rowSize * numLowerPipes)) % numBanks;
    pipeMsb = (tileOffset / (numBanks * numLowerPipes * rowSize)) % (numPipes / numLowerPipes);
    row     = (tileOffset / (numPipes * numBanks * rowSize));

    //
    // Place the pipe, bank, column, and row in the correct location and add element offset
    //
    addr = row * numPipes * numBanks * rowSize +
        colMsb * numPipes * numBanks * pipeInterleaveBytes +
        bank * numPipes * pipeInterleaveBytes +
        pipeMsb * numLowerPipes * pipeInterleaveBytes +
        pipeLsb * pipeInterleaveBytes +
        colLsb + pixelOffset;

    *pBitPosition = 0;

    return addr;
}

/**
****************************************************************************************************
*   R800Lib::ComputePixelIndexWithinPowerSave
*
*   @brief
*       Compute the pixel index inside a power save tile of surface
*
*   @return
*       Pixel index
*
****************************************************************************************************
*/
UINT_32 R800Lib::ComputePixelIndexWithinPowerSave(
    UINT_32         x,              ///< [in] x coord
    UINT_32         y,              ///< [in] y coord
    UINT_32         z,              ///< [in] slice/depth index
    UINT_32         bpp             ///< [in] bits per pixel
    ) const
{
    UINT_32 pixelBit0;
    UINT_32 pixelBit1;
    UINT_32 pixelBit2;
    UINT_32 pixelBit3;
    UINT_32 pixelBit4;
    UINT_32 pixelBit5;
    UINT_32 pixelNumber;

    // Compute the pixel number within the power save tile.

    switch (bpp)
    {
        case 8:
            pixelBit0 = (x & 0x1);
            pixelBit1 = (x & 0x2) >> 1;
            pixelBit2 = (x & 0x4) >> 2;
            pixelBit3 = (y & 0x2) >> 1;
            pixelBit4 = (y & 0x1);
            pixelBit5 = (y & 0x4) >> 2;
            break;
        case 16:
            pixelBit0 = (x & 0x1);
            pixelBit1 = (x & 0x2) >> 1;
            pixelBit2 = (x & 0x4) >> 2;
            pixelBit3 = (y & 0x1);
            pixelBit4 = (y & 0x2) >> 1;
            pixelBit5 = 0;
            break;
        case 32:
            pixelBit0 = (x & 0x1);
            pixelBit1 = (x & 0x2) >> 1;
            pixelBit2 = (y & 0x1);
            pixelBit3 = (x & 0x4) >> 2;
            pixelBit4 = 0;
            pixelBit5 = 0;
            break;
        case 64:
            pixelBit0 = (x & 0x1);
            pixelBit1 = (y & 0x1);
            pixelBit2 = (x & 0x2) >> 1;
            pixelBit3 = 0;
            pixelBit4 = 0;
            pixelBit5 = 0;
            break;
        default:
            ADDR_ASSERT_ALWAYS();

            pixelBit0 = 0;
            pixelBit1 = 0;
            pixelBit2 = 0;
            pixelBit3 = 0;
            pixelBit4 = 0;
            pixelBit5 = 0;
            break;
    }

    pixelNumber = ((pixelBit0     ) |
                   (pixelBit1 << 1) |
                   (pixelBit2 << 2) |
                   (pixelBit3 << 3) |
                   (pixelBit4 << 4) |
                   (pixelBit5 << 5));

    return pixelNumber;
}

/**
****************************************************************************************************
*   R800Lib::ComputeSurfaceCoordFromAddrPowerSave
*
*   @brief
*       Compute the coord from an address of a power save tiled surface
*
*   @return
*       N/A
****************************************************************************************************
*/
VOID R800Lib::ComputeSurfaceCoordFromAddrPowerSave(
    UINT_64         addr,               ///< [in] address
    UINT_32         bitPosition,        ///< [in] bitPosition in a byte
    UINT_32         bpp,                ///< [in] bits per pixel
    UINT_32         pitch,              ///< [in] pitch
    UINT_32         height,             ///< [in] height
    ADDR_TILEINFO*  pTileInfo,          ///< [in] Tile info
    UINT_32*        pX,                 ///< [out] x coord
    UINT_32*        pY                  ///< [out] y coord
    ) const
{
    const UINT_32 groupBits     = BYTES_TO_BITS(m_pipeInterleaveBytes);
    const UINT_32 rowBits       = BYTES_TO_BITS(m_rowSize);
    const UINT_32 numLowerPipes = m_lowerPipes;
    const UINT_32 numPipes      = m_pipes;
    const UINT_32 numBanks      = m_banks;

    UINT_64 bitAddr;
    UINT_32 elemOffset;
    UINT_32 tilesPerRow;
    UINT_64 tileOffset;
    UINT_64 tileOffsetPre;
    UINT_64 tileIndex;

    UINT_32 tileWidth;
    UINT_32 tileHeight;
    UINT_32 colLsb;
    UINT_32 colMsb;
    UINT_32 pipeLsb;
    UINT_32 pipeMsb;
    UINT_32 bank;
    UINT_32 row;

    ADDR_ASSERT(bitPosition == 0);

    //
    // Compute tile width, tile height and number of micro tiles per row.
    //
    tileWidth = (bpp > 32) ? 4 : 8;
    tileHeight = (bpp > 32) ? 2 : (64 / bpp);
    tilesPerRow = pitch / tileWidth;

    //
    // Convert byte address to bit address.
    //
    bitAddr = BYTES_TO_BITS(addr);

    //
    // Compute pixel offset coord in a tile.
    //
    elemOffset = (UINT_32)(bitAddr % BYTES_TO_BITS(PowerSaveTileBytes));
    ComputePixelCoordFromOffsetPowerSave(elemOffset, bpp, pX, pY);

    //
    // Compute tile offset.
    //
    tileOffsetPre = bitAddr - elemOffset;
    colLsb = (UINT_32)(tileOffsetPre % groupBits);
    pipeLsb = (UINT_32)((tileOffsetPre / groupBits) % numLowerPipes);
    pipeMsb = (UINT_32)((tileOffsetPre / groupBits / numLowerPipes) % ( numPipes / numLowerPipes));
    bank = (UINT_32)(( tileOffsetPre / groupBits / numPipes) % numBanks);
    colMsb = (UINT_32)((tileOffsetPre / groupBits / numPipes / numBanks) % ( rowBits / groupBits));
    row = (UINT_32)(tileOffsetPre / rowBits / numPipes / numBanks);

    tileOffset = (UINT_64)row * rowBits * numPipes * numBanks +
        pipeMsb * numBanks * (rowBits / groupBits) * numLowerPipes * groupBits +
        bank * (rowBits / groupBits) * numLowerPipes * groupBits +
        colMsb * numLowerPipes * groupBits +
        pipeLsb * groupBits+
        colLsb;

    //
    // Convert tile offset coord.
    //
    tileIndex =  tileOffset / BYTES_TO_BITS(PowerSaveTileBytes);

    *pX += (UINT_32)((tileIndex % tilesPerRow) * tileWidth);
    *pY += (UINT_32)((tileIndex / tilesPerRow) * tileHeight);
}

/**
****************************************************************************************************
*   R800Lib::ComputePixelCoordFromOffsetPowerSave
*
*   @brief
*       Compute pixel coordinate from offset inside a power save tile
*   @return
*       N/A
****************************************************************************************************
*/
VOID R800Lib::ComputePixelCoordFromOffsetPowerSave(
    UINT_32         offset,             ///< [in] offset in side micro tile in bits
    UINT_32         bpp,                ///< [in] bits per pixel
    UINT_32*        pX,                 ///< [out] x coordinate
    UINT_32*        pY                  ///< [out] y coordinate
    ) const
{
    const UINT_32 pixelIndex = offset / bpp;

    UINT_32 x;
    UINT_32 y;

    switch (bpp)
    {
        case 8:
            x = pixelIndex & 0x7;
            y = Bits2Number(3, _BIT(pixelIndex,5),_BIT(pixelIndex,3),_BIT(pixelIndex,4));
            break;
        case 16:
            x = pixelIndex & 0x7;
            y = Bits2Number(2, _BIT(pixelIndex,4),_BIT(pixelIndex,3));
            break;
        case 32:
            x = Bits2Number(3, _BIT(pixelIndex,3),_BIT(pixelIndex,1),_BIT(pixelIndex,0));
            y = (pixelIndex & 0x4) >> 2;
            break;
        case 64:
            x = Bits2Number(2, _BIT(pixelIndex,2),_BIT(pixelIndex,0));
            y = (pixelIndex & 0x2) >> 1;
            break;
        default:
            ADDR_ASSERT_ALWAYS();

            x = 0;
            y = 0;
            break;
    }

    *pX = x;
    *pY = y;
}

/**
****************************************************************************************************
*   R800Lib::ComputeDefaultBank
*
*   @brief
*       Compute a default number of banks
*   @return
*       Default number of banks (2,4,8,16)
****************************************************************************************************
*/
UINT_32 R800Lib::ComputeDefaultBank(
    AddrTileMode        tileMode,           ///< [in] tile mode
    UINT_32             bpp,                ///< [in] bits per pixel
    ADDR_SURFACE_FLAGS  flags,              ///< [in] surface flags
    UINT_32             numSamples,         ///< [in] number of samples
    UINT_32             pitch,              ///< [in] pitch in bytes
    UINT_32             height,             ///< [in] height in bytes
    UINT_32             tileSplitBytes      ///< [in] tile spilt size in bytes
    ) const
{
    UINT_32 optimalBanks;

    // Client should tell me physical banks
    ADDR_ASSERT(m_banks != 0);

    UINT_32 logicalBanks = m_logicalBanks;

    // TODO: Is 64 and 8*bank the optimal value??

    if ((pitch >= 64) && (height >= 8 * logicalBanks))
    {
        optimalBanks = logicalBanks; // 2D tiling is fine for this size
    }
    else if (IsMacro3dTiled(tileMode))
    {
        optimalBanks = logicalBanks;
        ADDR_ASSERT(optimalBanks > m_pipes / 2 - 1);
    }
    else if ((m_pipes == 1) && (logicalBanks <= 4))
    {
        optimalBanks = logicalBanks;
        ADDR_ASSERT(logicalBanks == 4);
    }
    else
    {
        UINT_32 microTileThickness = Thickness(tileMode);
        UINT_32 microTileBits = bpp * microTileThickness * MicroTilePixels * numSamples;
        UINT_32 microTileBytes = BITS_TO_BYTES(microTileBits);

        microTileBytes = Min(tileSplitBytes, microTileBytes);

        if (microTileBytes > 1024 && logicalBanks >= 8)
        {
            optimalBanks = logicalBanks >> 1;
        }
        else
        {
            optimalBanks = logicalBanks;
        }
    }

    return optimalBanks;
}

} // V1
} // Addr
