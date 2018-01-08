/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "core/platform.h"
#include "palHashMap.h"
#include "palMutex.h"
#include "protocols/ddURIService.h"

// Forward declarations.
namespace DevDriver
{
    namespace DriverControlProtocol
    {
        enum struct DeviceClockMode : Pal::uint32;
    }
}

namespace Pal
{

DevDriver::Result QueryClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData);

DevDriver::Result QueryMaxClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData);

DevDriver::Result SetClockModeCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    void*                                             pUserData);

void* DevDriverAlloc(
    void* pUserdata,
    size_t size,
    size_t alignment,
    bool zero);

void DevDriverFree(
    void* pUserdata,
    void* pMemory);

// =====================================================================================================================
// PAL Pipeline Dump Service
// Used to allow clients on the developer driver bus to remotely dump pipelines from the driver.
class PipelineDumpService : public DevDriver::URIProtocol::URIService
{
public:
    explicit PipelineDumpService(Platform* pPlatform);
    ~PipelineDumpService();

    Result Init();

    // Handles a request from a developer driver client.
    DevDriver::Result HandleRequest(char* pArguments,
                                    DevDriver::SharedPointer<DevDriver::TransferProtocol::LocalBlock> pBlock) override;

    // Registers a pipeline hash / binary pair with the dump service.
    void RegisterPipeline(void* pPipelineBinary, uint32 pipelineBinaryLength, uint64 pipelineHash);

private:
    // Struct for keeping track of pipeline binary data.
    struct PipelineRecord
    {
        void* pPipelineBinary;
        uint32 pipelineBinaryLength;
    };

    // Typedef for pipeline record map.
    typedef Util::HashMap<uint64, PipelineRecord, Platform> PipelineRecordMap;

    Platform*         m_pPlatform;
    Util::Mutex       m_mutex;
    PipelineRecordMap m_pipelineRecords;
    uint32            m_maxPipelineBinarySize;

    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineDumpService);
    PAL_DISALLOW_DEFAULT_CTOR(PipelineDumpService);
};

} // Pal
