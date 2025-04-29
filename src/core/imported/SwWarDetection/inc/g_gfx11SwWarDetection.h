/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __GFX11_SW_WAR_DETECTION_H__
#define __GFX11_SW_WAR_DETECTION_H__

#include <cstdint>
#include <cstdlib>

#if SWD_STRINGIFY
#include <cstring>
#endif

// Forward declarations:
union Gfx11SwWarDetection;

// =====================================================================================================================
// Determines the target major, minor, and stepping from the specified chip revision.
// Returns true if a target was found, false otherwise. If false, the values in pMajor, pMinor, and pStepping are
// invalid.
extern bool DetermineGfx11Target(
    uint32_t  familyId,
    uint32_t  eRevId,
    uint32_t* pMajor,
    uint32_t* pMinor,
    uint32_t* pStepping);

// =====================================================================================================================
// Main entrypoint. Returns, in place, which hardware bugs require software workarounds for the specified chip revision.
// Returns true if an ASIC was detected. False otherwise.
extern bool DetectGfx11SoftwareWorkaroundsByChip(
    uint32_t             familyId,
    uint32_t             eRevId,
    Gfx11SwWarDetection* pWorkarounds);

// =====================================================================================================================
// Main entrypoint. Returns, in place, which hardware bugs require software workarounds for the specified target based
// on major, minor, and stepping.
// Returns true if an ASIC was detected. False otherwise.
extern bool DetectGfx11SoftwareWorkaroundsByGfxIp(
    uint32_t             major,
    uint32_t             minor,
    uint32_t             stepping,
    Gfx11SwWarDetection* pWorkarounds);

// Number of workarounds that are represented in Gfx11SwWarDetection.
constexpr uint32_t Gfx11NumWorkarounds = 56;

// Number of DWORDs that make up the Gfx11SwWarDetection structure.
constexpr uint32_t Gfx11StructDwords = 2;

// Array of masks representing inactive workaround bits.
// It is expected that the client who consumes these headers will check the u32All of the structure
// against their own copy of the inactive mask. This one is provided as reference.
constexpr uint32_t Gfx11InactiveMask[] =
{
    0x00000000,
    0xff000000,
};

// Bitfield structure containing all workarounds active for the Gfx11 family.
union Gfx11SwWarDetection
{
    struct
    {
        uint32_t ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  : 1;

        uint32_t shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             : 1;

        uint32_t shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_B_                                                                                          : 1;

        uint32_t shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_                                                                                          : 1;

        uint32_t shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          : 1;

        uint32_t shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_                                                                                                   : 1;

        uint32_t shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      : 1;

        uint32_t shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_                                                                                                               : 1;

        uint32_t ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                : 1;

        uint32_t ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             : 1;

        uint32_t geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        : 1;

        uint32_t ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               : 1;

        uint32_t sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                : 1;

        uint32_t shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        : 1;

        uint32_t shaderSqSqgPCSentToSQThrCmdBusMayBeDropped_A_                                                                                                                        : 1;

        uint32_t sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          : 1;

        uint32_t ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 : 1;

        uint32_t sioSpiBciSoftLockIssue_A_                                                                                                                                            : 1;

        uint32_t sioSpiBciSpyGlassRevealedABugInSpiRaRscselGsThrottleModuleWhichIsCausedByGsPsVgprLdsInUsesVariableDroppingMSBInRelevantMathExpression_A_                             : 1;

        uint32_t geometryPaStereoPositionNanCheckBug_A_                                                                                                                               : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t geometryPaPALineStippleResetError_A_                                                                                                                                 : 1;

        uint32_t gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                : 1;

        uint32_t ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   : 1;

        uint32_t sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  : 1;

        uint32_t textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         : 1;

        uint32_t cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  : 1;

        uint32_t shaderSqcMissingTTTokens_A_                                                                                                                                          : 1;

        uint32_t controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  : 1;

        uint32_t textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      : 1;

#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             : 1;

        uint32_t shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   : 1;

        uint32_t geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             : 1;

#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_PHX2|| SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      : 1;

        uint32_t controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t entireSubsystemUndershootCausesHighDroop_A_                                                                                                                          : 1;

        uint32_t                                                                                                                                                                      : 1;

        uint32_t ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   : 1;

        uint32_t ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ : 1;

#if SWD_BUILD_STRIX1
        uint32_t shaderLdsPotentialIssueValdnGCTheBehaviorChangeInDsWriteB8_A_                                                                                                        : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           : 1;

#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            : 1;

