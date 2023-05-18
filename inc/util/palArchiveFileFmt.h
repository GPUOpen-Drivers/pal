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
* @file  palArchiveFileFmt.h
* @brief Declaration of format used to store pal archive files.
***********************************************************************************************************************
*/
/**
***********************************************************************************************************************
* @note Due to the need for binary compatibility with external sources, all values in this file must be of explict
*       size types. (eg: uint8, uint32, uint64)
***********************************************************************************************************************
*/

#pragma once

#include "palUtil.h"

namespace Util
{

#pragma pack(push, 1)
/**
***********************************************************************************************************************
* @brief "Magic Numbers" used to identify parts of the archive
***********************************************************************************************************************
*/
constexpr uint8 MagicArchiveMarker[16]  =                       ///< Identifies the start of the archive file data.
    {0x23, 0xd8, 0xfa, 0xe7, 0x0f, 0x5f, 0x47, 0xbe,            ///  Aligns with the start of ArchiveFileHeader
     0x8b, 0xd1, 0x48, 0xf5, 0xd8, 0xf0, 0xb4, 0xa7};
constexpr uint8 MagicFooterMarker[4]    = {'F','O','T','R'};    ///< Identifies the start of the ArchiveFileFooter
constexpr uint8 MagicEntryMarker[4]     = {'N','T','R','Y'};    ///< Identifies the start of an ArchiveEntryHeader

/**
***********************************************************************************************************************
* @brief Version constants. Must be updated if this file is changed
***********************************************************************************************************************
*/
#if PAL_64BIT_ARCHIVE_FILE_FMT
constexpr uint32 CurrentMajorVersion    = 2;    ///< Version number denoting compatibility breaking changes
constexpr uint32 CurrentMinorVersion    = 0;    ///< Version number denoting changes that should be backward compatible
#else
constexpr uint32 CurrentMajorVersion    = 1;    ///< Version number denoting compatibility breaking changes
constexpr uint32 CurrentMinorVersion    = 2;    ///< Version number denoting changes that should be backward compatible
#endif

/**
***********************************************************************************************************************
* @brief A header stored at the front of the archive file
***********************************************************************************************************************
*/
struct ArchiveFileHeader
{
    uint8  archiveMarker[16];   ///< Fixed marker bookending our archive format, must match MagicArchiveMarker
    uint32 majorVersion;        ///< Major (breaking) version of the archive format
    uint32 minorVersion;        ///< Minor (compatible) version of the archive format
#if PAL_64BIT_ARCHIVE_FILE_FMT
    uint64 firstBlock;          ///< Byte offset of first block from the start of the archive
#else
    uint32 firstBlock;          ///< Byte offset of first block from the start of the archive
#endif
    uint32 archiveType;         ///< Optional type ID signifying the intended consumer type of this archive
    uint8  platformKey[20];     ///< Optional 160-bit (max) hash value of the OS/Hardware/Driver
};

/**
***********************************************************************************************************************
* @brief A footer stored at the end of the archive file
***********************************************************************************************************************
*/
struct ArchiveFileFooter
{
    uint8  footerMarker[4];    ///< Fixed marker to designate the footer, must match MagicFooterMarker
#if PAL_64BIT_ARCHIVE_FILE_FMT
    uint64 entryCount;         ///< Count of all entries stored within the archive
#else
    uint32 entryCount;         ///< Count of all entries stored within the archive
#endif
    uint64 lastWriteTimestamp; ///< Timestamp of when this file was last written to according to the application
    uint8  archiveMarker[16];  ///< Fixed marker bookending our archive format, must match MagicArchiveMarker
};

/**
***********************************************************************************************************************
* @brief A header stored for each archive entry
***********************************************************************************************************************
*/
struct ArchiveEntryHeader
{
    uint8  entryMarker[4];  ///< Fixed marker to designate an entry, must match MagicEntryMarker
#if PAL_64BIT_ARCHIVE_FILE_FMT
    uint64 ordinalId;       ///< Index of entry in the archive file as ordinal number
    uint64 nextBlock;       ///< Byte offset of next block in file from start of archive
    uint64 dataSize;        ///< Size of entry data
    uint64 dataPosition;    ///< Byte offset of entry data from start of archive
#else
    uint32 ordinalId;       ///< Index of entry in the archive file as ordinal number
    uint32 nextBlock;       ///< Byte offset of next block in file from start of archive
    uint32 dataSize;        ///< Size of entry data
    uint32 dataPosition;    ///< Byte offset of entry data from start of archive
#endif
    uint64 dataCrc64;       ///< Checksum for data integrity
    uint32 dataType;        ///< Optional ID signifying the data type for the entry
    uint8  entryKey[20];    ///< 160-bit (max) hash key for the entry
#if PAL_64BIT_ARCHIVE_FILE_FMT
    uint64 metaValue;       ///< Optional meta-data value for use by consumer of data
#else
    uint32 metaValue;       ///< Optional meta-data value for use by consumer of data
#endif
};
#pragma pack(pop)

} // namespace Util
