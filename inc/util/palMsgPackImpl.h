/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMsgPackImpl.h
 * @brief PAL MessagePack reader and writer utility class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palMsgPack.h"

namespace Util
{

// =====================================================================================================================
inline Result TranslateCwpReturnCode(
    int32 returnCode)
{
    Result result = Result::ErrorUnknown;

    switch (returnCode)
    {
    case CWP_RC_OK:
        result = Result::Success;
        break;

    case CWP_RC_END_OF_INPUT:
    case CWP_RC_STOPPED:
        result = Result::Eof;
        break;

    case CWP_RC_BUFFER_OVERFLOW:
    case CWP_RC_MALLOC_ERROR:
    case CWP_RC_ERROR_IN_HANDLER:
        result = Result::ErrorOutOfMemory;
        break;

    case CWP_RC_MALFORMED_INPUT:
    case CWP_RC_WRONG_BYTE_ORDER:
    case CWP_RC_ILLEGAL_CALL:
    case CWP_RC_BUFFER_UNDERFLOW:
        result = Result::ErrorInvalidValue;
        break;

    default:
        break;
    }

    return result;
}

// =====================================================================================================================
template <typename Allocator>
MsgPackWriter::MsgPackWriter(
    Allocator*const  pAllocator)
    :
    m_pfnFree(&FreeBuffer<Allocator>),
    m_numItems(0),
    m_containerNumItemsRemaining(0)
{
    cw_pack_context_init(&m_context, nullptr, 0, &GrowBuffer<Allocator>, pAllocator);
}

// =====================================================================================================================
inline MsgPackWriter::MsgPackWriter(
    void*   pBuffer,
    uint32  sizeInBytes)
    :
    m_pfnFree(nullptr),
    m_numItems(0),
    m_containerNumItemsRemaining(0)
{
    cw_pack_context_init(&m_context, pBuffer, ((pBuffer != nullptr) ? sizeInBytes : 0), nullptr, nullptr);
}

// =====================================================================================================================
inline Result MsgPackWriter::Reserve(
    uint32 newSizeInBytes)
{
    const uint32 curSize = static_cast<uint32>(Util::VoidPtrDiff(m_context.end, m_context.start));

    if ((m_context.return_code == CWP_RC_OK) && (newSizeInBytes > curSize))
    {
        m_context.return_code = m_context.handle_pack_overflow(&m_context, newSizeInBytes - curSize);
    }

    return GetStatus();
}

// =====================================================================================================================
inline Result MsgPackWriter::Append(
    const MsgPackWriter& src)
{
    if ((m_context.return_code == CWP_RC_OK) &&
        ((src.m_context.return_code != CWP_RC_OK) || (src.m_containerNumItemsRemaining != 0)))
    {
        m_context.return_code = CWP_RC_MALFORMED_INPUT;
    }

    cw_pack_insert(&m_context, src.m_context.start, src.GetSize());
    return CountAndStatus(src.NumItems());
}

// =====================================================================================================================
inline void MsgPackWriter::Reset()
{
    m_context.current = m_context.start;
    m_numItems = 0;
    m_containerNumItemsRemaining = 0;
}

// =====================================================================================================================
inline void MsgPackWriter::CountItems(
    uint32 num)
{
    if (m_containerNumItemsRemaining >= num)
    {
        m_containerNumItemsRemaining -= num;
    }
    else
    {
        m_numItems += (num - m_containerNumItemsRemaining);
        m_containerNumItemsRemaining = 0;
    }
}

// =====================================================================================================================
template <typename Allocator>
int32 CWP_CALL MsgPackWriter::GrowBuffer(
    cw_pack_context*  pContext,
    unsigned long     requestedNumBytesToAdd)
{
    auto*const pAllocator = static_cast<Allocator*>(pContext->client_data);

    const size_t currentSize = Util::VoidPtrDiff(pContext->end, pContext->start);
    const size_t newSize     = Util::Pow2Align(currentSize + requestedNumBytesToAdd, BufferAllocSize);

    void* pNewMemory = PAL_MALLOC(newSize, pAllocator, AllocInternalTemp);

    if (pNewMemory != nullptr)
    {
        void*const pOldMemory      = pContext->start;
        const size_t currentOffset = Util::VoidPtrDiff(pContext->current, pContext->start);

        if (currentOffset > 0)
        {
            memcpy(pNewMemory, pOldMemory, currentOffset);
        }

        pContext->start   = static_cast<uint8*>(pNewMemory);
        pContext->current = static_cast<uint8*>(Util::VoidPtrInc(pNewMemory, currentOffset));
        pContext->end     = static_cast<uint8*>(Util::VoidPtrInc(pNewMemory, newSize));

        PAL_FREE(pOldMemory, pAllocator);
    }

    return (pNewMemory != nullptr) ? CWP_RC_OK : CWP_RC_MALLOC_ERROR;
}

// =====================================================================================================================
template <typename T>
Result MsgPackWriter::PackArray(
    const T*  pArray,
    uint32    numElements)
{
    DeclareArray(numElements);

    for (uint32 i = 0; i < numElements; ++i)
    {
        Pack(pArray[i]);
    }

    return GetStatus();
}

// =====================================================================================================================
template <typename T, uint32 DefaultCapacity, typename Allocator>
void MsgPackWriter::Pack(
    const Vector<T, DefaultCapacity, Allocator>& vector)
{
    DeclareArray(vector.NumElements());

    for (auto iter = vector.Begin(); iter.IsValid(); iter.Next())
    {
        Pack(iter.Get());
    }
}

// =====================================================================================================================
template <typename Key,
          typename Value,
          typename Allocator,
          template<typename> class HashFunc,
          template<typename> class EqualFunc,
          typename AllocFunc,
          size_t GroupSize>
Result MsgPackWriter::Pack(
    const HashMap<Key, Value, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>& map)
{
    DeclareMap(map.GetNumEntries());

    for (auto iter = map.Begin(); iter.Get() != nullptr; iter.Next())
    {
        const auto*const pEntry = iter.Get();
        PackPair(pEntry->key, pEntry->value);
    }

    return GetStatus();
}

// =====================================================================================================================
inline Result MsgPackWriter::DeclareArray(
    uint32 numElements)
{
    cw_pack_array_size(&m_context, numElements);
    CountItems(1);
    m_containerNumItemsRemaining += numElements;
    return GetStatus();
}

// =====================================================================================================================
inline Result MsgPackWriter::DeclareMap(
    uint32 numElements)
{
    cw_pack_map_size(&m_context, numElements);
    CountItems(1);
    m_containerNumItemsRemaining += (numElements * 2);
    return GetStatus();
}

// =====================================================================================================================
inline Result MsgPackWriter::AppendArray(
    const MsgPackWriter& src)
{
    Result result = DeclareArray(src.NumItems());
    if (result == Result::Success)
    {
        result = Append(src);
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackWriter::AppendMap(
    const MsgPackWriter& src)
{
    Result result = DeclareMap(src.NumItems() / 2);
    if (result == Result::Success)
    {
        result = Append(src);
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackReader::Seek(
    uint32 offset)
{
    const uint32 clampedOffset = Min(offset, static_cast<uint32>(VoidPtrDiff(m_context.end, m_context.start)));
    m_context.current = static_cast<const uint8*>(VoidPtrInc(m_context.start, clampedOffset));

    if (m_context.return_code == CWP_RC_END_OF_INPUT)
    {
        // If we previously reached EOF, reset the state so CWPack doesn't just fail out of every call. If we're seeking
        // to EOF, the call to Next() after this will return Result::Eof.
        m_context.return_code = CWP_RC_OK;
    }

    return Next();
}

// =====================================================================================================================
template <typename T>
Result MsgPackReader::UnpackScalar(
    T*  pValue)
{
    PAL_ASSERT(pValue != nullptr);
    Result result = Result::Success;

    if (m_context.item.type == CWP_ITEM_ARRAY)
    {
        result = (m_context.item.as.array.size == 1) ? Next() : Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        switch (m_context.item.type)
        {
        case CWP_ITEM_NIL:
            *pValue = static_cast<T>(0);
            break;

        case CWP_ITEM_BOOLEAN:
            *pValue = static_cast<T>(m_context.item.as.boolean);
            break;

        case CWP_ITEM_POSITIVE_INTEGER:
            *pValue = static_cast<T>(m_context.item.as.u64);
            PAL_ASSERT(static_cast<uint64>(*pValue) == m_context.item.as.u64);
            break;

        case CWP_ITEM_NEGATIVE_INTEGER:
            *pValue = static_cast<T>(m_context.item.as.i64);
            PAL_ASSERT(static_cast<int64>(*pValue) == m_context.item.as.i64);
            break;

        case CWP_ITEM_FLOAT:
            *pValue = static_cast<T>(m_context.item.as.real);
            PAL_ASSERT(static_cast<float>(*pValue) == m_context.item.as.real);
            break;

        case CWP_ITEM_DOUBLE:
            *pValue = static_cast<T>(m_context.item.as.long_real);
            PAL_ASSERT(static_cast<double>(*pValue) == m_context.item.as.long_real);
            break;

        case CWP_ITEM_BIN:
            if (m_context.item.as.bin.length == sizeof(T))
            {
                memcpy(pValue, m_context.item.as.bin.start, sizeof(T));
            }
            else
            {
                result = Result::ErrorInvalidValue;
            }
            break;

        default:
            result = Result::ErrorInvalidValue;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackReader::Unpack(
    char*   pString,
    uint32  sizeInBytes)
{
    PAL_ASSERT((pString != nullptr) && (sizeInBytes > 0));

    Result result = Result::Success;
    if (m_context.item.type == CWP_ITEM_STR)
    {
        if (m_context.item.as.str.length < sizeInBytes)
        {
            const uint32 strLen = m_context.item.as.str.length;

            // MsgPack-encoded strings are not null-terminated, so we must add it ourselves.
            memcpy(pString, m_context.item.as.str.start, strLen);
            pString[strLen] = '\0';
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }
    else if (m_context.item.type == CWP_ITEM_NIL)
    {
        pString[0] = '\0';
    }
    else
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackReader::Unpack(
    Util::StringView<char>* pStringView)
{
    PAL_ASSERT(pStringView != nullptr);
    Result result = Result::Success;

    if (m_context.item.type == CWP_ITEM_STR)
    {
        *pStringView = StringView<char>{static_cast<const char*>(m_context.item.as.str.start),
                                        m_context.item.as.str.length};
    }
    else if (m_context.item.type == CWP_ITEM_NIL)
    {
        *pStringView = { };
    }
    else
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
template <typename T>
Result MsgPackReader::Unpack(
    T*      pArray,
    uint32  numElements)
{
    PAL_ASSERT(pArray != nullptr);
    Result result = Result::Success;

    if (m_context.item.type == CWP_ITEM_ARRAY)
    {
        const uint32 arrayLen = m_context.item.as.array.size;
        result = (arrayLen <= numElements) ? Result::Success : Result::ErrorInvalidValue;

        for (uint32 i = 0; ((result == Result::Success) && (i < arrayLen)); ++i)
        {
            result = UnpackNext(&pArray[i]);
        }
    }
    else if (m_context.item.type == CWP_ITEM_BIN)
    {
        if (m_context.item.as.bin.length <= static_cast<uint32>(sizeof(T) * numElements))
        {
            memcpy(pArray, m_context.item.as.bin.start, m_context.item.as.bin.length);
        }
        else
        {
            result = Result::ErrorInvalidValue;
        }
    }
    else
    {
        // If the item is not an array, we can try it as a scalar.
        result = Unpack(&pArray[0]);
    }

    return result;
}

// =====================================================================================================================
template <typename T, uint32 DefaultCapacity, typename Allocator>
Result MsgPackReader::Unpack(
    Vector<T, DefaultCapacity, Allocator>*  pVector)
{
    PAL_ASSERT(pVector != nullptr);
    Result result = Result::ErrorInvalidValue;

    if (m_context.item.type == CWP_ITEM_ARRAY)
    {
        result = pVector->Reserve(pVector->NumElements() + m_context.item.as.array.size);

        for (uint32 i = m_context.item.as.array.size; ((result == Result::Success) && (i > 0)); --i)
        {
            T element;
            result = UnpackNext(&element);

            if (result == Result::Success)
            {
                result = pVector->PushBack(element);
            }
        }
    }
    else
    {
        // If the item is not an array, we can try it as a scalar.
        T element;
        result = Unpack(&element);

        if (result == Result::Success)
        {
            result = pVector->PushBack(element);
        }
    }

    return result;
}

// =====================================================================================================================
template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
Result MsgPackReader::Unpack(
    SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>*  pSparseVector)
{
    PAL_ASSERT(pSparseVector != nullptr);
    Result result = (m_context.item.type == CWP_ITEM_MAP) ?
        pSparseVector->Reserve(pSparseVector->NumElements() + m_context.item.as.map.size) : Result::ErrorInvalidValue;

    for (uint32 i = m_context.item.as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        uint32 key;
        T      value;
        result = UnpackNextPair(&key, &value);

        if (result == Result::Success)
        {
            result = pSparseVector->Insert(key, value);
        }
    }

    return result;
}

// =====================================================================================================================
template <typename Key,
          typename Value,
          typename Allocator,
          template<typename> class HashFunc,
          template<typename> class EqualFunc,
          typename AllocFunc,
          size_t GroupSize>
Result MsgPackReader::Unpack(
    HashMap<Key, Value, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>*  pMap)
{
    PAL_ASSERT(pMap != nullptr);
    Result result = (m_context.item.type == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = m_context.item.as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        Key    key;
        Value* pValue;
        bool   existed;

        result = UnpackNext(&key);

        if (result == Result::Success)
        {
            result = pMap->FindAllocate(key, &existed, &pValue);
        }

        if (result == Result::Success)
        {
            result = UnpackNext(pValue);
        }
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackReader::Unpack(
    const void**  ppData,
    uint32*       pSizeInBytes)
{
    PAL_ASSERT((ppData != nullptr) && (pSizeInBytes != nullptr));
    const Result result = (m_context.item.type == CWP_ITEM_BIN) ? Result::Success : Result::ErrorInvalidValue;

    if (result == Result::Success)
    {
        *ppData       = m_context.item.as.bin.start;
        *pSizeInBytes = m_context.item.as.bin.length;
    }

    return result;
}

// =====================================================================================================================
inline Result MsgPackReader::Unpack(
    void*   pDst,
    uint32  sizeInBytes)
{
    PAL_ASSERT(pDst != nullptr);
    const Result result = ((m_context.item.type == CWP_ITEM_BIN) && (m_context.item.as.bin.length <= sizeInBytes)) ?
                          Result::Success : Result::ErrorInvalidValue;

    if (result == Result::Success)
    {
        memcpy(pDst, m_context.item.as.bin.start, m_context.item.as.bin.length);
    }

    return result;
}

// =====================================================================================================================
template <typename T>
Result MsgPackReader::UnpackNext(
    T*  pDst)
{
    Result result = Next();

    if (result == Result::Success)
    {
        result = Unpack(pDst);
    }

    return result;
}

// =====================================================================================================================
template <typename T1, typename T2>
Result MsgPackReader::UnpackNextPair(
    T1*  pFirst,
    T2*  pSecond)
{
    Result result = UnpackNext(pFirst);

    if (result == Result::Success)
    {
        result = UnpackNext(pSecond);
    }

    return result;
}

} // Util