        uint32_t                                                                                                                                                                      : 1;

#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
        uint32_t geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_                                                                                       : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t reserved                                                                                                                                                             : 8;
    };

    uint32_t u32All[Gfx11StructDwords];
};

static_assert(sizeof(Gfx11InactiveMask) == sizeof(Gfx11SwWarDetection),
              "Size of the inactive mask is different than the workaround structure.");

namespace swd_internal
{

// =====================================================================================================================
void DetectNavi31A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_                                                                                                               = 1;
    pWorkarounds->shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_                                                                                                   = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgPCSentToSQThrCmdBusMayBeDropped_A_                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_                                                                                          = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
    pWorkarounds->textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           = 1;
}

// =====================================================================================================================
void DetectNavi32A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_                                                                                                               = 1;
    pWorkarounds->shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_                                                                                                   = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_                                                                                          = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
    pWorkarounds->textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           = 1;
}

// =====================================================================================================================
void DetectNavi32GLXLWorkarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->entireSubsystemUndershootCausesHighDroop_A_                                                                                                                          = 1;
    pWorkarounds->gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_                                                                                                               = 1;
    pWorkarounds->shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_                                                                                                   = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_                                                                                          = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
    pWorkarounds->textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           = 1;
}

// =====================================================================================================================
void DetectNavi33A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_                                                                                                               = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgPCSentToSQThrCmdBusMayBeDropped_A_                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_                                                                                          = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSpiBciSpyGlassRevealedABugInSpiRaRscselGsThrottleModuleWhichIsCausedByGsPsVgprLdsInUsesVariableDroppingMSBInRelevantMathExpression_A_                             = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
    pWorkarounds->textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           = 1;
}

// =====================================================================================================================
void DetectPhoenix1A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_                                                                                                = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_B_                                                                                          = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
}

#if SWD_BUILD_PHX2
// =====================================================================================================================
void DetectPhoenix2A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_                                                                                                               = 1;
    pWorkarounds->ppDbPWSIssueForDepthWrite_TextureRead_A_                                                                                                                             = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_                                                                                                  = 1;
    pWorkarounds->shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_                                                                                          = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_                                                                                                      = 1;
    pWorkarounds->shaderSpfailedToDetectPartialForwardingStall_A_                                                                                                                      = 1;
    pWorkarounds->shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_                                                                                                             = 1;
    pWorkarounds->shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            = 1;
    pWorkarounds->shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_                                                                                        = 1;
    pWorkarounds->shaderSqSqgSQGTTWPTRIssues_A_                                                                                                                                        = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->shaderSqcMissingTTTokens_A_                                                                                                                                          = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_  = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_                                                                                                                      = 1;
    pWorkarounds->textureTaGfx11TAUnableToSupportScratchSVS_A_                                                                                                                         = 1;
}
#endif

#if SWD_BUILD_GAINSBOROUGH
// =====================================================================================================================
void DetectGainsboroughA0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_                                                                                       = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     = 1;
    pWorkarounds->shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_                                                                                          = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_                                                                                                                = 1;
    pWorkarounds->textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         = 1;
}
#endif

#if SWD_BUILD_STRIX1
// =====================================================================================================================
void DetectStrix1A0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_                                                                                       = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                = 1;
    pWorkarounds->ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_                                                                                                   = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     = 1;
    pWorkarounds->shaderLdsPotentialIssueValdnGCTheBehaviorChangeInDsWriteB8_A_                                                                                                        = 1;
    pWorkarounds->shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         = 1;
}
#endif

#if SWD_BUILD_STRIX1
// =====================================================================================================================
void DetectStrix1B0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_                                                                                       = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     = 1;
    pWorkarounds->shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         = 1;
}
#endif

