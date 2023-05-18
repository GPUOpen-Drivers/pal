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

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(RDF_BUILD_LIBRARY) && RDF_BUILD_STATIC == 0
#define RDF_EXPORT __attribute__((visibility("default")))
#else
#define RDF_EXPORT
#endif

#define RDF_IDENTIFIER_SIZE 16

#define RDF_MAKE_VERSION(major, minor, patch) \
    ((static_cast<std::uint32_t>(major) << 22) | \
     (static_cast<std::uint32_t>(minor) << 12) | \
     (static_cast<std::uint32_t>(patch)))

#define RDF_INTERFACE_VERSION RDF_MAKE_VERSION(1, 1, 2)

extern "C" {
struct rdfChunkFile;
struct rdfStream;
struct rdfChunkFileWriter;

enum rdfResult
{
    rdfResultOk = 0,
    rdfResultError = 1,
    rdfResultInvalidArgument = 2
};

enum rdfCompression
{
    rdfCompressionNone = 0,
    rdfCompressionZstd = 1
};

enum rdfStreamAccess
{
    rdfStreamAccessRead = 1,
    rdfStreamAccessReadWrite = 3
};

enum rdfFileMode
{
    // Open an existing file
    rdfFileModeOpen,
    // Create a new file, if it exists, truncate
    rdfFileModeCreate,
};

struct rdfStreamFromFileCreateInfo
{
    const char* filename;
    rdfStreamAccess accessMode;
    rdfFileMode fileMode;
};

/**
 * @brief User-provided I/O callbacks
 *
 * There are five callback functions here which will be called with the user-
 * provided context variable:
 *
 * - Seek/Tell/GetSize must be always non-null
 * - Read/Write can be null. Note that a stream which has both set to null
 *   is invalid. The chunk writer can work with a stream that is in write-
 *   only mode only if it's not appending.
*/
struct rdfUserStream
{
    /**
     * @brief Read count bytes into buffer
     * @return rdfResult
     *
     * This function can be `null` if the stream doesn't support reading.
     *
     * - `bytesRead` can be optionally set, if it's non-null, the number of
     *   bytes actually read must be set.
     * - `buffer` can be null only if `count` is 0
     * - The provided context will be passed into `ctx`
    */
    int (*Read)(void* ctx, const std::int64_t count, void* buffer, std::int64_t* bytesRead);

    /**
     * @brief Write count bytes from buffer
     * @return rdfResult
     *
     * This function can be `null` if the stream doesn't support writing.
     *
     * - `bytesWritten` can be optionally set, if it's non-null, the number of
     *   bytes actually written must be set.
     * - `buffer` can be null only if `count` is 0
     * - The provided context will be passed into `ctx`
    */
    int (*Write)(void* ctx,
                 const std::int64_t count,
                 const void* buffer,
                 std::int64_t* bytesWritten);

    /**
     * @brief Get the current position
     * @return rdfResult
     *
     * This function must be always provided.
     *
     * - `position` must not be null
     * - The provided context will be passed into `ctx`
    */
    int (*Tell)(void* ctx, std::int64_t* position);

    /**
     * @brief Set the current position
     * @return rdfResult
     *
     * This function must be always provided.
     *
     * - `position` must not positive or 0
     * - The provided context will be passed into `ctx`
    */
    int (*Seek)(void* ctx, std::int64_t position);

    /**
     * @brief Get the size
     * @return rdfResult
     *
     * This function must be always provided.
     *
     * -  `size` must not be `null`
     * - The provided context will be passed into `ctx`
    */
    int (*GetSize)(void* ctx, std::int64_t* size);

    void* context;
};

int RDF_EXPORT rdfStreamFromFile(const rdfStreamFromFileCreateInfo* info, rdfStream** stream);

int RDF_EXPORT rdfStreamOpenFile(const char* filename, rdfStream** stream);
int RDF_EXPORT rdfStreamCreateFile(const char* filename, rdfStream** stream);
int RDF_EXPORT rdfStreamFromReadOnlyMemory(const std::int64_t size,
                                           const void* buffer,
                                           rdfStream** stream);
int RDF_EXPORT rdfStreamCreateMemoryStream(rdfStream** stream);

/**
 * @deprecated Use `rdfStreamFromUserStream` instead
 *
 * This entry point will be removed in the next major version
 */
int RDF_EXPORT rdfStreamCreateFromUserStream(const rdfUserStream* userStream, rdfStream** stream);

/**
 * @since 1.1
 */
int RDF_EXPORT rdfStreamFromUserStream(const rdfUserStream* userStream, rdfStream** stream);
int RDF_EXPORT rdfStreamClose(rdfStream** stream);

int RDF_EXPORT rdfStreamRead(rdfStream*,
                             const std::int64_t count,
                             void* buffer,
                             std::int64_t* bytesRead);
int RDF_EXPORT rdfStreamWrite(rdfStream*,
                              const std::int64_t count,
                              const void* buffer,
                              std::int64_t* bytesWritten);
int RDF_EXPORT rdfStreamTell(rdfStream* stream, std::int64_t* position);
int RDF_EXPORT rdfStreamSeek(rdfStream* stream, const std::int64_t offset);
int RDF_EXPORT rdfStreamGetSize(rdfStream* stream, std::int64_t* size);

int RDF_EXPORT rdfChunkFileOpenFile(const char* filename, rdfChunkFile** handle);
int RDF_EXPORT rdfChunkFileOpenStream(rdfStream* stream, rdfChunkFile** handle);
int RDF_EXPORT rdfChunkFileClose(rdfChunkFile** handle);

int RDF_EXPORT rdfChunkFileGetChunkVersion(rdfChunkFile* handle,
                                           const char* chunkId,
                                           const int chunkIndex,
                                           std::uint32_t* chunkVersion);

int RDF_EXPORT rdfChunkFileReadChunkHeader(rdfChunkFile* handle,
                                           const char* chunkId,
                                           const int chunkIndex,
                                           void* buffer);
int RDF_EXPORT rdfChunkFileReadChunkData(rdfChunkFile* handle,
                                         const char* chunkId,
                                         const int chunkIndex,
                                         void* buffer);

int RDF_EXPORT rdfChunkFileGetChunkHeaderSize(rdfChunkFile* handle,
                                              const char* chunkId,
                                              const int chunkIndex,
                                              std::int64_t* size);
int RDF_EXPORT rdfChunkFileGetChunkDataSize(rdfChunkFile* handle,
                                            const char* chunkId,
                                            const int chunkIndex,
                                            std::int64_t* size);
int RDF_EXPORT rdfChunkFileGetChunkCount(rdfChunkFile* handle,
                                         const char* chunkId,
                                         std::int64_t* count);

int RDF_EXPORT rdfChunkFileContainsChunk(rdfChunkFile* handle,
                                         const char* chunkId,
                                         const int chunkIndex,
                                         int* result);

struct rdfChunkFileIterator;
int RDF_EXPORT rdfChunkFileCreateChunkIterator(rdfChunkFile* handle,
                                               rdfChunkFileIterator** iterator);
int RDF_EXPORT rdfChunkFileDestroyChunkIterator(rdfChunkFileIterator** iterator);
int RDF_EXPORT rdfChunkFileIteratorAdvance(rdfChunkFileIterator* iterator);
int RDF_EXPORT rdfChunkFileIteratorIsAtEnd(rdfChunkFileIterator* iterator, int* atEnd);
int RDF_EXPORT rdfChunkFileIteratorGetChunkIdentifier(rdfChunkFileIterator* iterator,
                                                      char identifier[RDF_IDENTIFIER_SIZE]);
int RDF_EXPORT rdfChunkFileIteratorGetChunkIndex(rdfChunkFileIterator* iterator, int* index);

struct rdfChunkCreateInfo
{
    char identifier[RDF_IDENTIFIER_SIZE];
    std::int64_t headerSize;
    const void* pHeader;
    rdfCompression compression;
    std::uint32_t version;
};

struct rdfChunkFileWriterCreateInfo
{
    rdfStream* stream;
    bool appendToFile;
};

int RDF_EXPORT rdfChunkFileWriterCreate(rdfStream* stream, rdfChunkFileWriter** writer);
int RDF_EXPORT rdfChunkFileWriterCreate2(const rdfChunkFileWriterCreateInfo* info,
                                         rdfChunkFileWriter** writer);
int RDF_EXPORT rdfChunkFileWriterDestroy(rdfChunkFileWriter** writer);

int RDF_EXPORT rdfChunkFileWriterBeginChunk(rdfChunkFileWriter* writer,
                                            const rdfChunkCreateInfo* info);
int RDF_EXPORT rdfChunkFileWriterAppendToChunk(rdfChunkFileWriter* writer,
                                               const std::int64_t size,
                                               const void* data);
int RDF_EXPORT rdfChunkFileWriterEndChunk(rdfChunkFileWriter* writer, int* index);

int RDF_EXPORT rdfChunkFileWriterWriteChunk(rdfChunkFileWriter* writer,
                                            const rdfChunkCreateInfo* info,
                                            const std::int64_t size,
                                            const void* data,
                                            int* index);

int RDF_EXPORT rdfResultToString(rdfResult result, const char** output);
}

