/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palMsgPack.h
 * @brief PAL MessagePack reader and writer utility class declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palSysMemory.h"
#include "palVector.h"
#include "palSparseVector.h"
#include "palHashMap.h"

#include <type_traits>

#include "cwpack.h"

namespace Util
{

Result TranslateCwpReturnCode(int32 returnCode);

/**
 ***********************************************************************************************************************
 * @brief Utility class that emits a MsgPack blob.
 *
 * See http://www.msgpack.org/ for a complete description of the MsgPack standard.
 *
 * NOTE: If an error is encountered, the result code gets saved, and all subsequent pack method calls become a no-op and
 *       just return the saved result code. Therefore, checking the result between pack method calls is not necessary.
 ***********************************************************************************************************************
 */
class MsgPackWriter
{
public:
    /// Constructor.
    ///
    /// @param [in] pAllocator  The MsgPackWriter will use this Allocator to create and resize the output buffer.
    template <typename Allocator>
    explicit MsgPackWriter(Allocator*const pAllocator);

    /// Alternate constructor where the MsgPackWriter writes to a fixed-size, user-managed buffer.
    ///
    /// @warning The buffer will not be grown in this mode.
    ///
    /// @param [in/out] pBuffer      The buffer to write the MsgPack blob into.
    /// @param [in]     sizeInBytes  Size in bytes of the buffer.
    MsgPackWriter(void* pBuffer, uint32 sizeInBytes);

    /// Destructor.
    ~MsgPackWriter()
    {
        if ((m_pfnFree != nullptr) && (m_context.client_data != nullptr))
        {
            (*m_pfnFree)(&m_context);
        }
    }

    /// Returns a read-only pointer to the MsgPack buffer.
    const void* GetBuffer() const
    {
        PAL_ASSERT((m_context.return_code == CWP_RC_OK) && (m_containerNumItemsRemaining == 0));
        return m_context.start;
    }

    /// Returns the used size (not overall capacity) in bytes of the MsgPack buffer.
    uint32 GetSize() const { return static_cast<uint32>(Util::VoidPtrDiff(m_context.current, m_context.start)); }

    /// Reserves the specified number of bytes in the buffer.
    ///
    /// @param [in] newSizeInBytes  Size of the space to reserve.
    ///
    /// @returns Success if successful, ErrorOutOfMemory is memory allocation fails.
    Result Reserve(uint32 newSizeInBytes);

    /// Appends the contents of a MsgPack token stream created by another MsgPackWriter to this one.
    ///
    /// @param [in] src  Reference to the other MsgPackWriter to copy from.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result Append(const MsgPackWriter& src);

    /// Resets the state of the writer, allowing it to be reused to write another MsgPack blob.
    void Reset();

    /// Packs a nil element.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result PackNil()
    {
        cw_pack_nil(&m_context);
        return CountAndStatus(1);
    }

    ///@{
    /// Packs a scalar element.
    ///
    /// @param [in] value  The value to write.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result Pack(bool value)
    {
        cw_pack_boolean(&m_context, value);
        return CountAndStatus(1);
    }

    Result Pack(char   value) { return PackSigned(value);   }
    Result Pack(uint8  value) { return PackUnsigned(value); }
    Result Pack(uint16 value) { return PackUnsigned(value); }
    Result Pack(uint32 value) { return PackUnsigned(value); }
    Result Pack(uint64 value) { return PackUnsigned(value); }
    Result Pack(int8   value) { return PackSigned(value);   }
    Result Pack(int16  value) { return PackSigned(value);   }
    Result Pack(int32  value) { return PackSigned(value);   }
    Result Pack(int64  value) { return PackSigned(value);   }

    Result Pack(float value)
    {
        cw_pack_float(&m_context, value);
        return CountAndStatus(1);
    }

    Result Pack(double value)
    {
        cw_pack_double(&m_context, value);
        return CountAndStatus(1);
    }
    ///@}