#if SWD_BUILD_STRIX_HALO
// =====================================================================================================================
void DetectStrixHaloA0Workarounds(
    Gfx11SwWarDetection* pWorkarounds)
{
    pWorkarounds->cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_                                                                                                                  = 1;
    pWorkarounds->controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_                                                                                      = 1;
    pWorkarounds->controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             = 1;
    pWorkarounds->controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_                                                                                               = 1;
    pWorkarounds->geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_                                                                                       = 1;
    pWorkarounds->geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_                                                                                                        = 1;
    pWorkarounds->geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             = 1;
    pWorkarounds->geometryPaPALineStippleResetError_A_                                                                                                                                 = 1;
    pWorkarounds->geometryPaStereoPositionNanCheckBug_A_                                                                                                                               = 1;
    pWorkarounds->ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_                                                                                                                = 1;
    pWorkarounds->ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_                                                                                                = 1;
    pWorkarounds->ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ = 1;
    pWorkarounds->ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_                                                 = 1;
    pWorkarounds->ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_                                                                                                                   = 1;
    pWorkarounds->ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     = 1;
    pWorkarounds->shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       = 1;
    pWorkarounds->shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_                                                                                                                  = 1;
    pWorkarounds->shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     = 1;
    pWorkarounds->shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   = 1;
    pWorkarounds->sioSpiBciSoftLockIssue_A_                                                                                                                                            = 1;
    pWorkarounds->textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         = 1;
}
#endif

// =====================================================================================================================
// Perform overrides to the workaround structure.
static void Gfx11OverrideDefaults(
    Gfx11SwWarDetection* pWorkarounds)
{
    char* pOverride = getenv("SWD_GFX11_OVERRIDE");
    char* pMask     = getenv("SWD_GFX11_MASK");

    if ((pOverride != nullptr) && (pMask != nullptr))
    {
        char* pOverrideEnd = pOverride;
        char* pMaskEnd     = pMask;

        uint32_t i = 0;
        while ((pOverrideEnd != nullptr) && (pMaskEnd != nullptr) && (i < Gfx11StructDwords))
        {
            uint32_t overrideVal = strtol(pOverrideEnd, &pOverrideEnd, 16);
            uint32_t maskVal     = strtol(pMaskEnd,     &pMaskEnd,     16);
            pWorkarounds->u32All[i] = (pWorkarounds->u32All[i] & ~maskVal) | (maskVal & overrideVal);

            pOverrideEnd = (pOverrideEnd[0] != '\0') ? pOverrideEnd + 1 : nullptr;
            pMaskEnd     = (pMaskEnd[0]     != '\0') ? pMaskEnd     + 1 : nullptr;
            i++;
        }
    }
}

} // namespace swd_internal

