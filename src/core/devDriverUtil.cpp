/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverUtil.h"
#include "core/device.h"
#include "palHashMapImpl.h"

#include "ddTransferManager.h"
#include "protocols/driverControlServer.h"

using namespace Util;

namespace Pal
{

// Pipeline dump format versions
static const uint32 PipelineDumpInitialVersion = 1;

// Latest pipeline dump format
static const uint32 PipelineDumpVersion = PipelineDumpInitialVersion;

// Pipeline dump format magic number            "pdmp"
static const uint32 PipelineDumpMagicNumber = 0x70646d70;

// =====================================================================================================================
// Header structure for the pipeline dump format
struct PipelineDumpHeader
{
    uint32 magicNumber; // Magic number that identifies the dump format
    uint32 version;     // Format version
    uint64 numRecords;  // Number of pipeline dump records present after this structure
};

// =====================================================================================================================
// Structure that identifies an individual pipeline within the pipeline dump format
struct PipelineDumpRecord
{
    uint64 hash;   // Pipeline hash
    uint64 offset; // Offset of the associated pipeline ELF binary from the beginning of the dump
    uint64 size;   // Size of the associated pipeline ELF binary
};

// =====================================================================================================================
// DevDriver DeviceClockMode to Pal::DeviceClockMode table
static DeviceClockMode PalDeviceClockModeTable[] =
{
    DeviceClockMode::Default,       // Unknown       = 0
    DeviceClockMode::Default,       // Default       = 1
    DeviceClockMode::Profiling,     // Profiling     = 2
    DeviceClockMode::MinimumMemory, // MinimumMemory = 3
    DeviceClockMode::MinimumEngine, // MinimumEngine = 4
    DeviceClockMode::Peak           // Peak          = 5
};

// =====================================================================================================================
// Callback function which returns the current device clock for the requested gpu.
DevDriver::Result QueryClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = DeviceClockMode::Query;

        SetClockModeOutput clockModeOutput = {};

        Result palResult = pPalDevice->SetClockMode(clockModeInput, &clockModeOutput);

