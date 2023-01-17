/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  palArchiveFile.h
* @brief PAL utility library IArchiveFile class declaration.
***********************************************************************************************************************
*/

#pragma once

#include "palUtil.h"
#include "palSysMemory.h"

#include <limits.h>
#include <stdlib.h>

namespace Util
{

class IArchiveFile;
class IPlatformKey;
struct ArchiveEntryHeader;

// On Linux, NAME_MAX is the maximum filename length, while PATH_MAX defines the maximum path length.
// On Windows, maximum file and path lengths are defined by _MAX_FNAME and MAX_PATH, respectively.
// We add 1 to accommodate a null terminator.
// Note that PathBufferLen already considers the full path length, including the file name part, so
// there's no need to add them when creating buffers.
#if defined(__unix__)
static constexpr size_t     FilenameBufferLen = NAME_MAX + 1;
static constexpr size_t     PathBufferLen     = PATH_MAX + 1;
#else
static constexpr size_t     FilenameBufferLen = _MAX_FNAME + 1;
static constexpr size_t     PathBufferLen     = _MAX_PATH + 1;
#endif

/**
***********************************************************************************************************************
* @brief Description of an archive file to be opened
***********************************************************************************************************************
*/
struct ArchiveFileOpenInfo
{
    AllocCallbacks*     pMemoryCallbacks;         ///< Allocation callbacks suitable for long-term use. Must live
                                                  ///  for the lifetime of the ArchiveFile object
    const char*         pFilePath;                ///< Path to where the archive file can be found
    const char*         pFileName;                ///< Name of the archive file to be opened
    const IPlatformKey* pPlatformKey;             ///< Optional ID containing information about driver/platform. If
                                                  ///  nullptr is passed the platform verification will be skipped
    uint32              archiveType;              ///< Optional type ID signifying the intended consumer type of
                                                  ///  this archive. The client may use this as an extra ID check to
                                                  ///  distinguish between valid and invalid files. A value of 0
                                                  ///  will perform no check.
    bool                useStrictVersionControl;  ///< Forbid minor version number differences in archive format
    bool                allowCreateFile;          ///< Create the file if one does not exist
    bool                allowWriteAccess;         ///< Open file with write access
    bool                allowAsyncFileIo;         ///< Allow use of OS specific asynchronous file routines
    bool                useBufferedReadMemory;    ///< Allow preloading/read-ahead of file into memory
    size_t              maxReadBufferMem;         ///< Maximum size allowed for read buffer
    void*               pSecurity;                ///< Pointer to an os-specific security attribute to use for file ops.
};

/// Get the memory size needed for an archive file object
///
/// @param [in] pOpenInfo   Information describing how to open the archive file
///
/// @return Minimum number of bytes needed for placement new buffers passed into OpenArchiveFile() for pOpenInfo
size_t GetArchiveFileObjectSize(
    const ArchiveFileOpenInfo* pOpenInfo);

/// Opens a file on disk as a "PAL Archive File"
///
/// This Interface may cause disk access routines to be called by the underlying OS.
///
/// @param [in]     pOpenInfo       Information about which file to open
/// @param [in]     pPlacementAddr  Pointer to the location where the interface should be constructed. There must
///                                 be as much size available here as reported by calling GetArchiveFileObjectSize().
/// @param [out]    ppHashProvider  Archive file interface. On failure this value will be set to nullptr.
///
/// @returns Success if the file archive was opened from disc. Otherwise, one of the following errors may be returned:
///          + ErrorUnavailable if no file matching pOpenInfo could be found on disc or the file could not be opened
///          + ErrorNotShareable if the file was found but is locked for the access requested
///          + ErrorInvalidPointer if pOpenInfo, pPlacementAddr, or ppArchiveFile, is nullptr.
///          + ErrorOutOfMemory when there is not enough system memory to open the file.
///          + ErrorIncompatibleLibrary if the archive file found is invalid or is not compatible with the current driver
///          + ErrorInitializationFailed if there is an internal error.
Result OpenArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo,
    void*                      pPlacementAddr,
    IArchiveFile**             ppArchiveFile);

/// Attempt to create an empty archive file on disc
///
/// @param [in]     pOpenInfo       Information about which file to create
///
/// @returns Success if the file archive was deleted from disc. Otherwise, one of the following errors may be returned:
///          + AlreadyExists if the file already exists on disc.
///          + ErrorUnavailable if the file was not able to be created.
///          + ErrorInvalidPointer if pOpenInfo, is nullptr.
///          + ErrorUnknown if there is an internal error.
Result CreateArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo);

