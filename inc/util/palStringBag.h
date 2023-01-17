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
/**
***********************************************************************************************************************
* @file  palStringBag.h
* @brief PAL utility collection StringBag and StringBagIterator class declarations.
***********************************************************************************************************************
*/

#pragma once

#include "palUtil.h"
#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palStringView.h"
#include "palSysMemory.h"
#include <type_traits>

namespace Util
{

// Forward declarations.
template<typename T, typename Allocator> class StringBag;
template<typename T, typename Allocator> class StringBagIterator;

/**
***********************************************************************************************************************
* @brief  Handle to a string in a @ref StringBag.
***********************************************************************************************************************
*/
template<typename T>
class StringBagHandle
{
    static constexpr uint32 InvalidInternalValue = uint32(-1);

public:
    constexpr StringBagHandle()
        :
        m_value(InvalidInternalValue)
    {}

#if PAL_ENABLE_PRINTS_ASSERTS
    template<typename Allocator>
    constexpr StringBagHandle(const StringBag<T, Allocator>& stringBag, uint32 v)
        :
        m_value(v),
        m_ppStringBagData{&(stringBag.m_pData)}
    {}
#else
    constexpr explicit StringBagHandle(uint32 v)
        :
        m_value(v)
    {}
#endif

    constexpr bool IsValid() const { return m_value != InvalidInternalValue; }

    constexpr uint32 InternalValue() const { return m_value; }

    constexpr bool operator==(StringBagHandle rhs) const { return m_value == rhs.m_value; }
    constexpr bool operator!=(StringBagHandle rhs) const { return (m_value == rhs.m_value) == false; }

private:
    uint32             m_value;
#if PAL_ENABLE_PRINTS_ASSERTS
    const T* const*    m_ppStringBagData{};
#endif
};

/**
***********************************************************************************************************************
* @brief  Iterator for traversal of strings in StringBag.
*
* Supports forward traversal.
***********************************************************************************************************************
*/
template<typename T, typename Allocator>
class StringBagIterator
{
public:
    /// Checks if the current index is within bounds of the strings in the bag.
    ///
    /// @returns True if the current string this iterator is pointing to is within the permitted range.
    bool IsValid() const { return m_pSrcBag->IsValid(m_currIndex); }

    /// Returns the string the iterator is currently pointing to as const pointer.
    ///
    /// @warning This may cause an access violation if the iterator is not valid.
    ///
    /// @returns The string the iterator is currently pointing to as const pointer.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 717
    const T* Get() const
    {
        PAL_ASSERT(IsValid());
        return (m_pSrcBag->m_pData + m_currIndex);
    }
#else
    StringView<T> Get() const
    {
        return StringView<T>{m_pSrcBag->GetDataAt(m_currIndex), m_pSrcBag->GetLengthAt(m_currIndex)};
    }
#endif

    /// Returns a handle for the string the iterator is currently pointing to.
    ///
    /// @warning This may cause an access violation if the iterator is not valid.
    ///
    /// @returns A handle for the string the iterator is currently pointing to.
    StringBagHandle<T> GetHandle() const { return m_pSrcBag->GetHandleAt(m_currIndex); }

    /// Advances the iterator to the next string.  Iterating to the next string in the container is an O(1) operation.
    ///
    /// @warning Does not do bounds checking.
    void Next()
    {
        m_currIndex += uint32(StringBag<T, Allocator>::RequiredSpace(m_pSrcBag->GetLengthAt(m_currIndex)));
    }

    /// Retrieves the current position of this iterator.
    ///
    /// @returns The location in the bag the iterator is currently pointing to.
    uint32 Position() const { return m_currIndex; }

private:
    StringBagIterator() = delete;

    StringBagIterator(
        uint32                         index,
        const StringBag<T, Allocator>* pSrcBag)
        :
        m_currIndex(index),
        m_pSrcBag(pSrcBag)
    {}

