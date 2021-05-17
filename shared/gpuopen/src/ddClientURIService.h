/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "ddUriInterface.h"

namespace DevDriver
{
    class IMsgChannel;

    // String used to identify the client URI service
    DD_STATIC_CONST char kClientURIServiceName[] = "client";

    DD_STATIC_CONST Version kClientURIServiceVersion = 1;

    class ClientURIService : public IService
    {
    public:
        ClientURIService();
        ~ClientURIService();

        // Returns the name of the service
        const char* GetName() const override final { return kClientURIServiceName; }
        Version GetVersion() const override final { return kClientURIServiceVersion; }

        // Binds a message channel to the service
        // All requests will be handled using the currently bound message channel
        void BindMessageChannel(IMsgChannel* pMsgChannel) { m_pMsgChannel = pMsgChannel; }

        // Handles an incoming URI request
        Result HandleRequest(IURIRequestContext* pContext) override final;

    private:
        // Currently bound message channel
        IMsgChannel* m_pMsgChannel;
    };
} // DevDriver
