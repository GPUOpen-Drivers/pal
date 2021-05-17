/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "ddUriInterface.h"
#include "util/sharedptr.h"
#include "ddTransferManager.h"

namespace DevDriver
{
    namespace URIProtocol
    {
        // Base class for URI services
        class URIService : public IService
        {
        public:
            virtual ~URIService() {}

            // Returns the name of the service
            const char* GetName() const override final { return m_name; }

        protected:
            URIService(const char* pName)
            {
                // Copy the service name into a member variable for later use.
                Platform::Strncpy(m_name, pName, sizeof(m_name));
            }

            DD_STATIC_CONST uint32 kServiceNameSize = 64;

            // The name of the service
            char m_name[kServiceNameSize];
        };
    }
} // DevDriver