    /// Packs a string element.
    ///
    /// @param [in] pString  The null-terminated string to write.
    /// @param [in] length   Length of the string, excluding null-terminator.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result PackString(const char* pString, uint32 length)
    {
        cw_pack_str(&m_context, pString, length);
        return CountAndStatus(1);
    }

    /// Packs a string element when the length is not already known.
    ///
    /// @param [in] pString  The null-terminated string to write.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result PackString(const char* pString) { return PackString(pString, uint32(strlen(pString))); }

    /// Packs a string element from a string literal as input.
    ///
    /// @param [in] str  The null-terminated string to write.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <size_t N>
    Result Pack(const char (&string)[N]) { return PackString(&string[0], N - 1); }

    /// Packs an array of scalar elements.
    ///
    /// @param [in] pArray       The beginning of the array to write.
    /// @param [in] numElements  How many elements wide the array is.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T>
    Result PackArray(const T* pArray, uint32 numElements);

    /// Packs an array of scalar elements (bools, ints, uints, or floats) from a C-style array as input.
    ///
    /// @param [in] array  Reference to the C-style source array.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T, size_t N>
    Result Pack(const T (&array)[N]) { return PackArray(&array[0], static_cast<uint32>(N)); }

    /// Packs a binary blob element.
    ///
    /// @param [in] pBuffer      Pointer to a buffer with data to be written.
    /// @param [in] sizeInBytes  Size of the buffer in bytes.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result Pack(const void* pBuffer, uint32 sizeInBytes)
    {
        cw_pack_bin(&m_context, pBuffer, sizeInBytes);
        return CountAndStatus(1);
    }

    /// Packs an object as a raw binary encoding.
    ///
    /// @param [in] src  Object to pack as binary.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T>
    Result PackBinary(const T& src) { return Pack(static_cast<const void*>(&src), sizeof(src)); }

    /// Packs a user-extended typed blob element.
    ///
    /// @param [in] type         User-extended type ID as an 8-bit signed integer.
    /// @param [in] pBuffer      Pointer to a buffer with data to be written.
    /// @param [in] sizeInBytes  Size of the buffer in bytes.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result Pack(int8 type, const void* pBuffer, uint32 sizeInBytes)
    {
        cw_pack_ext(&m_context, type, pBuffer, sizeInBytes);
        return CountAndStatus(1);
    }

    /// Packs an object as a user-extended typed element.
    ///
    /// @param [in] type  User-extended type ID as an 8-bit signed integer.
    /// @param [in] src   Object to pack.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T>
    Result Pack(int8 type, const T& src) { return Pack(type, static_cast<const void*>(&src), sizeof(src)); }

    /// Packs an array element from a Vector.
    ///
    /// @param [in] vector  Reference to the Vector whose contents are to be packed.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T, uint32 DefaultCapacity, typename Allocator>
    Result Pack(const Vector<T, DefaultCapacity, Allocator>& vector);

    /// Packs a map element from a HashMap.
    ///
    /// @param [in] map  Reference to the HashMap whose contents are to be packed.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename Key,
              typename Value,
              typename Allocator,
              template<typename> class HashFunc,
              template<typename> class EqualFunc,
              typename AllocFunc,
              size_t GroupSize>
    Result Pack(const HashMap<Key, Value, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>& map);

    /// Creates a map from the contents of an existing MsgPack token stream which was created by another
    /// MsgPackWriter object.
    ///
    /// @param [in] src  Reference to the other MsgPackWriter to copy from.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result AppendMap(const MsgPackWriter& src);

    /// Convenience function that combines two Pack() calls. Useful for manually packing a map.
    ///
    /// @param [in] first   First element to pack (key).
    /// @param [in] second  Second element to pack (value).
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    template <typename T1, typename T2>
    Result PackPair(const T1& first, const T2& second)
    {
        Pack(first);
        return Pack(second);
    }

