/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "RgdMgr.h"
#include <ddCommon.h>
#include <dd_settings_api.h>
#include <dd_driver_utils_api.h>
#include <dd_gpu_detective_api.h>
#include <g_RouterUtilsModuleInterface.h>
#include <g_SystemTraceModuleStatic.h>
#include <dd_result.h>
#include <win/ddWinKmIoCtlDevice.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>

namespace DevDriver
{

constexpr const char kRgdToolId[] = "RgdMgr";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileRead(void* pUserData, const int64_t count, void* pBuffer, int64_t* pBytesRead)
{
    int   ret = rdfResult::rdfResultInvalidArgument;
    FILE* pFile = static_cast<FILE*>(pUserData);
    if (pFile != nullptr)
    {
        size_t bytesRead = fread(pBuffer, 1, count, pFile);
        if (pBytesRead)
        {
            *pBytesRead = bytesRead;
        }

        ret = (bytesRead == count) ? rdfResult::rdfResultOk : rdfResult::rdfResultError;
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileWrite(void* pUserData, const int64_t count, const void* pBuffer, int64_t* pBytesWritten)
{
    int   ret = rdfResult::rdfResultInvalidArgument;
    FILE* pFile = static_cast<FILE*>(pUserData);
    if (pFile)
    {
        size_t bytesWritten = fwrite(pBuffer, 1, count, pFile);
        if (pBytesWritten)
        {
            *pBytesWritten = bytesWritten;
        }
        ret = (bytesWritten == count) ? rdfResult::rdfResultOk : rdfResult::rdfResultError;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileTell(void* pUserData, int64_t* pPosition)
{
    int   ret = rdfResult::rdfResultInvalidArgument;
    FILE* pFile = static_cast<FILE*>(pUserData);
    if ((pFile != nullptr) && (pPosition))
    {
        *pPosition = ftell(pFile);
        ret = (*pPosition == -1) ? rdfResult::rdfResultError : rdfResult::rdfResultOk;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileSeek(void* pUserData, int64_t position)
{
    int   ret = rdfResult::rdfResultInvalidArgument;
    FILE* pFile = static_cast<FILE*>(pUserData);
    if (pFile != nullptr)
    {
        fseek(pFile, position, SEEK_SET);
        ret = rdfResult::rdfResultOk;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int FileGetSize(void* pUserData, int64_t* pSize)
{
    int   ret = rdfResult::rdfResultInvalidArgument;
    FILE* pFile = static_cast<FILE*>(pUserData);
    if ((pFile != nullptr) && (pSize != nullptr))
    {
        // 1. Save the current location
        // 2. Jump to the end to get the size.
        // 3. Return back to the original location
        int64_t currentPos = ftell(pFile);
        fseek(pFile, 0, SEEK_END);
        *pSize = ftell(pFile);
        fseek(pFile, currentPos, SEEK_SET);
        ret = rdfResult::rdfResultOk;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static DD_RESULT WriteHeartbeat(void* pUserdata, DD_RESULT result, DD_IO_STATUS status, size_t bytes)
{
    if (DD_IO_STATUS_END == status)
    {
        // Log a message if we have a logger
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool ShouldClientBeIgnored(void* pUserdata, const DDConnectionInfo* pConnectionInfo)
{
    RgdMgr* pRgd = static_cast<RgdMgr*>(pUserdata);
    bool ret = true;

    if (pRgd)
    {
        if (pConnectionInfo->pProcessName == pRgd->GetAppName())
        {
            ret = false;
        }
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void OnDriverStateChangedCb(DDConnectionCallbacksImpl* pImpl, DDConnectionId umdConnectionId, DD_DRIVER_STATE state)
{
    RgdMgr* pRgd = reinterpret_cast<RgdMgr*>(pImpl);
    if (pRgd)
    {
        pRgd->OnDriverStateChangedImpl(umdConnectionId, state);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void OnDriverConnectedCb(DDConnectionCallbacksImpl* pImpl, const DDConnectionInfo* pConnInfo)
{
    RgdMgr* pRgd = reinterpret_cast<RgdMgr*>(pImpl);
    if (pRgd)
    {
        pRgd->OnDriverConnectedImpl(pConnInfo);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void OnDriverDisconnectedCb(DDConnectionCallbacksImpl* pImpl, DDConnectionId umdConnectionId)
{
    RgdMgr* pRgd = reinterpret_cast<RgdMgr*>(pImpl);
    if (pRgd)
    {
        pRgd->OnDriverDisconnectedImpl(umdConnectionId);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create a temp file to write the data to:
// Note:
//     This temp file needs to be cleaned up explicitly as it will not be automatically deleted at close.
//     We can't just use tempfile() because that will only give a handle to the file, we need the name.
static std::string BuildOutputFilePath()
{
    std::string outputFile = "";
    char systemDir[MAX_PATH];
    UINT size = GetSystemDirectory(systemDir, MAX_PATH);

    if (size > 0)
    {
        std::time_t       t   = std::time(nullptr);
        std::tm*          now = std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(now, "%Y%m%d_%H%M%S");

        outputFile = std::string(systemDir) + "\\drivers\\DriverData\\AMD\\" + ss.str();
    }

    return outputFile;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::InitDevDriver()
{
    // We shouldn't already be init
    if (m_devDriverInit)
    {
        return DD_RESULT_DD_GENERIC_CONNECTION_EXITS;
    }

    // Create Router
    DD_RESULT result = CreateRouter(&m_router);

    // Create tool API
    if (result == DD_RESULT_SUCCESS)
    {
        result = CreateToolApi(&m_pToolApi);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = InitApis();
    }

    if (result == DD_RESULT_SUCCESS)
    {
        m_pIoCtlDevice = DD_NEW(WinKmIoCtlDevice, Platform::GenericAllocCb);

        if (m_pIoCtlDevice != nullptr)
        {
            Result r = m_pIoCtlDevice->Initialize();
            if (r != Result::Success)
            {
                DD_DELETE(m_pIoCtlDevice, Platform::GenericAllocCb);
                m_pIoCtlDevice = nullptr;
            }
            result = DevDriverToDDResult(r);
        }
        else
        {
            result = DevDriverToDDResult(Result::InsufficientMemory);
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        m_devDriverInit = true;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::ShutdownDevDriver()
{
    // If we started monitoring we should end it before cleanup as this can cause crashes sometimes
    DD_ASSERT(m_monitorStarted == false);

    // Disable the crash analysis feature flag first since it requires the DriverUtils API
    SetCrashAnalysisFeatureFlag(false);

    if (m_pToolApi)
    {
        m_pToolApi->Disconnect(m_pToolApi->pInstance);
    }

    if (m_pIoCtlDevice)
    {
        m_pIoCtlDevice->Destroy();
        DD_DELETE(m_pIoCtlDevice, Platform::GenericAllocCb);
        m_pIoCtlDevice = nullptr;
    }

    if (m_router != DD_API_INVALID_HANDLE)
    {
        ddRouterDestroy(m_router);
    }

    if (m_pToolApi)
    {
        DDToolApiDestroy(&m_pToolApi);
        m_pToolApi = nullptr;
    }

    m_devDriverInit = false;
    m_reachedPostDeviceInit = false;
    m_pid = 0;
    m_rgdState = RgdStateMonitoringNotEnabled;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::MonitorApp(const std::string& appName)
{
    DD_RESULT result = InitDevDriver();

    // Only allow one app to be monitor at a time
    if ((result == DD_RESULT_SUCCESS) && (m_monitorStarted == false))
    {
        m_outputFile = BuildOutputFilePath();

        FILE* pFile = fopen(m_outputFile.c_str(), "wb+");
        m_appName   = appName;

        if (pFile != nullptr)
        {
            m_rdfFileWriter.pUserData      = pFile;
            m_rdfFileWriter.pfnFileRead    = FileRead;
            m_rdfFileWriter.pfnFileWrite   = FileWrite;
            m_rdfFileWriter.pfnFileTell    = FileTell;
            m_rdfFileWriter.pfnFileSeek    = FileSeek;
            m_rdfFileWriter.pfnFileGetSize = FileGetSize;
            result                         = DD_RESULT_SUCCESS;
        }
        else
        {
            result = DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR;
        }

        m_heartbeat.pfnWriteHeartbeat = WriteHeartbeat;
        m_heartbeat.pUserdata         = nullptr;

        // Load connection callbacks now since the filter requires the app name to be set
        if (result == DD_RESULT_SUCCESS)
        {
            result = LoadConnectionCallbacks();
        }

        // Connect to the router
        if (result == DD_RESULT_SUCCESS)
        {
            result = m_pToolApi->Connect(m_pToolApi->pInstance, nullptr, 0);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = SetCrashAnalysisFeatureFlag(true);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            m_monitorStarted = true;
            m_rgdState       = RgdStateMonitoringEnabledNotLaunched;
        }
    }
    else
    {
        result = DD_RESULT_DD_GENERIC_NOT_READY;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::CloseTraceFile()
{
    FILE* pFile = static_cast<FILE*>(m_rdfFileWriter.pUserData);
    if (pFile)
    {
        fclose(pFile);
        m_rdfFileWriter.pUserData = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::EndMonitoring()
{
    if (m_devDriverInit)
    {
        // The app filter will continue to be in place as there isn't an unset,
        // but setting the name to an empty string will make it so it will ignore all the apps
        m_appName = "";

        m_monitorStarted = false;
        m_rgdState       = RgdStateMonitoringNotEnabled;

        CloseTraceFile();

        // We only support monitoring a single app right now, so just shut down DevDriver when we are done.
        ShutdownDevDriver();
    }

    // We are now safe to delete the file since we have either sent it to the KMD or are shutting down:
    if (m_outputFile != "")
    {
        std::remove(m_outputFile.c_str());
        m_outputFile = "";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::OnDriverStateChangedImpl(DDConnectionId umdConnectionId, DD_DRIVER_STATE state)
{
    if (state == DD_DRIVER_STATE_PLATFORMINIT)
    {
        ForceDisableDriverOverlay(umdConnectionId);

        m_pGpuDetectiveApi->EnableTracing(m_pGpuDetectiveApi->pInstance, umdConnectionId, m_pid);
        m_rgdState = RgdStateEarlyConnection;
    }
    else if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
    {
        m_reachedPostDeviceInit = true;
        m_rgdState              = RgdStateConnectionPostDeviceInit;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::OnDriverConnectedImpl(const DDConnectionInfo* pConnInfo)
{
    m_pid = pConnInfo->processId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RgdMgr::OnDriverDisconnectedImpl(DDConnectionId umdConnectionId)
{
    bool crashDetected = false;
    RgdOcaClientUpdate info = {};
    snprintf(info.AppName, sizeof(info.AppName), "%s", m_appName.c_str());

    DD_RESULT result = m_pGpuDetectiveApi->EndTracing(m_pGpuDetectiveApi->pInstance, umdConnectionId, m_reachedPostDeviceInit, &crashDetected);

    if (crashDetected)
    {
        result = m_pGpuDetectiveApi->TransferTraceData(m_pGpuDetectiveApi->pInstance, umdConnectionId, &m_rdfFileWriter, &m_heartbeat);

        if (result == DD_RESULT_SUCCESS)
        {
            m_rgdState = RgdStateDisconnectedTraceCaptured;
            snprintf(info.RgdFilePath, sizeof(info.RgdFilePath), "%s", m_outputFile.c_str());
            info.state = m_rgdState;
            CloseTraceFile();
            result = DevDriverToDDResult(m_pIoCtlDevice->IoCtl(DevDriver::DevDriverRgdOcaBuffered, sizeof(info), &info, sizeof(info), &info));
        }
        else
        {
            m_rgdState = RgdStateDisconnectedPostDeviceInitTraceError;
            info.state = m_rgdState;
            result = DevDriverToDDResult(m_pIoCtlDevice->IoCtl(DevDriver::DevDriverRgdOcaBuffered, sizeof(info), &info, sizeof(info), &info));
        }
    }
    else
    {
        if (m_reachedPostDeviceInit == true)
        {
            m_rgdState = RgdStateDisconnectedPostDeviceInitNoCrash;
            info.state = m_rgdState;
            result = DevDriverToDDResult(m_pIoCtlDevice->IoCtl(DevDriver::DevDriverRgdOcaBuffered, sizeof(info), &info, sizeof(info), &info));
        }
        else
        {
            m_rgdState = RgdStateDisconnectedEarlyNoCrash;
        }
    }

    // Reset any state we collected during the driver connection process:
    m_reachedPostDeviceInit = false;
    m_pid = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::CreateRouter(DDRouter* pOutRouter)
{
    DDRouterCreateInfo routerCreateInfo = {};
    routerCreateInfo.pDescription = kRgdToolId;
    routerCreateInfo.alloc = { ddApiDefaultAlloc, ddApiDefaultFree, nullptr };

    DDLoggerInfo quietLogger = {};
    quietLogger.pUserdata    = nullptr;
    quietLogger.pfnLog       = [](void*, const DDLogEvent*, const char*) {};
    quietLogger.pfnWillLog   = [](void*, const DDLogEvent*) { return 0; };
    quietLogger.pfnPush      = [](void*, const DDLogEvent*, const char*) {};
    quietLogger.pfnPop       = [](void*, const DDLogEvent*, const char*) {};

    routerCreateInfo.logger = quietLogger;

    DD_RESULT result = ddRouterCreate(&routerCreateInfo, pOutRouter);
    if (result == DD_RESULT_SUCCESS)
    {
        result = ddRouterLoadBuiltinModule(*pOutRouter, RouterUtilsQueryModuleInterface(), nullptr);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddRouterLoadBuiltinModule(*pOutRouter, SystemTraceQueryModule(), nullptr);
    }

    // TODO: Add Siphon if we need to query the settings blobs

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::CreateToolApi(DDToolApi** ppOutToolApi)
{
    DDToolApiCreateInfo createInfo = {};
    createInfo.pDescription        = kRgdToolId;
    createInfo.descriptionSize     = strlen(kRgdToolId) + 1;
    createInfo.pModulesDir         = nullptr;
    createInfo.moduleDirSize       = 0;
    createInfo.pLogFilePath        = nullptr;
    createInfo.logFilePathSize     = 0;

    return DDToolApiCreate(&createInfo, ppOutToolApi);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::LoadConnectionCallbacks()
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_pConnectionApi != nullptr)
    {
        DDConnectionFilter connectionFilter = {};
        connectionFilter.pUserData          = this;
        connectionFilter.filter             = &ShouldClientBeIgnored;
        m_pConnectionApi->SetConnectionFilter(m_pConnectionApi->pInstance, connectionFilter);

        m_connectionCbs                       = {};
        m_connectionCbs.pImpl                 = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        m_connectionCbs.OnDriverStateChanged  = &OnDriverStateChangedCb;
        m_connectionCbs.OnDriverConnected     = &OnDriverConnectedCb;
        m_connectionCbs.OnDriverDisconnected  = &OnDriverDisconnectedCb;
        result = m_pConnectionApi->AddConnectionCallbacks(m_pConnectionApi->pInstance, &m_connectionCbs);
    }

    return result;
}

#define INIT_API(APINAME, pApi)                                             \
    m_pApiRegistry->Get(m_pApiRegistry->pInstance,                          \
                        DD_##APINAME##_API_NAME,                            \
                        DDVersion{ DD_##APINAME##_API_VERSION_MAJOR,        \
                                   DD_##APINAME##_API_VERSION_MINOR,        \
                                   DD_##APINAME##_API_VERSION_PATCH },      \
                        reinterpret_cast<void**>(&pApi))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::InitApis()
{
    m_pApiRegistry = m_pToolApi->GetApiRegistry(m_pToolApi->pInstance);

    DD_RESULT result = DD_RESULT_DD_GENERIC_NOT_READY;

    if (m_pApiRegistry)
    {
        result = INIT_API(DRIVER_UTILS, m_pDriverUtilsApi);

        if (result == DD_RESULT_SUCCESS)
        {
            result = INIT_API(GPU_DETECTIVE, m_pGpuDetectiveApi);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result  = INIT_API(CONNECTION, m_pConnectionApi);
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = INIT_API(SETTINGS, m_pSettingsApi);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::SetCrashAnalysisFeatureFlag(bool enable)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_NOT_READY;

    if (m_pDriverUtilsApi != nullptr)
    {
        const size_t setterNameLen = strlen(kRgdToolId) + 1;
        result = m_pDriverUtilsApi->SetFeature(m_pDriverUtilsApi->pInstance,
                                               DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS,
                                               enable ? DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE : DD_DRIVER_UTILS_FEATURE_FLAG_DISABLE,
                                               kRgdToolId,
                                               setterNameLen);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT RgdMgr::ForceDisableDriverOverlay(uint16_t umdConnectionId)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_NOT_READY;

    if (m_pSettingsApi != nullptr)
    {
        // This setting is hardcoded since it isn't likely to change
        uint32_t           settingValue   = 0x2;
        DDSettingsValueRef overlaySetting = {};
        overlaySetting.type               = DD_SETTINGS_TYPE_UINT32;
        overlaySetting.size               = sizeof(uint32_t);
        overlaySetting.pValue             = reinterpret_cast<void*>(&settingValue);
        DDSettingsComponentValueRefs componentValues = {};
        componentValues.pValues                      = &overlaySetting;
        componentValues.numValues                    = 1;

        snprintf(componentValues.componentName, sizeof(componentValues.componentName), "PalPlatform");

        result = m_pSettingsApi->SendAllUserOverrides(m_pSettingsApi->pInstance, umdConnectionId, 1, &componentValues);
    }

    return result;
}

} // DevDriver namespace
