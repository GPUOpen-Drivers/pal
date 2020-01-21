/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palIntervalTreeImpl.h
 * @brief PAL utility collection IntervalTree class implementation.
 ***********************************************************************************************************************
 */

#pragma once

#include "palIntervalTree.h"

namespace Util
{

//======================================================================================================================
// Returns the tree node containing the specified interval - Null node is converted to nullptr.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::FindContainingNode(
    const Interval<T, K>* pInterval
    ) const
{
    IntervalTreeNode<T, K>* pNode = FindContaining(pInterval);

    return (pNode == GetNull()) ? nullptr : pNode;
}

//======================================================================================================================
// Returns the tree node containing the specified interval.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::FindContaining(
    const Interval<T, K>* pInterval
    ) const
{
    IntervalTreeNode<T, K>* pNode = m_pRoot;

    while ((pNode != GetNull()) &&
           ((pNode->interval.high < pInterval->high) || (pNode->interval.low > pInterval->low)))
    {
        if ((pNode->pLeftChild != GetNull()) && (pInterval->low <= pNode->pLeftChild->highest))
        {
            pNode = pNode->pLeftChild;
        }
        else
        {
            pNode = pNode->pRightChild;
        }
    }

    return pNode;
}

//======================================================================================================================
// Returns a tree node that overlaps the specified interval - Null node is converted to nullptr.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::FindOverlappingNode(
    const Interval<T, K>* pInterval
    ) const
{
    IntervalTreeNode<T, K>* pNode = FindOverlapping(pInterval);

    return (pNode == GetNull()) ? nullptr : pNode;
}

//======================================================================================================================
// Returns a tree node that overlaps the specified interval.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::FindOverlapping(
    const Interval<T, K>* pInterval
    ) const
{
    IntervalTreeNode<T, K>* pNode = m_pRoot;

    while ((pNode != GetNull()) &&
           ((pInterval->low > pNode->interval.high) || (pNode->interval.low > pInterval->high)))
    {
        if ((pNode->pLeftChild != GetNull()) && (pInterval->low <= pNode->pLeftChild->highest))
        {
            pNode = pNode->pLeftChild;
        }
        else
        {
            pNode = pNode->pRightChild;
        }
    }

    return pNode;
}

//======================================================================================================================
// Inserts the specified interval into the red-black tree.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::Insert(
    const Interval<T, K>* pInterval)
{
    // The PAL_NEW macro doesn't work correctly with IntervalTreeNode because there is a comma in the template argument
    // list.  Create a temporary typedef to get around this.
    typedef IntervalTreeNode<T, K> TypeName;
    IntervalTreeNode<T, K>* pNode = PAL_NEW(TypeName, m_pAllocator, AllocInternal);

    if (pNode != nullptr)
    {
        pNode->pLeftChild   = GetNull();
        pNode->pRightChild  = GetNull();
        pNode->pParent      = GetNull();
        pNode->color        = NodeColor::Red;
        pNode->highest      = pInterval->high;
        pNode->interval     = *pInterval;

        // Inserts the new node into the tree as a binary search tree.
        IntervalTreeNode<T, K>* pX = m_pRoot;
        IntervalTreeNode<T, K>* pY = GetNull();

        while (pX != GetNull())
        {
            if (pX->highest < pNode->highest)
            {
                pX->highest = pNode->highest;
            }

            pY = pX;

            if (pX->interval.low > pInterval->low)
            {
                pX = pX->pLeftChild;
            }
            else
            {
                pX = pX->pRightChild;
            }
        }

        if (pY == GetNull())
        {
            m_pRoot = pNode;
        }
        else // Inserts pNode as child of pY.
        {
            if (pY->interval.low > pInterval->low)
            {
                pY->pLeftChild = pNode;
            }
            else
            {
                pY->pRightChild = pNode;
            }
            pNode->pParent = pY;
        }

        // Fix possible violation of property 3.
        InsertFixup(pNode);

        m_count++;
    }

    return pNode;
}

