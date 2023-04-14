/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palList.h
 * @brief PAL utility collection List and ListIterator class declarations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"
#include "palAssert.h"

namespace Util
{

// Forward declarations.
template<typename T, typename Allocator> class List;

/// @internal Encapsulates one node of a double-linked-list
template<typename T>
struct ListNode
{
    T            data;   ///< Actual user data being stored by this node.
    ListNode<T>* pPrev;  ///< Previous node in this list.
    ListNode<T>* pNext;  ///< Next node in this list.
};

/**
 ***********************************************************************************************************************
 * @brief  Iterator for traversal of elements in a List collection.
 *
 * Allows traversal of all elements in a List going either forwards or backwards.
 ***********************************************************************************************************************
 */
template<typename T, typename Allocator>
class ListIterator
{
public:
    /// Returns a pointer to the current element.  Will return null if the iterator has been advanced off the end of the
    /// list.
    T* Get() const;

    /// Advances the iterator to the previous position (move backward).
    void Prev();

    /// Advances the iterator to the next position (move forward).
    void Next();

    /// Moves the iterator back to the start of the list.
    void Restart() { m_pCurrent = m_pList->m_header.pNext; }

    /// Equality operator. Returns true if two iterators point to the same position in the list.
    bool operator ==(const ListIterator<T, Allocator>& listIterator)
    {
        return (m_pList == listIterator.m_pList) && (m_pCurrent == listIterator.m_pCurrent);
    }

    /// Inequality operator. Returns false if two iterators are equal.
    bool operator !=(const ListIterator<T, Allocator>& listIterator)
    {
        return !(*this == listIterator);
    }

private:
    ListIterator(const List<T, Allocator>* pList, ListNode<T>* pStart);

    // Returns true if the iterator is pointing to the header.
    bool IsHeader() const { return m_pList->IsHeader(m_pCurrent); }

    // Returns false if the iterator is pointing to the footer
    bool IsFooter() const { return m_pList->IsFooter(m_pCurrent); }

    const List<T, Allocator>* m_pList;    // List we're iterating over.
    ListNode<T>*              m_pCurrent; // Pointer to the current element.

    PAL_DISALLOW_DEFAULT_CTOR(ListIterator);

    // Although this is a transgression of coding standards, it means that List does not need to have a public interface
    // specifically to implement this class.  The added encapsulation this provides is worthwhile.
    friend class List<T, Allocator>;
};

/**
 ***********************************************************************************************************************
 * @brief Templated doubly linked list container.
 *
 * This is meant for storing elements of an arbitrary (but uniform) type. Operations which this class supports are:
 *
 * - Insertion at any point
 * - Deletion at any point
 * - Forwards and reverse iteration
 *
 * @warning This class is not thread-safe for push, pop, or iteration!
 *
 * @warning It is the client's responsibility to empty the list before destroying the list so that they can handle
 *          proper destruction of all data elements.
 ***********************************************************************************************************************
 */
template<typename T, typename Allocator>
class List
{
public:
    /// Constructor.
    ///
    /// @param [in] pAllocator The allocator that will allocate memory if required.
    List(Allocator*const pAllocator);
    ~List();

    /// Returns the number of elements in the list, not counting the permanent header and footer nodes.
    size_t NumElements() const { return m_numElements; }

    /// Returns an iterator pointing to the first element in the list.  If the list is empty, the iterator starts out
    /// pointing at the permanent footer node.
    ///
    /// @returns An iterator pointing at the front end of the list.
    ListIterator<T, Allocator> Begin() const { return ListIterator<T, Allocator>(this, m_header.pNext); }

    /// Returns an iterator pointing to the permanent footer, which does not contain any real valid data.
    ///
    /// This is useful for iterating while "(it != list.End())".
    ///
    /// @returns An iterator pointing to the permanent footer node.
    ListIterator<T, Allocator> End() const { return ListIterator<T, Allocator>(this, m_footer.pPrev->pNext); }

    /// Pushes a copy of the specified value onto the front of the list.
    ///
    /// @param [in] data value to be added to the front of the list.
    ///
    /// @returns @ref Success if the value was successfully added to the list or @ref ErrorOutOfMemory if the operation
    ///          failed because of an internal failure to allocate system memory.
    Result PushFront(const T& data) { return InsertBefore(m_header.pNext, data); }

    /// Pushes a copy of the specified value onto the back of the list.
    ///
    /// @param [in] data value to be added to the back of the list.
    ///
    /// @returns @ref Success if the value was successfully added to the list or @ref ErrorOutOfMemory if the operation
    ///          failed because of an internal failure to allocate system memory.
    Result PushBack(const T& data)  { return InsertBefore(&m_footer, data); }

    /// Inserts the specified data before a particular node in a list.
    ///
    /// If the iterator has advanced off the end of the list (i.e., the iterator is End()), the added node will be the
    /// new tail node.
    ///
    /// @param [in,out] pIterator Identifies a node where the insertion should take place.  The iterator will point to
    ///                           the same spot in the list after insertion, though it will be updated internally.
    /// @param [in]     data      Data to be added to the list.
    ///
    /// @returns @ref Success if the value was successfully added to the list or @ref ErrorOutOfMemory if the operation
    ///          failed because of an internal failure to allocate system memory.
    Result InsertBefore(ListIterator<T, Allocator>* pIterator, const T& data);

    /// Removes the node at the specified position from the list.
    ///
    /// No action taken if the iterator has already advanced off the end of the list.
    ///
    /// @param [in,out] pIterator Iterator identifying the node to be removed.  After the node is removed, this iterator
    ///                           will normally be advanced to the next node.  However, if this call removes the tail
    ///                           node in the list then the iterator will point at the new tail, and if this call
    ///                           removes the final remaining node in the list then the iterator will point at the
    ///                           End() footer.
    void Erase(ListIterator<T, Allocator>* pIterator);

private:
    void   Erase(ListNode<T>* pNode);
    Result InsertBefore(ListNode<T>* pBeforeMe, const T& data);

    bool IsHeader(ListNode<T>* pNode) const { return (pNode == &m_header); }
    bool IsFooter(ListNode<T>* pNode) const { return (pNode == &m_footer); }

    size_t           m_numElements;  // Number of elements.
    ListNode<T>      m_header;       // Fake node, always the first thing in the list.
    ListNode<T>      m_footer;       // Fake node, always the last thing in the list.
    Allocator*const  m_pAllocator;   // Allocator for this list.

    PAL_DISALLOW_COPY_AND_ASSIGN(List);

    // Although this is a transgression of coding standards, it prevents ListIterator requiring a public constructor;
    // constructing a 'bare' ListIterator (i.e. without calling List::GetIterator) can never be a legal operation, so
    // this means that these two classes are much safer to use.
    friend class ListIterator<T, Allocator>;
};

// =====================================================================================================================
template<typename T, typename Allocator>
ListIterator<T, Allocator>::ListIterator(
    const List<T, Allocator>*  pList,
    ListNode<T>*               pStart)
    :
    m_pList(pList),
    m_pCurrent(pStart)
{
}

// =====================================================================================================================
template<typename T, typename Allocator>
List<T, Allocator>::List(
    Allocator*const pAllocator)
    :
    m_numElements(0),
    m_pAllocator(pAllocator)
{
    m_header.pNext = &m_footer;
    m_header.pPrev = nullptr;

    m_footer.pPrev = &m_header;
    m_footer.pNext = nullptr;
}

// =====================================================================================================================
template<typename T, typename Allocator>
List<T, Allocator>::~List()
{
    // The client must make sure the list is empty before destroying it.
    PAL_ASSERT(NumElements() == 0);
}

} // Util