    /// Declares the beginning of a fixed-size array, with the exact number of elements specified.
    ///
    /// @param [in] numElements  Exact number of elements this array contains.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result DeclareArray(uint32 numElements);

    /// Declares the beginning of a fixed-size map, with the exact number of elements specified.
    ///
    /// @param [in] numElements  Exact number of (key, value) pairs this map contains.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result DeclareMap(uint32 numElements);

    /// Returns the number of items written so far at the "root" level - entire containers count as a single item.
    /// This value is appropriate for DeclareArray(), and 0.5x this value is appropriate for DeclareMap().
    uint32 NumItems() const { return m_numItems; }

    /// Gets the status of the writer.
    ///
    /// @returns Success if no errors have been encountered, ErrorOutOfMemory if a memory allocation failed,
    /// ErrorInvalidValue if malformed input was provided, ErrorUnknown otherwise.
    Result GetStatus() const { return TranslateCwpReturnCode(m_context.return_code); }

private:
    Result PackSigned(int64 value)
    {
        cw_pack_signed(&m_context, value);
        return CountAndStatus(1);
    }

    Result PackUnsigned(uint64 value)
    {
        cw_pack_unsigned(&m_context, value);
        return CountAndStatus(1);
    }

    void CountItems(uint32 num);

    Result CountAndStatus(uint32 num)
    {
        CountItems(num);
        return GetStatus();
    }

    template <typename Allocator>
    static void FreeBuffer(cw_pack_context* pContext)
        { PAL_FREE(pContext->start, static_cast<Allocator*>(pContext->client_data)); }

    using FreeBufferFunc = void(cw_pack_context*);

    /// Overflow handler callback provided to CWPack.
    template <typename Allocator>
    static int32 CWP_CALL GrowBuffer(cw_pack_context* pContext, unsigned long requestedNumBytesToAdd);

    /// Writer buffer is allocated with, and grown in multiples of, this size.
    static constexpr uint32 BufferAllocSize = 1024;

    cw_pack_context  m_context;
    FreeBufferFunc*  m_pfnFree;

    uint32 m_numItems;
    uint32 m_containerNumItemsRemaining;

    PAL_DISALLOW_DEFAULT_CTOR(MsgPackWriter);
    PAL_DISALLOW_COPY_AND_ASSIGN(MsgPackWriter);
};

/**
 ***********************************************************************************************************************
 * @brief Iterator-like utility class that parses in a MsgPack blob and translates it to C++ types.
 *
 * See http://www.msgpack.org/ for a complete description of the MsgPack standard.
 *
 * NOTE: Non-MsgPack errors resulting from unpack calls do not get saved to the internal state, although other errors
 *       do. That means unlike with the MsgPackWriter class, you will need to check the result between method calls.
 ***********************************************************************************************************************
 */
class MsgPackReader
{
public:
    /// Constructor.
    MsgPackReader() : m_context{} {}

    /// Initializes the reader's state with the provided buffer as the input MsgPack to read from.
    ///
    /// @returns Success if successful, ErrorInvalidValue if input is not valid MsgPack.
    Result InitFromBuffer(const void* pBuffer, uint32 sizeInBytes)
    {
        cw_unpack_context_init(&m_context, pBuffer, sizeInBytes, nullptr, nullptr);
        return Next();
    }

    /// Gets the current item token.
    ///
    /// @returns A const reference to the current item token.
    const cwpack_item& Get() const
    {
        PAL_ASSERT(GetStatus() == Result::Success);
        return m_context.item;
    }

    /// Gets the current item token's type.
    cwpack_item_types Type() const { return Get().type; }

    /// Advances the reader to the next item token.
    ///
    /// @returns Success if successful, Eof if the end of the buffer has been reached, ErrorInvalidValue if input
    /// is not valid MsgPack.
    Result Next()
    {
        cw_unpack_next(&m_context);
        return GetStatus();
    }

