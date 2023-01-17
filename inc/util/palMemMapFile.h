/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palMemMapFile.h
* @brief PAL utility class for a memory mapped, expandable on-disk storage file
***********************************************************************************************************************
*/

#pragma once

#include "palFile.h"
#include "palUtil.h"
#include "palFileMap.h"

namespace Util
{

/// Enumerates access modes for expandable storage
enum StorageAccessModeFlags : uint32
{
    Writeable       = 0x1,
    AllowGrowth     = 0x2,
    DiscardContents = 0x4
};

/// Structure defining the expanded storage header
struct MemMapFileHeader
{
    uint32 headerSize;         ///< complete file header size for file type
    uint32 fileVersion;        ///< file version

    // Navigation
    size_t storageCapacity;    ///< How large the last successful memory mapping requested was
    size_t storageEnd;         ///< current "end" of the cache file for appending new blocks
    uint32 reserved[10];       ///< reserved for future expansion
};

/**
***********************************************************************************************************************
* @brief This class provides an interface for a memory mapped file on disk.
***********************************************************************************************************************
*/
class MemMapFile
{
public:

    /// Constructor.
    MemMapFile() {}
    ///Destructor.
    ~MemMapFile();

    /// Opens a memory mapped file for this expanding storage container that can be shared across processes. This
    /// function will create a file that does not exist, but only if write access is specified.
    ///
    /// @param  accessFlags     Bitmask of StorageAccessModeFlags.
    /// @param  mappingSize     Initial size of the container, can be set to zero to load the entire file for an
    ///                         existing file.
    /// @param  pFileName       Name of the file to open.
    /// @param  pSystemName     Optional. Name to use for the file apping object, used for sharing mapping objects
    ///                         across processes. Can be nullptr.
    ///
    /// @returns Success if the file was opened successfully, an appropriate error code otherwise.
    Result OpenStorageFile(uint32 accessFlags, size_t mappingSize, const char* pFileName, const char* pSystemName);

    /// Closes the memory mapped file associated with this expanding storage container.
    void CloseStorageFile();

    /// Flushes the file buffer ensuring that all cached writes to disk are completed
    bool Flush() { return m_memoryMapping.Flush(); }

    /// Gets read-write storage space for new data.
    ///
    /// @param  dataSize       Size, in bytes, of additional storage required.
    /// @param  advanceStorage Indicates whether the used storage offset should be advanced.
    /// @param  pOutView       Optional. File View that will be mapped onto the shared memory
    ///
    /// @note See documentation on @ref FileView for accessing data held by the FileView.
    ///
    /// @returns Success if new space was successfully created.
    Result GetNewStorageSpace(size_t dataSize, bool advanceStorage, FileView* pOutView);

    /// Gets a read-only FileView of the desired range in the expanding storage file.
    ///
    /// @param  dataOffset Offset into storage to create the FileView.
    /// @param  dataSize   Desired size of the FileView.
    /// @param  pOutView   File View that will be mapped onto the desired memory range
    ///
    /// @note See documentation on @ref FileView for accessing data held by the FileView.
    ///
    /// @returns Success if successful.
    Result GetExistingStorage(size_t dataOffset, size_t dataSize, FileView* pOutView) const;

    /// Manually advances the storage container end by a fixed size.
    ///
    /// @param       dataSize   Desired number of bytes to advance the container end.
    ///
    /// @returns Success if successful.
    Result ManualStorageAdvance(size_t dataSize);

    /// Reloads an open storage container if the underlying file was changed by another instance.
    ///
    /// @param [out] pWasReloaded  Indicates whether the storage container was reloaded.
    ///
    /// @returns Success if successful. On failure the storage container is no longer valid.
    Result ReloadIfNeeded(bool* pWasReloaded);

    /// Value returned for invalid offset or size
    static const size_t InvalidOffset = 0xffffffff;

    /// Checks if this expanding storage container is writeable
    ///
    /// @return True if the container is writeable, false otherwise.
    bool IsWriteable() const { return TestAnyFlagSet(m_accessFlags, StorageAccessModeFlags::Writeable); }

    /// Checks if this expanding storage container allows growth
    ///
    /// @return True if the container allows growth, false otherwise.
    bool AllowGrowth() const { return TestAnyFlagSet(m_accessFlags, StorageAccessModeFlags::AllowGrowth); }

    /// Returns the current usable size of the storage container (not counting the storage header).
    ///
    /// @return Size of the storage container, or -1 if the container is invalid.
    size_t GetStorageSize() const { return GetStorageCapacity() - GetHeaderSize(); }

private:

    /// Opens the memory mapping handle.
    Result OpenMemoryMapping(const char* pFileName, size_t mappingSize, bool validateHeader, const char* pSystemName);
    /// Closes the memory mapping handle.
    void CloseMemoryMapping();
    /// Expands the available storage by reopening the memory mapping.
    Result ExpandStorage(size_t minimumNewSize);

    /// Accessor wrappings for memory mapped values (with full SEH)
    /// Gets current storage capacity
    size_t GetStorageCapacity() const;
    /// Sets current storage capacity
    Result SetStorageCapacity(size_t storageCapacity);
    /// Sets the offset of the current end of storage
    size_t GetStorageEnd() const;
    /// Sets the offset for the current end of storage.
    Result SetStorageEnd(size_t storageEnd);
    /// Gets the size of the header data
    size_t GetHeaderSize() const;

    /// Converts a local offset (includes header) to an external offset (which doesn't account for header).
    size_t LocalToExternalOffset(size_t localOffset) const
    {
        return localOffset - GetHeaderSize();
    }

    /// Converts a external offset (which doesn't account for header) to an internal offset (includes header).
    size_t ExternalToLocalOffset(size_t externalOffset) const
    {
        return externalOffset + GetHeaderSize();
    }

    /// Initializes the header data in storage.
    static void InitializeHeader(MemMapFileHeader* pHeader, size_t storageSize);
    /// Validates header data.
    static Result ValidateHeader(const MemMapFileHeader* pHeader);

    // Data members
    FileMapping       m_memoryMapping;            ///< File mapping for storage file
    FileView          m_rootFileView;             ///< Memory view into the file header
    MemMapFileHeader* m_pActiveContainerHeader;   ///< Shared container header
    size_t            m_mappingSize;              ///< Last known size of storage file
    uint32            m_accessFlags;              ///< file was opened with write access

    // Dissallow copy/assign
    PAL_DISALLOW_COPY_AND_ASSIGN(MemMapFile);
};

}