//======================================================================================================================
// Deletes the specified node from the tree.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::Delete(
    IntervalTreeNode<T, K>* pNode)
{
    if (pNode != GetNull())
    {
        if (pNode->pLeftChild != GetNull() && pNode->pRightChild != GetNull())
        {
            IntervalTreeNode<T, K>* pNext = Next(pNode);

            // Swap the topology between pNode and pNext, includes color and topology, which keeps the red-balck
            // property.  Thus, pNode will be really deleted.
            SwapNodeTopology(pNode, pNext);

            // Adjust the highest value of every node of the path from pNode to root.
            pNext->highest = CalcHighestValue(pNext);

            while (pNext->pParent != GetNull())
            {
                pNext = pNext->pParent;
                pNext->highest = CalcHighestValue(pNode);
            }
        }

        // Delete pNode.
        IntervalTreeNode<T, K>* pTemp = (pNode->pLeftChild != GetNull()) ? pNode->pLeftChild : pNode->pRightChild;
        pTemp->pParent = pNode->pParent;
        // NOTE: pTemp might be "NULL" here and we still need to adjust its parent.
        if (pNode == m_pRoot)
        {
            m_pRoot = pTemp;
        }
        else
        {
            if (pNode == pNode->pParent->pLeftChild)
            {
                pNode->pParent->pLeftChild = pTemp;
            }
            else
            {
                pNode->pParent->pRightChild = pTemp;
            }

            IntervalTreeNode<T, K>* pUp = pNode->pParent;
            pUp->highest = CalcHighestValue(pUp);
            while (pUp->pParent != GetNull())
            {
                pUp = pUp->pParent;
                pUp->highest = CalcHighestValue(pUp);
            }
        }

        if (pNode->color == NodeColor::Black)
        {
            DeleteFixup(pTemp);
        }

        PAL_SAFE_DELETE(pNode, m_pAllocator);
        m_count--;
    }
}

//======================================================================================================================
// Returns a pointer to the tree node corresponding to the specified interval.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::Search(
    const Interval<T, K>* pInterval
    ) const
{
    IntervalTreeNode<T, K>* pRoot = m_pRoot;

    while (pRoot != GetNull())
    {
        if ((pRoot->interval.low == pInterval->low) &&
            (pRoot->interval.high == pInterval->high))
        {
            break;
        }
        else if (pRoot->interval.low > pInterval->low)
        {
            pRoot = pRoot->pLeftChild;
        }
        else
        {
            pRoot = pRoot->pRightChild;
        }
    }

    return pRoot;
}

