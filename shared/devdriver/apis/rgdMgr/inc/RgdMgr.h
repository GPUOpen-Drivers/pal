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

#pragma once

#include <ddRouter.h>
#include <ddCommon.h>
#include <dd_tool_api.h>
#include <dd_connection_api.h>
#include <ddRdf.h>
#include <amdrdf.h>
#include <ddDevModeControlDevice.h>
#include <ddAmdLogInterface.h>

// Forwards
struct DDDriverUtilsApi;
struct DDSettingsApi;
struct DDGpuDetectiveApi;

namespace DevDriver
{
class WinKmIoCtlDevice;

class RgdMgr
{
public:
    RgdMgr()
        : m_router(DD_API_INVALID_HANDLE)
        , m_pToolApi(nullptr)
        , m_pApiRegistry(nullptr)
        , m_pConnectionApi(nullptr)
        , m_connectionCbs()
        , m_pDriverUtilsApi(nullptr)
        , m_pSettingsApi(nullptr)
        , m_pGpuDetectiveApi(nullptr)
        , m_rdfFileWriter()
        , m_heartbeat()
        , m_monitorStarted(false)
        , m_rgdState(DevDriver::RgdStateMonitoringNotEnabled)
        , m_reachedPostDeviceInit(false)
        , m_devDriverInit(false)
        , m_pid(0)
        , m_pIoCtlDevice(nullptr)
    {}

    ~RgdMgr()
    {
        EndMonitoring();
    }

    // Functions used by end users:
    DD_RESULT           MonitorApp(const std::string& appName);
    void                EndMonitoring();
    DevDriver::RgdState GetRgdState() { return m_rgdState; }
    std::string         GetOutputFile() { return m_outputFile; }

    // Functions used by callbacks:
    std::string GetAppName() { return m_appName; }
    void        OnDriverStateChangedImpl(DDConnectionId umdConnectionId, DD_DRIVER_STATE state);
    void        OnDriverConnectedImpl(const DDConnectionInfo* pConnInfo);
    void        OnDriverDisconnectedImpl(DDConnectionId umdConnectionId);

private:
    DD_RESULT InitDevDriver();
    void      ShutdownDevDriver();
    DD_RESULT CreateRouter(DDRouter* pOutRouter);
    DD_RESULT CreateToolApi(DDToolApi** ppOutToolApi);
    DD_RESULT LoadConnectionCallbacks();
    DD_RESULT InitApis();
    DD_RESULT SetCrashAnalysisFeatureFlag(bool enable);
    DD_RESULT ForceDisableDriverOverlay(uint16_t umdConnectionId);
    void      CloseTraceFile();

    uint32_t                      m_pid;
    DDRouter                      m_router;
    DDToolApi*                    m_pToolApi;
    DDApiRegistry*                m_pApiRegistry;
    DDConnectionApi*              m_pConnectionApi;
    DDConnectionCallbacks         m_connectionCbs;
    DDDriverUtilsApi*             m_pDriverUtilsApi;
    DDSettingsApi*                m_pSettingsApi;
    DDGpuDetectiveApi*            m_pGpuDetectiveApi;
    std::string                   m_appName;
    std::string                   m_outputFile;
    bool                          m_monitorStarted;
    DevDriver::RgdState           m_rgdState;
    bool                          m_reachedPostDeviceInit;
    bool                          m_devDriverInit;
    struct DDRdfFileWriter        m_rdfFileWriter;
    struct DDIOHeartbeat          m_heartbeat;
    DevDriver::Platform::Mutex    m_mutex;
    DevDriver::WinKmIoCtlDevice*  m_pIoCtlDevice;
};

} // DevDriver namespace
