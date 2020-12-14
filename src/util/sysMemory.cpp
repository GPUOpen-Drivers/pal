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

#include "palAssert.h"
#include "palSysMemory.h"

// =====================================================================================================================
// PAL-internal placement new override.  The DummyEnum is used to ensure there won't be a conflict if a client tries to
// override global placement new.  Must be in the global namespace, not Util.
void* PAL_CDECL operator new(
    size_t        size,     // Size of the memory allocation.
    void*         pObjMem,  // Memory where the object will be constructed.
    Util::Dummy   dummy     // Unused.
    ) noexcept
{
    PAL_ALERT(pObjMem == nullptr);
    return pObjMem;
}

// =====================================================================================================================
// Silences compiler warnings about not have a matching delete for the placement new override above.  Will never be
// called.  Must be in the global namespace, not Util.
void PAL_CDECL operator delete(
    void*        pObj,
    void*        pObjMem,
    Util::Dummy  dummy
    ) noexcept
{
    PAL_NEVER_CALLED();
}

namespace Util
{
// =====================================================================================================================
template<size_t size>
void* PAL_CDECL FastMemCpySmall(
    void*       pDst,
    const void* pSrc,
    size_t      count)
{
    return memcpy(pDst, pSrc, size);
}

FastMemCpySmallFunc FastMemCpySmallFuncTable[] =
{
    FastMemCpySmall<0>,
    FastMemCpySmall<1>,
    FastMemCpySmall<2>,
    FastMemCpySmall<3>,
    FastMemCpySmall<4>,
    FastMemCpySmall<5>,
    FastMemCpySmall<6>,
    FastMemCpySmall<7>,
    FastMemCpySmall<8>,
    FastMemCpySmall<9>,
    FastMemCpySmall<10>,
    FastMemCpySmall<11>,
    FastMemCpySmall<12>,
    FastMemCpySmall<13>,
    FastMemCpySmall<14>,
    FastMemCpySmall<15>,
    FastMemCpySmall<16>,
    FastMemCpySmall<17>,
    FastMemCpySmall<18>,
    FastMemCpySmall<19>,
    FastMemCpySmall<20>,
    FastMemCpySmall<21>,
    FastMemCpySmall<22>,
    FastMemCpySmall<23>,
    FastMemCpySmall<24>,
    FastMemCpySmall<25>,
    FastMemCpySmall<26>,
    FastMemCpySmall<27>,
    FastMemCpySmall<28>,
    FastMemCpySmall<29>,
    FastMemCpySmall<30>,
    FastMemCpySmall<31>,
    FastMemCpySmall<32>,
    FastMemCpySmall<33>,
    FastMemCpySmall<34>,
    FastMemCpySmall<35>,
    FastMemCpySmall<36>,
    FastMemCpySmall<37>,
    FastMemCpySmall<38>,
    FastMemCpySmall<39>,
    FastMemCpySmall<40>,
    FastMemCpySmall<41>,
    FastMemCpySmall<42>,
    FastMemCpySmall<43>,
    FastMemCpySmall<44>,
    FastMemCpySmall<45>,
    FastMemCpySmall<46>,
    FastMemCpySmall<47>,
    FastMemCpySmall<48>,
    FastMemCpySmall<49>,
    FastMemCpySmall<50>,
    FastMemCpySmall<51>,
    FastMemCpySmall<52>,
    FastMemCpySmall<53>,
    FastMemCpySmall<54>,
    FastMemCpySmall<55>,
    FastMemCpySmall<56>,
    FastMemCpySmall<57>,
    FastMemCpySmall<58>,
    FastMemCpySmall<59>,
    FastMemCpySmall<60>,
    FastMemCpySmall<61>,
    FastMemCpySmall<62>,
    FastMemCpySmall<63>,
    FastMemCpySmall<64>,

    memcpy,
};

} // Util