        if (palResult == Result::Success)
        {
            *pGpuClock = (static_cast<float>(chipProps.maxEngineClock) * clockModeOutput.engineClockRatioToPeak);
            *pMemClock = (static_cast<float>(chipProps.maxMemoryClock) * clockModeOutput.memoryClockRatioToPeak);

            result = DevDriver::Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Callback function which returns the max device clock for the requested gpu.
DevDriver::Result QueryMaxClockCallback(
    uint32 gpuIndex,
    float* pGpuClock,
    float* pMemClock,
    void*  pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();

        *pGpuClock = static_cast<float>(chipProps.maxEngineClock);
        *pMemClock = static_cast<float>(chipProps.maxMemoryClock);

        result = DevDriver::Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Callback function which sets the current device clock mode for the requested gpu.
DevDriver::Result SetClockModeCallback(
    uint32                                            gpuIndex,
    DevDriver::DriverControlProtocol::DeviceClockMode clockMode,
    void*                                             pUserData)
{
    Platform* pPlatform = reinterpret_cast<Platform*>(pUserData);

    DevDriver::Result result = DevDriver::Result::Error;

    Device* pPalDevice = nullptr;

    if (gpuIndex < pPlatform->GetDeviceCount())
    {
        pPalDevice = pPlatform->GetDevice(gpuIndex);
    }

    if (pPalDevice != nullptr)
    {
        // Convert the DevDriver DeviceClockMode enum into a Pal enum
        DeviceClockMode palClockMode = PalDeviceClockModeTable[static_cast<uint32>(clockMode)];

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = palClockMode;

        Result palResult = pPalDevice->SetClockMode(clockModeInput, nullptr);

        result = (palResult == Result::Success) ? DevDriver::Result::Success : DevDriver::Result::Error;
    }

    return result;
}

// =====================================================================================================================
// Callback function used to allocate memory inside the developer driver component.
void* DevDriverAlloc(
    void*  pUserdata,
    size_t size,
    size_t alignment,
    bool   zero)
{
    Platform* pAllocator = reinterpret_cast<Platform*>(pUserdata);

    //NOTE: Alignment is ignored here since PAL always aligns to an entire cache line by default. This shouldn't be an
    //      issue because no type should require more than a cache line of alignment (64 bytes).
    PAL_ASSERT(alignment <= PAL_CACHE_LINE_BYTES);

    void* pMemory = zero ? PAL_CALLOC(size, pAllocator, Util::AllocInternal)
                         : PAL_MALLOC(size, pAllocator, Util::AllocInternal);

    return pMemory;
}

// =====================================================================================================================
// Callback function used to free memory inside the developer driver component.
void DevDriverFree(
    void* pUserdata,
    void* pMemory)
{
    Platform* pAllocator = reinterpret_cast<Platform*>(pUserdata);

    PAL_FREE(pMemory, pAllocator);
}

// =====================================================================================================================
PipelineDumpService::PipelineDumpService(
    Platform* pPlatform)
    :
    DevDriver::IService(),
    m_pPlatform(pPlatform),
    m_pipelineRecords(0x4000, pPlatform)
{
}

// =====================================================================================================================
PipelineDumpService::~PipelineDumpService()
{
    auto iter = m_pipelineRecords.Begin();
    while (iter.Get())
    {
        void* pPipelineBinary = iter.Get()->value.pPipelineBinary;
        PAL_ASSERT(pPipelineBinary != nullptr);
        PAL_SAFE_FREE(pPipelineBinary, m_pPlatform);
        iter.Next();
    }
}

// =====================================================================================================================
Result PipelineDumpService::Init()
{
    Result result = m_mutex.Init();

    if (result == Result::Success)
    {
        result = m_pipelineRecords.Init();
    }

    return result;
}

// =====================================================================================================================
// Handles pipeline dump requests from the developer driver bus
#if DD_VERSION_SUPPORTS(GPUOPEN_URIINTERFACE_CLEANUP_VERSION)
DevDriver::Result PipelineDumpService::HandleRequest(
    DevDriver::IURIRequestContext* pContext)
{
    DevDriver::Result result = DevDriver::Result::Error;

    const char* pArgs = pContext->GetRequestArguments();
    if (strcmp(pArgs, "index") == 0)
    {
        // The client requested an index of the pipeline binaries.

        DevDriver::IByteWriter* pWriter = nullptr;
        result = pContext->BeginByteResponse(&pWriter);
        if (result == DevDriver::Result::Success)
        {
            m_mutex.Lock();

            // Write the pipeline dump header

            const uint64 numRecords = static_cast<uint64>(m_pipelineRecords.GetNumEntries());

            WritePipelineDumpHeader(pWriter, numRecords);

            // Write the pipeline dump records without a valid offset parameter since we won't be including any actual
            // pipeline binary data.

            auto recordIter = m_pipelineRecords.Begin();
            while (recordIter.Get())
            {
                const PipelineRecord* pRecord = &recordIter.Get()->value;

                const uint64 pipelineHash = recordIter.Get()->key;
                const uint32 pipelineSize = pRecord->pipelineBinaryLength;

                WritePipelineDumpRecord(pWriter,
                    pipelineHash,
                    UINT64_MAX,
                    pipelineSize);

                recordIter.Next();
            }

            m_mutex.Unlock();
            result = pWriter->End();
        }
    }
    else if (strcmp(pArgs, "all") == 0)
    {
        // The client requested that we dump all of the pipeline binaries.

        DevDriver::IByteWriter* pWriter = nullptr;
        result = pContext->BeginByteResponse(&pWriter);
        if (result == DevDriver::Result::Success)
        {
            m_mutex.Lock();

            // Write the pipeline dump header

            const uint64 numRecords = static_cast<uint64>(m_pipelineRecords.GetNumEntries());

            WritePipelineDumpHeader(pWriter, numRecords);

            const uint64 pipelineBinaryBaseOffset =
                sizeof(PipelineDumpHeader) + (sizeof(PipelineDumpRecord) * numRecords);

            uint64 currentOffset = pipelineBinaryBaseOffset;

            // Write the pipeline dump records

            auto recordIter = m_pipelineRecords.Begin();
            while (recordIter.Get())
            {
                const PipelineRecord* pRecord = &recordIter.Get()->value;

                const uint64 pipelineHash = recordIter.Get()->key;
                const uint32 pipelineSize = pRecord->pipelineBinaryLength;

                WritePipelineDumpRecord(pWriter,
                    pipelineHash,
                    currentOffset,
                    pipelineSize);

                currentOffset += pipelineSize;

                recordIter.Next();
            }

            // Write the binary data for each pipeline into the dump

            auto binaryIter = m_pipelineRecords.Begin();
            while (binaryIter.Get())
            {
                const PipelineRecord* pRecord = &binaryIter.Get()->value;

                const void* pPipelineBinary = pRecord->pPipelineBinary;
                const uint32 pipelineSize = pRecord->pipelineBinaryLength;

                pWriter->WriteBytes(reinterpret_cast<const void*>(pPipelineBinary),
                    static_cast<size_t>(pipelineSize));

                binaryIter.Next();
            }

            m_mutex.Unlock();
            result = pWriter->End();
        }
    }
    else
    {
        // The client requested a specific pipeline dump via the pipeline hash.

        DevDriver::IByteWriter* pWriter = nullptr;
        result = pContext->BeginByteResponse(&pWriter);
        if (result == DevDriver::Result::Success)
        {
            m_mutex.Lock();

            // Attempt to find the requested pipeline.
            const uint64 pipelineHash = strtoull(pArgs, nullptr, 16);
            const PipelineRecord* pRecord = m_pipelineRecords.FindKey(pipelineHash);
            if (pRecord != nullptr)
            {
                // Write a pipeline dump header with only one pipeline record in it
                WritePipelineDumpHeader(pWriter, 1);

                // Write the pipeline dump record for the specific pipeline

                const uint32 pipelineSize = pRecord->pipelineBinaryLength;
                const uint32 pipelineOffset = sizeof(PipelineDumpHeader) + sizeof(PipelineDumpRecord);

                WritePipelineDumpRecord(pWriter,
                    pipelineHash,
                    pipelineOffset,
                    pipelineSize);

                // Write the binary data for the specific pipeline

                const void* pPipelineBinary = pRecord->pPipelineBinary;

                pWriter->WriteBytes(reinterpret_cast<const void*>(pPipelineBinary),
                    static_cast<size_t>(pipelineSize));
            }

            m_mutex.Unlock();
            result = pWriter->End();
        }
    }

    return result;
}

// =====================================================================================================================
// Writes a header into a pipeline dump file
void PipelineDumpService::WritePipelineDumpHeader(
    DevDriver::IByteWriter* pWriter,
    uint64                  numRecords)
{
    PipelineDumpHeader header = {};
    header.magicNumber = PipelineDumpMagicNumber;
    header.version = PipelineDumpVersion;
    header.numRecords = numRecords;

    pWriter->WriteBytes(reinterpret_cast<const void*>(&header), sizeof(PipelineDumpHeader));
}

// =====================================================================================================================
// Writes a pipeline record into a pipeline dump file
void PipelineDumpService::WritePipelineDumpRecord(
    DevDriver::IByteWriter* pWriter,
    uint64                  pipelineHash,
    uint64                  pipelineOffset,
    uint64                  pipelineSize)
{
    PipelineDumpRecord record = {};
    record.hash = pipelineHash;
    record.offset = pipelineOffset;
    record.size = pipelineSize;

    pWriter->WriteBytes(reinterpret_cast<const void*>(&record), sizeof(PipelineDumpRecord));
}
#else
DevDriver::Result PipelineDumpService::HandleRequest(
    DevDriver::URIRequestContext* pContext)
{
    DevDriver::Result result = DevDriver::Result::Error;

    if (strcmp(pContext->pRequestArguments, "index") == 0)
    {
        // The client requested an index of the pipeline binaries.

        m_mutex.Lock();

        // Write the pipeline dump header

        const uint64 numRecords = static_cast<uint64>(m_pipelineRecords.GetNumEntries());

        WritePipelineDumpHeader(pContext, numRecords);

        // Write the pipeline dump records without a valid offset parameter since we won't be including any actual
        // pipeline binary data.

        auto recordIter = m_pipelineRecords.Begin();
        while (recordIter.Get())
        {
            const PipelineRecord* pRecord = &recordIter.Get()->value;

            const uint64 pipelineHash = recordIter.Get()->key;
            const uint32 pipelineSize = pRecord->pipelineBinaryLength;

            WritePipelineDumpRecord(pContext,
                                    pipelineHash,
                                    UINT64_MAX,
                                    pipelineSize);

            recordIter.Next();
        }

        m_mutex.Unlock();

        pContext->responseDataFormat = DevDriver::URIDataFormat::Binary;

        result = DevDriver::Result::Success;
    }
    else if (strcmp(pContext->pRequestArguments, "all") == 0)
    {
        // The client requested that we dump all of the pipeline binaries.

        m_mutex.Lock();

        // Write the pipeline dump header

        const uint64 numRecords = static_cast<uint64>(m_pipelineRecords.GetNumEntries());

        WritePipelineDumpHeader(pContext, numRecords);

        const uint64 pipelineBinaryBaseOffset =
            sizeof(PipelineDumpHeader) + (sizeof(PipelineDumpRecord) * numRecords);

        uint64 currentOffset = pipelineBinaryBaseOffset;

        // Write the pipeline dump records

        auto recordIter = m_pipelineRecords.Begin();
        while (recordIter.Get())
        {
            const PipelineRecord* pRecord = &recordIter.Get()->value;

            const uint64 pipelineHash = recordIter.Get()->key;
            const uint32 pipelineSize = pRecord->pipelineBinaryLength;

            WritePipelineDumpRecord(pContext,
                                    pipelineHash,
                                    currentOffset,
                                    pipelineSize);

            currentOffset += pipelineSize;

            recordIter.Next();
        }

        // Write the binary data for each pipeline into the dump

        auto binaryIter = m_pipelineRecords.Begin();
        while (binaryIter.Get())
        {
            const PipelineRecord* pRecord = &binaryIter.Get()->value;

            const void* pPipelineBinary = pRecord->pPipelineBinary;
            const uint32 pipelineSize   = pRecord->pipelineBinaryLength;

            pContext->pResponseBlock->Write(reinterpret_cast<const DevDriver::uint8*>(pPipelineBinary),
                                            static_cast<size_t>(pipelineSize));

            binaryIter.Next();
        }

        m_mutex.Unlock();

        pContext->responseDataFormat = DevDriver::URIDataFormat::Binary;

        result = DevDriver::Result::Success;
    }
    else
    {
        // The client requested a specific pipeline dump via the pipeline hash.

        m_mutex.Lock();

        // Attempt to find the requested pipeline.
        const uint64 pipelineHash     = strtoull(pContext->pRequestArguments, nullptr, 16);
        const PipelineRecord* pRecord = m_pipelineRecords.FindKey(pipelineHash);
        if (pRecord != nullptr)
        {
            // Write a pipeline dump header with only one pipeline record in it
            WritePipelineDumpHeader(pContext, 1);

            // Write the pipeline dump record for the specific pipeline

            const uint32 pipelineSize   = pRecord->pipelineBinaryLength;
            const uint32 pipelineOffset = sizeof(PipelineDumpHeader) + sizeof(PipelineDumpRecord);

            WritePipelineDumpRecord(pContext,
                                    pipelineHash,
                                    pipelineOffset,
                                    pipelineSize);

            // Write the binary data for the specific pipeline

            const void* pPipelineBinary = pRecord->pPipelineBinary;

            pContext->pResponseBlock->Write(reinterpret_cast<const DevDriver::uint8*>(pPipelineBinary),
                                            static_cast<size_t>(pipelineSize));

            pContext->responseDataFormat = DevDriver::URIDataFormat::Binary;

            result = DevDriver::Result::Success;
        }

        m_mutex.Unlock();
    }

    return result;
}

// =====================================================================================================================
// Writes a header into a pipeline dump file
void PipelineDumpService::WritePipelineDumpHeader(
    DevDriver::URIRequestContext* pContext,
    uint64                        numRecords)
{
    PipelineDumpHeader header = {};
    header.magicNumber = PipelineDumpMagicNumber;
    header.version = PipelineDumpVersion;
    header.numRecords = numRecords;

    pContext->pResponseBlock->Write(reinterpret_cast<const DevDriver::uint8*>(&header),
        sizeof(PipelineDumpHeader));
}

// =====================================================================================================================
// Writes a pipeline record into a pipeline dump file
void PipelineDumpService::WritePipelineDumpRecord(
    DevDriver::URIRequestContext* pContext,
    uint64                        pipelineHash,
    uint64                        pipelineOffset,
    uint64                        pipelineSize)
{
    PipelineDumpRecord record = {};
    record.hash = pipelineHash;
    record.offset = pipelineOffset;
    record.size = pipelineSize;

    pContext->pResponseBlock->Write(reinterpret_cast<const DevDriver::uint8*>(&record),
        sizeof(PipelineDumpRecord));
}
#endif

// =====================================================================================================================
// Registers a pipeline into the pipeline records map and exposes it to the developer driver bus
void PipelineDumpService::RegisterPipeline(
    void*  pPipelineBinary,
    uint32 pipelineBinaryLength,
    uint64 pipelineHash)
{
    m_mutex.Lock();

    PipelineRecord* pRecord = nullptr;
    bool existed = false;
    const Util::Result result = m_pipelineRecords.FindAllocate(pipelineHash, &existed, &pRecord);

    // We only need to store the pipeline binary if it hasn't been registered already.
    // No need to store redundant pipeline data.
    // Also make sure we only continue if we successfully allocated memory.
    if ((existed == false) && (result == Result::Success))
    {
        // Allocate memory to store the pipeline binary data in.
        pRecord->pPipelineBinary = PAL_MALLOC(pipelineBinaryLength, m_pPlatform, AllocInternal);

        // Only continue with the copy if we were able to allocate memory for the pipeline.
        if (pRecord->pPipelineBinary != nullptr)
        {
            // Copy the pipeline binary data into the memory.
            memcpy(pRecord->pPipelineBinary, pPipelineBinary, pipelineBinaryLength);

            // Save the length.
            pRecord->pipelineBinaryLength = pipelineBinaryLength;
        }
        else
        {
            // Erase the pipeline record from the map if we fail to allocate memory to store its binary data.
            const bool eraseResult = m_pipelineRecords.Erase(pipelineHash);
            PAL_ASSERT(eraseResult);
        }
    }

    m_mutex.Unlock();
}

} // Pal
