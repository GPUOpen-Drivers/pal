/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddPlatform.h"
#include "protocols/ddPipelineUriService.h"
#include "protocols/ddURIProtocol.h"

#include "util/ddByteReader.h"
#include "util/template.h"
#include "util/vector.h"

#include <cctype>

namespace DevDriver
{

// Helper to parse exclusion bit fields. Accepts strings like these:
//      "0x1234"
//      "0123"
//      "1234"
//      ""
// Rejects strings like these:
//      "0x1z23"
//      "0x10      "
static Result ParseExclusionFlags(ExclusionFlags* pFlags, const char* pString)
{
    DD_ASSERT(pFlags != nullptr);
    auto result = Result::UriInvalidChar;

    if (pString == nullptr)
    {
        pFlags->allFlags = 0;
        result = Result::Success;
    }
    else
    {
        char* pEnd = nullptr;
        uint64 flags = strtoull(pString, &pEnd, 16);
        if (pEnd != nullptr && *pEnd == '\0')
        {
            pFlags->allFlags = flags;
            result = Result::Success;
        }
    }

    return result;
}

PipelineRecordsIterator::PipelineRecordsIterator(const void* pBlobBegin, size_t blobSize)
    :
    m_record(),
    m_reader(pBlobBegin, VoidPtrInc(pBlobBegin, blobSize)),
    m_lastResult(Result::Success)
{
    // Try and read the first item now, so that calls to Get() work immediately.
    Next();
}

bool PipelineRecordsIterator::Get(PipelineRecord* pRecord) const
{
    // Errors will halt the iterator
    // When the iterator is exhausted, m_lastResult is EndOfStream
    bool hasMoreRecords = (m_lastResult == Result::Success);
    if (hasMoreRecords && (pRecord != nullptr))
    {
        *pRecord = m_record;
    }
    return hasMoreRecords;
}

void PipelineRecordsIterator::Next()
{
    if (m_reader.Remaining() != 0)
    {
        PipelineRecord record;
        if (m_lastResult == Result::Success)
        {
            m_lastResult = m_reader.Read(&record.header);
        }

        if (m_lastResult == Result::Success)
        {
            record.pBinary = m_reader.Get();
            m_lastResult = m_reader.Skip(record.header.size);
        }

        if (m_lastResult == Result::Success)
        {
            // Only overwrite our persistant record if the read succeeds.
            m_record = record;
        }
    }
    else
    {
        // We have no more space to read. If we've not hit any errors thus far, mark it as EndOfStream.
        if (m_lastResult == Result::Success)
        {
            m_lastResult = Result::EndOfStream;
        }
    }
}

PipelineUriService::PipelineUriService()
    :
    DevDriver::IService(),
    m_pWriter(nullptr),
    m_driverInfo({}),
    m_lock()
{}

PipelineUriService::~PipelineUriService()
{}

Result PipelineUriService::Init(const DriverInfo& driverInfo)
{
    Platform::LockGuard<Platform::AtomicLock> guard(m_lock);

    m_driverInfo = driverInfo;

    // The `QueryPostSizeLimit()` is only called when the post data is not inline.
    // we override 0-entries here with that limit to avoid confusion: Setting 0 does not disable post.
    if (m_driverInfo.postSizeLimit < URIProtocol::kMaxInlineDataSize)
    {
        m_driverInfo.postSizeLimit = URIProtocol::kMaxInlineDataSize;
    }

    Result result = Result::Error;
    if (m_driverInfo.pUserData == nullptr)
    {
        result = Result::InvalidParameter;
    }
    else if ((m_driverInfo.pfnGetPipelineHashes         == nullptr) &&
             (m_driverInfo.pfnGetPipelineCodeObjects    == nullptr) &&
             (m_driverInfo.pfnInjectPipelineCodeObjects == nullptr))
    {
        // At least one callback must be provided. Otherwise, what's the point?
        result = Result::InvalidParameter;
    }
    else
    {
        result = Result::Success;
    }

    return result;
}

// Handles a request from a developer driver client.
DevDriver::Result PipelineUriService::HandleRequest(DevDriver::IURIRequestContext* pContext)
{
    DD_ASSERT(m_pWriter == nullptr);

    Platform::LockGuard<Platform::AtomicLock> guard(m_lock);
    Result result = Result::UriInvalidParameters;

    const char* const pArgDelim = " ";
    char* pStrtokContext = nullptr;
    // Safety note: Strtok handles nullptr by returning nullptr. We handle that below.
    char* pCmdName       = Platform::Strtok(pContext->GetRequestArguments(), pArgDelim, &pStrtokContext);
    char* pCmdArg1       = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);
    char* pCmdArg2       = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);

    if (pCmdName == nullptr)
    {
        // This happens when no command is given, and the request string looks like: "pipeline://".
        // Really, no command *is* a command... that we don't support.
        // We must explicitly handle this here, because it is undefined behavior to pass nullptr to strcmp.
    }
    else if ((strcmp(pCmdName, "getIndex") == 0) && //
             (pCmdArg2 == nullptr))                 // One or zero arguments
    {
        if (m_driverInfo.pfnGetPipelineHashes != nullptr)
        {
            // The client requested an index of the pipeline binaries.
            ExclusionFlags flags;
            result = ParseExclusionFlags(&flags, pCmdArg1);
            if (result == Result::Success)
            {
                result = pContext->BeginByteResponse(&m_pWriter);
                if (result == Result::Success)
                {
                    result = m_driverInfo.pfnGetPipelineHashes(this, m_driverInfo.pUserData, flags);
                }
                if (result == DevDriver::Result::Success)
                {
                    result = m_pWriter->End();
                    m_pWriter = nullptr;
                }
            }
        }
        else
        {
            result = Result::Unavailable;
        }
    }
    else if ((strcmp(pCmdName, "getPipelines") == 0) && //
             (pCmdArg2 == nullptr))                     // One or zero arguments
    {
        if (m_driverInfo.pfnGetPipelineCodeObjects != nullptr)
        {
            // The client requested the full binary code object of a specific pipeline or pipelines.
            const PostDataInfo& postData = pContext->GetPostData();
            if ((postData.size == 0) || (postData.size % sizeof(PipelineHash) != 0))
            {
                result = Result::UriInvalidPostDataSize;
            }
            else
            {
                ExclusionFlags flags;
                result = ParseExclusionFlags(&flags, pCmdArg1);
                if (result == Result::Success)
                {
                    const auto* pHashes = static_cast<const PipelineHash*>(postData.pData);
                    size_t numHashes = postData.size / sizeof(PipelineHash);

                    result = pContext->BeginByteResponse(&m_pWriter);
                    if (result == Result::Success)
                    {
                        // Pipeline code objects are written by the API client through
                        //      this->AddPipeline() (which then uses m_pWriter).
                        result = m_driverInfo.pfnGetPipelineCodeObjects(this, m_driverInfo.pUserData, flags, pHashes, numHashes);
                    }
                    if (result == Result::Success)
                    {
                        result = m_pWriter->End();
                        m_pWriter = nullptr;
                    }
                }
            }
        }
        else
        {
            result = Result::Unavailable;
        }
    }
    else if ((strcmp(pCmdName, "getAllPipelines") == 0) && //
             (pCmdArg2 == nullptr))                        // One or zero arguments
    {
        if (m_driverInfo.pfnGetPipelineCodeObjects != nullptr)
        {
            ExclusionFlags flags;
            result = ParseExclusionFlags(&flags, pCmdArg1);
            if (result == Result::Success)
            {
                result = pContext->BeginByteResponse(&m_pWriter);
                if (result == Result::Success)
                {
                    // Pass a null pHashes and a 0 numHashes to request all pipelines.
                    result = m_driverInfo.pfnGetPipelineCodeObjects(this, m_driverInfo.pUserData, flags, nullptr, 0);
                }
                if (result == Result::Success)
                {
                    result = m_pWriter->End();
                    m_pWriter = nullptr;
                }
            }
        }
        else
        {
            result = Result::Unavailable;
        }
    }
    else if ((strcmp(pCmdName, "reinject") == 0) && //
             (pCmdArg1 == nullptr))                 // Zero arguments
    {
        if (m_driverInfo.pfnInjectPipelineCodeObjects != nullptr)
        {
            const PostDataInfo& postData = pContext->GetPostData();
            if (postData.size < sizeof(PipelineRecordHeader))
            {
                // It's an error if there isn't enough data for 1 pipeline header.
                // This is likely to happen if the post data is missing or tragically wrong.
                result = Result::UriInvalidPostDataSize;
            }
            else
            {
                PipelineRecordsIterator pipelineIterator(postData.pData, postData.size);
                result = m_driverInfo.pfnInjectPipelineCodeObjects(m_driverInfo.pUserData,
                                                                   pipelineIterator);
            }
        }
        else
        {
            result = Result::Unavailable;
        }
    }

    if (m_pWriter != nullptr)
    {
        const Result end_result = m_pWriter->End();
        m_pWriter = nullptr;
        DD_PRINT(LogLevel::Error, "m_pWriter->End() == 0x%X", end_result);
        DD_ASSERT_REASON("PipelineUriService didn't finish writing a request.");
    }

    return result;
}

