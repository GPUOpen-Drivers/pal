/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  VamLink.h
* @brief Contains the double link list and stack data structure implementations.
***************************************************************************************************
*/

#ifndef __VAMLINK_H__
#define __VAMLINK_H__

/** Abstract template class for an unordered double-linked list. The class
  * that it manages must have these methods - next, prev, setNext, setPrev.
  */
template< class C > class VamList
{
private:
    /** Pointer to first object in the list.*/
    C              *m_pFirst;

    /** Pointer to the last object in the list.*/
    C              *m_pLast;

    /** Running count of the number of objects in the list.*/
    unsigned int    m_objectCount;

public:
    /** Constructor */
    VamList(void)
    {
        m_pFirst      = NULL;
        m_pLast       = NULL;
        m_objectCount = 0;
    }

    ~VamList( void )
    {
        VAM_ASSERT(m_pFirst == NULL);
    }

    /** Returns the number of objects in the list.*/
    unsigned int numObjects(void) const
    { return m_objectCount; }

    /** Returns true if the list is empty */
    bool isEmpty(void) const
    { return (m_pFirst == NULL); }

    /** Returns the first object in the list */
    C* first(void) const
    { return m_pFirst; }

    /** Returns the last object in the list */
    C* last(void) const
    { return m_pLast; }

    /** Inserts an object first in the list. */
    void insertFirst( C *pC )
    {
        VAM_ASSERT(!contains(pC));
        if ( NULL == m_pFirst )
        {
            m_pFirst = pC;
            m_pLast  = pC;
            pC->setNext( NULL );
            pC->setPrev( NULL );
        }
        else
        {
            pC->setNext( m_pFirst );
            pC->setPrev( NULL );
            m_pFirst->setPrev( pC );
            m_pFirst = pC;
        }
        m_objectCount++;
    }

    /** Inserts an object last in the list. */
    void insertLast( C *pC )
    {
        VAM_ASSERT(!contains(pC));
        if ( NULL == m_pLast )
        {
            m_pFirst = pC;
            m_pLast  = pC;
            pC->setNext( NULL );
            pC->setPrev( NULL );
        }
        else
        {
            pC->setNext( NULL );
            pC->setPrev( m_pLast );
            m_pLast->setNext( pC );
            m_pLast = pC;
        }
        m_objectCount++;
    }

    /** Inserts an object after a specified one.*/
    void insertAfter( C *pC, C *pNewC )
    {
        VAM_ASSERT(!contains(pNewC));
        VAM_ASSERT(contains(pC));
        if ( m_pLast == pC )
        {
            insertLast( pNewC );
        }
        else
        {
            C *pNext = pC->next();
            pC->setNext( pNewC );
            pNewC->setNext( pNext );
            pNext->setPrev( pNewC );
            pNewC->setPrev( pC );
            m_objectCount++;
        }
    }

    /** Inserts an object before a specified one.*/
    void insertBefore( C *pC, C *pNewC )
    {
        VAM_ASSERT(!contains(pNewC));
        VAM_ASSERT(contains(pC));
        if ( m_pFirst == pC )
        {
            insertFirst( pNewC );
        }
        else
        {
            C *pPrev = pC->prev();
            pPrev->setNext( pNewC );
            pNewC->setNext( pC );
            pC->setPrev( pNewC );
            pNewC->setPrev( pPrev );
            m_objectCount++;
        }
    }

    /** Removes a specified object.*/
    void remove( C *pC )
    {
        VAM_ASSERT(contains(pC));
        C *pPrev = pC->prev();
        C *pNext = pC->next();

        if ( NULL == pPrev )
        {
            m_pFirst = pNext;
            if ( NULL == pNext )
            {
                m_pLast = NULL;
            }
            else
            {
                pNext->setPrev( NULL );
            }
        }
        else
        if ( NULL == pNext )
        {
            m_pLast = pPrev;
            if ( NULL == pPrev )
            {
                m_pFirst = NULL;
            }
            else
            {
                pPrev->setNext( NULL );
            }
        }
        else
        {
            pPrev->setNext( pNext );
            pNext->setPrev( pPrev );
        }

        pC->setPrev( NULL );
        pC->setNext( NULL );
        m_objectCount--;
    }

    /** Returns true if a specified object is in the list.*/
    bool contains( C *pC_in ) const
    {
        for( C *pC = m_pFirst;
             pC != NULL;
             pC = pC->next() )
        {
            if ( pC == pC_in )
                return true;
        }
        return false;
    }

    /** Forward Iterator class for Cs */
    class Iterator
    {
    protected:
        /** Current position in the list. */
        C *m_current;

    public:
        /** Constructor that takes a list pointer. */
        Iterator( VamList * pList )
        {
            m_current = pList->first();
        }

        /** Constructor that takes a list reference. */
        Iterator( VamList & list )
        {
            m_current = list.first();
        }

        /** Destructor */
        ~Iterator(void)
        {}

        /** Reference operator */
        C& operator * () const
        { return *m_current; }

        /** Pointer operator */
        C* operator -> () const
        { return m_current; }

        /** Prefix increment operator used to advance to next node in the list. */
        Iterator& operator++()
        {
            m_current = m_current->next();
            return *this;
        }

        /** Postfix increment operator used to advance to next node in the list. */
        Iterator  operator++(int)
        {
            m_current = m_current->next();
            return *this;
        }

        /** cast operator to get current item in list.*/
        operator C*() const
        { return m_current; }

    };

    /** Forward Safe Iterator class for Cs.
      * This is used when the current item can be deleted.
      */
    class SafeIterator
    {
    protected:
        /** Current position in the list. */
        C *m_current;

        /** Next position in the list. */
        C *m_next;

    public:
        /** Constructor that takes a list pointer. */
        SafeIterator( VamList * pList )
        {
            m_current = pList->first();
            m_next = (m_current == NULL) ? NULL : m_current->next();
        }

        /** Constructor that takes a list reference. */
        SafeIterator( VamList & list )
        {
            m_current = list.first();
            m_next = (m_current == NULL) ? NULL : m_current->next();
        }

        /** Destructor */
        ~SafeIterator(void)
        {}

        /** Reference operator */
        C& operator * () const
        { return *m_current; }

        /** Pointer operator */
        C* operator -> () const
        { return m_current; }

        /** Prefix increment operator used to advance to next node in the list. */
        SafeIterator& operator++()
        {
            m_current = m_next;
            m_next = (m_current == NULL) ? NULL : m_current->next();
            return *this;
        }

        /** Postfix increment operator used to advance to next node in the list. */
        SafeIterator  operator++(int)
        {
            m_current = m_next;
            m_next = (m_current == NULL) ? NULL : m_current->next();
            return *this;
        }

        /** cast operator to get current item in list.*/
        operator C*() const
        { return m_current; }

    };

    /** Reverse Iterator class for Cs */
    class ReverseIterator
    {
    protected:
        /** Current position in the list. */
        C *m_current;

    public:
        /** Constructor that takes a list pointer. */
        ReverseIterator( VamList * pList )
        {
            m_current = pList->last();
        }

        /** Constructor that takes a list reference. */
        ReverseIterator( VamList & list )
        {
            m_current = list.last();
        }

        /** Destructor */
        ~ReverseIterator(void)
        {}

        /** Reference operator */
        C& operator * () const
        { return *m_current; }

        /** Pointer operator */
        C* operator -> () const
        { return m_current; }

        /** Prefix decrement operator used to advance to next node in the list. */
        ReverseIterator& operator--()
        {
            m_current = m_current->prev();
            return *this;
        }

        /** Postfix decrement operator used to advance to next node in the list. */
        ReverseIterator  operator--(int)
        {
            m_current = m_current->prev();
            return *this;
        }

        /** cast operator to get current item in list.*/
        operator C*() const
        { return m_current; }

    };

    /** Reverse Safe Iterator class for Cs */
    class SafeReverseIterator
    {
    protected:
        /** Current position in the list. */
        C *m_current;

        /** Next position in the list - in reverse order.*/
        C *m_next;

    public:
        /** Constructor that takes a list pointer. */
        SafeReverseIterator( VamList * pList )
        {
            m_current = pList->last();
            m_next = (m_current == NULL) ? NULL : m_current->prev();
        }

        /** Constructor that takes a list reference. */
        SafeReverseIterator( VamList & list )
        {
            m_current = list.last();
            m_next = (m_current == NULL) ? NULL : m_current->prev();
        }

        /** Destructor */
        ~SafeReverseIterator(void)
        {}

        /** Reference operator */
        C& operator * () const
        { return *m_current; }

        /** Pointer operator */
        C* operator -> () const
        { return m_current; }

        /** Prefix decrement operator used to advance to next node in the list. */
        SafeReverseIterator& operator--()
        {
            m_current = m_next;
            m_next = (m_current == NULL) ? NULL : m_current->prev();
            return *this;
        }

        /** Postfix decrement operator used to advance to next node in the list. */
        SafeReverseIterator  operator--(int)
        {
            m_current = m_next;
            m_next = (m_current == NULL) ? NULL : m_current->prev();
            return *this;
        }

        /** cast operator to get current item in list.*/
        operator C*() const
        { return m_current; }

    };
};

/** Abstract template class for the linkage that is used in linked lists.
  * This class is used by having the contained class C inherit from the
  * template.
  */
template< class C > class VamLink
{
private:
    /** Pointer to the next object in the list.*/
    C   *m_pNext;

    /** Pointer to the previous object in the list.*/
    C   *m_pPrev;

public:
    /** Constructor */
    VamLink(void)
    {
        m_pNext = NULL;
        m_pPrev = NULL;
    };

    /** Returns the next object in the list. */
    C* next(void) const
    { return m_pNext; };

    /** Returns the previous object in the list. */
    C* prev(void) const
    { return m_pPrev; };

    /** Sets the next pointer. */
    void setNext( C *pObject )
    { m_pNext = pObject; };

    /** Sets the previous pointer. */
    void setPrev( C *pObject )
    { m_pPrev = pObject; };
};

#endif // __VAMLINK_H__
