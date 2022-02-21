/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if PAL_BUILD_RDF
#include "palTraceSession.h"
#include "palHashSetImpl.h"
#include "core/imported/rdf/rdf/inc/amdrdf.h"

using namespace Pal;

namespace GpuUtil
{
static_assert(TextIdentifierSize == RDF_IDENTIFIER_SIZE,
              "The text identifer size of the trace chunk must match that of the rdf chunk!");

// =====================================================================================================================
// Translates a rdfResult to a Pal::Result
static Result RdfResultToPalResult(
    int rResult)
{
    Result result;

    switch (rResult)
    {
    case rdfResult::rdfResultOk:
        result = Result::Success;
        break;
    case rdfResult::rdfResultInvalidArgument:
    case rdfResult::rdfResultError:
    default:
        // The default case is being included here, since more error codes may be added to rdf in the future.
        result = Result::ErrorUnknown;
        break;
    }

    return result;
}

// =====================================================================================================================
TraceSession::TraceSession(IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_registeredTraceSources(64, pPlatform),
    m_registeredTraceControllers(64, pPlatform),
    m_sessionState(TraceSessionState::Ready),
    m_pChunkFileWriter(nullptr),
    m_pCurrentStream(nullptr)
{
}

// =====================================================================================================================
TraceSession::~TraceSession()
{
}

// =====================================================================================================================
Result TraceSession::Init()
{
    Result result = m_registeredTraceSources.Init();

    if (result == Result::Success)
    {
        result = m_registeredTraceControllers.Init();
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::RegisterController(
    ITraceController* pController)
{
    Result result = (pController != nullptr) ? Result::Success : Result::ErrorInvalidPointer;

    if (result == Result::Success)
    {
        if (m_sessionState == TraceSessionState::Ready)
        {
            m_registerTraceControllerLock.LockForWrite();
            result = m_registeredTraceControllers.Contains(pController) ?
                    Result::AlreadyExists : m_registeredTraceControllers.Insert(pController);
            m_registerTraceControllerLock.UnlockForWrite();
        }
        else if (m_sessionState == TraceSessionState::Progress)
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::UnregisterController(
    ITraceController* pController)
{
    Result result = Result::Success;

    if (m_sessionState == TraceSessionState::Ready)
    {
        m_registerTraceControllerLock.LockForWrite();
        result = m_registeredTraceControllers.Erase(pController) ? Result::Success : Result::NotFound;
        m_registerTraceControllerLock.UnlockForWrite();
    }
    else if (m_sessionState == TraceSessionState::Progress)
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::RegisterSource(
    ITraceSource* pSource)
{
    Result result = (pSource != nullptr) ? Result::Success : Result::ErrorInvalidPointer;

    if (result == Result::Success)
    {
        if (m_sessionState == TraceSessionState::Ready)
        {
            m_registerTraceSourceLock.LockForWrite();
            result = m_registeredTraceSources.Contains(pSource) ?
                    Result::AlreadyExists : m_registeredTraceSources.Insert(pSource);
            m_registerTraceSourceLock.UnlockForWrite();
        }
        else if (m_sessionState == TraceSessionState::Progress)
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::UnregisterSource(
    ITraceSource* pSource)
{
    Result result = Result::Success;

    if (m_sessionState == TraceSessionState::Ready)
    {
        m_registerTraceSourceLock.LockForWrite();
        result = m_registeredTraceSources.Erase(pSource) ? Result::Success : Result::NotFound;
        m_registerTraceSourceLock.UnlockForWrite();
    }
    else if (m_sessionState == TraceSessionState::Progress)
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::CollectTrace(
    void*   pData,
    size_t* pDataSize)
{
    Result result     = Result::Success;

    if (pDataSize == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (m_sessionState == TraceSessionState::Ready)
    {
        result = Result::ErrorUnavailable;
    }
    else if (m_sessionState == TraceSessionState::Progress)
    {
        int64 streamSize = static_cast<int64>(*pDataSize);

        // Check if the ChunkWriter hasn't already been closed ie. don't destroy it twice.
        if (m_pChunkFileWriter != nullptr)
        {
            // Destroying(ie.closing) the ChunkWriter ensures that all data, both compressed and uncompressed, is written
            // to the data stream. This step also completes the RDF file by adding the final parts(index entries) of the
            // file. Trace data and data-sizes will be correctly outputted only after this step.
            result = RdfResultToPalResult(rdfChunkFileWriterDestroy(&m_pChunkFileWriter));

            // We need to move the RDF offset manually to the beginning of the data stream
            if (result == Result::Success)
            {
                result = RdfResultToPalResult(rdfStreamSeek(m_pCurrentStream, 0));
            }

            if (result == Result::Success)
            {
                result = RdfResultToPalResult(rdfStreamGetSize(m_pCurrentStream, &streamSize));
            }
        }

        if (pData != nullptr)
        {
            if (*pDataSize < streamSize)
            {
                result = Result::ErrorInvalidMemorySize;
            }
            else
            {
                // Read all trace data in the current stream in RDF format
                int64 bytesRead = 0;
                if (result == Result::Success)
                {
                    result = RdfResultToPalResult(rdfStreamRead(m_pCurrentStream, streamSize, pData, &bytesRead));
                }

                if (result == Result::Success)
                {
                    result = RdfResultToPalResult(rdfStreamClose(&m_pCurrentStream));
                }

                if (result == Result::Success)
                {
                    m_sessionState = TraceSessionState::Ready;
                }
            }
        }
        else
        {
            *pDataSize = streamSize;
        }
    }
    else
    {
        result = Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Writes/appends the data chunks in one long data block ie. the current data stream
Result TraceSession::WriteDataChunk(
    ITraceSource*         pSource,
    const TraceChunkInfo& info)
{
    Result result = Result::Success;

    // Setup the chunk data stream and chunk writer at the beginning
    if (m_sessionState == TraceSessionState::Ready)
    {
        // Create the stream to be used in the chunkFileWriter. This will also be used to retrieve
        // the final list of appended chunks for a trace in CollectTrace()
        result = RdfResultToPalResult(rdfStreamCreateMemoryStream(&m_pCurrentStream));

        // Create the chunkFileWriter to setup the chunk data structures and buffers to collect the incoming chunks
        if (result == Result::Success)
        {
            result = RdfResultToPalResult(rdfChunkFileWriterCreate(m_pCurrentStream, &m_pChunkFileWriter));
        }

        m_sessionState = TraceSessionState::Progress;
    }

    // Populate rdfChunkCreateInfo parameters from TraceChunkInfo struct
    rdfChunkCreateInfo currentChunkInfo;
    currentChunkInfo.headerSize  = info.headerSize;
    currentChunkInfo.compression = info.enableCompression ?
                                   rdfCompression::rdfCompressionZstd : rdfCompression::rdfCompressionNone;
    currentChunkInfo.version     = info.version;
    currentChunkInfo.pHeader     = info.pHeader;
    memcpy(currentChunkInfo.identifier, info.id, TextIdentifierSize);

    m_chunkAppendLock.LockForWrite();

    // Append the incoming chunk to the data stream
    if (result == Result::Success)
    {
        result = RdfResultToPalResult(rdfChunkFileWriterWriteChunk(m_pChunkFileWriter,
                                                                   &currentChunkInfo,
                                                                   info.dataSize,
                                                                   info.pData,
                                                                   &m_currentChunkIndex));
    }

    m_chunkAppendLock.UnlockForWrite();

    return result;
}

// =====================================================================================================================
void TraceSession::FinishTrace()
{

    m_registerTraceSourceLock.LockForRead();

    // Notify all trace sources that the trace has finished
    for (auto iter = m_registeredTraceSources.Begin(); iter.Get() != nullptr; iter.Next())
    {
        iter.Get()->key->OnTraceFinished(); // Writes data into TraceSession
    }

    m_registerTraceSourceLock.UnlockForRead();
}

}
#endif
