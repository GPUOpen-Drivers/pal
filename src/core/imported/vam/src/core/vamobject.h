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
* @file  vamobject.h
* @brief Contains the VamObject base class definition.
***************************************************************************************************
*/

#ifndef __VAMOBJECT_H__
#define __VAMOBJECT_H__

#include "vaminterface.h"

/**
***************************************************************************************************
* @brief This is the base class for all VAM class objects.
***************************************************************************************************
*/
class VamObject
{
public:
    VamObject();
    VamObject(VAM_CLIENT_HANDLE hClient);
    virtual ~VamObject();

    VOID* operator new(size_t size, VAM_CLIENT_HANDLE hClient);
    VOID  operator delete(VOID* pObj, VAM_CLIENT_HANDLE hClient);
    VOID  operator delete(VOID* pObj);

    static VOID SetupSysMemFuncs( VAM_ALLOCSYSMEM allocSysMem,
                                  VAM_FREESYSMEM  freeSysMem );

protected:
    VAM_CLIENT_HANDLE       m_hClient;

    static VAM_ALLOCSYSMEM  m_allocSysMem;
    static VAM_FREESYSMEM   m_freeSysMem;

private:
    // Disallow the copy constructor
    VamObject(const VamObject& a);

    // Disallow the assignment operator
    VamObject& operator=(const VamObject& a);
};

#endif
