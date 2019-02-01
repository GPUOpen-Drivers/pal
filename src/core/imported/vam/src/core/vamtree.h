/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
***************************************************************************************************
* @file  vamtree.h
* @brief Contains the red blak tree class implementations.
***************************************************************************************************
*/

#ifndef __VAMTREE_H__
#define __VAMTREE_H__

/// Node color - black or red.
enum VamNodeColor
{
    Black = 0,
    Red
};

/// Describes a node in a tree.
template< class C > struct VamTreeNode
{
private:
    /** Pointer to the left child object in the tree.*/
    C           *m_pLeftChild;

    /** Pointer to the right child in the tree.*/
    C           *m_pRightChild;

    /** Pointer to the parent in the tree.*/
    C           *m_pParent;

    /** Color of the node.*/
    VamNodeColor m_color;
public:
    /** Constructor */
    VamTreeNode(void)
    {
        m_pLeftChild = NULL;
        m_pRightChild = NULL;
        m_pParent = NULL;
        m_color = VamNodeColor::Black;
    };

    /** Returns the left child in the tree.*/
    C*& leftChild(void)
    { return m_pLeftChild; };

    /** Returns the right child in the tree.*/
    C*& rightChild(void)
    { return m_pRightChild; };

    /** Returns the parent in the tree.*/
    C*& parent(void)
    { return m_pParent; };

    /** Returns node color.*/
    VamNodeColor& color(void)
    { return m_color; };
};

