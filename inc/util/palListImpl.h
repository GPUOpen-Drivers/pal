/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palListImpl.h
 * @brief PAL utility collection List and ListIterator class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palList.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
// Obtains a pointer to the data stored in the node pointed to by the iterator.  Returns null if the iterator is
// pointing at the footer.
template<typename T, typename Allocator>
T* ListIterator<T, Allocator>::Get() const
{
    // Assume that the iterator is pointing at either the header or the footer node, meaning the data in this node is
    // invalid.
    T* pRet = nullptr;

    // Shouldn't be able to walk off the end of the list...
    PAL_ASSERT(m_pCurrent != nullptr);

    // Shouldn't be able to point the iterator to the header node either.
    PAL_ASSERT(IsHeader() == false);

    if (IsFooter() == false)
    {
        // Valid node, return a pointer to the data.
        pRet = &m_pCurrent->data;
    }

    return pRet;
}

// =====================================================================================================================
// Advances the iterator to the next element in the list.
template<typename T, typename Allocator>
void ListIterator<T, Allocator>::Next()
{
    // Prevent our iterator from walking off the end of the list.
    if (IsFooter() == false)
    {
        m_pCurrent = m_pCurrent->pNext;
    }
}

// =====================================================================================================================
// Advances the iterator to the previous element in the list.  If the iterator currently points at the head, then the
// iterator is not changed.
template<typename T, typename Allocator>
void ListIterator<T, Allocator>::Prev()
{
    // Prevent our iterator from ever pointing to the permanent header node.  This prevents an "InsertBefore" call ever
    // being made while the iterator is pointing at the header, which would be bad.
    if (m_pList->IsHeader(m_pCurrent->pPrev) == false)
    {
        m_pCurrent = m_pCurrent->pPrev;
    }
}

// =====================================================================================================================
// Inserts "pData" before the specified iterator.  If the iterator has walked off the end of the list, this function
// will insert the new node as the tail of the list.
template<typename T, typename Allocator>
Result List<T, Allocator>::InsertBefore(
    ListIterator<T, Allocator>* pIterator,
    const T&         data)
{
    // Iterators are only a container for what we really need for the "insert" operation -- namely, a node in the list.
    // So just call the real insert function using the iterators node.
    return InsertBefore(pIterator->m_pCurrent, data);
}

// =====================================================================================================================
// Private method used that inserts "pData" before the specified node.  If "pNode" is null then this function will
// assume it is adding to an empty list.
template<typename T, typename Allocator>
Result List<T, Allocator>::InsertBefore(
    ListNode<T>* pBeforeMe,
    const T&     data)
{
    // Assume this is going to work...
    Result result = Result::_Success;

    // There is a "fake" footer node, meaning there's always a node to "insert before".
    PAL_ASSERT(pBeforeMe != nullptr);

    // Can't insert before the fake header node.
    PAL_ASSERT(IsHeader(pBeforeMe) == false);

    ListNode<T>* pNewNode = PAL_NEW(ListNode<T>, m_pAllocator, AllocInternal);
    if (pNewNode != nullptr)
    {
        pNewNode->data = data;

        // Ok, there is an element in this list that we can insert before, so connect our new node into the list.
        pNewNode->pNext = pBeforeMe;
        pNewNode->pPrev = pBeforeMe->pPrev;

        // The only node with a NULL "prev" pointer should be the header which we can't insert before.
        PAL_ASSERT(pNewNode->pPrev != nullptr);

        // And now connect the list into our new node.
        pBeforeMe->pPrev       = pNewNode;
        pNewNode->pPrev->pNext = pNewNode;

        m_numElements++;
    }
    else
    {
        // Couldn't allocate memory for a new node in the list.
        result = Result::ErrorOutOfMemory;
    }

    PAL_ALERT(result != Result::_Success);
    return result;
}

// =====================================================================================================================
// Public method used for removing the node pointed to be "pIterator" from the list.  If the iterator has walked off
// the end of the list, then nothing happens.
template<typename T, typename Allocator>
void List<T, Allocator>::Erase(
    ListIterator<T, Allocator>* pIterator)
{
    // Should be impossible to get the iterator to point to the permanent header node.
    PAL_ASSERT(pIterator->IsHeader() == false);

    // Make sure we're not trying to destroy the permanent footer node.
    if (pIterator->IsFooter() == false)
    {
        ListNode<T>* pDestroyMe = pIterator->m_pCurrent;

        // Make sure this iterator hasn't walked off the end of the list...  The "Next()" and "Prev()" functions should
        // make this impossible.
        PAL_ASSERT(pDestroyMe != nullptr);

        // We're about to destroy this iterators node.  In order to keep the iterator valid, we need to advance it.
        // Don't advance to the dummy footer node unless there is no valid node available.
        if (IsFooter(pDestroyMe->pNext) == false)
        {
            // Ok, our next node is not the dummy footer, so advance to there.
            pIterator->m_pCurrent = pDestroyMe->pNext;
        }
        else
        {
            // Next node is the dummy footer.  Move the iterator to the previous node instead.
            pIterator->m_pCurrent = pDestroyMe->pPrev;

            if (pIterator->IsHeader())
            {
                // Ok, the iterator is now pointing to the permanent header node, which isn't good either as now the
                // iterator can't be used for "insert before" operations.  Move to the header's "next", which should be
                // the footer.
                pIterator->m_pCurrent = pDestroyMe->pNext;

                PAL_ASSERT(pIterator->IsFooter());
            }
        }

        // And go ahead and destroy the node.
        Erase(pDestroyMe);
    }
}

// =====================================================================================================================
// Private method that removes "pNode" from the list.  pNode can not be null.
template<typename T, typename Allocator>
void List<T, Allocator>::Erase(
    ListNode<T>* pNode)
{
    // Something bad has happened.  We are trying to erase a node from an empty list?
    PAL_ASSERT(m_numElements != 0);

    // Can't erase a non-existant node!
    PAL_ASSERT(pNode != nullptr);

    // Make sure we're not trying to erase the permanent header or footer nodes.
    PAL_ASSERT((IsHeader(pNode) == false) && (IsFooter(pNode) == false));

    // Connect our previous node around.
    pNode->pPrev->pNext = pNode->pNext;

    // And connect our successor around as well.
    pNode->pNext->pPrev = pNode->pPrev;

    m_numElements--;

    PAL_SAFE_DELETE(pNode, m_pAllocator);
}

} // Util
