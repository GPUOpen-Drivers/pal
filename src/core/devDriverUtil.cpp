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

#include "core/devDriverUtil.h"
#include "core/device.h"
#include "palHashMapImpl.h"
#include "palJsonWriter.h"

#include "devDriverServer.h"
#include "protocols/driverControlServer.h"
#include "ddTransferManager.h"

using namespace Util;

namespace Pal
{

// Size required to hold a hash text string and null terminator.
static const uint32 HashStringBufferSize = 19;

// Lookup table for binary to hex string conversion.
static const char HexStringLookup[] = "0123456789ABCDEF";

// =====================================================================================================================
// Helper function which converts an array of bytes into a hex string.
void ConvertToHexString(
    char*        pDstBuffer,
    const uint8* pSrcBuffer,
    size_t       srcBufferSize)
{
    // Two characters for each byte in hex plus null terminator
    const size_t numTextBytes = (srcBufferSize * 2) + 1;

    // Build a hex string in the destination buffer.
    for (uint32 byteIndex = 0; byteIndex < srcBufferSize; ++byteIndex)
    {
        const size_t bufferOffset = (byteIndex * 2);
        char* pBufferStart = pDstBuffer + bufferOffset;
        const uint8 byteValue = pSrcBuffer[byteIndex];
        pBufferStart[0] = HexStringLookup[byteValue >> 4];
        pBufferStart[1] = HexStringLookup[byteValue & 0x0F];
    }

    // Null terminate the string
    pDstBuffer[numTextBytes - 1] = '\0';
}

// =====================================================================================================================
// An JsonStream implementation that writes json data directly to a developer driver transfer block.
class BlockJsonStream : public Util::JsonStream
{
public:
    explicit BlockJsonStream(
        DevDriver::TransferProtocol::LocalBlock* pBlock)
        :
        m_pBlock(pBlock) {}
    ~BlockJsonStream() {}

    virtual void WriteString(
        const char* pString,
        uint32      length) override
    {
        m_pBlock->Write(reinterpret_cast<const DevDriver::uint8*>(pString), length);
    }

    virtual void WriteCharacter(
        char character) override
    {
        m_pBlock->Write(reinterpret_cast<const DevDriver::uint8*>(&character), 1);
    }

private:
    DevDriver::TransferProtocol::LocalBlock* m_pBlock;

