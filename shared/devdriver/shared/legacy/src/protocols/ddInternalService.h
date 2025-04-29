/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddUriInterface.h>
#include <util/vector.h>

namespace DevDriver
{

static constexpr const char*        kInternalServiceName    = "internal";
static constexpr DevDriver::Version kInternalServiceVersion = 1;

/// A service for  internal features
/// e.g. a list of registered services
/// This service should always be available on any URI capable bus client.
class InternalService final : public IService
{
public:
    /// For service commands that accept POST data, they will not accept more than this limit.
    /// Commands not expecting POST data will reject any POST data.
    static size_t kPostSizeLimit;

    InternalService() : m_info({}) {}

    typedef Result (*PFN_QueryRegisteredServices)(void* pUserdata, Vector<const IService*>* pServices);

    struct ServiceInfo
    {
        AllocCb                     allocCb;                    // Allocation callbacks
        void*                       pUserdata = nullptr;        // Userdata for callbacks
        PFN_QueryRegisteredServices pfnQueryRegisteredServices; // Callback to query all available services
    };

    // Initialize the service
    // This must be called correctly exactly once before registering the service
    Result Init(const ServiceInfo& info)
    {
        Result result = Result::InvalidParameter;

        if (info.pfnQueryRegisteredServices != nullptr)
        {
            result = Result::Success;
            m_info = info;
        }

        return result;
    }

    virtual ~InternalService() {}

    // ===== IService Methods =========================================================================================
    // Handles a request from a consumer
    DevDriver::Result HandleRequest(DevDriver::IURIRequestContext* pContext) override;

    // Report a size limit for post data from consumers
    size_t QueryPostSizeLimit(char* pArgs) const override;

    // Returns the name of the service
    const char* GetName() const override final { return kInternalServiceName; }

    // Returns the version of the service
    DevDriver::Version GetVersion() const override final { return kInternalServiceVersion; }

private:
    DD_DISALLOW_COPY_AND_ASSIGN(InternalService);

    DevDriver::Result WriteServicesJsonResponse(DevDriver::IURIRequestContext* pRequestContext) const;
    DevDriver::Result WriteServicesTextResponse(DevDriver::IURIRequestContext* pRequestContext) const;

    ServiceInfo m_info;
};

} // namespace DevDriver