    uint32                          m_currIndex; // The current index of the bag iterator.
    const StringBag<T, Allocator>*  m_pSrcBag;   // The bag container this iterator is used for.

    // Although this is a transgression of coding standards, it means that StringBag does not need to have a public
    // interface specifically to implement this class. The added encapsulation this provides is worthwhile.
    friend class StringBag<T, Allocator>;
};

/**
***********************************************************************************************************************
* @brief StringBag container.
*
* StringBag is a templated array based storage. If space is needed it dynamically allocates double the required
* size every time the capacity is exceeded. Operations which this class supports are:
*
* - Insertion at the end of the array.
* - Forward iteration.
* - Random access from valid handles.
*
* All strings are stored with their length before the actual string data in the buffer.
*
* @warning This class is not thread-safe.
***********************************************************************************************************************
*/
template<typename T, typename Allocator>
class StringBag
{
    static_assert((std::is_same<T, char>::value || std::is_same<T, wchar_t>::value),
                  "StringBag type T must be either char or wchar_t.");

    using StringLengthType = uint32;
public:
    using Iter             = StringBagIterator<T, Allocator>;
    using Handle           = StringBagHandle<T>;
    using StringViewType   = StringView<T>;

    /// Constructor.
    ///
    /// @param [in] pAllocator The allocator that will allocate memory if required.
    explicit StringBag(Allocator*const pAllocator);

    /// Destructor.
    ~StringBag();

    /// Increases maximal buffer capacity to a value greater or equal to the @ref newCapacity.
    /// If @ref newCapacity is greater than the maximal buffer capacity, new storage is allocated,
    /// otherwise the method does nothing.
    ///
    /// @note All existing iterators, and handles will not get invalidated, even in case new storage is allocated,
    ///       because iterators are referencing the bag, rather than memory of that bag.
    ///
    /// @warning All pointers and references to strings of a bag will be invalidated,
    ///          in the case new storage is allocated.
    ///
    /// @param [in] newCapacity The new capacity of the bag, which is lower limit of the maximal capacity. The
    ///                         units are in terms of characters (T).
    ///
    /// @returns Result ErrorOutOfMemory if the operation failed.
    Result Reserve(uint32 newCapacity)
    {
        Result result = Result::_Success;
        // Not enough storage.
        if (m_maxCapacity < newCapacity)
        {
            result = ReserveInternal(newCapacity);
        }
        return result;
    }

    /// Copy a string to end of the bag. If there is not enough space available, new space will be allocated
    /// and the old strings will be copied to the new space.
    ///
    /// @warning Calling with a @ref pString that doesn't have a null terminator results in undefined behavior.
    ///
    /// @warning Calling with an invalid @ref pResult pointer will cause an access violation!
    ///
    /// @param [in] pString  The string to be pushed to the bag. The string will become the last string in the
    ///                      bag.
    ///
    /// @param [out] pResult - Set to ErrorInvalidPointer if @ref pString an invalid pointer, but leaves the bag
    ///                        in an unmodified state.
    ///                      - Set to ErrorOutOfMemory if the operation failed because memory couldn't be allocated.
    ///                      - Set to ErrorInvalidMemorySize if the operation would result in a memory allocation
    ///                        larger than the maximum value possible for a uint32.
    ///
    /// @returns A valid handle to the inserted string if @ref pResult is set to Success. The handle is the
    ///          interger offset to the start of the inserted string in the bag.
    Handle PushBack(const T* pString, Result* pResult);