#if RDF_CXX_BINDINGS
namespace rdf
{
class ApiException : public std::runtime_error
{
public:
    ApiException(rdfResult result) : std::runtime_error("RDF C API call failed"), result_(result) {}

    rdfResult GetResult() const
    {
        return result_;
    }

    const char* what() const noexcept override
    {
        const char* errorMessage = nullptr;
        rdfResultToString(result_, &errorMessage);
        return errorMessage;
    }

private:
    rdfResult result_;
};

#ifndef RDF_CHECK_CALL
#define RDF_CHECK_CALL(c)                                        \
    do {                                                         \
        const auto r_ = c;                                       \
        if (r_ != rdfResultOk) {                                 \
            throw rdf::ApiException(static_cast<rdfResult>(r_)); \
        }                                                        \
    } while (0)
#endif

class Stream final
{
public:
    static Stream OpenFile(const char* filename)
    {
        Stream result;
        RDF_CHECK_CALL(rdfStreamOpenFile(filename, &result.stream_));
        return result;
    }

    static Stream FromFile(const char* filename,
        rdfStreamAccess streamAccess,
        rdfFileMode fileMode)
    {
        Stream result;

        rdfStreamFromFileCreateInfo info = {};
        info.filename = filename;
        info.fileMode = fileMode;
        info.accessMode = streamAccess;

        RDF_CHECK_CALL(rdfStreamFromFile(&info, &result.stream_));
        return result;
    }

