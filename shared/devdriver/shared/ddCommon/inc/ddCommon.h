/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddApi.h>
#include <ddPlatform.h>
#include <util/vector.h>

/// Helper macro used to define a previously declared handle
///
/// This macro links the external handle type to the internal handle type and provides ToHandle and FromHandle
/// functions which safely convert between the two types.
#define DD_DEFINE_HANDLE(HandleType, NativeType)             \
    /** Converts the provided type into a handle */         \
    inline HandleType ToHandle(NativeType value)             \
    {                                                        \
        DD_ASSERT(value != nullptr);                         \
        return reinterpret_cast<HandleType>(value);          \
    }                                                        \
                                                             \
    /** Converts the provided handle into its native type */ \
    inline NativeType FromHandle(HandleType value)           \
    {                                                        \
        DD_ASSERT(value != nullptr);                         \
        return reinterpret_cast<NativeType>(value);          \
    }

/// Helper macro used to validate enums that conform to a specific format
///
/// Returns true if the EnumValue should be considered a valid EnumType and false otherwise
/// This macro only works on enums that do the following:
/// 1. Declare 0 as an "unknown" type
/// 2. Have a _COUNT value which contains the maximum valid value for the enum
#define DD_VALIDATE_ENUM(EnumType, EnumValue) ((EnumValue > 0) && (EnumValue < (EnumType ## _COUNT)))

// Forward declaration for DevDriver IMsgChannel to avoid including it in this common header
namespace DevDriver
{
    class IMsgChannel;
}

/// Define DDNetConnection as an alias for IMsgChannel*
DD_DEFINE_HANDLE(DDNetConnection, DevDriver::IMsgChannel*);

/// Default connection timeout value used by APIs
constexpr uint32_t kDefaultConnectionTimeoutMs = 1000;

/// Structure used to manage user specified allocation callbacks
struct ApiAllocCallbacks
{
    PFN_ddAllocCallback pAllocCallback; /// Allocation callback used for any internal memory allocations
    PFN_ddFreeCallback  pFreeCallback;  /// Free callback used to free any internal memory allocations
    void*               pUserdata;      /// Userdata for the allocation callbacks
};

/// Convert a result into a human-friendly string
///
/// This will sanitize `result`, so unrecognzied values are handled correctly.
const char* ddApiResultToString(DD_RESULT result);

/// Translate result codes into a valid DD_RESULT
///
/// This translation is either no-op (because the result is already a valid code), or
/// it translates to one of the UNKNOWN variants: The one from its section, if such a section has been declared,
/// or to DD_RESULT_UNKNOWN otherwise.
DD_RESULT ddApiClampResult(int32_t result);

/// Converts DevDriver::Result to DD_RESULT.
DD_RESULT DevDriverToDDResult(DevDriver::Result result);

/// Shared API allocation function implementation
void* ddApiAlloc(void* pUserdata, size_t size, size_t alignment, bool zero);

/// Shared API free function implementation
void ddApiFree(void* pUserdata, void* pMemory);

/// Shared API default memory allocation function
void* ddApiDefaultAlloc(void* pUserdata, size_t size, size_t alignment, int zero);

/// Shared API default memory free function
void ddApiDefaultFree(void* pUserdata, void* pMemory);

/// Utility class to wrap a DDLoggerInfo
class LoggerUtil
{
public:
    // Sizes a static buffer for formatting messages
    // Messages larger than this will be truncated.
    static constexpr size_t MaxFormattedMessageLen = 1024;

    // Validate whether we can use this `info` or not.
    // Rejects partially filled out structs.
    static bool IsValidInfo(const DDLoggerInfo& info)
    {
        // Note: pUserdata may be NULL, this is at the user's discretion.
        return (
            (info.pfnWillLog != nullptr) &&
            (info.pfnLog     != nullptr) &&
            (info.pfnPush    != nullptr) &&
            (info.pfnPop     != nullptr)
        );
    }

    // Check whether this info is only partially filled out, and missing something important.
    // It's useful to distinguish between a defaulted info, and one that's not filled out
    static bool IsPartialInfo(const DDLoggerInfo& info)
    {
        // Note: pUserdata may be NULL, this is at the user's discretion.
        return (
                (
                    (info.pfnWillLog != nullptr) ||
                    (info.pfnLog     != nullptr) ||
                    (info.pfnPush    != nullptr) ||
                    (info.pfnPop     != nullptr)
                ) &&
                (IsValidInfo(info) == false)
        );
    }

    LoggerUtil() = delete;
    LoggerUtil(const LoggerUtil& other) = default;

    LoggerUtil(const DDLoggerInfo& info);

    /// Returns the information that was used to create this utility class
    const DDLoggerInfo& GetInfo() const { return m_info; }

    /// Log a formatted message
    void Printf(
        const DDLogEvent& event, /// Event to log. This should be fully populated, except for pMessage.
                                 //< The format arguments are processed and pMessage will point to the resulting buffer.
        const char*       pFmt,  /// printf-style format string
        ...                      /// printf-style varargs
    );

