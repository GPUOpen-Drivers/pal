/*
 *******************************************************************************
 *
 * Copyright (c) 2009-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
***************************************************************************************************
* @file  vamobject.cpp
* @brief Contains the VamObject base class implementation.
***************************************************************************************************
*/

#include "vamobject.h"

VAM_ALLOCSYSMEM VamObject::m_allocSysMem  = NULL;
VAM_FREESYSMEM  VamObject::m_freeSysMem   = NULL;

/**
***************************************************************************************************
*   VamObject::VamObject
*
*   @brief
*       Constructor for the VamObject class.
***************************************************************************************************
*/
VamObject::VamObject() :
    m_hClient(NULL)
{
}

/**
***************************************************************************************************
*   VamObject::VamObject
*
*   @brief
*       Constructor for the VamObject class with client handle as parameter.
***************************************************************************************************
*/
VamObject::VamObject(VAM_CLIENT_HANDLE hClient) :
    m_hClient(hClient)
{
}

/**
***************************************************************************************************
*   VamObject::~VamObject
*
*   @brief
*       Destructor for the VamObject class.
***************************************************************************************************
*/
VamObject::~VamObject()
{
}

/**
***************************************************************************************************
*   VamObject::operator new
*
*   @brief
*       Allocates memory needed for VamObject object.
*
*   @return
*       Returns NULL if unsuccessful.
***************************************************************************************************
*/
VOID* VamObject::operator new(
    size_t              objSize,    ///< [in] Size to allocate
    VAM_CLIENT_HANDLE   hClient)    ///< [in] Client handle
{
    VOID* pObjMem = NULL;

    if (m_allocSysMem != NULL)
    {
        pObjMem = m_allocSysMem(hClient, static_cast<UINT>(objSize));
    }

    return pObjMem;
}

/**
***************************************************************************************************
*   VamObject::operator delete
*
*   @brief
*       Frees VamObject object memory with client handle as parameter.
***************************************************************************************************
*/
VOID VamObject::operator delete(
    VOID*               pObjMem,    ///< [in] User virtual address to free.
    VAM_CLIENT_HANDLE   hClient)    ///< [in] Client handle
{
    if (m_freeSysMem != NULL)
    {
        if (pObjMem != NULL)
        {
            m_freeSysMem(hClient, pObjMem);
        }
    }
}

/**
***************************************************************************************************
*   VamObject::operator delete
*
*   @brief
*       Frees VamObject object memory.
***************************************************************************************************
*/
VOID VamObject::operator delete(
    VOID*               pObjMem)    ///< [in] User virtual address to free.
{
    if (m_freeSysMem != NULL)
    {
        VamObject* pObj = static_cast<VamObject*>(pObjMem);

        if (pObjMem != NULL)
        {
            m_freeSysMem(pObj->m_hClient, pObjMem);
        }
    }
}

/**
***************************************************************************************************
*   VamObject::SetupSysMemFuncs
*
*   @brief
*       Sets up static function pointers for alloc and free system memory that is used
*       by the overloaded new and delete operator.
***************************************************************************************************
*/
VOID VamObject::SetupSysMemFuncs(
    VAM_ALLOCSYSMEM  allocSysMem,    ///< AllocSysMem function pointer
    VAM_FREESYSMEM   freeSysMem )    ///< FreeSysMem function pointer
{
    m_allocSysMem = allocSysMem;
    m_freeSysMem  = freeSysMem;
}