#if SWD_STRINGIFY
// =====================================================================================================================
std::string StringifyActiveGfx11Workarounds(
    const Gfx11SwWarDetection& workarounds)
{
    std::string output = "Workarounds enabled for Gfx11:\n";

    if (workarounds.ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_ != 0)
    {
        output += " - ppPbbPBBBreakBatchDifferenceWithPrimLimit_FpovLimit_DeallocLimit_A_\n";
    }
    if (workarounds.shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_ != 0)
    {
        output += " - shaderSpsubvectorExecutionSubv1SharedVgprGotWrongData_A_\n";
    }
    if (workarounds.shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_B_ != 0)
    {
        output += " - shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_B_\n";
    }
    if (workarounds.shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_ != 0)
    {
        output += " - shaderSqSqgSQPERFSNAPSHOT_ClockCyclesCountingModeDoesNotConsiderVMIDMASK_A_\n";
    }
    if (workarounds.shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_ != 0)
    {
        output += " - shaderSpQSADAndMADI64U64SrcDataCorruptionDueToIntraInstructionForwarding_A_\n";
    }
    if (workarounds.shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_ != 0)
    {
        output += " - shaderSpDPPStallDueToExecutionMaskForwardingMissesPermlane16_x__A_\n";
    }
    if (workarounds.shaderSpfailedToDetectPartialForwardingStall_A_ != 0)
    {
        output += " - shaderSpfailedToDetectPartialForwardingStall_A_\n";
    }
    if (workarounds.shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_ != 0)
    {
        output += " - shaderSqSqgSCLAUSEFollowedByVALU_SDELAYALUCoIssuePairCanExceedClauseLength_A_\n";
    }
    if (workarounds.ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_ != 0)
    {
        output += " - ppPbbPBBMayErroneouslyDropBinsWhenConfiguredTo24SEs_A_\n";
    }
    if (workarounds.ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_ != 0)
    {
        output += " - ppDbDBDEBUG__FORCEMISSIFNOTINFLIGHTCausesADeadlockBetweenDbDtt_Osb_A_\n";
    }
    if (workarounds.ppDbPWSIssueForDepthWrite_TextureRead_A_ != 0)
    {
        output += " - ppDbPWSIssueForDepthWrite_TextureRead_A_\n";
    }
    if (workarounds.geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_ != 0)
    {
        output += " - geometryGeGEWdTe11ClockCanStayHighAfterShaderMessageThdgrp_A_\n";
    }
    if (workarounds.ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_ != 0)
    {
        output += " - ppDbLostSamplesForRB_QuadsAt16xaaMayCauseCorruption_A_\n";
    }
    if (workarounds.sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_ != 0)
    {
        output += " - sioSxvmidResetForMrtZOnlyPixelShaderHitSXAssertion_A_\n";
    }
    if (workarounds.shaderSqSqgSQGTTWPTRIssues_A_ != 0)
    {
        output += " - shaderSqSqgSQGTTWPTRIssues_A_\n";
    }
    if (workarounds.shaderSqSqgPCSentToSQThrCmdBusMayBeDropped_A_ != 0)
    {
        output += " - shaderSqSqgPCSentToSQThrCmdBusMayBeDropped_A_\n";
    }
    if (workarounds.sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_ != 0)
    {
        output += " - sioPcSioSpiBciSPIAndPCCanGetOutOfSyncForNoLdsInitWavesWhenEXTRALDSSIZE_0_A_\n";
    }
    if (workarounds.ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_ != 0)
    {
        output += " - ppDbPWS_RtlTimeout_TimeStampEventPwsStall_eopDoneNotSentForOldestTSWaitingForSyncComplete__FlusherStalledInOpPipe_A_\n";
    }
    if (workarounds.sioSpiBciSoftLockIssue_A_ != 0)
    {
        output += " - sioSpiBciSoftLockIssue_A_\n";
    }
    if (workarounds.sioSpiBciSpyGlassRevealedABugInSpiRaRscselGsThrottleModuleWhichIsCausedByGsPsVgprLdsInUsesVariableDroppingMSBInRelevantMathExpression_A_ != 0)
    {
        output += " - sioSpiBciSpyGlassRevealedABugInSpiRaRscselGsThrottleModuleWhichIsCausedByGsPsVgprLdsInUsesVariableDroppingMSBInRelevantMathExpression_A_\n";
    }
    if (workarounds.geometryPaStereoPositionNanCheckBug_A_ != 0)
    {
        output += " - geometryPaStereoPositionNanCheckBug_A_\n";
    }
    if (workarounds.geometryPaPALineStippleResetError_A_ != 0)
    {
        output += " - geometryPaPALineStippleResetError_A_\n";
    }
    if (workarounds.gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_ != 0)
    {
        output += " - gcPvPpCbCBPerfcountersStuckAtZeroAfterPerfcounterStopEventReceived_A_\n";
    }
    if (workarounds.ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_ != 0)
    {
        output += " - ppDbPpScSCDBHangNotSendingWaveConflictBackToSPI_A_\n";
    }
    if (workarounds.sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_ != 0)
    {
        output += " - sioSpiBciSPI_TheOverRestrictedExportConflictHQ_HoldingQueue_PtrRuleMayReduceTheTheoreticalExpGrantThroughput_PotentiallyIncreaseOldNewPSWavesInterleavingChances_A_\n";
    }
    if (workarounds.textureTaGfx11TAUnableToSupportScratchSVS_A_ != 0)
    {
        output += " - textureTaGfx11TAUnableToSupportScratchSVS_A_\n";
    }
    if (workarounds.cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_ != 0)
    {
        output += " - cmmGl2GL2WriteAfterReadOrderingIssueDuringGL2INV_A_\n";
    }
    if (workarounds.shaderSqcMissingTTTokens_A_ != 0)
    {
        output += " - shaderSqcMissingTTTokens_A_\n";
    }
    if (workarounds.controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_ != 0)
    {
        output += " - controlRlcHw36RlcSpmIsAlwaysBusyIfISpmStopIssuedSoCloseToPerfSample_A_\n";
    }
    if (workarounds.shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_ != 0)
    {
        output += " - shaderSpSPSrcOperandInvalidatedByTdLdsDataReturn_A_\n";
    }
    if (workarounds.textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_ != 0)
    {
        output += " - textureTaGfx11ImageMsaaLoadNotHonoringDstSel_A_\n";
    }
#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_ != 0)
    {
        output += " - shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_\n";
    }
#endif
    if (workarounds.controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_ != 0)
    {
        output += " - controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_\n";
    }
    if (workarounds.shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_ != 0)
    {
        output += " - shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_\n";
    }
    if (workarounds.geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_ != 0)
    {
        output += " - geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_\n";
    }
#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_PHX2|| SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_ != 0)
    {
        output += " - ppCbFDCCKeysWithFragComp_MSAASettingCauseHangsInCB_A_\n";
    }
#endif
    if (workarounds.shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_ != 0)
    {
        output += " - shaderSpTranscendentalOpFollowedByALUDoesntEnforceDependency_A_\n";
    }
    if (workarounds.controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_ != 0)
    {
        output += " - controlCp1PHXRS64D_RS64MemoryRAWCoherencyIsBrokenOnAsyncHeavyWeightShootdown_A_\n";
    }
    if (workarounds.entireSubsystemUndershootCausesHighDroop_A_ != 0)
    {
        output += " - entireSubsystemUndershootCausesHighDroop_A_\n";
    }
    if (workarounds.ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_ != 0)
    {
        output += " - ppCbGFX11DCC31DXXPNeedForSpeedHeat_BlackFlickeringDotCorruption_A_\n";
    }
    if (workarounds.ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_ != 0)
    {
        output += " - ppDbDBOreoOpaqueModeHWBug_UdbOreoScoreBoard_udbOsbData_udbOsbdMonitor_ostSampleMaskMismatchOREOScoreboardStoresInvalidEWaveIDAndIncorrectlySetsRespectiveValidBit_A_\n";
    }
#if SWD_BUILD_STRIX1
    if (workarounds.shaderLdsPotentialIssueValdnGCTheBehaviorChangeInDsWriteB8_A_ != 0)
    {
        output += " - shaderLdsPotentialIssueValdnGCTheBehaviorChangeInDsWriteB8_A_\n";
    }
#endif
#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_ != 0)
    {
        output += " - textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_\n";
    }
#endif
    if (workarounds.textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_ != 0)
    {
        output += " - textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_\n";
    }
#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_ != 0)
    {
        output += " - ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_\n";
    }
#endif
#if SWD_BUILD_GAINSBOROUGH   || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_ != 0)
    {
        output += " - shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_\n";
    }
#endif
    if (workarounds.shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_ != 0)
    {
        output += " - shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_\n";
    }
#if SWD_BUILD_GAINSBOROUGH    || SWD_BUILD_STRIX1|| SWD_BUILD_STRIX_HALO
    if (workarounds.geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_ != 0)
    {
        output += " - geometryGeDRAWOPAQUERegUpdatesWithin5CyclesOnDifferentContextsCausesGEIssue_A_\n";
    }
#endif

    return output;
}