    PAL_DISALLOW_COPY_AND_ASSIGN(BlockJsonStream);
    PAL_DISALLOW_DEFAULT_CTOR(BlockJsonStream);
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
    DevDriver::URIProtocol::URIService("pipelinedump"),
    m_pPlatform(pPlatform),
    m_pipelineRecords(0x4000, pPlatform),
    m_maxPipelineBinarySize(0)
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
DevDriver::Result PipelineDumpService::HandleRequest(
    char*                                                             pArguments,
    DevDriver::SharedPointer<DevDriver::TransferProtocol::LocalBlock> pBlock)
{
    DevDriver::Result result = DevDriver::Result::Error;

    if (strcmp(pArguments, "index") == 0)
    {
        // The client requested an index of all available pipeline dumps.

        BlockJsonStream jsonStream(pBlock.Get());
        JsonWriter jsonWriter(&jsonStream);
        jsonWriter.BeginList(false);

        m_mutex.Lock();

        // Allocate a small buffer on the stack that's exactly large enough for a hex string.
        char hashStringBuffer[HashStringBufferSize];
        auto iter = m_pipelineRecords.Begin();
        while (iter.Get())
        {
            Util::Snprintf(hashStringBuffer, sizeof(hashStringBuffer), "0x%016llX", iter.Get()->key);
            jsonWriter.Value(hashStringBuffer);

            iter.Next();
        }

        m_mutex.Unlock();

        jsonWriter.EndList();

        result = DevDriver::Result::Success;
    }
    else if (strcmp(pArguments, "all") == 0)
    {
        // The client requested all pipeline dumps.

        m_mutex.Lock();

        // Allocate enough space to handle the request.

        // Two characters for each byte in hex plus null terminator
        const uint32 maxNumTextBytes = (m_maxPipelineBinarySize * 2) + 1;
        const uint32 scratchMemorySize = Util::Max(HashStringBufferSize, maxNumTextBytes);
        void* pScratchMemory = PAL_MALLOC(scratchMemorySize,
                                          m_pPlatform,
                                          AllocInternalTemp);

        if (pScratchMemory != nullptr)
        {
            BlockJsonStream jsonStream(pBlock.Get());
            JsonWriter jsonWriter(&jsonStream);
            jsonWriter.BeginList(false);

            auto iter = m_pipelineRecords.Begin();
            while (iter.Get())
            {
                const PipelineRecord* pRecord = &iter.Get()->value;

                const uint32 numBytes = pRecord->pipelineBinaryLength;

                // Two characters for each byte in hex plus null terminator
                const uint32 numTextBytes = (numBytes * 2) + 1;

                jsonWriter.BeginMap(false);

                Util::Snprintf(reinterpret_cast<char*>(pScratchMemory),
                               scratchMemorySize,
                               "0x%016llX",
                               iter.Get()->key);
                jsonWriter.KeyAndValue("hash", reinterpret_cast<char*>(pScratchMemory));

                jsonWriter.Key("binary");

                // Build a hex string of the pipeline binary data in the scratch buffer.
                ConvertToHexString(reinterpret_cast<char*>(pScratchMemory),
                                   reinterpret_cast<const uint8*>(pRecord->pPipelineBinary),
                                   numBytes);

                jsonWriter.Value(reinterpret_cast<char*>(pScratchMemory));

                jsonWriter.EndMap();

                iter.Next();
            }

            jsonWriter.EndList();

            PAL_SAFE_FREE(pScratchMemory, m_pPlatform);

            result = DevDriver::Result::Success;
        }

        m_mutex.Unlock();
    }
    else
    {
        // The client requested a specific pipeline dump via the pipeline hash.

        m_mutex.Lock();

        const uint64 pipelineHash = strtoull(pArguments, nullptr, 16);
        const PipelineRecord* pRecord = m_pipelineRecords.FindKey(pipelineHash);
        if (pRecord != nullptr)
        {
            const uint32 numBytes = pRecord->pipelineBinaryLength;

            // Two characters for each byte in hex plus null terminator
            const uint32 numTextBytes = (numBytes * 2) + 1;

            // Allocate exactly enough memory for the specific pipeline's binary data.
            const uint32 scratchMemorySize = Util::Max(HashStringBufferSize, numTextBytes);
            void* pScratchMemory = PAL_MALLOC(scratchMemorySize,
                                              m_pPlatform,
                                              AllocInternalTemp);
            if (pScratchMemory != nullptr)
            {
                BlockJsonStream jsonStream(pBlock.Get());
                JsonWriter jsonWriter(&jsonStream);

                jsonWriter.BeginMap(false);

                Util::Snprintf(reinterpret_cast<char*>(pScratchMemory), scratchMemorySize, "0x%016llX", pipelineHash);
                jsonWriter.KeyAndValue("hash", reinterpret_cast<char*>(pScratchMemory));

                jsonWriter.Key("binary");

                // Build a hex string of the pipeline binary data in the scratch buffer.
                ConvertToHexString(reinterpret_cast<char*>(pScratchMemory),
                                   reinterpret_cast<const uint8*>(pRecord->pPipelineBinary),
                                   numBytes);

                jsonWriter.Value(reinterpret_cast<char*>(pScratchMemory));

                jsonWriter.EndMap();

                result = DevDriver::Result::Success;

                PAL_FREE(pScratchMemory, m_pPlatform);
            }
        }

        m_mutex.Unlock();
    }

    return result;
}

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

            // Update the max pipeline binary size.
            m_maxPipelineBinarySize = Util::Max(m_maxPipelineBinarySize, pipelineBinaryLength);
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