//======================================================================================================================
// Overwrites the specified interval range, adjusting the tree as necessary (potentially inserting a new node and
// splitting or combining adjacent nodes).
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::OverwriteInterval(
    const Interval<T, K>* pInterval)
{
    IntervalTreeNode<T, K>* pLowerBound    = LowerOverlappingBound(pInterval);
    IntervalTreeNode<T, K>* pUpperBound    = UpperOverlappingBound(pInterval);
    Interval<T, K>          insertInterval = *pInterval;
    bool                    fillGap[2]     = { false, false };
    Interval<T, K>          gapRange[2]    = { };

    // pLowerBound == pUpperBound and pLowerBound->interval.value == pInterval->value implies that the interval tree
    // does not have to be modified, since interval of pInterval is inside of interval of pLowerBound(pUpperBound).
    if ((pLowerBound != GetNull() && pUpperBound != GetNull()) &&
        ((pLowerBound != pUpperBound) || (pLowerBound->interval.value != pInterval->value)))
    {
        IntervalTreeNode<T, K>* pLowerBoundPrev = Prev(pLowerBound);
        IntervalTreeNode<T, K>* pUpperBoundNext = Next(pUpperBound);

        if ((pLowerBound->interval.low == pInterval->low) &&
            (pLowerBoundPrev != GetNull() &&
            (pLowerBoundPrev->interval.value == pInterval->value)))
        {
            // If have the same low boundary as the inserting interval, and our neighbor has
            // the same state, combine the nodes and delete our left neighbor.
            insertInterval.low = pLowerBoundPrev->interval.low;
            Delete(pLowerBoundPrev);
        }
        else if (pLowerBound->interval.low != pInterval->low)
        {
            // We don't touch the low boundary of our containing node, create node to fill the gap.
            fillGap[0] = true;
            gapRange[0].low   = pLowerBound->interval.low;
            gapRange[0].high  = (pInterval->low - 1);
            gapRange[0].value = pLowerBound->interval.value;
        }

        if ((pUpperBound->interval.high == pInterval->high) &&
            (pUpperBoundNext != GetNull() &&
            (pUpperBoundNext->interval.value == pInterval->value)))
        {
            // If we're abutting the high boundary of our containing node, and our neighbor has the same state, combine
            // the nodes and delete our right neighbor.
            insertInterval.high = pUpperBoundNext->interval.high;
            Delete(pUpperBoundNext);
        }
        else if (pUpperBound->interval.high != pInterval->high)
        {
            // We don't touch the high boundary of our containing node, create node to fill the gap.
            fillGap[1] = true;
            gapRange[1].low   = (pInterval->high + 1);
            gapRange[1].high  = pUpperBound->interval.high;
            gapRange[1].value = pUpperBound->interval.value;
        }

        if ((insertInterval.low == pLowerBound->interval.low) &&
            (insertInterval.high == pLowerBound->interval.high))
        {
            PAL_ASSERT(pLowerBound == pUpperBound);
            pLowerBound->interval.value = insertInterval.value;
        }
        else
        {
            pUpperBoundNext = Next(pUpperBound);
            while (pLowerBound != pUpperBoundNext)
            {
                IntervalTreeNode<T, K>* pTempNode = pLowerBound;
                pLowerBound = Next(pLowerBound);
                Delete(pTempNode);
            }
            Insert(&insertInterval);
        }

        // Insert the gap range after tree topology updated with respect to insert inverval, in order to avoid
        // fill-gap-node being mis-deleted
        for (uint32 i = 0; i < 2; ++i)
        {
            if (fillGap[i])
            {
                Insert(&gapRange[i]);
            }
        }
    }
}

//======================================================================================================================
// In-order traverse helper.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::Inorder(
    IntervalTreeNode<T, K>* pRoot,
    void                  (*pfnTraverse)(IntervalTreeNode<T, K>*, void*),
    void*                   pData
    ) const
{
    if (pRoot->pLeftChild != GetNull())
    {
        Inorder(pRoot->pLeftChild, pfnTraverse, pData);
    }

    if (pRoot != GetNull())
    {
        (*pfnTraverse)(pRoot, pData);
    }

    if (pRoot->pRightChild != GetNull())
    {
        Inorder(pRoot->pRightChild, pfnTraverse, pData);
    }
}

//======================================================================================================================
// Calculates the highest value of sub-tree of pNode.
template<typename T, typename K, typename Allocator>
PAL_INLINE T IntervalTree<T, K, Allocator>::CalcHighestValue(
    IntervalTreeNode<T, K>* pNode
    ) const
{
    T highest = pNode->interval.high;

    if ((pNode->pLeftChild != GetNull()) && (highest < pNode->pLeftChild->highest))
    {
        highest = pNode->pLeftChild->highest;
    }

    if ((pNode->pRightChild != GetNull()) && (highest < pNode->pRightChild->highest))
    {
        highest = pNode->pRightChild->highest;
    }

    return highest;
}

//======================================================================================================================
// Gets previous node of pNode.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::Prev(
    IntervalTreeNode<T, K>* pNode
    ) const
{
    IntervalTreeNode<T, K>* pPrev = pNode;

    if (pNode != GetNull())
    {
        if (pNode->pLeftChild != GetNull())
        {
            pPrev = pNode->pLeftChild;

            while (pPrev->pRightChild != GetNull())
            {
                pPrev = pPrev->pRightChild;
            }
        }
        else
        {
            pPrev = pNode->pParent;

            while ((pPrev != GetNull()) && (pNode != pPrev->pRightChild))
            {
                pNode = pPrev;
                pPrev = pPrev->pParent;
            }
        }
    }

    return pPrev;
}

