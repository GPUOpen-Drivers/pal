/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "driverUtilsService.h"

using namespace Pal;

#include "util/ddJsonWriter.h"
#include "ddPlatform.h"
#include "devDriverServer.h"
#include "device.h"

struct DriverUtilFeatures
{
    bool tracing;
    bool crashAnalysis;
    bool raytracingShaderTokens;
    bool debugVmid;
};

struct DriverDbgLogOriginationOp
{
    uint32 origination;
    bool   enable;
};

namespace DriverUtilsService
{

// =====================================================================================================================
DriverUtilsService::DriverUtilsService(
    Platform* pPlatform)
    :
    m_isTracingEnabled(false),
    m_crashAnalysisModeEnabled(false),
    m_raytracingShaderTokenEnabled(false),
    m_staticVmid(false),
    m_useOverlayBuffer(false),
    m_pPlatform(pPlatform)
{
    memset(m_overlayBuffer, 0, kNumOverlayStrings * kMaxOverlayStringLength);
}

// =====================================================================================================================
DriverUtilsService::~DriverUtilsService()
{
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::EnableTracing()
{
    m_isTracingEnabled = true;

    return DD_RESULT_SUCCESS;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::EnableCrashAnalysisMode()
{
    m_crashAnalysisModeEnabled = true;

    return DD_RESULT_SUCCESS;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::QueryPalDriverInfo(
    const DDByteWriter& writer)
{
    DevDriver::Vector<char> jsonBuffer(DevDriver::Platform::GenericAllocCb);
    DevDriver::JsonWriter jsonWriter(&jsonBuffer);

    DD_RESULT result = DD_RESULT_SUCCESS;

    // This extended client info will be available to DevDriver tools to
    // display to the user to aid in uniquely identifying the bits that are
    // in the current driver. Additions to the data are encouraged,
    // though modification of existing fields should be first discussed with the
    // DevDriver team. A schema for this data exists in the RPC registry alongside
    // the RPC service defintion file.
    jsonWriter.BeginMap();
    {
        // Application Info
        jsonWriter.KeyAndBeginMap("application_info");
        {
            char clientName[128] = {};
            DevDriver::Platform::GetProcessName(&clientName[0], sizeof(clientName));
            jsonWriter.KeyAndValue("process_name", clientName);
            jsonWriter.KeyAndValue("process_id", Util::GetIdOfCurrentProcess());
            jsonWriter.KeyAndValue("devdriver_client_id",
                                   m_pPlatform->GetDevDriverServer()->GetMessageChannel()->GetClientId());
        }
        jsonWriter.EndMap();

        // Driver Info
        jsonWriter.KeyAndBeginMap("driver_info");
        {
            jsonWriter.KeyAndValue("pal_version", PAL_INTERFACE_MAJOR_VERSION);
#if PAL_BUILD_BRANCH
            jsonWriter.KeyAndValue("branch_number", PAL_BUILD_BRANCH);
#endif
        }
        jsonWriter.EndMap();

        // Target GPU Info
        jsonWriter.KeyAndBeginList("target_gpu");
        {
            Result palResult = Result::Success;
            for (uint32 i = 0; i < m_pPlatform->GetDeviceCount(); ++i)
            {
                const Device* pDevice = m_pPlatform->GetDevice(i);
                if (pDevice != nullptr)
                {
                    DeviceProperties deviceProps = {};
                    palResult = pDevice->GetProperties(&deviceProps);
                    if (palResult == Result::Success)
                    {
                        jsonWriter.BeginMap();
                        {
                            jsonWriter.KeyAndValue("gpu_name", deviceProps.gpuName);
                            jsonWriter.KeyAndValue("device_id", deviceProps.deviceId);
                            jsonWriter.KeyAndValue("revision_id", deviceProps.revisionId);
                            jsonWriter.KeyAndValue("vendor_id", deviceProps.vendorId);
                            jsonWriter.KeyAndValue("is_finalized", pDevice->IsFinalized());
                            jsonWriter.KeyAndValue("queue_count", pDevice->NumQueues());
                            jsonWriter.KeyAndValue("frame_count", pDevice->GetFrameCount());
                            jsonWriter.KeyAndValue("attached_screens", pDevice->AttachedScreenCount());
                        }
                        jsonWriter.EndMap();
                    }
                }

            }

        }
        jsonWriter.EndList();
    }
    jsonWriter.EndMap();

    if (jsonWriter.End() != DevDriver::Result::Success)
    {
        result = DD_RESULT_DD_URI_INVALID_JSON;
    }

    if (result == DD_RESULT_SUCCESS)
    {
        const size_t textSize = jsonBuffer.Size() - 1;

        result = writer.pfnBegin(writer.pUserdata, &textSize);
        if (result == DD_RESULT_SUCCESS)
        {
            result = writer.pfnWriteBytes(writer.pUserdata, jsonBuffer.Data(), textSize);
        }

        writer.pfnEnd(writer.pUserdata, result);
    }
    return DD_RESULT_SUCCESS;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::EnableDriverFeatures(
    const void* pParamBuffer,
    size_t      paramBufferSize
)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if ((paramBufferSize == sizeof(DriverUtilFeatures)) && (pParamBuffer != nullptr))
    {
        const DriverUtilFeatures* pUpdate = static_cast<const DriverUtilFeatures*>(pParamBuffer);

        m_isTracingEnabled              = pUpdate->tracing;
        m_crashAnalysisModeEnabled      = pUpdate->crashAnalysis;
        m_raytracingShaderTokenEnabled  = pUpdate->raytracingShaderTokens;
        m_staticVmid                    = pUpdate->debugVmid;
    }
    else
	{
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
	}

    return result;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::SetOverlayString(
    const void* pParamBuffer,
    size_t      paramBufferSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    DDOverlayInfo* pOverlayData = (DDOverlayInfo*)pParamBuffer;

    if ((pOverlayData != nullptr)                   &&
        (pOverlayData->strIdx < kNumOverlayStrings) &&
        (paramBufferSize == sizeof(DDOverlayInfo)))
    {
        DevDriver::LockGuard lock(m_overlayMutex);
        Util::Strncpy(m_overlayBuffer[pOverlayData->strIdx], pOverlayData->str, kMaxOverlayStringLength);
        m_useOverlayBuffer = true;
    }
    else
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::SetDbgLogSeverityLevel(
    const void* pParamBuffer,
    size_t      paramBufferSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

#if PAL_ENABLE_LOGGING
    if ((paramBufferSize == sizeof(uint32)) && (pParamBuffer != nullptr))
    {
        uint32 severity = *static_cast<const uint32*>(pParamBuffer);
        m_pPlatform->GetDbgLoggerDevDriver()->SetCutoffSeverityLevel(static_cast<Util::SeverityLevel>(severity));
    }
    else
#endif
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::SetDbgLogOriginationMask(
    const void* pParamBuffer,
    size_t      paramBufferSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

#if PAL_ENABLE_LOGGING
    if ((paramBufferSize == sizeof(uint32)) && (pParamBuffer != nullptr))
    {
        uint32 mask = *static_cast<const uint32*>(pParamBuffer);

        m_pPlatform->GetDbgLoggerDevDriver()->SetOriginationTypeMask(mask);
    }
    else
#endif
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

// =====================================================================================================================
DD_RESULT DriverUtilsService::ModifyDbgLogOriginationMask(
    const void* pParamBuffer,
    size_t      paramBufferSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

#if PAL_ENABLE_LOGGING
    if ((paramBufferSize == sizeof(DriverDbgLogOriginationOp)) && (pParamBuffer != nullptr))
    {
        DriverDbgLogOriginationOp op = *static_cast<const DriverDbgLogOriginationOp*>(pParamBuffer);
        uint32 mask = m_pPlatform->GetDbgLoggerDevDriver()->GetOriginationTypeMask();
        uint32 newMask = op.enable ? (mask | (1 << op.origination)) : (mask & ~(1 << op.origination));

        m_pPlatform->GetDbgLoggerDevDriver()->SetOriginationTypeMask(newMask);
    }
    else
#endif
    {
        result = DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    return result;
}

}
