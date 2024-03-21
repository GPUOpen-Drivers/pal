/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_common_api.h>
#include <stdint.h>

namespace DevDriver
{

struct SettingsBlob
{
    /// The total size of this object.
    /// Computed as the blob size plus the size of this struct and any alignment required.
    /// When multiple blobs are stored in one buffer, use `size` to get the relative
    /// offset to the next blob.
    uint32_t size;
    /// The size in bytes of the blob payload.
    uint32_t blobSize;
    /// Whether the blob is encoded.
    bool encoded;
    /// The starting offset of the magic buffer used for encoding.
    uint32_t magicOffset;
    /// hash of the blob
    uint64_t blobHash;
    /// A variable-sized byte array, representing a Settings blob.
    uint8_t blob[1];
};

/// All Settings blobs are packed in one buffer. This struct always sit at the
/// very beginning of the buffer. Each blob is prefixed with a `SettingsBlob`.
struct SettingsBlobsAll
{
    /// The version of the schema based on which Settings blobs are packed.
    /// Bump this number when either `SettingsBlobsAll` or `SettingsBlob`
    /// changes. `version` must always be the FIRST field in this struct.
    uint32_t version;
    /// The number of blobs in a buffer.
    uint32_t nblobs;
};

/// Each subclass of `SettingsBlobNode` holds a raw buffer of Settings data string
/// blob, and is intended to be linked in a global linked list. All `SettingsBlobNode`s
/// can be received together in one buffer.
class SettingsBlobNode
{
public:
    /// A pointer to the first `SettingsBlob` in the global list.
    static SettingsBlobNode* s_pFirst;
    static SettingsBlobNode* s_pLast;

private:
    SettingsBlobNode* m_pNext;

public:
    SettingsBlobNode();

    /// Return a pointer to the raw Settings data string blob. The byte-size of
    /// the Settings blob is written to `pOutSize`. Note, the byte-size does
    /// not include the null-terminator at the end of the string blob (if it
    /// has one).
    virtual const uint8_t* GetBlob(uint32_t* pOutSize) = 0;

    /// Return whether this blob is encoded.
    virtual bool IsEncoded() = 0;

    /// Return the starting offset of the magic buffer used for encoding.
    virtual uint32_t GetMagicOffset() = 0;

    /// Return the hash of the blob.
    virtual uint64_t GetBlobHash() = 0;

    /// Return a pointer to the next `SettingsBlob` in the global linked list.
    SettingsBlobNode* Next() const
    {
        return m_pNext;
    }

    /// Fill the `pBuffer` with Settings blobs from all linked `SettingsBlobNode`s. All
    /// Settings blobs are packed into one buffer. See `SettingsBlobsAll` to learn how they
    /// are packed.
    ///
    /// @param pBuffer A pointer to a buffer to receive all Settings blobs. It can be nullptr.
    /// @param bufferSize The size of \param pBuffer.
    ///
    /// @return The size required for a buffer to receive all Settings blobs, regardless of
    /// whether \param pBuffer is nullptr.
    static uint32_t GetAllSettingsBlobs(uint8_t* pBuffer, uint32_t bufferSize);

};

} // namespace DevDriver