    /// Log a formatted message
    void Vprintf(
        const DDLogEvent& event, /// Event to log. This should be fully populated, except for pMessage.
                                 //< The format arguments are processed and pMessage will point to the resulting buffer.
        const char*       pFmt,  /// printf-style format string
        va_list           args   /// printf-style varargs
    );

    /// Log a message
    void Log(const DDLogEvent& event, const char* pMessage);

    ///////////////////////////////////////////////////////////////////////////
    /// Basic Pushing and popping of scope
    void Push();
    void Pop();

    ///////////////////////////////////////////////////////////////////////////
    /// Basic Pushing and popping of scope with an associated event
    void Push(const DDLogEvent& event, const char* pMessage);
    void Pop(const DDLogEvent& event, const char* pMessage);

private:
    DDLoggerInfo m_info;
};

/// Manage a DDByteWriter object that receives data into a fixed size buffer
class FixedBufferByteWriter
{
public:
    /// Construct a ByteWriter that writes into a static type as its fixed buffer
    template <typename T>
    FixedBufferByteWriter(T* pData) : FixedBufferByteWriter(pData, sizeof(*pData))
    {
        static_assert(DevDriver::Platform::IsPod<T>::Value, "To use a type in a FixedByteBufferByteWriter, it must be memcpy safe!");
    }

    /// Construct a ByteWriter that writes into a given fixed buffer
    FixedBufferByteWriter(void* pBuffer, size_t bufferSize);

    const DDByteWriter* Writer() const { return &m_writer; }

private:
    void*        m_pBuffer;
    size_t       m_bufferSize;
    size_t       m_bytesWritten;
    DDByteWriter m_writer;
};

/// Manage a DDByteWriter object that receives data into a dynamically resized buffer
class DynamicBufferByteWriter
{
public:
    /// Construct a ByteWriter that writes into a given fixed buffer
    DynamicBufferByteWriter();

    const DDByteWriter* Writer() const { return &m_writer; }

    void* Buffer() { return m_buffer.Data(); }
    size_t Size() { return m_buffer.Size(); }

    /// Take the ownership of the underlying buffer.
    DevDriver::Vector<uint8_t> Take();

    /// Returns the contents of the buffer as a null terminated character string
    ///
    /// This function returns nullptr if the buffered data is not a valid string
    const char* DataAsString() const
    {
        const char* pStr = nullptr;

        if (m_buffer.IsEmpty() == false)
        {
            if (m_buffer[m_buffer.Size() - 1] == '\0')
            {
                pStr = reinterpret_cast<const char*>(m_buffer.Data());
            }
        }

        return pStr;
    }

private:
    DevDriver::Vector<uint8_t> m_buffer;
    DDByteWriter               m_writer;
};

/// Manage a DDByteWriter object that expects to never receive data
///
/// UsageError is returned if any attempts are made to call the writer functions
template <DD_RESULT UsageError = DD_RESULT_COMMON_UNSUPPORTED>
class EmptyByteWriter
{
public:
    EmptyByteWriter()
    {
        m_writer.pUserdata = this;

        m_writer.pfnBegin = [](void* pUserdata, const size_t* pTotalDataSize) -> DD_RESULT {
            DD_API_UNUSED(pUserdata);
            DD_API_UNUSED(pTotalDataSize);

            return UsageError;
        };

        m_writer.pfnWriteBytes = [](void* pUserdata, const void* pData, size_t dataSize) -> DD_RESULT {
            DD_API_UNUSED(pUserdata);
            DD_API_UNUSED(pData);
            DD_API_UNUSED(dataSize);

            return UsageError;
        };

        m_writer.pfnEnd = [](void* pUserdata, DD_RESULT result) {
            // Nothing to do here
            DD_API_UNUSED(pUserdata);
            DD_API_UNUSED(result);
        };
    }

    const DDByteWriter* Writer() const { return &m_writer; }

private:
    DDByteWriter m_writer;
};

/// Simple C++ wrapper class for the DDByteWriter C interface
///
/// This class drives the provided DDByteWriter with an easier to use interface
class ByteWriterWrapper
{
public:
    ByteWriterWrapper(const DDByteWriter& writer)
        : m_writer(writer)
        , m_started(false)
    {
    }

    /// Begins a byte writing operation and sets the total data size up-front
    ///
    /// NOTE: This method is optional and may be skipped if the caller isn't aware of the total number of bytes to be
    ///       written up-front.
    DD_RESULT Begin(size_t totalDataSize)
    {
        DD_RESULT result = DD_RESULT_UNKNOWN;

        if (m_started == false)
        {
            result = m_writer.pfnBegin(m_writer.pUserdata, &totalDataSize);

            if (result == DD_RESULT_SUCCESS)
            {
                m_started = true;
            }
        }

        return result;
    }