//======================================================================================================================
// Gets next node of pNode.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::Next(
    IntervalTreeNode<T, K>* pNode
    ) const
{
    IntervalTreeNode<T, K>* pNext = pNode;

    if (pNode != GetNull())
    {
        // If pNode has right child, return the minimum node in the subtree rooted at its right child.
        if (pNode->pRightChild != GetNull())
        {
            pNext = pNode->pRightChild;

            while (pNext->pLeftChild != GetNull())
            {
                pNext = pNext->pLeftChild;
            }
        }
        else // Otherwise, climb up the tree
        {
            pNext = pNode->pParent;

            while ((pNext != GetNull()) && (pNode != pNext->pLeftChild))
            {
                pNode = pNext;
                pNext = pNext->pParent;
            }
        }
    }

    return pNext;
}

//======================================================================================================================
// Finds the tree node that contains the interval point.
template<typename T, typename K, typename Allocator>
PAL_INLINE IntervalTreeNode<T, K>* IntervalTree<T, K, Allocator>::FindContaining(
    T intervalPoint
    ) const
{
    IntervalTreeNode<T, K>* pRoot = m_pRoot;

    while (pRoot != GetNull())
    {
        if ((pRoot->interval.low <= intervalPoint) && (pRoot->interval.high >= intervalPoint))
        {
            break;
        }
        else if (pRoot->interval.low > intervalPoint)
        {
            pRoot = pRoot->pLeftChild;
        }
        else
        {
            pRoot = pRoot->pRightChild;
        }
    }

    return pRoot;
}

//======================================================================================================================
// Fixes up tree after insertion.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::InsertFixup(
    IntervalTreeNode<T, K>* pX)
{
    IntervalTreeNode<T, K>* pY;

    // If pX is the root, its parent (NULL) must be black
    while (pX->pParent->color == NodeColor::Red)
    {
        if (pX->pParent == pX->pParent->pParent->pLeftChild)
        {
            pY = pX->pParent->pParent->pRightChild;

            if (pY->color == NodeColor::Red)
            {
                /* <Case 1>
                 *
                 *
                 *       P[P[X]](black)  // In this case, p[p[x]] must be black.
                 *       /      \
                 *    P[X](red)  y(red)
                 *       |
                 *     x(red)
                 */
                pX->pParent->color = NodeColor::Black;
                pY->color = NodeColor::Black;
                pX->pParent->pParent->color = NodeColor::Red;

                pX = pX->pParent->pParent;

                // The new pX may still violate property 3.
            }
            else
            {
                if (pX == pX->pParent->pRightChild)
                {
                    /* <Case 2>
                     *
                     *           p[p[x]](black) // p[p[x]] must be black.
                     *           /       \
                     *       p[x](red)   y(black)
                     *       /   \
                     *      a   x(red)
                     *           /  \
                     *          a    a   // The same symbol "a" means the BlackHeight.
                     */
                    LeftRotate(pX->pParent);
                    pX = pX->pLeftChild;
                }

                /* <Case 3>
                 *
                 *           p[p[x]](black) // p[p[x]] must be black.
                 *           /       \
                 *       p[x](red)   y(black)
                 *       /   \
                 *    x(red)  a
                 *    /  \
                 *   a    a   // The same symbol "a" means the BlackHeight.
                 */
                RightRotate(pX->pParent->pParent);
                pX->pParent->color = NodeColor::Black;
                pX->pParent->pRightChild->color = NodeColor::Red;
            }
        }
        else
        {
            // Switch the places of left and right in if clause.
            pY = pX->pParent->pParent->pLeftChild;

            if (pY->color == NodeColor::Red)
            {
                // <Case 1>
                pX->pParent->color = NodeColor::Black;
                pY->color = NodeColor::Black;
                pX->pParent->pParent->color = NodeColor::Red;
                pX = pX->pParent->pParent;
            }
            else
            {
                if (pX == pX->pParent->pLeftChild)
                {
                    // <Case 2>
                    RightRotate(pX->pParent);
                    pX = pX->pRightChild;
                }

                // <Case 3>
                LeftRotate(pX->pParent->pParent);
                pX->pParent->color = NodeColor::Black;
                pX->pParent->pLeftChild->color = NodeColor::Red;
            }
        }
    }

    m_pRoot->color = NodeColor::Black;
}

