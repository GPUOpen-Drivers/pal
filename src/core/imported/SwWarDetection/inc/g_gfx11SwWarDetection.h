/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
constexpr uint32_t Gfx11NumWorkarounds = 54;

// Number of DWORDs that make up the Gfx11SwWarDetection structure.
constexpr uint32_t Gfx11StructDwords = 2;

// Array of masks representing inactive workaround bits.
// It is expected that the client who consumes these headers will check the u32All of the structure
// against their own copy of the inactive mask. This one is provided as reference.
constexpr uint32_t Gfx11InactiveMask[] =
{
    0x00000000,
    0xffc00000,
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

        uint32_t cmmUtcl0UTCL0PrefetchRequest_permissions_0_Issue_A_                                                                                                                  : 1;

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

#if     SWD_BUILD_STRIX1
        uint32_t shaderSpFalsePositiveVGPRWriteKillForDUALOpcodeInstructions_A_                                                                                                       : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t controlCpUTCL1CAMInCPGGotErrorMoreThenOneCAMEntryMatchedWhenDCOffsetAddressIsSamePAWithMQDBaseAddress_A_                                                             : 1;

        uint32_t shaderSqSqgWave64VALUReadSGPRMaskToSALUDepdency_A_                                                                                                                   : 1;

        uint32_t geometryGeSioPcSioSpiBciATMDeallocsDoNotWaitForGSDONE_A_                                                                                                             : 1;

#if     SWD_BUILD_PHX2 || SWD_BUILD_STRIX1
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

#if     SWD_BUILD_STRIX1
        uint32_t textureTcpGfx11_5MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                         : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t textureTcpGfx11MainTCPHangsWhenSClauseHasTooManyInstrWithNoValidThreads_A_                                                                                           : 1;

#if      SWD_BUILD_STRIX1
        uint32_t ppSc1ApexLegendsImageCorruptionInZPrePassMode_A_                                                                                                                     : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

#if     SWD_BUILD_STRIX1
        uint32_t shaderSqSqgShaderHangDueToSQInstructionStore_IS_CacheDeadlock_A_                                                                                                     : 1;
#else
        uint32_t                                                                                                                                                                      : 1;
#endif

        uint32_t shaderSqSqgNV3xHWBugCausesHangOnCWSRWhenTGCreatedOnSA1_A_                                                                                                            : 1;

        uint32_t reserved                                                                                                                                                             : 10;
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

#if SWD_BUILD_STRIX1
// =====================================================================================================================
void DetectStrix1A0Workarounds(
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

// =====================================================================================================================
bool DetermineGfx11Target(
    uint32_t        familyId,
    uint32_t        eRevId,
    uint32_t*       pMajor,
    uint32_t*       pMinor,
    uint32_t*       pStepping)
{
    bool successful = true;

    if (false)
    {
    }
    else if (familyId == 145)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 0;
        }
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 2;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 1;
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 5;
        }
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
    }
    else if (familyId == 148)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 3;
        }
#if SWD_BUILD_PHX2
        else if ((0x80 <= eRevId) && (eRevId < 0xFF))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 0;
            (*pStepping) = 3;
        }
#endif
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
    }
#if SWD_BUILD_STRIX
    else if (familyId == 150)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
#if SWD_BUILD_STRIX1
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 65535;
        }
#endif
#if SWD_BUILD_STRIX1
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            (*pMajor)    = 11;
            (*pMinor)    = 5;
            (*pStepping) = 0;
        }
#endif
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
    }
#endif
    else
    {
        // No family detected. Return false.
        successful = false;
    }

    return successful;
}

// =====================================================================================================================
bool DetectGfx11SoftwareWorkaroundsByChip(
    uint32_t             familyId,
    uint32_t             eRevId,
    Gfx11SwWarDetection* pWorkarounds)
{
    bool successful = true;

    if (false)
    {
    }
    else if (familyId == 145)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectNavi31A0Workarounds(pWorkarounds);
        }
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            swd_internal::DetectNavi33A0Workarounds(pWorkarounds);
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            swd_internal::DetectNavi32A0Workarounds(pWorkarounds);
        }
        else if ((0x20 <= eRevId) && (eRevId < 0x30))
        {
            swd_internal::DetectNavi32GLXLWorkarounds(pWorkarounds);
        }
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
    }
    else if (familyId == 148)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectPhoenix1A0Workarounds(pWorkarounds);
        }
#if SWD_BUILD_PHX2
        else if ((0x80 <= eRevId) && (eRevId < 0xFF))
        {
            swd_internal::DetectPhoenix2A0Workarounds(pWorkarounds);
        }
#endif
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
    }
#if SWD_BUILD_STRIX
    else if (familyId == 150)
    {
        if (false)
        {
            // Handle sanitization woes.
        }
#if SWD_BUILD_STRIX1
        else if ((0x01 <= eRevId) && (eRevId < 0x10))
        {
            swd_internal::DetectStrix1A0Workarounds(pWorkarounds);
        }
#endif
#if SWD_BUILD_STRIX1
        else if ((0x10 <= eRevId) && (eRevId < 0x20))
        {
            swd_internal::DetectStrix1B0Workarounds(pWorkarounds);
        }
#endif
        else
        {
            // No ASIC detected. Return false.
            successful = false;
        }
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
    bool successful = true;

    if (false)
    {
    }
    else if ((major == 11) && (minor == 0) && (stepping == 0))
    {
        swd_internal::DetectNavi31A0Workarounds(pWorkarounds);
    }
    else if ((major == 11) && (minor == 0) && (stepping == 1))
    {
        swd_internal::DetectNavi32A0Workarounds(pWorkarounds);
    }
    else if ((major == 11) && (minor == 0) && (stepping == 2))
    {
        swd_internal::DetectNavi33A0Workarounds(pWorkarounds);
    }
    else if ((major == 11) && (minor == 0) && (stepping == 3))
    {
        swd_internal::DetectPhoenix1A0Workarounds(pWorkarounds);
#if SWD_BUILD_PHX2
        swd_internal::DetectPhoenix2A0Workarounds(pWorkarounds);
#endif
    }
    else if ((major == 11) && (minor == 0) && (stepping == 5))
    {
        swd_internal::DetectNavi32GLXLWorkarounds(pWorkarounds);
    }
#if SWD_BUILD_STRIX1
    else if ((major == 11) && (minor == 5) && (stepping == 0))
    {
        swd_internal::DetectStrix1B0Workarounds(pWorkarounds);
    }
#endif
#if SWD_BUILD_STRIX1
    else if ((major == 11) && (minor == 5) && (stepping == 65535))
    {
        swd_internal::DetectStrix1A0Workarounds(pWorkarounds);
    }
#endif
    else
    {
        // No ASIC detected. Return false.
        successful = false;
    }

    if (successful)
    {
        swd_internal::Gfx11OverrideDefaults(pWorkarounds);
    }

    return successful;
}

#endif
