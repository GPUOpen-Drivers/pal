/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palHashBaseImpl.h
 * @brief PAL utility collection shared class implementations used by the HashMap and HashSet containers.
 ***********************************************************************************************************************
 */

#pragma once

#include "palHashBase.h"

namespace Util
{

// =====================================================================================================================
// Default hash function implementation.  Simply shift the key to the right and use the resulting bits as the hash.
template<typename Key>
PAL_INLINE uint32 DefaultHashFunc<Key>::operator()(
    const void* pVoidKey,
    uint32      keyLen
    ) const
{
    // We need this union to do a type conversion from a Key type to a uint for the hash function.  This ensures
    // that our data won't change when casting and that we don't have to guess which _cast<> operation is the most
    // appropriate for each data type for which this template class may be instantiated.
    union KeyUint32
    {
        Key    key;
        uint32 uint;
    } hash = { };

    // Get the raw bits.
    const Key* pKey = static_cast<const Key*>(pVoidKey);
    hash.key = *pKey;

    // Discard the low bits.
    return (hash.uint >> ShiftNum);
}

// =====================================================================================================================
// Hashes the specified key value with the Jenkins hash algorithm.  Implementation based on the algorithm description
// found here: http://burtleburtle.net/bob/hash/doobs.html.
// By Bob Jenkins, 1996. bob_jenkins@compuserve.com. You may use this
// code any way you wish, private, educational, or commercial. It's free.
// See http:\\ourworld.compuserve.com\homepages\bob_jenkins\evahash.htm
// Use for hash table lookup, or anything where one collision in 2^^32 is
// acceptable. Do NOT use for cryptographic purposes.
template<typename Key>
PAL_INLINE uint32 JenkinsHashFunc<Key>::operator()(
    const void* pVoidKey,
    uint32      keyLen
    ) const
{
    // Mixing table.
    static const uint8 MixTable[256] =
    {
        251, 175, 119, 215,  81,  14,  79, 191, 103,  49, 181, 143, 186, 157,   0, 232,
         31,  32,  55,  60, 152,  58,  17, 237, 174,  70, 160, 144, 220,  90,  57, 223,
         59,   3,  18, 140, 111, 166, 203, 196, 134, 243, 124,  95, 222, 179, 197,  65,
        180,  48,  36,  15, 107,  46, 233, 130, 165,  30, 123, 161, 209,  23,  97,  16,
         40,  91, 219,  61, 100,  10, 210, 109, 250, 127,  22, 138,  29, 108, 244,  67,
        207,   9, 178, 204,  74,  98, 126, 249, 167, 116,  34,  77, 193, 200, 121,   5,
         20, 113,  71,  35, 128,  13, 182,  94,  25, 226, 227, 199,  75,  27,  41, 245,
        230, 224,  43, 225, 177,  26, 155, 150, 212, 142, 218, 115, 241,  73,  88, 105,
         39, 114,  62, 255, 192, 201, 145, 214, 168, 158, 221, 148, 154, 122,  12,  84,
         82, 163,  44, 139, 228, 236, 205, 242, 217,  11, 187, 146, 159,  64,  86, 239,
        195,  42, 106, 198, 118, 112, 184, 172,  87,   2, 173, 117, 176, 229, 247, 253,
        137, 185,  99, 164, 102, 147,  45,  66, 231,  52, 141, 211, 194, 206, 246, 238,
         56, 110,  78, 248,  63, 240, 189,  93,  92,  51,  53, 183,  19, 171,  72,  50,
         33, 104, 101,  69,   8, 252,  83, 120,  76, 135,  85,  54, 202, 125, 188, 213,
         96, 235, 136, 208, 162, 129, 190, 132, 156,  38,  47,   1,   7, 254,  24,   4,
        216, 131,  89,  21,  28, 133,  37, 153, 149,  80, 170,  68,   6, 169, 234, 151
    };

    const uint8* pKey = static_cast<const uint8*>(pVoidKey);

    uint32 a   = 0x9e3779b9;         // The golden ratio; an arbitrary value.
    uint32 b   = a;
    uint32 c   = MixTable[pKey[0]];  // Arbitrary value.
    uint32 len = keyLen;

    // Handle most of the key.
    while (len >= 12)
    {
        a = a + (pKey[0] + (static_cast<uint32>(pKey[1])  << 8)  +
                           (static_cast<uint32>(pKey[2])  << 16) +
                           (static_cast<uint32>(pKey[3])  << 24));
        b = b + (pKey[4] + (static_cast<uint32>(pKey[5])  << 8)  +
                           (static_cast<uint32>(pKey[6])  << 16) +
                           (static_cast<uint32>(pKey[7])  << 24));
        c = c + (pKey[8] + (static_cast<uint32>(pKey[9])  << 8)  +
                           (static_cast<uint32>(pKey[10]) << 16) +
                           (static_cast<uint32>(pKey[11]) << 24));

        a = a - b;  a = a - c;  a = a ^ (c >> 13);
        b = b - c;  b = b - a;  b = b ^ (a << 8);
        c = c - a;  c = c - b;  c = c ^ (b >> 13);
        a = a - b;  a = a - c;  a = a ^ (c >> 12);
        b = b - c;  b = b - a;  b = b ^ (a << 16);
        c = c - a;  c = c - b;  c = c ^ (b >> 5);
        a = a - b;  a = a - c;  a = a ^ (c >> 3);
        b = b - c;  b = b - a;  b = b ^ (a << 10);
        c = c - a;  c = c - b;  c = c ^ (b >> 15);

        pKey = pKey + 12;
        len  = len  - 12;
    }

    // Handle last 11 bytes.
    c = c + keyLen;
    switch (len)
    {
    case 11: c = c + (static_cast<uint32>(pKey[10]) << 24);  // fall through
    case 10: c = c + (static_cast<uint32>(pKey[9])  << 16);  // fall through
    case  9: c = c + (static_cast<uint32>(pKey[8])  << 8);   // fall through
    // the first byte of c is reserved for the length
    case  8: b = b + (static_cast<uint32>(pKey[7])  << 24);  // fall through
    case  7: b = b + (static_cast<uint32>(pKey[6])  << 16);  // fall through
    case  6: b = b + (static_cast<uint32>(pKey[5])  << 8);   // fall through
    case  5: b = b + pKey[4];                                // fall through
    case  4: a = a + (static_cast<uint32>(pKey[3])  << 24);  // fall through
    case  3: a = a + (static_cast<uint32>(pKey[2])  << 16);  // fall through
    case  2: a = a + (static_cast<uint32>(pKey[1])  << 8);   // fall through
    case  1: a = a + pKey[0];                                // fall through
    // case 0: nothing left to add
    }

    a = a - b;  a = a - c;  a = a ^ (c >> 13);
    b = b - c;  b = b - a;  b = b ^ (a << 8);
    c = c - a;  c = c - b;  c = c ^ (b >> 13);
    a = a - b;  a = a - c;  a = a ^ (c >> 12);
    b = b - c;  b = b - a;  b = b ^ (a << 16);
    c = c - a;  c = c - b;  c = c ^ (b >> 5);
    a = a - b;  a = a - c;  a = a ^ (c >> 3);
    b = b - c;  b = b - a;  b = b ^ (a << 10);
    c = c - a;  c = c - b;  c = c ^ (b >> 15);

    return c;
}

// =====================================================================================================================
// Hashes the specified C-style string key with the Jenkins hash algorithm.
template<typename Key>
PAL_INLINE uint32 StringJenkinsHashFunc<Key>::operator()(
    const void* pVoidKey,
    uint32      keyLen
    ) const
{
    const Key* pKey = static_cast<const Key*>(pVoidKey);
    const Key key = *pKey;
    keyLen = static_cast<uint32>(strlen(key));

    return JenkinsHashFunc<Key>::operator()(key, keyLen);
}

// =====================================================================================================================
// Returns true if the strings in key1 and key2 are the same.
template<typename Key>
PAL_INLINE bool StringEqualFunc<Key>::operator()(
    const Key& key1,
    const Key& key2
    ) const
{
    bool ret = false;

    // Can't do strcmp on null.
    if ((key1 != nullptr) && (key2 != nullptr))
    {
        ret = (strcmp(key1, key2) == 0);
    }
    else if ((key1 == nullptr) && (key2 == nullptr))
    {
        ret = true;
    }

    return ret;
}

// =====================================================================================================================
// Allocates a new block of memory.
template <typename Allocator>
PAL_INLINE void* HashAllocator<Allocator>::Allocate()
{
    void* pMemory = nullptr;

    // Leave pBlock null if this is the first allocation made with this object.
    MemBlock* pBlock = (m_curBlock >= 0) ? &m_blocks[m_curBlock] : nullptr;

    // If current block is used up (or we haven't allocated one yet), go to next.
    if ((pBlock == nullptr) || (pBlock->curGroup >= pBlock->numGroups))
    {
        // Only advance to the next block if the current one had memory allocated to it (which implies that it's
        // full).
        uint32_t nextBlock = m_curBlock;

        if ((pBlock == nullptr) || (pBlock->pMemory != nullptr))
        {
            nextBlock++;
        }

        PAL_ASSERT(nextBlock < NumBlocks);

        pBlock = &m_blocks[nextBlock];

        PAL_ASSERT(pBlock->curGroup == 0);

        // Allocate memory if needed (note that this may rarely fail)
        if (pBlock->pMemory == nullptr)
        {
            pBlock->pMemory = PAL_CALLOC(pBlock->numGroups * m_groupSize, m_pAllocator, AllocInternal);
        }

        // If we successfully allocated memory (or the block already had some), make it current
        if (pBlock->pMemory != nullptr)
        {
            m_curBlock = nextBlock;
        }
    }

    if (pBlock->pMemory != nullptr)
    {
        pMemory = VoidPtrInc(pBlock->pMemory, ((pBlock->curGroup++) * m_groupSize));
    }

    return pMemory;
}

// =====================================================================================================================
// Recycles all allocated memory.  Memory isn't actually freed, but becomes available for reuse.
template <typename Allocator>
PAL_INLINE void HashAllocator<Allocator>::Reset()
{
    for (int32 i = 0; i <= m_curBlock; ++i)
    {
        PAL_ASSERT(m_blocks[i].pMemory != nullptr);
        memset(m_blocks[i].pMemory, 0, m_blocks[i].numGroups * m_groupSize);

        m_blocks[i].curGroup = 0;
    }

    m_curBlock = -1;
}

// =====================================================================================================================
// Proceeds to the next entry, null if to the end.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE void HashIterator<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::Next()
{
    if (m_pCurrentEntry != nullptr)
    {
        PAL_ASSERT(m_pCurrentEntry < &m_pCurrentGroup[Container::EntriesInGroup]);

        auto pFooter = reinterpret_cast<GroupFooter<Entry>*>(&m_pCurrentGroup[Container::EntriesInGroup]);

        Entry* pNextGroup = Container::GetNextGroup(m_pCurrentGroup);

        // We're in the middle of a group.
        if ((m_pCurrentEntry < &m_pCurrentGroup[Container::EntriesInGroup - 1]) &&
            (m_indexInGroup + 1 < pFooter->numEntries))
        {
            m_pCurrentEntry++;
            m_indexInGroup++;
        }
        // We're in the last entry of a group.
        // Considering that the next chained group could be an empty group already, it is better to check the
        // next group's footer->numEntries before jump to the next group. If the numEntry of the next chained
        // group is 0 (invalid), we need to jump to the next bucket directly to avoid returning invalid entry.
        else if ((pNextGroup != nullptr) &&
                 (m_indexInGroup == pFooter->numEntries - 1) &&
                 (reinterpret_cast<GroupFooter<Entry>*>(&pNextGroup[Container::EntriesInGroup])->numEntries > 0))
        {
            m_pCurrentGroup = pNextGroup;
            m_pCurrentEntry = pNextGroup;
            m_indexInGroup  = 0;
        }
        // The current bucket is done, step to the next.
        else
        {
            do
            {
                m_currentBucket = (m_currentBucket + 1) % m_pContainer->m_numBuckets;

                pNextGroup = static_cast<Entry*>(VoidPtrInc(m_pContainer->m_pMemory,
                                                            m_currentBucket * GroupSize));

                pFooter = reinterpret_cast<GroupFooter<Entry>*>(&pNextGroup[Container::EntriesInGroup]);

                if (pFooter->numEntries > 0)
                {
                    m_indexInGroup = 0;
                    break;
                }
            } while(m_currentBucket != m_startBucket);

            if (m_currentBucket != m_startBucket)
            {
                m_pCurrentGroup = pNextGroup;
                m_pCurrentEntry = pNextGroup;
                m_indexInGroup  = 0;
            }
            else
            {
                m_pCurrentEntry = nullptr;
            }
        }
    }
}

// =====================================================================================================================
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE Result HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::Init()
{
    // Since (m_numBuckets - 1) will mask the hashing result, the hash func should make sure the hashing result always
    // contain enough effective bits.
    m_hashFunc.Init(Log2(m_numBuckets));

    // Allocate the hash table.  Zero out the memory to mark all entries invalid, since a key of 0 is invalid.
    m_pMemory = PAL_CALLOC(m_memorySize, &m_allocator, AllocInternal);

    PAL_ALERT(m_pMemory == nullptr);

    return (m_pMemory != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
}

// =====================================================================================================================
// Returns an iterator pointing to the first entry.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
HashIterator<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>
PAL_INLINE HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::Begin() const
{
    uint32 bucket = 0;

    if (m_numEntries != 0)
    {
        PAL_ASSERT(m_pMemory != nullptr);
        for (;bucket < m_numBuckets; ++bucket)
        {
            Entry* pEntry = static_cast<Entry*>(VoidPtrInc(m_pMemory, bucket * GroupSize));
            GroupFooter<Entry>* pFooter = reinterpret_cast<GroupFooter<Entry>*>(&pEntry[EntriesInGroup]);

            if (pFooter->numEntries > 0)
            {
                break;
            }
        }
    }
    else
    {
        // If the backing memory does not exist we should return a null Iterator.
        // This can be done by setting the start bucket such that it is off the end of the bucket list.
        bucket = m_numBuckets;
    }

    return Iterator(this, bucket);
}

// =====================================================================================================================
// Empty the hash table.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE void HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::Reset()
{
    if (m_pMemory != nullptr)
    {
        // Re-zero out the hash table.
        memset(m_pMemory, 0, m_memorySize);
    }

    m_numEntries = 0;

    m_allocator.Reset();
}

// =====================================================================================================================
// Returns pointer to start group of the bucket corresponding to the specified key.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE Entry* HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::FindBucket(
    const Key& key
    ) const
{
    const uint32 bucket = m_hashFunc(&key, sizeof(key)) & (m_numBuckets - 1);

    PAL_ASSERT(m_pMemory != nullptr);
    void*const pBucket = VoidPtrInc(m_pMemory, bucket * GroupSize);
    return static_cast<Entry*>(pBucket);
}

// =====================================================================================================================
// Returns pointer to the next group of the spcified group.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE Entry* HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::GetNextGroup(
    Entry* pGroup)
{
    // Footer of a group stores the pointer to the next group
    GroupFooter<Entry>* pFooter = HashBase::GetGroupFooter(pGroup);
    Entry** ppNextGroup = reinterpret_cast<Entry**>(&(pFooter->pNextGroup));

    return *ppNextGroup;
}

// =====================================================================================================================
// Allocates a new group if the footer of the specified group is null.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t   GroupSize>
PAL_INLINE Entry* HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::AllocateNextGroup(
    Entry* pGroup)
{
    // Footer of a group stores the pointer to the next group.
    GroupFooter<Entry>* pFooter = this->GetGroupFooter(pGroup);
    Entry** ppNextGroup = reinterpret_cast<Entry**>(&(pFooter->pNextGroup));

    if (*ppNextGroup == nullptr)
    {
        // We allocate the next entry group if it does not exist.
        *ppNextGroup = static_cast<Entry*>(m_allocator.Allocate());
    }

    PAL_ASSERT(*ppNextGroup != nullptr);

    return *ppNextGroup;
}

// =====================================================================================================================
// Return a pointer to the group footer.
template<
    typename Key,
    typename Entry,
    typename Allocator,
    typename HashFunc,
    typename EqualFunc,
    typename AllocFunc,
    size_t GroupSize>
PAL_INLINE GroupFooter<Entry>* HashBase<Key, Entry, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>::GetGroupFooter(
    Entry* pGroup)
{
    return reinterpret_cast<GroupFooter<Entry>*>(&pGroup[EntriesInGroup]);
}

} // Util