// =====================================================================================================================
std::string StringifyActiveGfx11Workarounds(
    const uint32_t* pWorkarounds)
{
    Gfx11SwWarDetection workarounds = {};
    memcpy(&workarounds.u32All[0], pWorkarounds, Gfx11StructDwords * sizeof(uint32_t));
    return StringifyActiveGfx11Workarounds(workarounds);
}
#endif

// =====================================================================================================================
bool DetermineGfx11Target(
    uint32_t        familyId,
    uint32_t        eRevId,
    uint32_t*       pMajor,
    uint32_t*       pMinor,
    uint32_t*       pStepping)
{
    bool successful = false;

    if (false)
    {
    }
    else if (familyId == 145)
    {
        if (false)
        {
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 0;
            successful   = true;
        }
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 2;
            successful   = true;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 1;
            successful   = true;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 5;
            successful   = true;
        }
    }
    else if (familyId == 148)
    {
        if (false)
        {
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 3;
            successful   = true;
        }
#if SWD_BUILD_PHX2
        else if ((0x80 <= eRevId) && (eRevId < 0xFF))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 3;
            successful   = true;
        }
#endif
    }
#if SWD_BUILD_STRIX
    else if (familyId == 150)
    {
        if (false)
        {
        }
#if SWD_BUILD_STRIX1
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 65535;
            successful   = true;
        }
#endif
#if SWD_BUILD_STRIX1
        else if ((0x10 <= eRevId) && (eRevId < 0x40))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 0;
            successful   = true;
        }
#endif
#if SWD_BUILD_STRIX_HALO
        else if ((0xC0 <= eRevId) && (eRevId < 0xD0))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 1;
            successful   = true;
        }
#endif
#if SWD_BUILD_GAINSBOROUGH
        else if ((0xD0 <= eRevId) && (eRevId < 0xE0))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 65532;
            successful   = true;
        }
#endif
    }
#endif

    return successful;
}

