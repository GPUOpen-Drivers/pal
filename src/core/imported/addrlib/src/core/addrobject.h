/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2007-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
****************************************************************************************************
* @file  addrobject.h
* @brief Contains the Object base class definition.
****************************************************************************************************
*/

#ifndef __ADDR_OBJECT_H__
#define __ADDR_OBJECT_H__

#include "addrtypes.h"
#include "addrcommon.h"

namespace Addr
{

/**
****************************************************************************************************
* @brief This structure contains client specific data
****************************************************************************************************
*/
struct Client
{
    ADDR_CLIENT_HANDLE  handle;
    ADDR_CALLBACKS      callbacks;
};
/**
****************************************************************************************************
* @brief This class is the base class for all ADDR class objects.
****************************************************************************************************
*/
class Object
{
public:
    Object();
    Object(const Client* pClient);
    virtual ~Object();

    VOID* operator new(size_t size, VOID* pMem) noexcept;
    VOID  operator delete(VOID* pObj);
    /// Microsoft compiler requires a matching delete implementation, which seems to be called when
    /// bad_alloc is thrown. But currently C++ exception isn't allowed so a dummy implementation is
    /// added to eliminate the warning.
    VOID  operator delete(VOID* pObj, VOID* pMem) { ADDR_ASSERT_ALWAYS(); }

    VOID* Alloc(size_t size) const;
    VOID  Free(VOID* pObj) const;

#if DEBUG
    // This function should be called on every addrlib entrypoint (usually implicit by GetLib())
    VOID  SetDebugPrinters() const { ApplyDebugPrinters(m_client.callbacks.debugPrint, m_client.handle); }
#endif

    const Client* GetClient() const {return &m_client;}

protected:
    Client m_client;

    static VOID* ClientAlloc(size_t size, const Client* pClient);
    static VOID  ClientFree(VOID* pObj, const Client* pClient);

private:
    // disallow the copy constructor
    Object(const Object& a);

    // disallow the assignment operator
    Object& operator=(const Object& a);
};

} // Addr
#endif