//======================================================================================================================
// Fixes up tree after deletion.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::DeleteFixup(
    IntervalTreeNode<T, K>* pX)
{
    while ((pX != m_pRoot) && (pX->color == NodeColor::Black))
    {
        if (pX == pX->pParent->pLeftChild)
        {
            IntervalTreeNode<T, K>* pSibling = pX->pParent->pRightChild;
            if (pSibling->color == NodeColor::Red)
            {
                pSibling->color = NodeColor::Black;
                pX->pParent->color = NodeColor::Red;
                LeftRotate(pX->pParent);
                pSibling = pX->pParent->pRightChild;
            }

            if ((pSibling->pLeftChild->color == NodeColor::Black) &&
                (pSibling->pRightChild->color == NodeColor::Black))
            {
                pSibling->color = NodeColor::Red;
                pX = pX->pParent;
            }
            else
            {
                if (pSibling->pRightChild->color == NodeColor::Black)
                {
                    pSibling->pLeftChild->color = NodeColor::Black;
                    pSibling->color = NodeColor::Red;
                    RightRotate(pSibling);
                    pSibling = pX->pParent->pRightChild;
                }
                pSibling->color = pX->pParent->color;
                pX->pParent->color = NodeColor::Black;
                pSibling->pRightChild->color = NodeColor::Black;
                LeftRotate(pX->pParent);
                pX = m_pRoot;
            }
        }
        else // Switch the left and right in the last if clause.
        {
            IntervalTreeNode<T, K>* pSibling = pX->pParent->pLeftChild;
            if (pSibling->color == NodeColor::Red)
            {
                pSibling->color = NodeColor::Black;
                pX->pParent->color = NodeColor::Red;
                RightRotate(pX->pParent);
                pSibling = pX->pParent->pLeftChild;
            }

            if ((pSibling->pLeftChild->color == NodeColor::Black) &&
                (pSibling->pRightChild->color == NodeColor::Black))
            {
                pSibling->color = NodeColor::Red;
                pX = pX->pParent;
            }
            else
            {
                if (pSibling->pLeftChild->color == NodeColor::Black)
                {
                    pSibling->pRightChild->color = NodeColor::Black;
                    pSibling->color = NodeColor::Red;
                    LeftRotate(pSibling);
                    pSibling = pX->pParent->pLeftChild;
                }
                pSibling->color = pX->pParent->color;
                pX->pParent->color = NodeColor::Black;
                pSibling->pLeftChild->color = NodeColor::Black;
                RightRotate(pX->pParent);
                pX = m_pRoot;
            }
        }
    }

    pX->color = NodeColor::Black;
}

//======================================================================================================================
// Left rotation for node A.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::LeftRotate(
    IntervalTreeNode<T, K>* pA)
{
    /*
     *        |                  |
     *        A                  B
     *       / \                / \
     *      *   B    ==>       A   *
     *         / \            / \
     *        C   *          *   C
     */

    // A and  A->pRightChild should not be NULL.
    IntervalTreeNode<T, K>* pB = pA->pRightChild;
    IntervalTreeNode<T, K>* pC = pB->pLeftChild; // C could be NULL.

    if (pA->pParent != GetNull()) // A is not the root.
    {
        if (pA == pA->pParent->pLeftChild)
        {
            pA->pParent->pLeftChild = pB;
        }
        else
        {
            pA->pParent->pRightChild = pB;
        }
    }
    else
    {
        m_pRoot = pB;
    }

    pB->pParent = pA->pParent;
    pB->pLeftChild = pA;
    pA->pParent = pB;
    pA->pRightChild = pC;
    if (pC != GetNull())
    {
        pC->pParent = pA;
    }
    pB->highest = pA->highest;
    pA->highest = pC->highest > pA->interval.high ? pC->highest : pA->interval.high;

    if ((pA->pLeftChild != GetNull()) && (pA->highest < pA->pLeftChild->highest))
    {
        pA->highest = pA->pLeftChild->highest;
    }
}