// =====================================================================================================================
bool DetectGfx11SoftwareWorkaroundsByChip(
    uint32_t             familyId,
    uint32_t             eRevId,
    Gfx11SwWarDetection* pWorkarounds)
{
    bool successful = false;

    if (false)
    {
    }
    else if (familyId == 145)
    {
        if (false)
        {
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectNavi31A0Workarounds(pWorkarounds);
            successful = true;
        }
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            swd_internal::DetectNavi33A0Workarounds(pWorkarounds);
            successful = true;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            swd_internal::DetectNavi32A0Workarounds(pWorkarounds);
            successful = true;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            swd_internal::DetectNavi32GLXLWorkarounds(pWorkarounds);
            successful = true;
        }
    }
    else if (familyId == 148)
    {
        if (false)
        {
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectPhoenix1A0Workarounds(pWorkarounds);
            successful = true;
        }
#if SWD_BUILD_PHX2
        else if ((0x80 <= eRevId) && (eRevId < 0xFF))
        {
            swd_internal::DetectPhoenix2A0Workarounds(pWorkarounds);
            successful = true;
        }
#endif
    }
#if SWD_BUILD_STRIX
    else if (familyId == 150)
    {
        if (false)
        {
        }
#if SWD_BUILD_STRIX1
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectStrix1A0Workarounds(pWorkarounds);
            successful = true;
        }
#endif
#if SWD_BUILD_STRIX1
        else if ((0x10 <= eRevId) && (eRevId < 0x40))
        {
            swd_internal::DetectStrix1B0Workarounds(pWorkarounds);
            successful = true;
        }
#endif
#if SWD_BUILD_STRIX_HALO
        else if ((0xC0 <= eRevId) && (eRevId < 0xD0))
        {
            swd_internal::DetectStrixHaloA0Workarounds(pWorkarounds);
            successful = true;
        }
#endif
#if SWD_BUILD_GAINSBOROUGH
        else if ((0xD0 <= eRevId) && (eRevId < 0xE0))
        {
            swd_internal::DetectGainsboroughA0Workarounds(pWorkarounds);
            successful = true;
        }
#endif
    }
#endif

    if (successful)
    {
        swd_internal::Gfx11OverrideDefaults(pWorkarounds);
    }

    return successful;
}

// =====================================================================================================================
bool DetectGfx11SoftwareWorkaroundsByGfxIp(
    uint32_t             major,
    uint32_t             minor,
    uint32_t             stepping,
    Gfx11SwWarDetection* pWorkarounds)
{
    bool successful = false;

    if (false)
    {
    }
    else if ((major == 11) && (minor == 0) && (stepping == 0))
    {
        swd_internal::DetectNavi31A0Workarounds(pWorkarounds);
        successful = true;
    }
    else if ((major == 11) && (minor == 0) && (stepping == 1))
    {
        swd_internal::DetectNavi32A0Workarounds(pWorkarounds);
        successful = true;
    }
    else if ((major == 11) && (minor == 0) && (stepping == 2))
    {
        swd_internal::DetectNavi33A0Workarounds(pWorkarounds);
        successful = true;
    }
    else if ((major == 11) && (minor == 0) && (stepping == 3))
    {
        swd_internal::DetectPhoenix1A0Workarounds(pWorkarounds);
        successful = true;
#if SWD_BUILD_PHX2
        swd_internal::DetectPhoenix2A0Workarounds(pWorkarounds);
        successful = true;
#endif
    }
    else if ((major == 11) && (minor == 0) && (stepping == 5))
    {
        swd_internal::DetectNavi32GLXLWorkarounds(pWorkarounds);
        successful = true;
    }
#if SWD_BUILD_STRIX1
    else if ((major == 11) && (minor == 5) && (stepping == 0))
    {
        swd_internal::DetectStrix1B0Workarounds(pWorkarounds);
        successful = true;
    }
#endif
#if SWD_BUILD_STRIX_HALO
    else if ((major == 11) && (minor == 5) && (stepping == 1))
    {
        swd_internal::DetectStrixHaloA0Workarounds(pWorkarounds);
        successful = true;
    }
#endif
#if SWD_BUILD_GAINSBOROUGH
    else if ((major == 11) && (minor == 5) && (stepping == 65532))
    {
        swd_internal::DetectGainsboroughA0Workarounds(pWorkarounds);
        successful = true;
    }
#endif
#if SWD_BUILD_STRIX1
    else if ((major == 11) && (minor == 5) && (stepping == 65535))
    {
        swd_internal::DetectStrix1A0Workarounds(pWorkarounds);
        successful = true;
    }
#endif
    if (successful)
    {
        swd_internal::Gfx11OverrideDefaults(pWorkarounds);
    }

    return successful;
}

#endif
