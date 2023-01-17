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
 * @file  palIntervalTree.h
 * @brief PAL utility collection IntervalTree class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palSysMemory.h"

namespace Util
{

/// Node color - black or red.
enum class NodeColor : uint32
{
    Black = 0,
    Red
};

/// Describes the interval of a node in the interval tree.
template<typename T, typename K>
struct Interval
{
    T low;    ///< Low bound of interval.
    T high;   ///< High bound of interval.
    K value;  ///< Value of interval.
};

/// Describes a node in an interval tree.
template<typename T, typename K>
struct IntervalTreeNode
{
    IntervalTreeNode<T, K>* pLeftChild;   ///< Left child of current node.
    IntervalTreeNode<T, K>* pRightChild;  ///< Right child of current node.
    IntervalTreeNode<T, K>* pParent;      ///< Parent of current node.
    NodeColor               color;        ///< Color.
    T                       highest;      ///< Highest value of the sub-tree of current node.
    Interval<T, K>          interval;     ///< Interval.
};

/// Interval tree traverse callback function prototype.
template<typename T, typename K>
void (TraverseCallback)(IntervalTreeNode<T, K>* pNode, void* pData);

/**
 ***********************************************************************************************************************
 * @brief Red-black tree based interval tree.
 *
 * @note The Red-Black tree properties:
 *
 * 1. Every node is either red or black.
 * 2. The root and leaves(NULLs) are black.
 * 3. If a node is red, then its parent must be black.
 * 4. All simple paths from any node to a descendant leaf have the same number of black nodes.
 ***********************************************************************************************************************
 */
template<typename T, typename K, typename Allocator>
class IntervalTree
{
public:
    /// Constructor.
    ///
    /// @param [in] pAllocator The allocator that will allocate memory if required.
    IntervalTree(Allocator*const pAllocator) : m_null(), m_pRoot(&m_null), m_count(0), m_pAllocator(pAllocator) { }
    ~IntervalTree() { Destroy(m_pRoot); }

    /// Returns the number of nodes in the tree.
    size_t GetCount() const { return m_count; }

    /// Returns a pointer to the null (leaf) node - null node's parent info might be change in deletion.
    IntervalTreeNode<T, K>* GetNull() const { return const_cast<IntervalTreeNode<T, K>*>(&m_null); }

    /// Returns a pointer to the root node.
    IntervalTreeNode<T, K>* GetRoot() const { return (m_pRoot != GetNull()) ? m_pRoot : nullptr; }

    /// Returns true if the tree contains an interval that overlaps the specified interval.
    bool Overlap(const Interval<T, K>* pInterval) const
    {
        return (FindOverlapping(pInterval) != GetNull());
    }

    /// Returns the tree node containing the specified interval - Null node is converted to nullptr.
    IntervalTreeNode<T, K>* FindContainingNode(const Interval<T, K>* pInterval) const;
    /// Returns the tree node containing the specified interval.
    IntervalTreeNode<T, K>* FindContaining(const Interval<T, K>* pInterval) const;

    /// Returns a tree node that overlaps the specified interval - Null node is converted to nullptr.
    IntervalTreeNode<T, K>* FindOverlappingNode(const Interval<T, K>* pInterval) const;
    /// Returns a tree node that overlaps the specified interval.
    IntervalTreeNode<T, K>* FindOverlapping(const Interval<T, K>* pInterval) const;

    /// Inserts the specified interval into the red-black tree.
    IntervalTreeNode<T, K>* Insert(const Interval<T, K>* pInterval);

    /// Deletes the specified node from the tree.
    void Delete(IntervalTreeNode<T, K>* pNode);

    /// Delete a node matching the specified interval from the tree.
    void Delete(const Interval<T, K>* pInterval)
    {
        IntervalTreeNode<T, K>* pNode = Search(pInterval);
        Delete(pNode);
    }

    /// Clears the tree, removing all nodes.
    void Clear()
    {
        Destroy(m_pRoot);
        m_pRoot = GetNull();
        m_count = 0;
    }

