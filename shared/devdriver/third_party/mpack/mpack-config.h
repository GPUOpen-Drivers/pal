/* Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved. */

#ifndef MPACK_CONFIG_H
#define MPACK_CONFIG_H 1

#include <wdm.h>

#define DD_MPACK_LOG_TAG 'DDMK'

static void* AllocMemoryWithTag( size_t Size, POOL_TYPE PoolType, unsigned long Tag )
{
    void* pv = NULL;

    // According to MSDN: Do not set NumberOfBytes = 0. Avoid zero-length allocations because they waste pool header space and, 
    // in many cases, indicate a potential validation issue in the calling code. For this reason, Driver Verifier flags such allocations as possible errors
    if (Size != 0)
    {
        pv = ExAllocatePoolZero(PoolType, Size, Tag);
        ASSERT( NULL != pv );

        if (NULL != pv)
        {
            memset(pv, 0, Size);
        }
    }

    return pv;
}

static void FreeMemoryWithTag(void* pMem, unsigned long Tag)
{
    if (pMem != NULL)
    {
        ExFreePoolWithTag(pMem, Tag);
    }
}

// Comment from Kernel code:
// From Win8, MS has introduced non paged NX (Non-execute) pool.
// KMD should always use this pool.  For allocate memory, if non paged memory
// is requested, then NX pool would be used.
#define MPACK_MALLOC(s) AllocMemoryWithTag(s, NonPagedPoolNxCacheAligned, DD_MPACK_LOG_TAG)

#define MPACK_FREE(s) FreeMemoryWithTag(s, DD_MPACK_LOG_TAG)


#endif // MPACK_CONFIG_H