    /// Copy a string to end of the bag. If there is not enough space available, new space will be allocated
    /// and the old strings will be copied to the new space.
    ///
    /// @note If the length the string must be calculated, or is already known before insertion this overload of
    ///       PushBack() should be a slight optimization.
    ///
    /// @warning Calling with an invalid @ref length results in undefined behavior!
    ///
    /// @warning Calling with an invalid @ref pResult pointer will cause an access violation!
    ///
    /// @param [in] pString  The string to be pushed to the bag. The string will become the last string in the
    ///                      bag.
    ///
    /// @param [in] length   The length of the string to be copied not including the null terminator.
    ///
    /// @param [out] pResult - Set to ErrorInvalidPointer if @ref pString an invalid pointer, but leaves the bag
    ///                        in an unmodified state.
    ///                      - Set to ErrorOutOfMemory if the operation failed because memory couldn't be allocated.
    ///                      - Set to ErrorInvalidMemorySize if the operation would result in a memory allocation
    ///                        larger than the maximum value possible for a uint32.
    ///
    /// @returns A valid handle to the inserted string if @ref pResult is set to Success. The handle is the
    ///          interger offset to the start of the inserted string in the bag.
    Handle PushBack(const T* pString, uint32 length, Result* pResult);

    /// Copy the string from a @ref StringView to end of the bag. If there is not enough space available, new space will
    /// be allocated and the old strings will be copied to the new space.
    ///
    /// @note Even if the string view does not point to a null terminated string, the stored string will be null
    ///       terminated.
    ///
    /// @warning Calling with an invalid @ref length results in undefined behavior!
    ///
    /// @warning Calling with an invalid @ref pResult pointer will cause an access violation!
    ///
    /// @param [in] sv  The string view with the string to be pushed to the bag. The string will become the last string
    ///                 in the bag.
    ///
    /// @param [out] pResult - Set to ErrorInvalidPointer if @ref pString an invalid pointer, but leaves the bag
    ///                        in an unmodified state.
    ///                      - Set to ErrorOutOfMemory if the operation failed because memory couldn't be allocated.
    ///                      - Set to ErrorInvalidMemorySize if the operation would result in a memory allocation
    ///                        larger than the maximum value possible for a uint32.
    ///
    /// @returns A valid handle to the inserted string if @ref pResult is set to Success. The handle is the
    ///          interger offset to the start of the inserted string in the bag.
    Handle PushBack(Util::StringView<T> sv, Result* pResult);

    /// Resets the bag. All dynamically allocated memory will be saved for reuse.
    ///
    /// @note All existing iterators, handles, and pointers to internal bag data will be invalidated.
    void Clear()
    {
        m_currOffset = 0;
    }

    /// Returns the string specified by @ref handle.
    ///
    /// @warning Calling this function with an out-of-bounds @ref handle will cause an access violation!
    ///
    /// @warning Calling this function with an invalid @ref handle results in undefined behavior!
    ///
    /// @param [in]  handle    Integer offset to the start of a string in the bag. Valid handles can
    ///                        only be obtained from:
    ///                             - @ref PushBack()
    ///                             - @ref BackHandle()
    ///                             - @ref StringBagIterator::GetHandle()
    ///
    /// @returns A const pointer to the string at the location specified by @ref handle.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 717
    const T* At(Handle handle) const { return GetDataAt(handle.InternalValue()); }
#else
    StringViewType At(Handle handle) const
    {
        return StringViewType{GetDataAt(handle.InternalValue()), GetLengthAt(handle.InternalValue())};
    }
#endif

    /// Returns an iterator to the first string in the bag.
    ///
    /// @warning If the bag is empty the iterator is immediately invalid.
    ///
    /// @returns An iterator to first string in the bag.
    Iter Begin() const { return Iter(0, this); }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 717
    /// Returns a pointer to the underlying buffer serving as the data storage.
    /// The returned pointer defines an always valid range [Data(), Data() + NumElements()),
    /// even if the container is empty (Data() is not dereferenceable in that case).
    ///
    /// @warning Dereferencing a pointer returned by Data() from an empty bag will cause an access violation!
    ///
    /// @returns Pointer to the underlying data storage for read only access.
    ///          For a non-empty bag, the returned pointer contains the address of the first string.
    ///          For an empty bag, the returned pointer may or may not be a null pointer.
    const T* Data() const { return m_pData; }

