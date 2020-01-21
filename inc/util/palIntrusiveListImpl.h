/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palIntrusiveListImpl.h
 * @brief PAL utility collection IntrusiveList and IntrusiveListIterator class implementations.
 ***********************************************************************************************************************
 */

#pragma once

#include "palIntrusiveList.h"
#include "palSysMemory.h"

namespace Util
{

// =====================================================================================================================
template<typename T>
IntrusiveList<T>::IntrusiveList()
    :
    m_sentinel()
{
    // The sentinel must start out pointing at itself.
    m_sentinel.m_pPrev = &m_sentinel;
    m_sentinel.m_pNext = &m_sentinel;
}

// =====================================================================================================================
// Public method that inserts pNode into the list immediately before the iterator.
template<typename T>
void IntrusiveList<T>::InsertBefore(
    const Iter& iter,
    Node*       pNode)
{
    // The iterator must be an iterator for this list.
    PAL_ASSERT(iter.m_pSentinel == &m_sentinel);

    InsertBefore(iter.m_pCurrent, pNode);
}

// =====================================================================================================================
// Public method that inserts the contents of pSource at the front of the list, emptying out pSource in the process.
template<typename T>
void IntrusiveList<T>::PushFrontList(
    IntrusiveList<T>* pSource)
{
    // pSource must be non-null, non-empty and non-this.
    PAL_ASSERT((pSource != nullptr) && (pSource->IsEmpty() == false) && (pSource != this));

    // The source list's sentinel gives us the first and last nodes we must add to our list. We also need to know what
    // will be the old beginning of our list (it will be our sentinel if our list is empty).
    Node*const pSrcFirst = pSource->m_sentinel.m_pNext;
    Node*const pSrcLast  = pSource->m_sentinel.m_pPrev;
    Node*const pOldBegin = m_sentinel.m_pNext;

    // Point the two leftmost nodes at each other.
    pOldBegin->m_pPrev = pSrcLast;
    pSrcLast->m_pNext  = pOldBegin;

    // Point the two rightmost nodes at each other.
    pSrcFirst->m_pPrev = &m_sentinel;
    m_sentinel.m_pNext = pSrcFirst;

    // Point the source list's sentinel back at itself.
    pSource->m_sentinel.m_pPrev = &pSource->m_sentinel;
    pSource->m_sentinel.m_pNext = &pSource->m_sentinel;
}

// =====================================================================================================================
// Public method that inserts the contents of pSource at the end of the list, emptying out pSource in the process.
template<typename T>
void IntrusiveList<T>::PushBackList(
    IntrusiveList<T>* pSource)
{
    // pSource must be non-null, non-empty and non-this.
    PAL_ASSERT((pSource != nullptr) && (pSource->IsEmpty() == false) && (pSource != this));

    // The source list's sentinel gives us the first and last nodes we must add to our list. We also need to know what
    // will be the old end of our list (it will be our sentinel if our list is empty).
    Node*const pSrcFirst = pSource->m_sentinel.m_pNext;
    Node*const pSrcLast  = pSource->m_sentinel.m_pPrev;
    Node*const pOldEnd   = m_sentinel.m_pPrev;

    // Point the two leftmost nodes at each other.
    pOldEnd->m_pNext   = pSrcFirst;
    pSrcFirst->m_pPrev = pOldEnd;

    // Point the two rightmost nodes at each other.
    pSrcLast->m_pNext  = &m_sentinel;
    m_sentinel.m_pPrev = pSrcLast;

    // Point the source list's sentinel back at itself.
    pSource->m_sentinel.m_pPrev = &pSource->m_sentinel;
    pSource->m_sentinel.m_pNext = &pSource->m_sentinel;
}

// =====================================================================================================================
// Private method that inserts pNode into the list immediately before pBeforeMe.
template<typename T>
void IntrusiveList<T>::InsertBefore(
    Node* pBeforeMe,
    Node* pNode)
{
    // Both node arguments must be non-null and pNode must not be in a list.
    PAL_ASSERT((pBeforeMe != nullptr) && (pNode != nullptr) && (pNode->InList() == false));

    // Find the nodes that will become the neighbors of pNode once insertion is complete, then point pNode at its new
    // neighbors and the neighbors at pNode.
    Node*const pNextNode = pBeforeMe;
    Node*const pPrevNode = pBeforeMe->m_pPrev;

    pNode->m_pPrev = pPrevNode;
    pNode->m_pNext = pNextNode;

    pNextNode->m_pPrev = pNode;
    pPrevNode->m_pNext = pNode;
}

// =====================================================================================================================
// Private method that removes the node currently pointed to by pNode from the list.
template<typename T>
void IntrusiveList<T>::Unlink(
    Node* pNode)
{
    // Find the nodes that are on either side of this node and point them at each other.
    Node*const pNextNode = pNode->m_pNext;
    Node*const pPrevNode = pNode->m_pPrev;

    // Make the prev and next nodes point to each other.
    pNextNode->m_pPrev = pPrevNode;
    pPrevNode->m_pNext = pNextNode;

    // Reset the node by setting its node pointers to null.
    pNode->m_pPrev = nullptr;
    pNode->m_pNext = nullptr;
}

// =====================================================================================================================
// Public method that removes the node currently pointed to by pIter from the list. It is illegal to call this with an
// invalid iterator.
template<typename T>
void IntrusiveList<T>::Erase(
    Iter* pIter)
{
    // The iterator must be a valid iterator for this list.
    PAL_ASSERT((pIter != nullptr) && (pIter->m_pSentinel == &m_sentinel) && pIter->IsValid());

    Node*const pNextNode = pIter->m_pCurrent->m_pNext;
    Unlink(pIter->m_pCurrent);

    // Advance the iterator to the next node.
    pIter->m_pCurrent  = pNextNode;
}

// =====================================================================================================================
// Public method that removes the node from the list. It is illegal to call this with a node that is not in this list.
// Nothing is returned and the caller is expected to have a reference to the node.
template<typename T>
void IntrusiveList<T>::Erase(
    Node* pNode)
{
    // The node must be in a list.
    PAL_ASSERT(pNode->InList());

    Unlink(pNode);
}

// =====================================================================================================================
// Public method that removes all nodes from the list.
template<typename T>
void IntrusiveList<T>::EraseAll()
{
    for (auto iter = Begin(); iter.IsValid();)
    {
        Erase(&iter);
    }
}

} // Util
