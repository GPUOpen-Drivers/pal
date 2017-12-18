/*
 *******************************************************************************
 *
 * Copyright (c) 2012-2017 Advanced Micro Devices, Inc. All rights reserved.
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
* @file  vamsectionalloc.h
* @brief Contains definitions of sections specific to section allocations.
***************************************************************************************************
*/

#ifndef __VAMSECTIONALLOC_H__
#define __VAMSECTIONALLOC_H__

#include "vamcommon.h"
#include "vamrange.h"

class VamSection : public VamObject, public VamLink<VamSection>
{
public:
    // methods
    VamSection() {}
    VamSection(VAM_CLIENT_HANDLE            hClient,
               VamDevice*                   pVamDevice,
               VAM_CLIENT_OBJECT            clientObject,
               VAM_CREATESECTION_FLAGS      flags);

   ~VamSection();

    VamVARange& VASpace(void)
    { return m_VASpace; }

private:
    // data members
    VAM_CLIENT_OBJECT           m_clientObject;         // client's opaque object
    VAM_CREATESECTION_FLAGS     m_flags;                // section's creation flags
    VamVARange                  m_VASpace;              // section's VA space status
    VamDevice*                  m_pVamDevice;           // pointer to the device object
};

typedef VamList<VamSection> SectionList;

#endif  // __VAMSECTIONALLOC_H__