    /// Returns the size of the bag in characters.
    ///
    /// @returns An unsigned integer equal to the number of characters in all the strings currently present
    ///          in the bag. The size in bytes of this portion of the data buffer is equal to:
    ///                 size(T) * NumChars()
    uint32 NumChars() const { return m_currOffset; }
#endif

    /// Returns the size of the bag in bytes.
    uint32 GetByteSize() const { return m_currOffset; }

    /// Returns true if the number of strings present in the bag is equal to zero.
    ///
    /// @returns True if the bag is empty.
    bool IsEmpty() const { return (m_currOffset == 0); }

    /// Returns a pointer to the allocator used for this container's memory management.
    ///
    /// @returns Allocator pointer.
    Allocator* GetAllocator() const { return m_pAllocator; }

private:
    /// Space required for the string length. <summary>
    constexpr static size_t StringLengthSizeof = sizeof(StringLengthType);

    /// Returns how much space is required for a string with the given length, with additional padding for alignment.
    static constexpr size_t RequiredSpace(uint32 stringLength)
    {
        return Pow2Align(StringLengthSizeof + stringLength + 1, alignof(StringLengthType));
    }

    /// Returns if the offset is within bounds of the strings in the bag.
    ///
    /// @returns True if the current string this iterator is pointing to is within the permitted range.
    bool IsValid(uint32 offset) const { return (offset < m_currOffset); }

    /// Returns the length for the string at the given offset.
    uint32 GetLengthAt(uint32 offset) const
    {
        PAL_ASSERT(IsValid(offset));
        const auto* pStringLength =
            Util::AssumeAligned<alignof(StringLengthType)>(reinterpret_cast<uint32*>(m_pData + offset));
        return *pStringLength;
    }

    /// Returns the data of the string at the given offset.
    const T* GetDataAt(uint32 offset) const
    {
        PAL_ASSERT(IsValid(offset));
        return m_pData + StringLengthSizeof + offset;
    }

    Handle GetHandleAt(uint32 offset) const
    {
        PAL_ASSERT(IsValid(offset));
#if PAL_ENABLE_PRINTS_ASSERTS
        return Handle{*this, offset};
#else
        return Handle{offset};
#endif
    }

    Result ReserveInternal(uint32 newCapacity);

    T*               m_pData;        // Pointer to the string buffer.
    uint32           m_currOffset;   // Current character offset into the string buffer.
    uint32           m_maxCapacity;  // Maximum size it can hold.
    Allocator*const  m_pAllocator;   // Allocator for this StringBag.

    StringBag()                            = delete;
    StringBag(StringBag const&)            = delete;
    StringBag& operator=(const StringBag&) = delete;
    StringBag(StringBag&&)                 = delete;
    StringBag& operator=(StringBag&&)      = delete;

    // Although this is a transgression of coding standards, it prevents StringBagIterator requiring a public
    // constructor; constructing a default StringBagIterator (i.e. without calling StringBag::GetIterator) can never be
    // a legal operation, so this means that these two classes are much safer to use.
    friend class StringBagIterator<T, Allocator>;
#if PAL_ENABLE_PRINTS_ASSERTS
    // Although this is a transgression of coding standards, it allows StringBagHandle to access &StringBag::m_pData.
    friend class StringBagHandle<T>;
#endif
};

// =====================================================================================================================
template<typename T, typename Allocator>
StringBag<T, Allocator>::StringBag(
    Allocator*const pAllocator)
    :
    m_pData(nullptr),
    m_currOffset(0),
    m_maxCapacity(0),
    m_pAllocator(pAllocator)
{}

// =====================================================================================================================
template<typename T, typename Allocator>
StringBag<T, Allocator>::~StringBag()
{
    PAL_FREE(m_pData, m_pAllocator);
}

} // Util