size_t PipelineUriService::QueryPostSizeLimit(char* pArgs) const
{
    // Note: Commands whose callbacks were not provided at Init() will report a postSizeLimit of 0.
    // Commands that never need post are not explicitly checked here - e.g. pipeline://index.

    char* pStrtokContext = nullptr;
    char* pCmdName = Platform::Strtok(pArgs, " ", &pStrtokContext);

    size_t postSizeLimit = 0;
    if (((strcmp(pCmdName, "getPipelines") == 0) && (m_driverInfo.pfnGetPipelineCodeObjects    != nullptr)) ||
        ((strcmp(pCmdName, "reinject")     == 0) && (m_driverInfo.pfnInjectPipelineCodeObjects != nullptr)))
    {
        postSizeLimit = m_driverInfo.postSizeLimit;
    }

    return postSizeLimit;
}

void PipelineUriService::AddHash(const PipelineHash& hash, uint64 size)
{
    DD_ASSERT(m_pWriter != nullptr);
    m_pWriter->WriteBytes(&hash, sizeof(hash));
    m_pWriter->WriteBytes(&size, sizeof(size));
}

void PipelineUriService::AddPipeline(const PipelineRecord& record)
{
    DD_ASSERT(m_pWriter != nullptr);
    m_pWriter->Write(record.header);
    if ((record.header.size > 0) && (record.pBinary != nullptr))
    {
        if (record.header.size >= UINT32_MAX) {
            // The protocol does not support sizes this large.
            DD_ASSERT_ALWAYS();
        }
        m_pWriter->WriteBytes(record.pBinary, static_cast<uint32>(record.header.size));
    }
}

} // DevDriver