template< class C, typename T > class VamTree
{
public:
    /// Constructor.
    VamTree() : m_null(nullptr), m_pRoot(&m_null), m_count(0) { }
    ~VamTree() { m_pRoot = getNull(); m_count = 0; }

    /// Returns the number of nodes in the tree.
    unsigned int numObjects() const { return m_count; }

    /// Find the front and back nodes whose range contain value.
    void findContainingNodes(T value, C** ppFront, C** ppBack)
    {
        VAM_ASSERT(m_count > 0);

        C* pX = m_pRoot;
        C* pY = nullptr;

        while (pX != getNull())
        {
            pY = pX;

            if (pX->value() > value)
            {
                pX = pX->leftChild();
            }
            else
            {
                pX = pX->rightChild();
            }
        }

        VAM_ASSERT(pY != nullptr);

        if (pY->value() > value)
        {
            *ppFront = pY->prev();
            *ppBack  = pY;
        }
        else
        {
            *ppFront = pY;
            *ppBack  = pY->next();
        }
    }

    /// Inserts the specified value into the red-black tree.
    void insert(C* pNode)
    {
        pNode->leftChild() = getNull();
        pNode->rightChild() = getNull();
        pNode->parent() = getNull();
        pNode->color() = VamNodeColor::Red;

        // Inserts the new node into the tree as a binary search tree.
        C* pX = m_pRoot;
        C* pY = getNull();

        while (pX != getNull())
        {
            pY = pX;

            if (pX->value() > pNode->value())
            {
                pX = pX->leftChild();
            }
            else
            {
                pX = pX->rightChild();
            }
        }

        if (pY == getNull())
        {
            m_pRoot = pNode;
        }
        else // Inserts pNode as child of pY.
        {
            if (pY->value() > pNode->value())
            {
                pY->leftChild() = pNode;
            }
            else
            {
                pY->rightChild() = pNode;
            }
            pNode->parent() = pY;
        }

        // Fix possible violation of property 3.
        insertFixup(pNode);

        m_count++;
    }
    /// Deletes the specified node from the tree.
    void remove(C* pNode)
    {
        if (pNode != getNull())
        {
            if (pNode->leftChild() != getNull() && pNode->rightChild() != getNull())
            {
                C* pNext = Next(pNode);

                // Swap the topology between pNode and pNext, includes color and topology, which keeps the red-balck
                // property.  Thus, pNode will be really deleted.
                SwapNodeTopology(pNode, pNext);
            }

            // Delete pNode.
            C* pTemp = (pNode->leftChild() != getNull()) ? pNode->leftChild() : pNode->rightChild();
            pTemp->parent() = pNode->parent();
            // NOTE: pTemp might be "NULL" here and we still need to adjust its parent.
            if (pNode == m_pRoot)
            {
                m_pRoot = pTemp;
            }
            else
            {
                if (pNode == pNode->parent()->leftChild())
                {
                    pNode->parent()->leftChild() = pTemp;
                }
                else
                {
                    pNode->parent()->rightChild() = pTemp;
                }
            }

            if (pNode->color() == VamNodeColor::Black)
            {
                removeFixup(pTemp);
            }

            m_count--;
        }
    }

private:
    /// Returns a pointer to the null (leaf) node - null node's parent info might be change in deletion.
    C* getNull() const { return const_cast<C*>(&m_null); }

    C* Prev(C* pNode) const
    {
        C* pPrev = pNode;

        if (pNode != getNull())
        {
            if (pNode->leftChild() != getNull())
            {
                pPrev = pNode->leftChild();

                while (pPrev->rightChild() != getNull())
                {
                    pPrev = pPrev->rightChild();
                }
            }
            else
            {
                pPrev = pNode->parent();

                while ((pPrev != getNull()) && (pNode != pPrev->rightChild()))
                {
                    pNode = pPrev;
                    pPrev = pPrev->parent();
                }
            }
        }

        return pPrev;
    }

    C* Next(C* pNode) const
    {
        C* pNext = pNode;

        if (pNode != getNull())
        {
            // If pNode has right child, return the minimum node in the subtree rooted at its right child.
            if (pNode->rightChild() != getNull())
            {
                pNext = pNode->rightChild();

                while (pNext->leftChild() != getNull())
                {
                    pNext = pNext->leftChild();
                }
            }
            else // Otherwise, climb up the tree
            {
                pNext = pNode->parent();

                while ((pNext != getNull()) && (pNode != pNext->leftChild()))
                {
                    pNode = pNext;
                    pNext = pNext->parent();
                }
            }
        }

        return pNext;
    }

    void insertFixup(C* pX)
    {
        C* pY;

        // If pX is the root, its parent (NULL) must be black
        while (pX->parent()->color() == VamNodeColor::Red)
        {
            if (pX->parent() == pX->parent()->parent()->leftChild())
            {
                pY = pX->parent()->parent()->rightChild();

                if (pY->color() == VamNodeColor::Red)
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
                    pX->parent()->color() = VamNodeColor::Black;
                    pY->color() = VamNodeColor::Black;
                    pX->parent()->parent()->color() = VamNodeColor::Red;

                    pX = pX->parent()->parent();

                    // The new pX may still violate property 3.
                }
                else
                {
                    if (pX == pX->parent()->rightChild())
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
                        LeftRotate(pX->parent());
                        pX = pX->leftChild();
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
                    RightRotate(pX->parent()->parent());
                    pX->parent()->color() = VamNodeColor::Black;
                    pX->parent()->rightChild()->color() = VamNodeColor::Red;
                }
            }
            else
            {
                // Switch the places of left and right in if clause.
                pY = pX->parent()->parent()->leftChild();

                if (pY->color() == VamNodeColor::Red)
                {
                    // <Case 1>
                    pX->parent()->color() = VamNodeColor::Black;
                    pY->color() = VamNodeColor::Black;
                    pX->parent()->parent()->color() = VamNodeColor::Red;
                    pX = pX->parent()->parent();
                }
                else
                {
                    if (pX == pX->parent()->leftChild())
                    {
                        // <Case 2>
                        RightRotate(pX->parent());
                        pX = pX->rightChild();
                    }

                    // <Case 3>
                    LeftRotate(pX->parent()->parent());
                    pX->parent()->color() = VamNodeColor::Black;
                    pX->parent()->leftChild()->color() = VamNodeColor::Red;
                }
            }
        }

        m_pRoot->color() = VamNodeColor::Black;
    }

    void removeFixup(C* pX)
    {
        while ((pX != m_pRoot) && (pX->color() == VamNodeColor::Black))
        {
            if (pX == pX->parent()->leftChild())
            {
                C* pSibling = pX->parent()->rightChild();
                if (pSibling->color() == VamNodeColor::Red)
                {
                    pSibling->color() = VamNodeColor::Black;
                    pX->parent()->color() = VamNodeColor::Red;
                    LeftRotate(pX->parent());
                    pSibling = pX->parent()->rightChild();
                }

                if ((pSibling->leftChild()->color() == VamNodeColor::Black) &&
                    (pSibling->rightChild()->color() == VamNodeColor::Black))
                {
                    pSibling->color() = VamNodeColor::Red;
                    pX = pX->parent();
                }
                else
                {
                    if (pSibling->rightChild()->color() == VamNodeColor::Black)
                    {
                        pSibling->leftChild()->color() = VamNodeColor::Black;
                        pSibling->color() = VamNodeColor::Red;
                        RightRotate(pSibling);
                        pSibling = pX->parent()->rightChild();
                    }
                    pSibling->color() = pX->parent()->color();
                    pX->parent()->color() = VamNodeColor::Black;
                    pSibling->rightChild()->color() = VamNodeColor::Black;
                    LeftRotate(pX->parent());
                    pX = m_pRoot;
                }
            }
            else // Switch the left and right in the last if clause.
            {
                C* pSibling = pX->parent()->leftChild();
                if (pSibling->color() == VamNodeColor::Red)
                {
                    pSibling->color() = VamNodeColor::Black;
                    pX->parent()->color() = VamNodeColor::Red;
                    RightRotate(pX->parent());
                    pSibling = pX->parent()->leftChild();
                }

                if ((pSibling->leftChild()->color() == VamNodeColor::Black) &&
                    (pSibling->rightChild()->color() == VamNodeColor::Black))
                {
                    pSibling->color() = VamNodeColor::Red;
                    pX = pX->parent();
                }
                else
                {
                    if (pSibling->leftChild()->color() == VamNodeColor::Black)
                    {
                        pSibling->rightChild()->color() = VamNodeColor::Black;
                        pSibling->color() = VamNodeColor::Red;
                        LeftRotate(pSibling);
                        pSibling = pX->parent()->leftChild();
                    }
                    pSibling->color() = pX->parent()->color();
                    pX->parent()->color() = VamNodeColor::Black;
                    pSibling->leftChild()->color() = VamNodeColor::Black;
                    RightRotate(pX->parent());
                    pX = m_pRoot;
                }
            }
        }

        pX->color() = VamNodeColor::Black;
    }

    void LeftRotate(C* pA)
    {
        /*
         *        |                  |
         *        A                  B
         *       / \                / \
         *      *   B    ==>       A   *
         *         / \            / \
         *        C   *          *   C
         */

        // A and  A->rightChild() should not be NULL.
        C* pB = pA->rightChild();
        C* pC = pB->leftChild(); // C could be NULL.

        if (pA->parent() != getNull()) // A is not the root.
        {
            if (pA == pA->parent()->leftChild())
            {
                pA->parent()->leftChild() = pB;
            }
            else
            {
                pA->parent()->rightChild() = pB;
            }
        }
        else
        {
            m_pRoot = pB;
        }

        pB->parent() = pA->parent();
        pB->leftChild() = pA;
        pA->parent() = pB;
        pA->rightChild() = pC;
        if (pC != getNull())
        {
            pC->parent() = pA;
        }
    }
    void RightRotate(C* pA)
    {
        /*
         *        |                  |
         *        A                  B
         *       / \                / \
         *      B   *    ==>       *   A
         *     / \                    / \
         *    *   C                  C   *
         */

        // A and  A->leftChild() should not be NULL.
        C* pB = pA->leftChild();
        C* pC = pB->rightChild(); // C could be NULL.

        if (pA->parent() != getNull())
        {
            if (pA == pA->parent()->leftChild())
            {
                pA->parent()->leftChild() = pB;
            }
            else
            {
                pA->parent()->rightChild() = pB;
            }
        }
        else
        {
            m_pRoot = pB;
        }

        pB->parent() = pA->parent();
        pB->rightChild() = pA;
        pA->parent() = pB;
        pA->leftChild() = pC;
        if (pC != getNull())
        {
            pC->parent() = pA;
        }
    }
    void SwapNodeTopology(C* pA, C* pB)
    {
        // Swap data of Node A and Node B.
        C tempNode(nullptr);
        tempNode.leftChild() = pA->leftChild();
        tempNode.rightChild() = pA->rightChild();
        tempNode.parent() = pA->parent();
        tempNode.color() = pA->color();

        pA->leftChild() = pB->leftChild();
        pA->rightChild() = pB->rightChild();
        pA->parent() = pB->parent();
        pA->color() = pB->color();

        pB->leftChild() = tempNode.leftChild();
        pB->rightChild() = tempNode.rightChild();
        pB->parent() = tempNode.parent();
        pB->color() = tempNode.color();

        // Re-setup the RBTree topology of Node A and Node B.
        ResetNodeTopology(pA, pB);
        ResetNodeTopology(pB, pA);
    }

    void ResetNodeTopology(C* pNode, C* pRefNode)
    {
        // If the reference node is the root node, set the root node to pNode.
        if (m_pRoot == pRefNode)
        {
            m_pRoot = pNode;
        }

        // Set up the information of the new left child of pNode.
        if (pNode->leftChild() != getNull())
        {
            if (pNode->leftChild() == pNode)
            {
                // Deal with the case that pNode and pRefNode are adjacent.
                pNode->leftChild() = pRefNode;
            }
            else
            {
                pNode->leftChild()->parent() = pNode;
            }
        }

        // Set up the information of the new right child of pNode.
        if (pNode->rightChild() != getNull())
        {
            if (pNode->rightChild() == pNode)
            {
                // Deal with the case that pNode and pRefNode are adjacent.
                pNode->rightChild() = pRefNode;
            }
            else
            {
                pNode->rightChild()->parent() = pNode;
            }
        }

        // Set up the information of the new parent of pNode.
        if (pNode->parent() != getNull())
        {
            if (pNode->parent() == pNode)
            {
                pNode->parent() = pRefNode;
            }
            else
            {
                if (pRefNode == pNode->parent()->leftChild())
                {
                    // Deal with the case that pNode and pRefNode are adjacent.
                    pNode->parent()->leftChild() = pNode;
                }
                else
                {
                    pNode->parent()->rightChild() = pNode;
                }
            }
        }
    }

    C               m_null;       // "Null" node/leaf.
    C*              m_pRoot;      // Tree root node.
    unsigned int    m_count;      // Node count in the tree.
};

#endif //__VAMTREE_H__