//======================================================================================================================
// Right rotation for node A.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::RightRotate(
    IntervalTreeNode<T, K>* pA)
{
    /*
     *        |                  |
     *        A                  B
     *       / \                / \
     *      B   *    ==>       *   A
     *     / \                    / \
     *    *   C                  C   *
     */

    // A and  A->pLeftChild should not be NULL.
    IntervalTreeNode<T, K>* pB = pA->pLeftChild;
    IntervalTreeNode<T, K>* pC = pB->pRightChild; // C could be NULL.

    if (pA->pParent != GetNull())
    {
        if (pA == pA->pParent->pLeftChild)
        {
            pA->pParent->pLeftChild = pB;
        }
        else
        {
            pA->pParent->pRightChild = pB;
        }
    }
    else
    {
        m_pRoot = pB;
    }

    pB->pParent = pA->pParent;
    pB->pRightChild = pA;
    pA->pParent = pB;
    pA->pLeftChild = pC;
    if (pC != GetNull())
    {
        pC->pParent = pA;
    }
    pB->highest = pA->highest;
    pA->highest = pC->highest > pA->interval.high ? pC->highest : pA->interval.high;

    if ((pA->pRightChild != GetNull()) && (pA->highest < pA->pRightChild->highest))
    {
        pA->highest = pA->pRightChild->highest;
    }
}

//======================================================================================================================
// Swaps the node color and swap its topology in tree simultaneously.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::SwapNodeTopology(
    IntervalTreeNode<T, K>* pA,
    IntervalTreeNode<T, K>* pB)
{
    // Swap data of Node A and Node B.
    IntervalTreeNode<T, K> tempNode;
    tempNode.pLeftChild = pA->pLeftChild;
    tempNode.pRightChild = pA->pRightChild;
    tempNode.pParent = pA->pParent;
    tempNode.color = pA->color;

    pA->pLeftChild = pB->pLeftChild;
    pA->pRightChild = pB->pRightChild;
    pA->pParent = pB->pParent;
    pA->color = pB->color;

    pB->pLeftChild = tempNode.pLeftChild;
    pB->pRightChild = tempNode.pRightChild;
    pB->pParent = tempNode.pParent;
    pB->color = tempNode.color;

    // Re-setup the RBTree topology of Node A and Node B.
    ResetNodeTopology(pA, pB);
    ResetNodeTopology(pB, pA);
}

//======================================================================================================================
// Links the node topology with its parent and children.
template<typename T, typename K, typename Allocator>
PAL_INLINE void IntervalTree<T, K, Allocator>::ResetNodeTopology(
    IntervalTreeNode<T, K>* pNode,
    IntervalTreeNode<T, K>* pRefNode)
{
    // If the reference node is the root node, set the root node to pNode.
    if (m_pRoot == pRefNode)
    {
        m_pRoot = pNode;
    }

    // Set up the information of the new left child of pNode.
    if (pNode->pLeftChild != GetNull())
    {
        if (pNode->pLeftChild == pNode)
        {
            // Deal with the case that pNode and pRefNode are adjacent.
            pNode->pLeftChild = pRefNode;
        }
        else
        {
            pNode->pLeftChild->pParent = pNode;
        }
    }

    // Set up the information of the new right child of pNode.
    if (pNode->pRightChild != GetNull())
    {
        if (pNode->pRightChild == pNode)
        {
            // Deal with the case that pNode and pRefNode are adjacent.
            pNode->pRightChild = pRefNode;
        }
        else
        {
            pNode->pRightChild->pParent = pNode;
        }
    }

    // Set up the information of the new parent of pNode.
    if (pNode->pParent != GetNull())
    {
        if (pNode->pParent == pNode)
        {
            pNode->pParent = pRefNode;
        }
        else
        {
            if (pRefNode == pNode->pParent->pLeftChild)
            {
                // Deal with the case that pNode and pRefNode are adjacent.
                pNode->pParent->pLeftChild = pNode;
            }
            else
            {
                pNode->pParent->pRightChild = pNode;
            }
        }
    }
}

} // Util
