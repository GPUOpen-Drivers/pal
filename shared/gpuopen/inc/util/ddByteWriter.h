/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */
#pragma once
#include "ddUriInterface.h"
#include "ddPlatform.h"

#include <util/vector.h>

namespace DevDriver
{

    class ByteWriter final : public IByteWriter
    {
    public:
        // A Callback to write bytes. The user data pointer is passed through the Writer class's constructor.
        // The callback is expected to handle a special "End-Of-Writer" message when
        // both `pBytes` == `nullptr` and `numBytes` == `0`. This is sent during `End()`, and signifies that
        // all writing through this writer is finished.
        // `pUserData` may be `nullptr`.
        typedef Result(*WriteBytesCb)(void* pUserData, const void* pBytes, size_t numBytes);

        // Write bytes into a Vector<uint8>
        explicit ByteWriter(Vector<uint8>* pBuff)
            : m_pUserData(pBuff),
              m_pfnWriter(WriteBytesViaVectorCb),
              m_lastResult(Result::Success)
        {}

        // Constructs a `ByteWriter` with a callback and its expected user data pointer.
        // `pUserData` may be `nullptr`, if your callback doesn't use it.
        explicit ByteWriter(void* pUserData, WriteBytesCb callback)
            : m_pUserData(pUserData),
              m_pfnWriter(callback),
              m_lastResult(Result::Success)
        {}

        ~ByteWriter() override = default;

        // Finish all writing and return the last error.
        Result End() override
        {
            if (CanWrite())
            {
                // Special "End of Writer" call
                m_lastResult = (m_pfnWriter)(m_pUserData, nullptr, 0);
            }

            // Overwrite the last result with success to allow for subsequent use of the writer.
            const Result result = m_lastResult;
            m_lastResult = Result::Success;

            return result;
        }

        // Write bytes
        void WriteBytes(const void* pBytes, size_t numBytes) override
        {
            if (CanWrite())
            {
                if (pBytes != nullptr)
                {
                    m_lastResult = (m_pfnWriter)(m_pUserData, (uint8*)pBytes, numBytes);
                }
                else
                {
                    m_lastResult = Result::Error;
                }
            }
        }

    private:
        // If the writer's callback returns an error, it saves that error and predicates all of its write functions.
        // This is called before every invocation of the callback.
        bool CanWrite() const
        {
            return (m_lastResult == Result::Success);
        }

        // User-supplied pointer that is passed to every 'm_pfnWriter' call.
        void* m_pUserData;

        // The callback to write bytes. This is never changed after initialization.
        WriteBytesCb const m_pfnWriter;

        // The last result generated.
        Result m_lastResult;

        static inline Result WriteBytesViaVectorCb(void* pUserData, const void* pInBytes, size_t numBytes)
        {
            Result result = Result::Success;
            Vector<uint8>& outBuffer  = *static_cast<Vector<uint8>*>(pUserData);

            // Special "End of Writer" call
            if ((pInBytes == nullptr) && (numBytes == 0))
            {
                // We can use this to flush a buffer or something exotic.
                // Vector<> has no such requirements, so we do nothing.
            }
            // Regular write - copy the buffer out
            else if ((pInBytes != nullptr))
            {
                const uint8* pInBuffer = static_cast<const uint8*>(pInBytes);
                result = outBuffer.Append(pInBuffer, numBytes) ? Result::Success : Result::InsufficientMemory;
            }
            else
            {
                // We should not have landed here - either the pointer is valid, or it's not.
                // To land here, we got a NULL pointer and a non-zero size!
                // This is a programmer error in TextWriter
                DD_PRINT(LogLevel::Warn, "pInBytes=%p, numBytes=%zu", pInBytes, numBytes);
                DD_ASSERT_ALWAYS();
                result = Result::Error;
            }

            return result;
        };
    };

} // DevDriver