    static Stream CreateFile(const char* filename)
    {
        Stream result;
        RDF_CHECK_CALL(rdfStreamCreateFile(filename, &result.stream_));
        return result;
    }

    static Stream FromReadOnlyMemory(const std::int64_t size, const void* buffer)
    {
        Stream result;
        RDF_CHECK_CALL(rdfStreamFromReadOnlyMemory(size, buffer, &result.stream_));
        return result;
    }

    static Stream CreateMemoryStream()
    {
        Stream result;
        RDF_CHECK_CALL(rdfStreamCreateMemoryStream(&result.stream_));
        return result;
    }

    static Stream FromUserStream(const rdfUserStream* userStream)
    {
        Stream result;
        RDF_CHECK_CALL(rdfStreamFromUserStream(userStream, &result.stream_));
        return result;
    }

    ~Stream()
    {
        if (stream_) {
            // No CHECK_CALL -- cannot throw exception from destructor
            rdfStreamClose(&stream_);
        }
    }

    void Close()
    {
        if (stream_) {
            RDF_CHECK_CALL(rdfStreamClose(&stream_));
        }
    }

    std::int64_t Read(const std::int64_t count, void* buffer)
    {
        std::int64_t bytesRead = 0;
        RDF_CHECK_CALL(rdfStreamRead(stream_, count, buffer, &bytesRead));

        return bytesRead;
    }

    template <typename T>
    bool Read(T& v)
    {
        return Read(sizeof(v), &v) == sizeof(v);
    }

    std::int64_t Write(const std::int64_t count, const void* buffer)
    {
        std::int64_t bytesWritten = 0;
        RDF_CHECK_CALL(rdfStreamWrite(stream_, count, buffer, &bytesWritten));

        return bytesWritten;
    }

    template <typename T>
    bool Write(const T& v)
    {
        return Write(sizeof(v), &v) == sizeof(v);
    }

    std::int64_t Tell() const
    {
        std::int64_t position = 0;
        RDF_CHECK_CALL(rdfStreamTell(stream_, &position));

        return position;
    }

    void Seek(const std::int64_t offset)
    {
        RDF_CHECK_CALL(rdfStreamSeek(stream_, offset));
    }