/// Attempt to delete an archive file on disc
///
/// @param [in]     pOpenInfo       Information about which file to delete
///
/// @returns Success if the file archive was deleted from disc. Otherwise, one of the following errors may be returned:
///          + NotFound if no file matching pOpenInfo could be found on disc.
///          + ErrorUnavailable/ErrorNotShareable if the file was found but cannot be deleted.
///          + ErrorInvalidPointer if pOpenInfo, is nullptr.
///          + ErrorUnknown if there is an internal error.
Result DeleteArchiveFile(
    const ArchiveFileOpenInfo* pOpenInfo);

/**
***********************************************************************************************************************
* @brief Interface for reading and writing to a file adhering to the PAL Archive file format
***********************************************************************************************************************
*/
class IArchiveFile
{
public:
    /// Get the number of entries stored within the archive file
    ///
    /// @return Total count of entries in archive file
    virtual size_t GetEntryCount() const = 0;

    /// Signal that information from a file block should be read into the read buffer if available
    ///
    /// If async file reads are allowed, this function will return before the read is complete.
    ///
    /// @param [in] startLocation   Location in file to begin reading
    /// @param [in] maxReadSize     Maximum size in bytes of data to read from file
    ///
    /// @return Success if the data preloaded. Otherwise, one of the following may be returned:
    ///         + Unsupported if preloading is not allowed on this file.
    ///         + ErrorInvalidValue if startLocation is past the end of the file/
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Preload(
        size_t startLocation,
        size_t maxReadSize) = 0;

    /// Reads entry headers from the file and populates an in memory array of ArchiveEntryHeader structures
    ///
    /// @param [out] pHeaders       Array of ArchiveEntryHeader structures. There should be sufficient memory to hold
    ///                             maxEntries worth of headers.
    /// @param [in]  startEntry     Ordinal number of entry to start reading from.
    /// @param [in]  maxEntries     Maximum number of entries to read from the file.
    /// @param [out] pEntriesFilled Number of entries read out from the file into the provided table
    ///
    /// @return Success if the headers are read into the array. Otherwise, one of the following may be returned:
    ///         + NotReady if the data requested is still being streamed in (requires Async File IO)
    ///         + ErrorInvalidValue if startEntry is not present in the file
    ///         + ErrorUnknown if there is an internal error.
    virtual Result FillEntryHeaderTable(
        ArchiveEntryHeader* pHeaders,
        size_t              startEntry,
        size_t              maxEntries,
        size_t*             pEntriesFilled) = 0;

    /// Gets a specific entry by Ordinal ID
    ///
    /// @param [in]  index      Ordinal ID number corresponding to the header requested
    /// @param [out] pHeader    Header entry to be filled out
    ///
    /// @return Success if the header was retrieved. Otherwise one of the following may be returned:
    ///         + NotReady if the data requested is still being streamed in (requires Async File IO)
    ///         + ErrorInvalidValue if index is not present in the file
    ///         + ErrorUnknown if there is an internal error.
    virtual Result GetEntryByIndex(
        size_t              index,
        ArchiveEntryHeader* pHeader) = 0;

    /// Read the data for an entry located by its header
    ///
    /// @param [in]  pHeader        Header of data entry desired
    /// @param [out] pDataBuffer    Buffer to read data into. There should be sufficient memory to hold
    ///                             pHeader->dataSize number of bytes.
    ///
    /// @return Success if the data read completed without error. Otherwise, one of the following may be returned:
    ///         + NotFound if pHeader could not be found in the ArchiveFile
    ///         + NotReady if the data requested is still being streamed in (requires Async File IO)
    ///         + ErrorInvalidPointer if pHeader or pDataBuffer is nullptr
    ///         + ErrorInvalidValue if pHeader->dataPosition is past the end of the file
    ///         + ErrorDataCorrupted if pData fails pHeader->dataCrc64 check
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Read(
        const ArchiveEntryHeader*   pHeader,
        void*                       pDataBuffer) = 0;

    /// Write header and data out to archive file
    ///
    /// If async file writes are allowed, this function will return before the write is fully complete.
    ///
    /// @param [in/out] pHeader Header for new data entry. Header data will be modified to refect output file
    /// @param [in]     pData   Data to be stored for the entry. pHeader->dataSize number of bytes will be read from
    ///                         this memory location
    ///
    /// @return Success if the data write completed without error. Otherwise, one of the following may be returned:
    ///         + Unsupported if the file was not opened with write access
    ///         + ErrorInvalidPointer if pHeader or pDataBuffer is nullptr
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Write(
        ArchiveEntryHeader* pHeader,
        const void*         pData) = 0;

    /// Return whether the file allows writes.
    ///
    /// @return true if the file was opened with allowWriteAccess, false otherwise.
    virtual bool   AllowWriteAccess() const = 0;

    /// Destroy the archive file interface. Closing the file if necessary.
    ///
    ///  If async file writes are allowed this function may block if there are pending writes to complete.
    virtual void   Destroy() = 0;

protected:
    IArchiveFile() {}
    virtual ~IArchiveFile() {}

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(IArchiveFile);
};

} // namespace Util