    /// Writes the provided bytes into the underlying writer
    ///
    /// This method will automatically begin the underlying writer if this is the first write into it
    DD_RESULT Write(const void* pData, size_t dataSize)
    {
        DD_RESULT result = DD_RESULT_SUCCESS;

        if (m_started == false)
        {
            result = m_writer.pfnBegin(m_writer.pUserdata, nullptr);

            if (result == DD_RESULT_SUCCESS)
            {
                m_started = true;
            }
        }

        if (result == DD_RESULT_SUCCESS)
        {
            result = m_writer.pfnWriteBytes(m_writer.pUserdata, pData, dataSize);
        }

        return result;
    }

    /// Ends the byte writing operation and closes the underlying writer
    ///
    /// This method MUST be called to finish the write operation!
    void End(DD_RESULT result)
    {
        m_writer.pfnEnd(m_writer.pUserdata, result);
    }

private:
    DDByteWriter m_writer;
    bool         m_started;
};

namespace Internal
{
    /// Helper method to make an Event. Don't call this directly.
    /// We can enable or disable optional fields in this
    DDLogEvent MakeEventHelper(
        DD_LOG_LEVEL level,
        const char*  pCategory,
        const char*  pFilename,
        const char*  pFunction,
        uint32_t     lineNumber
    );
}

// Construct a LogEvent with correct fields
// This is mostly used to make sure __FILE__ and company are set-up correctly.
#define DD_MAKE_LOG_EVENT(level, category) ::Internal::MakeEventHelper(level, category, __FILE__, __FUNCTION__, __LINE__)

// Helper macros to make logging simple
// These can be expanded per component if you never vary category:
#define DD_API_LOG(logger,  level, category, message)  (logger).Log(DD_MAKE_LOG_EVENT(level, category), message)
#define DD_API_LOGF(logger, level, category, fmt, ...) (logger).Printf(DD_MAKE_LOG_EVENT(level, category), fmt, __VA_ARGS__);

DDLoggerInfo GetApiDefaultLoggerInfo();

/// Converts a DevDriver LogLevel to a ddApi DD_LOG_LEVEL
DD_LOG_LEVEL ToDDLogLevel(DevDriver::LogLevel lvl);

/// Converts a ddApi DD_LOG_LEVEL to a DevDriver LogLevel
DevDriver::LogLevel  ToLogLevel(DD_LOG_LEVEL lvl);

/// Default log function that passes messages through to printf
void ddApiDefaultLog(void* pUserdata, DD_LOG_LEVEL level, const char* pMsg);

/// Convert a `DD_DRIVER_STATE` into a human recognizable string.
const char* ddApiDriverStateToString(DD_DRIVER_STATE state);

/// Returns true if the provided driver state implies that the driver has finished its initialization process
bool ddApiIsDriverInitialized(DD_DRIVER_STATE state);

/// Convert a `DDAllocCallbacks` structure into a DevDriver::AllocCb structure.
//< The ApiAllocCallbacks structure must live as long as the AllocCb structure.
void ConvertAllocCallbacks(
    const DDAllocCallbacks& callbacks,
    ApiAllocCallbacks*      pApiAlloc,
    DevDriver::AllocCb*     pAllocCb);

// Verify that alloc callbacks are valid and present, or missing.
//< Returns a valid callback (possibly the default one) in `pCallbacks`.
DD_RESULT ValidateAlloc(
    const DDAllocCallbacks& alloc,
    DDAllocCallbacks*       pCallbacks);

// Default logging function
void DefaultLog(
    void*             pUserdata,
    const DDLogEvent* pEvent,
    const char*       pMessage);

// Helper to validate logging callbacks
DD_RESULT ValidateLog(
    const DDLoggerInfo& logger,
    DDLoggerInfo*       pCallbacks);

// Validate a DDByteWriter object
// This can handle NULL and should be checked before using the writer.
inline bool IsValidDDByteWriter(const DDByteWriter* pWriter)
{
    return ((pWriter != nullptr)                 &&
            (pWriter->pfnBegin      != nullptr)  &&
            (pWriter->pfnWriteBytes != nullptr)  &&
            (pWriter->pfnEnd        != nullptr));
}

// Validate a DDFileWriter object
// This can handle NULL and should be checked before using the writer.
inline bool IsValidDDIOHeartbeat(const DDIOHeartbeat* pIoHeartbeat)
{
    return ((pIoHeartbeat != nullptr)                &&
            (pIoHeartbeat->pfnWriteHeartbeat != nullptr));
}

/// Returns true if the provided buffer should be considered valid
///
/// A buffer with no data inside it is not considered valid
inline bool ValidateBuffer(const void* pBuffer, size_t bufferSize)
{
    return ((bufferSize > 0) && (pBuffer != nullptr));
}

/// Returns true if the caller correctly indicates that no buffer is provided, or if the caller provides a valid buffer
///
/// To correctly indicate a non-present buffer, the caller must provider a nullptr for the buffer and zero for the size
inline bool ValidateOptionalBuffer(const void* pBuffer, size_t bufferSize)
{
    return (((pBuffer == nullptr) && (bufferSize == 0)) ||
            ValidateBuffer(pBuffer, bufferSize));
}