    std::int64_t GetSize() const
    {
        std::int64_t size = 0;
        RDF_CHECK_CALL(rdfStreamGetSize(stream_, &size));

        return size;
    }

    explicit operator rdfStream*() const
    {
        return stream_;
    }

    Stream(Stream&& rhs) noexcept
    {
        stream_ = rhs.stream_;
        rhs.stream_ = nullptr;
    }

    Stream& operator=(Stream&& rhs) noexcept
    {
        stream_ = rhs.stream_;
        rhs.stream_ = nullptr;
        return *this;
    }

private:
    Stream() = default;
    Stream(const Stream& rhs)
    {
        stream_ = rhs.stream_;
    }

    Stream& operator=(const Stream& rhs)
    {
        stream_ = rhs.stream_;
        return *this;
    }

    rdfStream* stream_ = nullptr;
};

class ChunkFileIterator final
{
public:
    ChunkFileIterator(rdfChunkFileIterator* iterator) : it_(iterator) {}

    ChunkFileIterator(ChunkFileIterator&& rhs) noexcept
    {
        it_ = rhs.it_;
        rhs.it_ = nullptr;
    }

    ChunkFileIterator& operator=(ChunkFileIterator&& rhs) noexcept
    {
        it_ = rhs.it_;
        rhs.it_ = nullptr;

        return *this;
    }

    ~ChunkFileIterator()
    {
        if (it_) {
            // No CHECK_CALL -- cannot throw exceptions from destructor
            rdfChunkFileDestroyChunkIterator(&it_);
        }
    }

    ChunkFileIterator(const ChunkFileIterator& rhs) = delete;
    ChunkFileIterator& operator=(const ChunkFileIterator& rhs) = delete;

    void Advance()
    {
        RDF_CHECK_CALL(rdfChunkFileIteratorAdvance(it_));
    }

    bool IsAtEnd() const
    {
        int result = 0;
        RDF_CHECK_CALL(rdfChunkFileIteratorIsAtEnd(it_, &result));

        return result == 1;
    }

    void GetChunkIdentifier(char buffer[RDF_IDENTIFIER_SIZE]) const
    {
        RDF_CHECK_CALL(rdfChunkFileIteratorGetChunkIdentifier(it_, buffer));
    }

    int GetChunkIndex() const
    {
        int index = 0;
        RDF_CHECK_CALL(rdfChunkFileIteratorGetChunkIndex(it_, &index));

        return index;
    }

private:
    rdfChunkFileIterator* it_;
};

class ChunkFile final
{
public:
    ChunkFile(const char* filename)
    {
        RDF_CHECK_CALL(rdfChunkFileOpenFile(filename, &chunkFile_));
    }

    ChunkFile(Stream& stream)
    {
        RDF_CHECK_CALL(rdfChunkFileOpenStream(static_cast<rdfStream*>(stream), &chunkFile_));
    }

    ~ChunkFile()
    {
        if (chunkFile_) {
            // No CHECK_CALL -- cannot throw exceptions from destructor
            rdfChunkFileClose(&chunkFile_);
        }
    }

    ChunkFile(const ChunkFile&) = delete;
    ChunkFile& operator=(const ChunkFile&) = delete;

    ChunkFile(ChunkFile&& rhs) noexcept
    {
        chunkFile_ = rhs.chunkFile_;
        rhs.chunkFile_ = nullptr;
    }

    ChunkFile& operator=(ChunkFile&& rhs) noexcept
    {
        chunkFile_ = rhs.chunkFile_;
        rhs.chunkFile_ = nullptr;
        return *this;
    }

    ChunkFileIterator GetIterator() const
    {
        rdfChunkFileIterator* it;
        RDF_CHECK_CALL(rdfChunkFileCreateChunkIterator(chunkFile_, &it));

        return ChunkFileIterator(it);
    }

    void ReadChunkHeader(
        const char* chunkId,
        const std::function<void(const std::int64_t dataSize, const void* data)>& readCallback)
    {
        ReadChunkHeader(chunkId, 0, readCallback);
    }