    /// Advances the reader to the next item token, and sanity checks that it is an item with the given type.
    ///
    /// @param [in] expectedType  The type the next item is expected to be.
    ///
    /// @returns Success if successful, ErrorInvalidValue if the next item's type does not match @ref expectedType,
    /// the end of the buffer has been reached, or the input is not valid MsgPack.
    Result Next(cwpack_item_types expectedType)
    {
        return ((Next() == Result::Success) && (m_context.item.type == expectedType)) ? Result::Success
                                                                                      : Result::ErrorInvalidValue;
    }

    /// Skips ahead by the specified number of elements. Skipping a container also skips all of its elements.
    ///
    /// @param [in] numElements  Number of elements to be skipped.
    ///
    /// @returns Success if successful, Eof if the end of the buffer has been reached, ErrorInvalidValue if input
    /// is not valid MsgPack.
    Result Skip(int32 numElements)
    {
        cw_skip_items(&m_context, numElements);
        return GetStatus();
    }

    /// Returns the position (in bytes) of the next item the reader would unpack.
    uint32 Tell() const { return static_cast<uint32>(VoidPtrDiff(m_context.current, m_context.start)); }

    /// Seeks the reader's position to the specified offset (in bytes) and attempts to unpack the next item there.
    ///
    /// @param [in] offset  Offset in bytes to seek to. Valid values may be obtained from @ref Tell().
    ///                     A value larger than the size of the buffer seeks to the end of the buffer.
    ///
    /// @returns Success if successful, Eof is the end of the buffer has been reached, ErrorInvalidValue if input
    /// is not valid MsgPack or @ref offset is an invalid reading frame.
    Result Seek(uint32 offset);

    ///@{
    /// Unpacks the current item as a scalar, type casting if necessary.
    ///
    /// @param [out] pValue  Pointer to where to store the data.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pValue cannot represent the current item.
    /// Note that loss of information due to type casting will not return an error, but will assert in debug builds.
    Result Unpack(bool*   pValue) { return UnpackScalar(pValue); }
    Result Unpack(char*   pValue) { return UnpackScalar(pValue); }
    Result Unpack(uint8*  pValue) { return UnpackScalar(pValue); }
    Result Unpack(uint16* pValue) { return UnpackScalar(pValue); }
    Result Unpack(uint32* pValue) { return UnpackScalar(pValue); }
    Result Unpack(uint64* pValue) { return UnpackScalar(pValue); }
    Result Unpack(int8*   pValue) { return UnpackScalar(pValue); }
    Result Unpack(int16*  pValue) { return UnpackScalar(pValue); }
    Result Unpack(int32*  pValue) { return UnpackScalar(pValue); }
    Result Unpack(int64*  pValue) { return UnpackScalar(pValue); }
    Result Unpack(float*  pValue) { return UnpackScalar(pValue); }
    Result Unpack(double* pValue) { return UnpackScalar(pValue); }
    ///@}

    /// Unpacks the current item as a string.
    ///
    /// @param [out] pString      Pointer to the beginning of where to store the string data in.
    /// @param [in]  sizeInBytes  How large the storage buffer is.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pString cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    Result Unpack(char* pString, uint32 sizeInBytes);

    /// Unpacks the current item as an array of scalars or as binary data, type casting if necessary.
    /// NOTE: This will advance the iterator to the last element.
    ///
    /// @param [out] pArray       Pointer to the beginning of where to store the data in.
    /// @param [in]  numElements  How many elements wide the storage is.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pArray cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T>
    Result Unpack(T* pArray, uint32 numElements);

    /// Unpacks the current item as a string if @ref T is char, otherwise as an array of scalars or binary data,
    /// type casting if necessary.
    /// NOTE: This will advance the iterator to the last element.
    ///
    /// @param [out] pArray  Pointer to the C-style array to store the data in.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pArray cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T, size_t N>
    Result Unpack(T (*pArray)[N]) { return Unpack(&((*pArray)[0]), static_cast<uint32>(N)); }

