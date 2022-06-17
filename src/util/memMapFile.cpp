/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palMemMapFile.h"
#include "palFile.h"
#include "palFileMap.h"
#include "palLiterals.h"

using namespace Util::Literals;

namespace Util
{

static const uint32 FileHeaderVersion = 0x1;
constexpr size_t ContainerInitialSize = 0x10000;

// =====================================================================================================================
MemMapFile::~MemMapFile()
{
    CloseStorageFile();
}

// =====================================================================================================================
// Opens a memory mapped file container shared across processes. This function will create a file that doesn't already
// exist, but only if write access is specified.
Result MemMapFile::OpenStorageFile(
    uint32      accessFlags,    // Bitmask of StorageAccessModeFlags
    size_t      mappingSize,    // Initial size of the container, can be set to zero to load the entire file for an
                                // existing file.
    const char* pFileName,      // [In] Fully qualified path and file name of the file to create.
    const char* pSystemName)    // [in] Optional. Name to use for the mapping object, used for sharing mapping objects
                                //      across processes. Can be nullptr
{
    Result result = Result::Success;

    m_accessFlags = accessFlags;
    const bool alreadyExisted = File::Exists(pFileName);

    // Write access is required to create a new file.
    PAL_ASSERT(alreadyExisted || IsWriteable());
    // A valid size must be provided for a new file.
    PAL_ASSERT(alreadyExisted || mappingSize > 0);

    const size_t fileSize = File::GetFileSize(pFileName);
    if (mappingSize == 0)
    {
        mappingSize = fileSize;
    }

    // Try to open a memory mapping for the file.
    result = OpenMemoryMapping(pFileName, mappingSize, alreadyExisted, pSystemName);
    if ((result == Result::Success) && (alreadyExisted == false))
    {
        // If the file did not already exist then we need to intialize the header
        InitializeHeader(m_pActiveContainerHeader, m_mappingSize);
    }

    if ((result == Result::Success) && (mappingSize > fileSize))
    {
        result = ExpandStorage(mappingSize);
    }

    return result;
}

// =====================================================================================================================
void MemMapFile::CloseStorageFile()
{
    m_accessFlags = 0;

    CloseMemoryMapping();
}

// =====================================================================================================================
// Opens a shared memory mapping for the expanding storage container. Returns true if successful, false otherwise.
Result MemMapFile::OpenMemoryMapping(
    const char* pFileName,      // [in] Name of the file to open.
    size_t      mappingSize,    // Size of the memory mapping requested
    bool        validateHeader, // Whether or not to verify the header is valid
    const char* pSystemName)    // [in] Optional. Name to use for the mapping object, used for sharing mapping objects
                                //      across processes.
{
    Result result = m_memoryMapping.Create(pFileName, IsWriteable(), mappingSize, pSystemName);

    if (result == Result::Success)
    {
        m_mappingSize = mappingSize;

        m_pActiveContainerHeader = static_cast<MemMapFileHeader*>(
            m_rootFileView.Map(m_memoryMapping,
                               IsWriteable(),
                               0,
                               sizeof(MemMapFileHeader)));

        if (validateHeader && (m_pActiveContainerHeader != nullptr))
        {
            TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
            {
                result = ValidateHeader(m_pActiveContainerHeader);
            }
            CATCH_ACCESS_FILE_VIEW
            {
                PAL_ASSERT_ALWAYS();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Close the shared memory mapping for the storage container.
void MemMapFile::CloseMemoryMapping()
{
    m_mappingSize = 0;
    m_pActiveContainerHeader = nullptr;
    // No need to flush the view here because we're about to close the file which will implicitly perform a flush.
    m_rootFileView.UnMap(false);
    m_memoryMapping.Close();
}

// =====================================================================================================================
// Get read-write storage space for new data. Returns the offset to the current end of used storage. See documentation
// on FileView for accessing data held by FileView
Result MemMapFile::GetNewStorageSpace(
    size_t    dataSize,       // size required
    bool      advanceStorage, // Indicates whether the used storage offset should be advanced.
    FileView* pOutView)       // [In/Out] Optional. File View that will be mapped onto shared memory
{
    PAL_ASSERT(IsWriteable());

    Result result = Result::ErrorUnknown;

    size_t currentEnd  = GetStorageEnd();
    size_t capacity    = GetStorageCapacity();
    size_t spaceNeeded = currentEnd + dataSize;

    if (spaceNeeded > capacity)
    {
        if (AllowGrowth())
        {
            if (ExpandStorage(spaceNeeded) == Result::Success)
            {
                capacity = GetStorageCapacity();
            }
            else
            {
                capacity = 0;
                CloseStorageFile();
            }
        }
    }

    if (capacity >= spaceNeeded)
    {
        // We successfully got new space, so mark this as a success and map the provided file view and/or advance
        // the storage end position.
        result = Result::Success;
        if (pOutView != nullptr)
        {
            pOutView->Map(m_memoryMapping, IsWriteable(), currentEnd, dataSize);
        }

        if (advanceStorage)
        {
            ManualStorageAdvance(dataSize);
        }
    }

    return result;
}

// =====================================================================================================================
// Gets a file view of a specific range of storage space for an existing object. Returns true if successful, false
// otherwise. See documentation on FileView for accessing data held by FileView.
Result MemMapFile::GetExistingStorage(
    size_t    dataOffset, // desired offest into the storage container
    size_t    dataSize,   // desired view size
    FileView* pOutView    // [In/Out] File View that will be mapped onto the desired memory range
    ) const
{
    PAL_ASSERT(pOutView != nullptr);

    Result result = Result::ErrorUnknown;
    size_t currentOffset = ExternalToLocalOffset(dataOffset);

    if (pOutView->Map(m_memoryMapping, IsWriteable(), currentOffset, dataSize) != nullptr)
    {
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Manually advances the storage conatiner end by a fixed size. Returns true if successful, false otherwise.
Result MemMapFile::ManualStorageAdvance(
    size_t dataSize)    // size to increase the storage conatiner by.
{
    Result result = Result::ErrorUnknown;
    size_t currentEnd = GetStorageEnd();
    size_t capacity   = GetStorageCapacity();
    size_t nextEnd    = currentEnd + dataSize;

    if (capacity >= nextEnd)
    {
        SetStorageEnd(nextEnd);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Reloads an open storage conatiner if changed by another instance. Returns true if successful, false otherwise. On
// failure to reload, the storage container will no longer be valid.
Result MemMapFile::ReloadIfNeeded(
    bool* pWasReloaded)  // [Out] whether or not the conatiner was reloaded
{
    Result result = Result::Success;

    const size_t curFileSize = GetStorageCapacity();

    if (curFileSize != m_mappingSize)
    {
        result = m_memoryMapping.ReloadMap(curFileSize);

        if (result != Result::Success)
        {
            CloseStorageFile();
            PAL_ASSERT_ALWAYS();
        }

        if (pWasReloaded != nullptr)
        {
            *pWasReloaded = true;
        }
    }

    return result;
}

// =====================================================================================================================
// Expand the storage container. Returns success if the container was successfully expanded, and error code otherwise.
// On failure, the storage conatiner will no longer be valid.
Result MemMapFile::ExpandStorage(
    size_t minimumNewSize)  // minimum size requested for expanded storage
{
    Result result = Result::ErrorUnknown;
    const size_t curCapacity = GetStorageCapacity();
    size_t nextCapacity = curCapacity;

    if (minimumNewSize == 0)
    {
        minimumNewSize = curCapacity + 1;
    }

    constexpr size_t DoubleThreshold   = 64_MiB;
    constexpr size_t BlockIncreaseSize = 32_MiB;

    while(nextCapacity < minimumNewSize)
    {
        if (nextCapacity < DoubleThreshold)
        {
            nextCapacity = nextCapacity * 2;
        }
        else
        {
            nextCapacity = nextCapacity + BlockIncreaseSize;
        }
    }

    result = m_memoryMapping.ReloadMap(nextCapacity);
    if (result == Result::Success)
    {
        SetStorageCapacity(nextCapacity);
    }

    return result;
}

// =====================================================================================================================
// Accesses the storage capacity from the header. Returns the size of storage container capacity reported by the
// header, InvalidOffset on failure. Uses SEH for accessing memory mapped file view.
size_t MemMapFile::GetStorageCapacity() const
{
    size_t storageCapacity = InvalidOffset;

    TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
    {
        storageCapacity = m_pActiveContainerHeader->storageCapacity;
    }
    CATCH_ACCESS_FILE_VIEW
    {
        PAL_ASSERT_ALWAYS();
    }

    return storageCapacity;
}

// =====================================================================================================================
// Accesses the storage end from the header. Returns the size of current storage container end reported by the
// header, InvalidOffset on failure. Uses SEH for accessing memory mapped file view
size_t MemMapFile::GetStorageEnd() const
{
    size_t storageEnd = InvalidOffset;

    TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
    {
        storageEnd = m_pActiveContainerHeader->storageEnd;
    }
    CATCH_ACCESS_FILE_VIEW
    {
        PAL_ASSERT_ALWAYS();
    }

    return storageEnd;
}

// =====================================================================================================================
// Sets the storage capacity to the header. Returns true if successful, false otherwise. Uses SEH for accessing
// memory mapped file view
Result MemMapFile::SetStorageCapacity(
    size_t storageCapacity) // storage capactiy to set
{
    Result result = Result::ErrorUnknown;

    TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
    {
        m_pActiveContainerHeader->storageCapacity = storageCapacity;
        result = Result::Success;
    }
    CATCH_ACCESS_FILE_VIEW
    {
        PAL_ASSERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Sets the storage container data end. Returns true if successful, false otherwise. Uses SEH for accessing memory
// mapped file view.
Result MemMapFile::SetStorageEnd(
    size_t storageEnd)  // storage end offset to set
{
    Result result = Result::ErrorUnknown;

    TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
    {
        m_pActiveContainerHeader->storageEnd = storageEnd;
        result = Result::Success;
    }
    CATCH_ACCESS_FILE_VIEW
    {
        PAL_ASSERT_ALWAYS();
    }

    return result;
}

// =====================================================================================================================
// Gets size of storage container header from the  Returns the size of the header, InvalidOffset on failure. Uses SEH
// for accessing memory mapped file view
size_t MemMapFile::GetHeaderSize() const
{
    size_t headerSize = InvalidOffset;

    TRY_ACCESS_FILE_VIEW(m_pActiveContainerHeader != nullptr)
    {
        headerSize = m_pActiveContainerHeader->headerSize;
    }
    CATCH_ACCESS_FILE_VIEW
    {
        PAL_ASSERT_ALWAYS();
    }

    return headerSize;
}

// =====================================================================================================================
void MemMapFile::InitializeHeader(
    MemMapFileHeader* pHeader,     // [Out] header to initialize
    size_t            storageSize) // starting size for container
{
    PAL_ASSERT(pHeader != nullptr);

    pHeader->headerSize      = sizeof(MemMapFileHeader);
    pHeader->storageEnd      = pHeader->headerSize;
    pHeader->storageCapacity = storageSize;
}

// =====================================================================================================================
// Validates a storage container header. Returns true if valid, false otherwise.
Result MemMapFile::ValidateHeader(
    const MemMapFileHeader* pHeader)   // [In] header to validate
{
    PAL_ASSERT(pHeader != nullptr);

    Result result = Result::ErrorUnknown;

    if ((sizeof(MemMapFileHeader) == pHeader->headerSize) &&
        (pHeader->storageEnd >= pHeader->headerSize) &&
        (pHeader->storageCapacity >= pHeader->storageEnd))
    {
        result = Result::Success;
    }

    return result;
}

} // Util
