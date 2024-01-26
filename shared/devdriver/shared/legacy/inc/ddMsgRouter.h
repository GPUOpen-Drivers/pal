/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <gpuopen.h>
#include <ddRouterInterface.h>
#include <router/ddRouterContext.h>
#include <util/ringbuffer.h>
#include <router/ddAmdLogUtilsService.h>

namespace DevDriver
{
    // MsgRouter is expected to be the only implementation of IRouter.
    // We're only using an abstract interface here to avoid exposing implementation details.
    class MsgRouter final : public IRouter
    {
    public:
        MsgRouter(const AllocCb&         allocCb,
                  pfnNotifyKernalEnable  kernalEnableCb,
                  pfnNotifyKernalDisable kernalDisableCb,
                  AmdLogCallback         amdlogCb,
                  size_t                 ringBufferSize);
        ~MsgRouter();

        const AllocCb& GetAllocCb() const { return m_allocCb; }

        Result Initialize(const RouterCreateInfo& createInfo);
        void Destroy();

        void RegisterServices(size_t servicesCount, IService* const* pServices);

		Kmd::KContext* GetContext() { return &m_context; }

        void SignalDriverResetEvent();

        Result ProcessDevModeCmd(ProcessId processId, DevModeCmd cmd, size_t bufferSize, void* pBuffer) override;

        void OnProcessClose(ProcessId processId) override;

        Result GetLogBuffer(void* pBuffer, size_t bufferSize) override;

        Result GetLogBufferSize(size_t* pBufferSize) override;

        Result GetHeader(void* pOutBuffer) override;

        void RegisterRpcServices();
        void DestroyRpcServices();

    private:
        AllocCb                m_allocCb;
        Kmd::KContext          m_context;
        Vector<IService*>      m_servicesToRegister;
        pfnNotifyKernalEnable  m_kernalEnableCb;
        pfnNotifyKernalDisable m_kernalDisableCb;
        ProcessId              m_devDriverPID;
        // m_kernalEnableCb() and m_kernalDisableCb() modify global OS state
        // and we must not call m_kernalEnableCb() twice without calling
        // m_kernalDisableCb() in between.
        // m_kernalEnableState tracks whether we have called m_kernalEnableCb()
        // so we can follow the correct sequence.
        bool                   m_kernalEnableState;
        AmdLogCallback         m_amdlogCb;
        RingBuffer             m_ringBuffer;
        AmdLogEventVersion     m_version;
        AmdLogUtilsService::AmdLogUtilsService  m_amdLogUtilsService;
        DDRpcServer                             m_rpcServer;
    };

} // DevDriver