    /// Unpacks the current array item as a Vector of scalars, type casting if necessary.
    /// NOTE: This will advance the iterator to the last element.
    ///
    /// @param [out] pVector  Pointer to the Vector to store the data in.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pVector cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T, uint32 DefaultCapacity, typename Allocator>
    Result Unpack(Vector<T, DefaultCapacity, Allocator>* pVector);

    /// Unpacks the current map item as a SparseVector of scalars, type casting if necessary.
    /// NOTE: This will advance the iterator to the last element.
    ///
    /// @param [out] pSparseVector  Pointer to the SparseVector to store the data in.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pSparseVector cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T, typename CapacityType, CapacityType DefaultCapacity, typename Allocator, uint32... KeyRanges>
    Result Unpack(SparseVector<T, CapacityType, DefaultCapacity, Allocator, KeyRanges...>* pSparseVector);

    /// Unpacks the current map item as a HashMap of scalars, type casting if necessary.
    /// NOTE: This will advance the iterator to the last element.
    ///
    /// @param [out] pMap  Pointer to the HashMap to store the data in.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pMap cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename Key,
              typename Value,
              typename Allocator,
              template<typename> class HashFunc,
              template<typename> class EqualFunc,
              typename AllocFunc,
              size_t GroupSize>
    Result Unpack(HashMap<Key, Value, Allocator, HashFunc, EqualFunc, AllocFunc, GroupSize>* pMap);

    /// Unpacks the current item as a binary blob.
    ///
    /// @param [out] ppData        Pointer to where to store the pointer to the binary data.
    /// @param [out] pSizeInBytes  Pointer to where to store the size in bytes of the data.
    ///
    /// @returns Result if successful, ErrorInvalidValue if the current item is not a binary blob,
    /// Eof if unexpected end-of-file was reached.
    Result Unpack(const void** ppData, uint32* pSizeInBytes);

    /// Unpacks the current item as a binary blob, copying the data to the given destination.
    ///
    /// @param [out] pDst         Pointer to the memory to copy the binary data to.
    /// @param [in]  sizeInBytes  Size in bytes of the buffer represented by @ref pDst.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pDst cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    Result Unpack(void* pDst, uint32 sizeInBytes);

    /// Unpacks the current item as a binary encoding of T.
    ///
    /// @param [out] pDst  Pointer to the variable or object to copy the binary data to.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pDst cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T>
    Result UnpackBinary(T* pDst) { return Unpack(static_cast<void*>(pDst), static_cast<uint32>(sizeof(T))); }

    /// Convenience function that combines Next() and Unpack(), which advances the reader to the next item token, and
    /// unpacks it into the given destination.
    ///
    /// @param [out] pDst  Pointer to where to store the data.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pDst cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T>
    Result UnpackNext(T* pDst);

    /// Convenience function that combines Next() and Unpack() twice, which unpacks the next pair of items to their
    /// respective given destinations. Useful for manually unpacking a map.
    ///
    /// @param [out] pFirst   Pointer to where to store the first element.
    /// @param [out] pSecond  Pointer to where to store the second element.
    ///
    /// @returns Result if successful, ErrorInvalidValue if @ref pDst cannot represent the current item,
    /// Eof if unexpected end-of-file was reached.
    template <typename T1, typename T2>
    Result UnpackNextPair(T1* pFirst, T2* pSecond);

    /// Get the status of the reader.
    ///
    /// @returns Success if no errors have been encountered, Eof if end of buffer has been reached,
    /// ErrorInvalidValue if input was malformed, ErrorUnknown otherwise.
    Result GetStatus() const { return TranslateCwpReturnCode(m_context.return_code); }

private:
    template <typename T>
    Result UnpackScalar(T* pValue);

    cw_unpack_context  m_context;
};

} // Util
