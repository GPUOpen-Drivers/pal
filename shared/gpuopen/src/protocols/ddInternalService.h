/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