    /// Returns a pointer to the tree node corresponding to the specified interval.
    IntervalTreeNode<T, K>* Search(const Interval<T, K>* pInterval) const;

    /// In-order tree traversal.
    ///
    /// @param [in] pfnVisit Function to be called on each node in the tree.
    /// @param [in] pData    Optional additional data to be passed along on each call to pfnVisit.
    void InorderTraverse(void (*pfnVisit)(IntervalTreeNode<T, K>*, void*), void* pData) const
    {
        IntervalTreeNode<T, K>* pRoot = m_pRoot;

        if (pRoot != GetNull())
        {
            Inorder(pRoot, pfnVisit, pData);
        }
    }

    /// Returns the next tree node relative to the specified node.  Returns null if the specified node is the last node
    /// in the tree.
    IntervalTreeNode<T, K>* PrevNode(IntervalTreeNode<T, K>* pNode) const
    {
        IntervalTreeNode<T, K>* pPrev = Prev(pNode);

        return (pPrev == GetNull()) ? nullptr : pPrev;
    }

    /// Returns the previous tree node relative to the specified node.  Returns null if the specified node is the first
    /// node in the tree.
    IntervalTreeNode<T, K>* NextNode(IntervalTreeNode<T, K>* pNode) const
    {
        IntervalTreeNode<T, K>* pNext = Next(pNode);

        return (pNext == GetNull()) ? nullptr : pNext;
    }

    /// Overwrites the specified interval range, adjusting the tree as necessary (potentially inserting a new node and
    /// splitting or combining adjacent nodes).
    void OverwriteInterval(const Interval<T, K>* pInterval);

private:
    // Destroys tree rooted pRoot.
    void Destroy(IntervalTreeNode<T, K>* pRoot)
    {
        if (pRoot != GetNull())
        {
            Destroy(pRoot->pLeftChild);
            Destroy(pRoot->pRightChild);
            PAL_SAFE_DELETE(pRoot, m_pAllocator);
        }
    }

    void Inorder(IntervalTreeNode<T, K>* pRoot, void (*pfnTraverse)(IntervalTreeNode<T, K>*, void*), void* pData) const;
    T CalcHighestValue(IntervalTreeNode<T, K>* pNode) const;

    IntervalTreeNode<T, K>* Prev(IntervalTreeNode<T, K>* pNode) const;
    IntervalTreeNode<T, K>* Next(IntervalTreeNode<T, K>* pNode) const;

    // Find the lower bound node that is overlapping to the interval.  The node itself is overlapping to the interval.
    IntervalTreeNode<T, K>* LowerOverlappingBound(const Interval<T, K>* pInterval) const
    {
        return FindContaining(pInterval->low);
    }

    // Find the upper bound node that is overlapping to the interval.  The node itself is overlapping to the interval.
    IntervalTreeNode<T, K>* UpperOverlappingBound(const Interval<T, K>* pInterval) const
    {
        return FindContaining(pInterval->high);
    }

    IntervalTreeNode<T, K>* FindContaining(T intervalPoint) const;

    void InsertFixup(IntervalTreeNode<T, K>* pX);
    void DeleteFixup(IntervalTreeNode<T, K>* pX);

    void LeftRotate(IntervalTreeNode<T, K>* pA);
    void RightRotate(IntervalTreeNode<T, K>* pA);

    void SwapNodeTopology(IntervalTreeNode<T, K>* pA, IntervalTreeNode<T, K>* pB);
    void ResetNodeTopology(IntervalTreeNode<T, K>* pNode, IntervalTreeNode<T, K>* pRefNode);

    IntervalTreeNode<T, K>        m_null;       // "Null" node/leaf.
    IntervalTreeNode<T, K>*       m_pRoot;      // Tree root node.
    size_t                        m_count;      // Node count in the tree.
    Allocator*const               m_pAllocator; // Allocator for this interval tree.

    PAL_DISALLOW_COPY_AND_ASSIGN(IntervalTree);
};

} // Util