    void ReadChunkHeader(
        const char* chunkId,
        const int chunkIndex,
        const std::function<void(const std::int64_t dataSize, const void* data)>& readCallback)
    {
        const auto size = GetChunkHeaderSize(chunkId, chunkIndex);
        std::vector<unsigned char> buffer(size);
        ReadChunkHeaderToBuffer(chunkId, chunkIndex, buffer.data());
        readCallback(size, buffer.data());
    }

    void ReadChunkData(
        const char* chunkId,
        const std::function<void(const std::int64_t dataSize, const void* data)>& readCallback)
    {
        ReadChunkData(chunkId, 0, readCallback);
    }

    void ReadChunkData(
        const char* chunkId,
        const int chunkIndex,
        const std::function<void(const std::int64_t dataSize, const void* data)>& readCallback)
    {
        const auto size = GetChunkDataSize(chunkId, chunkIndex);
        std::vector<unsigned char> buffer(size);
        ReadChunkDataToBuffer(chunkId, chunkIndex, buffer.data());
        readCallback(size, buffer.data());
    }

    void ReadChunkHeaderToBuffer(const char* chunkId, const int chunkIndex, void* buffer)
    {
        RDF_CHECK_CALL(rdfChunkFileReadChunkHeader(chunkFile_, chunkId, chunkIndex, buffer));
    }

    void ReadChunkDataToBuffer(const char* chunkId, const int chunkIndex, void* buffer)
    {
        RDF_CHECK_CALL(rdfChunkFileReadChunkData(chunkFile_, chunkId, chunkIndex, buffer));
    }

    void ReadChunkHeaderToBuffer(const char* chunkId, void* buffer)
    {
        ReadChunkHeaderToBuffer(chunkId, 0, buffer);
    }

    void ReadChunkDataToBuffer(const char* chunkId, void* buffer)
    {
        ReadChunkDataToBuffer(chunkId, 0, buffer);
    }

    std::int64_t GetChunkHeaderSize(const char* chunkId) const
    {
        return GetChunkHeaderSize(chunkId, 0);
    }

    std::int64_t GetChunkHeaderSize(const char* chunkId, const int chunkIndex) const
    {
        std::int64_t size = 0;
        RDF_CHECK_CALL(rdfChunkFileGetChunkHeaderSize(chunkFile_, chunkId, chunkIndex, &size));
        return size;
    }

    std::int64_t GetChunkDataSize(const char* chunkId) const
    {
        return GetChunkDataSize(chunkId, 0);
    }

    std::int64_t GetChunkDataSize(const char* chunkId, const int chunkIndex) const
    {
        std::int64_t size = 0;
        RDF_CHECK_CALL(rdfChunkFileGetChunkDataSize(chunkFile_, chunkId, chunkIndex, &size));
        return size;
    }

    std::uint32_t GetChunkVersion(const char* chunkId) const
    {
        return GetChunkVersion(chunkId, 0);
    }

    std::uint32_t GetChunkVersion(const char* chunkId, const int chunkIndex) const
    {
        std::uint32_t version = 0;
        RDF_CHECK_CALL(rdfChunkFileGetChunkVersion(chunkFile_, chunkId, chunkIndex, &version));
        return version;
    }

    std::int64_t GetChunkCount(const char* chunkId) const
    {
        std::int64_t size = 0;
        RDF_CHECK_CALL(rdfChunkFileGetChunkCount(chunkFile_, chunkId, &size));
        return size;
    }

    bool ContainsChunk(const char* chunkId) const
    {
        return ContainsChunk(chunkId, 0);
    }

    bool ContainsChunk(const char* chunkId, const int chunkIndex) const
    {
        int result = 0;
        RDF_CHECK_CALL(rdfChunkFileContainsChunk(chunkFile_, chunkId, chunkIndex, &result));

        return result == 1;
    }

    explicit operator rdfChunkFile*() const
    {
        return chunkFile_;
    }

private:
    rdfChunkFile* chunkFile_ = nullptr;
};

enum ChunkFileWriteMode
{
    Create,
    Append
};

class ChunkFileWriter final
{
public:
    ChunkFileWriter(Stream& stream)
    {
        RDF_CHECK_CALL(rdfChunkFileWriterCreate(static_cast<rdfStream*>(stream), &writer_));
    }

    ChunkFileWriter(Stream& stream, ChunkFileWriteMode writeMode)
    {
        rdfChunkFileWriterCreateInfo info = {};

        info.stream = static_cast<rdfStream*>(stream);
        info.appendToFile = writeMode == ChunkFileWriteMode::Append;

        RDF_CHECK_CALL(rdfChunkFileWriterCreate2(&info, &writer_));
    }

    ~ChunkFileWriter()
    {
        if (writer_) {
            // No CHECK_CALL -- cannot throw exception from destructor
            rdfChunkFileWriterDestroy(&writer_);
        }
    }

    void Close()
    {
        if (writer_) {
            RDF_CHECK_CALL(rdfChunkFileWriterDestroy(&writer_));
        }
    }

    ChunkFileWriter(const ChunkFileWriter&) = delete;
    ChunkFileWriter& operator=(const ChunkFileWriter&) = delete;

    int WriteChunk(const char* chunkId,
                   const std::int64_t chunkHeaderSize,
                   const void* chunkHeader,
                   const std::int64_t chunkDataSize,
                   const void* chunkData)
    {
        return WriteChunk(
            chunkId, chunkHeaderSize, chunkHeader, chunkDataSize, chunkData, rdfCompressionNone);
    }

    int WriteChunk(const char* chunkId,
                   const std::int64_t chunkHeaderSize,
                   const void* chunkHeader,
                   const std::int64_t chunkDataSize,
                   const void* chunkData,
                   const rdfCompression compression)
    {
        return WriteChunk(
            chunkId, chunkHeaderSize, chunkHeader, chunkDataSize, chunkData, compression, 1);
    }

    int WriteChunk(const char* chunkId,
                   const std::int64_t chunkHeaderSize,
                   const void* chunkHeader,
                   const std::int64_t chunkDataSize,
                   const void* chunkData,
                   const rdfCompression compression,
                   const std::uint32_t version)
    {
        rdfChunkCreateInfo info = {};
        ::memcpy(info.identifier, chunkId,
            SafeStringLength(chunkId, RDF_IDENTIFIER_SIZE));
        info.headerSize = chunkHeaderSize;
        info.pHeader = chunkHeader;
        info.compression = compression;
        info.version = version;

        int index = 0;
        RDF_CHECK_CALL(
            rdfChunkFileWriterWriteChunk(writer_, &info, chunkDataSize, chunkData, &index));
        return index;
    }

    void BeginChunk(const char* chunkId,
                    const std::int64_t chunkHeaderSize,
                    const void* chunkHeader)
    {
        BeginChunk(chunkId, chunkHeaderSize, chunkHeader, rdfCompressionNone);
    }

    void BeginChunk(const char* chunkId,
                    const std::int64_t chunkHeaderSize,
                    const void* chunkHeader,
                    const rdfCompression compression)
    {
        BeginChunk(chunkId, chunkHeaderSize, chunkHeader, compression, 1);
    }

    void BeginChunk(const char* chunkId,
                    const std::int64_t chunkHeaderSize,
                    const void* chunkHeader,
                    const rdfCompression compression,
                    const std::uint32_t version)
    {
        rdfChunkCreateInfo info = {};
        ::memcpy(info.identifier, chunkId,
            SafeStringLength(chunkId, RDF_IDENTIFIER_SIZE));
        info.headerSize = chunkHeaderSize;
        info.pHeader = chunkHeader;
        info.compression = compression;
        info.version = version;

        RDF_CHECK_CALL(rdfChunkFileWriterBeginChunk(writer_, &info));
    }

    template <typename T>
    void AppendToChunk(const T& item)
    {
        AppendToChunk(sizeof(item), static_cast<const void*>(&item));
    }

    void AppendToChunk(const std::int64_t chunkDataSize, const void* chunkData)
    {
        RDF_CHECK_CALL(rdfChunkFileWriterAppendToChunk(writer_, chunkDataSize, chunkData));
    }

    int EndChunk()
    {
        int index = 0;
        RDF_CHECK_CALL(rdfChunkFileWriterEndChunk(writer_, &index));

        return index;
    }

private:
    rdfChunkFileWriter* writer_ = nullptr;

    size_t SafeStringLength(const char* s, const size_t maxLength)
    {
        if (s == nullptr) {
            return 0;
        }

        for (size_t i = 0; i < maxLength; ++i) {
            if (s[i] == '\0') {
                return i;
            }
        }

        return maxLength;
    }
};
}  // namespace rdf
#endif
