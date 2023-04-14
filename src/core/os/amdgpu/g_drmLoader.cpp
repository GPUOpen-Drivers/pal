/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// Modify the procAnalysis.py and drmLoader.py in the tools/generate directory OR drmLoader.proc instead
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/g_drmLoader.h"
#include "palAssert.h"
#include "palSysUtil.h"

#include <dlfcn.h>
#include <time.h>
#include <string.h>

using namespace Util;

namespace Pal
{
namespace Amdgpu
{
// =====================================================================================================================
#if defined(PAL_DEBUG_PRINTS)
void DrmLoaderFuncsProxy::Init(const char* pLogPath)
{
    char file[128] = {0};
    Util::Snprintf(file, sizeof(file), "%s/DrmLoaderTimeLogger.csv", pLogPath);
    m_timeLogger.Open(file, FileAccessMode::FileAccessWrite);
    Util::Snprintf(file, sizeof(file), "%s/DrmLoaderParamLogger.trace", pLogPath);
    m_paramLogger.Open(file, FileAccessMode::FileAccessWrite);
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryHwIpInfo(
    amdgpu_device_handle           hDevice,
    uint32                         type,
    uint32                         ipInstance,
    struct drm_amdgpu_info_hw_ip*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryHwIpInfo(hDevice,
                                                 type,
                                                 ipInstance,
                                                 pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryHwIpInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryHwIpInfo(%p, %x, %x, %p)\n",
        hDevice,
        type,
        ipInstance,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoVaOp(
    amdgpu_bo_handle  hBuffer,
    uint64            offset,
    uint64            size,
    uint64            address,
    uint64            flags,
    uint32            ops
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoVaOp(hBuffer,
                                          offset,
                                          size,
                                          address,
                                          flags,
                                          ops);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoVaOp,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoVaOp(%p, %lx, %lx, %lx, %lx, %x)\n",
        hBuffer,
        offset,
        size,
        address,
        flags,
        ops);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoVaOpRaw(
    amdgpu_device_handle  hDevice,
    amdgpu_bo_handle      hBuffer,
    uint64                offset,
    uint64                size,
    uint64                address,
    uint64                flags,
    uint32                ops
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoVaOpRaw(hDevice,
                                             hBuffer,
                                             offset,
                                             size,
                                             address,
                                             flags,
                                             ops);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoVaOpRaw,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoVaOpRaw(%p, %p, %lx, %lx, %lx, %lx, %x)\n",
        hDevice,
        hBuffer,
        offset,
        size,
        address,
        flags,
        ops);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSemaphore(
    amdgpu_semaphore_handle*  pSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCreateSemaphore(pSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCreateSemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCreateSemaphore(%p)\n",
        pSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSignalSemaphore(
    amdgpu_context_handle    hContext,
    uint32                   ipType,
    uint32                   ipInstance,
    uint32                   ring,
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSignalSemaphore(hContext,
                                                     ipType,
                                                     ipInstance,
                                                     ring,
                                                     hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSignalSemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSignalSemaphore(%p, %x, %x, %x, %p)\n",
        hContext,
        ipType,
        ipInstance,
        ring,
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsWaitSemaphore(
    amdgpu_context_handle    hConext,
    uint32                   ipType,
    uint32                   ipInstance,
    uint32                   ring,
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsWaitSemaphore(hConext,
                                                   ipType,
                                                   ipInstance,
                                                   ring,
                                                   hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsWaitSemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsWaitSemaphore(%p, %x, %x, %x, %p)\n",
        hConext,
        ipType,
        ipInstance,
        ring,
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySemaphore(
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsDestroySemaphore(hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsDestroySemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsDestroySemaphore(%p)\n",
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle*    pSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCreateSem(hDevice,
                                               pSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCreateSem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCreateSem(%p, %p)\n",
        hDevice,
        pSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSignalSem(
    amdgpu_device_handle   hDevice,
    amdgpu_context_handle  hContext,
    uint32                 ipType,
    uint32                 ipInstance,
    uint32                 ring,
    amdgpu_sem_handle      hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSignalSem(hDevice,
                                               hContext,
                                               ipType,
                                               ipInstance,
                                               ring,
                                               hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSignalSem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSignalSem(%p, %p, %x, %x, %x, %x)\n",
        hDevice,
        hContext,
        ipType,
        ipInstance,
        ring,
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsWaitSem(
    amdgpu_device_handle   hDevice,
    amdgpu_context_handle  hContext,
    uint32                 ipType,
    uint32                 ipInstance,
    uint32                 ring,
    amdgpu_sem_handle      hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsWaitSem(hDevice,
                                             hContext,
                                             ipType,
                                             ipInstance,
                                             ring,
                                             hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsWaitSem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsWaitSem(%p, %p, %x, %x, %x, %x)\n",
        hDevice,
        hContext,
        ipType,
        ipInstance,
        ring,
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsExportSem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle     hSemaphore,
    int32*                pSharedFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsExportSem(hDevice,
                                               hSemaphore,
                                               pSharedFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsExportSem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsExportSem(%p, %x, %p)\n",
        hDevice,
        hSemaphore,
        pSharedFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsImportSem(
    amdgpu_device_handle  hDevice,
    int32                 fd,
    amdgpu_sem_handle*    pSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsImportSem(hDevice,
                                               fd,
                                               pSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsImportSem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsImportSem(%p, %x, %p)\n",
        hDevice,
        fd,
        pSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle     hSemaphore
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsDestroySem(hDevice,
                                                hSemaphore);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsDestroySem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsDestroySem(%p, %x)\n",
        hDevice,
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
const char* DrmLoaderFuncsProxy::pfnAmdgpuGetMarketingName(
    amdgpu_device_handle  hDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    const char* pRet = m_pFuncs->pfnAmdgpuGetMarketingName(hDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuGetMarketingName,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuGetMarketingName(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuVaRangeFree(
    amdgpu_va_handle  hVaRange
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuVaRangeFree(hVaRange);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVaRangeFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVaRangeFree(%p)\n",
        hVaRange);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuVaRangeQuery(
    amdgpu_device_handle      hDevice,
    enum amdgpu_gpu_va_range  type,
    uint64*                   pStart,
    uint64*                   pEnd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuVaRangeQuery(hDevice,
                                                type,
                                                pStart,
                                                pEnd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVaRangeQuery,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVaRangeQuery(%p, %x, %p, %p)\n",
        hDevice,
        type,
        pStart,
        pEnd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuVaRangeAlloc(
    amdgpu_device_handle      hDevice,
    enum amdgpu_gpu_va_range  vaRangeType,
    uint64                    size,
    uint64                    vaBaseAlignment,
    uint64                    vaBaseRequired,
    uint64*                   pVaAllocated,
    amdgpu_va_handle*         pVaRange,
    uint64                    flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuVaRangeAlloc(hDevice,
                                                vaRangeType,
                                                size,
                                                vaBaseAlignment,
                                                vaBaseRequired,
                                                pVaAllocated,
                                                pVaRange,
                                                flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVaRangeAlloc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVaRangeAlloc(%p, %x, %lx, %lx, %lx, %p, %p, %lx)\n",
        hDevice,
        vaRangeType,
        size,
        vaBaseAlignment,
        vaBaseRequired,
        pVaAllocated,
        pVaRange,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuVmReserveVmid(
    amdgpu_device_handle  hDevice,
    uint32                flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuVmReserveVmid(hDevice,
                                                 flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVmReserveVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVmReserveVmid(%p, %x)\n",
        hDevice,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuVmUnreserveVmid(
    amdgpu_device_handle  hDevice,
    uint32                flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuVmUnreserveVmid(hDevice,
                                                   flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVmUnreserveVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVmUnreserveVmid(%p, %x)\n",
        hDevice,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuReadMmRegisters(
    amdgpu_device_handle  hDevice,
    uint32                dwordOffset,
    uint32                count,
    uint32                instance,
    uint32                flags,
    uint32*               pValues
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuReadMmRegisters(hDevice,
                                                   dwordOffset,
                                                   count,
                                                   instance,
                                                   flags,
                                                   pValues);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuReadMmRegisters,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuReadMmRegisters(%p, %x, %x, %x, %x, %p)\n",
        hDevice,
        dwordOffset,
        count,
        instance,
        flags,
        pValues);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuDeviceInitialize(
    int                    fd,
    uint32*                pMajorVersion,
    uint32*                pMinorVersion,
    amdgpu_device_handle*  pDeviceHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuDeviceInitialize(fd,
                                                    pMajorVersion,
                                                    pMinorVersion,
                                                    pDeviceHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuDeviceInitialize,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuDeviceInitialize(%x, %p, %p, %p)\n",
        fd,
        pMajorVersion,
        pMinorVersion,
        pDeviceHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuDeviceDeinitialize(
    amdgpu_device_handle  hDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuDeviceDeinitialize(hDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuDeviceDeinitialize,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuDeviceDeinitialize(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoAlloc(
    amdgpu_device_handle             hDevice,
    struct amdgpu_bo_alloc_request*  pAllocBuffer,
    amdgpu_bo_handle*                pBufferHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoAlloc(hDevice,
                                           pAllocBuffer,
                                           pBufferHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoAlloc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoAlloc(%p, %p, %p)\n",
        hDevice,
        pAllocBuffer,
        pBufferHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoSetMetadata(
    amdgpu_bo_handle            hBuffer,
    struct amdgpu_bo_metadata*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoSetMetadata(hBuffer,
                                                 pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoSetMetadata,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoSetMetadata(%p, %p)\n",
        hBuffer,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoQueryInfo(
    amdgpu_bo_handle        hBuffer,
    struct amdgpu_bo_info*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoQueryInfo(hBuffer,
                                               pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoQueryInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoQueryInfo(%p, %p)\n",
        hBuffer,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoExport(
    amdgpu_bo_handle            hBuffer,
    enum amdgpu_bo_handle_type  type,
    uint32*                     pFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoExport(hBuffer,
                                            type,
                                            pFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoExport,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoExport(%p, %x, %p)\n",
        hBuffer,
        type,
        pFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoImport(
    amdgpu_device_handle             hDevice,
    enum amdgpu_bo_handle_type       type,
    uint32                           fd,
    struct amdgpu_bo_import_result*  pOutput
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoImport(hDevice,
                                            type,
                                            fd,
                                            pOutput);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoImport,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoImport(%p, %x, %x, %p)\n",
        hDevice,
        type,
        fd,
        pOutput);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCreateBoFromUserMem(
    amdgpu_device_handle  hDevice,
    void*                 pCpuAddress,
    uint64                size,
    amdgpu_bo_handle*     pBufferHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCreateBoFromUserMem(hDevice,
                                                       pCpuAddress,
                                                       size,
                                                       pBufferHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCreateBoFromUserMem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCreateBoFromUserMem(%p, %p, %lx, %p)\n",
        hDevice,
        pCpuAddress,
        size,
        pBufferHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCreateBoFromPhysMem(
    amdgpu_device_handle  hDevice,
    uint64                physAddress,
    uint64                size,
    amdgpu_bo_handle*     pBufferHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCreateBoFromPhysMem(hDevice,
                                                       physAddress,
                                                       size,
                                                       pBufferHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCreateBoFromPhysMem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCreateBoFromPhysMem(%p, %lx, %lx, %p)\n",
        hDevice,
        physAddress,
        size,
        pBufferHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuFindBoByCpuMapping(
    amdgpu_device_handle  hDevice,
    void*                 pCpuAddress,
    uint64                size,
    amdgpu_bo_handle*     pBufferHandle,
    uint64*               pOffsetInBuffer
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuFindBoByCpuMapping(hDevice,
                                                      pCpuAddress,
                                                      size,
                                                      pBufferHandle,
                                                      pOffsetInBuffer);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuFindBoByCpuMapping,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuFindBoByCpuMapping(%p, %p, %lx, %p, %p)\n",
        hDevice,
        pCpuAddress,
        size,
        pBufferHandle,
        pOffsetInBuffer);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoFree(
    amdgpu_bo_handle  hBuffer
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoFree(hBuffer);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoFree(%p)\n",
        hBuffer);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoCpuMap(
    amdgpu_bo_handle  hBuffer,
    void**            ppCpuAddress
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoCpuMap(hBuffer,
                                            ppCpuAddress);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoCpuMap,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoCpuMap(%p, %p)\n",
        hBuffer,
        ppCpuAddress);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoCpuUnmap(
    amdgpu_bo_handle  hBuffer
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoCpuUnmap(hBuffer);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoCpuUnmap,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoCpuUnmap(%p)\n",
        hBuffer);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoRemapSecure(
    amdgpu_bo_handle  buf_handle,
    bool              secure_map
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoRemapSecure(buf_handle,
                                                 secure_map);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoRemapSecure,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoRemapSecure(%p, %x)\n",
        buf_handle,
        secure_map);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoWaitForIdle(
    amdgpu_bo_handle  hBuffer,
    uint64            timeoutInNs,
    bool*             pBufferBusy
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoWaitForIdle(hBuffer,
                                                 timeoutInNs,
                                                 pBufferBusy);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoWaitForIdle,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoWaitForIdle(%p, %lx, %p)\n",
        hBuffer,
        timeoutInNs,
        pBufferBusy);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoListCreate(
    amdgpu_device_handle    hDevice,
    uint32                  numberOfResources,
    amdgpu_bo_handle*       pResources,
    uint8*                  pResourcePriorities,
    amdgpu_bo_list_handle*  pBoListHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoListCreate(hDevice,
                                                numberOfResources,
                                                pResources,
                                                pResourcePriorities,
                                                pBoListHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoListCreate,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoListCreate(%p, %x, %p, %p, %p)\n",
        hDevice,
        numberOfResources,
        pResources,
        pResourcePriorities,
        pBoListHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoListDestroy(
    amdgpu_bo_list_handle  hBoList
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoListDestroy(hBoList);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoListDestroy,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoListDestroy(%p)\n",
        hBoList);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoListCreateRaw(
    amdgpu_device_handle              hDevice,
    uint32                            numberOfResources,
    struct drm_amdgpu_bo_list_entry*  pBoListEntry,
    uint32*                           pBoListHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoListCreateRaw(hDevice,
                                                   numberOfResources,
                                                   pBoListEntry,
                                                   pBoListHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoListCreateRaw,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoListCreateRaw(%p, %x, %p, %p)\n",
        hDevice,
        numberOfResources,
        pBoListEntry,
        pBoListHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoListDestroyRaw(
    amdgpu_device_handle  hDevice,
    uint32                boListHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoListDestroyRaw(hDevice,
                                                    boListHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoListDestroyRaw,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoListDestroyRaw(%p, %x)\n",
        hDevice,
        boListHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCtxCreate(
    amdgpu_device_handle    hDevice,
    amdgpu_context_handle*  pContextHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCtxCreate(hDevice,
                                               pContextHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxCreate,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxCreate(%p, %p)\n",
        hDevice,
        pContextHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCtxFree(
    amdgpu_context_handle  hContext
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCtxFree(hContext);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxFree(%p)\n",
        hContext);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSubmit(
    amdgpu_context_handle      hContext,
    uint64                     flags,
    struct amdgpu_cs_request*  pIbsRequest,
    uint32                     numberOfRequests
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSubmit(hContext,
                                            flags,
                                            pIbsRequest,
                                            numberOfRequests);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSubmit,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSubmit(%p, %lx, %p, %x)\n",
        hContext,
        flags,
        pIbsRequest,
        numberOfRequests);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsQueryFenceStatus(
    struct amdgpu_cs_fence*  pFence,
    uint64                   timeoutInNs,
    uint64                   flags,
    uint32*                  pExpired
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsQueryFenceStatus(pFence,
                                                      timeoutInNs,
                                                      flags,
                                                      pExpired);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsQueryFenceStatus,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsQueryFenceStatus(%p, %lx, %lx, %p)\n",
        pFence,
        timeoutInNs,
        flags,
        pExpired);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsWaitFences(
    struct amdgpu_cs_fence*  pFences,
    uint32                   fenceCount,
    bool                     waitAll,
    uint64                   timeoutInNs,
    uint32*                  pStatus,
    uint32*                  pFirst
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsWaitFences(pFences,
                                                fenceCount,
                                                waitAll,
                                                timeoutInNs,
                                                pStatus,
                                                pFirst);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsWaitFences,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsWaitFences(%p, %x, %x, %lx, %p, %p)\n",
        pFences,
        fenceCount,
        waitAll,
        timeoutInNs,
        pStatus,
        pFirst);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCtxStablePstate(
    amdgpu_context_handle  context,
    uint32_t               op,
    uint32_t               flags,
    uint32_t *             out_flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCtxStablePstate(context,
                                                     op,
                                                     flags,
                                                     out_flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxStablePstate,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxStablePstate(%p, %x, %x, %p)\n",
        context,
        op,
        flags,
        out_flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryBufferSizeAlignment(
    amdgpu_device_handle                   hDevice,
    struct amdgpu_buffer_size_alignments*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryBufferSizeAlignment(hDevice,
                                                            pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryBufferSizeAlignment,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryBufferSizeAlignment(%p, %p)\n",
        hDevice,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryFirmwareVersion(
    amdgpu_device_handle  hDevice,
    uint32                fwType,
    uint32                ipInstance,
    uint32                index,
    uint32*               pVersion,
    uint32*               pFeature
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryFirmwareVersion(hDevice,
                                                        fwType,
                                                        ipInstance,
                                                        index,
                                                        pVersion,
                                                        pFeature);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryFirmwareVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryFirmwareVersion(%p, %x, %x, %x, %p, %p)\n",
        hDevice,
        fwType,
        ipInstance,
        index,
        pVersion,
        pFeature);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryVideoCapsInfo(
    amdgpu_device_handle  hDevice,
    uint32                capType,
    uint32                size,
    void*                 pCaps
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryVideoCapsInfo(hDevice,
                                                      capType,
                                                      size,
                                                      pCaps);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryVideoCapsInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryVideoCapsInfo(%p, %x, %x, %p)\n",
        hDevice,
        capType,
        size,
        pCaps);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryHwIpCount(
    amdgpu_device_handle  hDevice,
    uint32                type,
    uint32*               pCount
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryHwIpCount(hDevice,
                                                  type,
                                                  pCount);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryHwIpCount,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryHwIpCount(%p, %x, %p)\n",
        hDevice,
        type,
        pCount);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryHeapInfo(
    amdgpu_device_handle      hDevice,
    uint32                    heap,
    uint32                    flags,
    struct amdgpu_heap_info*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryHeapInfo(hDevice,
                                                 heap,
                                                 flags,
                                                 pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryHeapInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryHeapInfo(%p, %x, %x, %p)\n",
        hDevice,
        heap,
        flags,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryGpuInfo(
    amdgpu_device_handle     hDevice,
    struct amdgpu_gpu_info*  pInfo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryGpuInfo(hDevice,
                                                pInfo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryGpuInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryGpuInfo(%p, %p)\n",
        hDevice,
        pInfo);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQuerySensorInfo(
    amdgpu_device_handle  hDevice,
    uint32                sensor_type,
    uint32                size,
    void*                 value
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQuerySensorInfo(hDevice,
                                                   sensor_type,
                                                   size,
                                                   value);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQuerySensorInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQuerySensorInfo(%p, %x, %x, %p)\n",
        hDevice,
        sensor_type,
        size,
        value);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryInfo(
    amdgpu_device_handle  hDevice,
    uint32                infoId,
    uint32                size,
    void*                 pValue
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryInfo(hDevice,
                                             infoId,
                                             size,
                                             pValue);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryInfo,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryInfo(%p, %x, %x, %p)\n",
        hDevice,
        infoId,
        size,
        pValue);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQueryPrivateAperture(
    amdgpu_device_handle  hDevice,
    uint64*               pStartVa,
    uint64*               pEndVa
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQueryPrivateAperture(hDevice,
                                                        pStartVa,
                                                        pEndVa);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQueryPrivateAperture,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQueryPrivateAperture(%p, %p, %p)\n",
        hDevice,
        pStartVa,
        pEndVa);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuQuerySharedAperture(
    amdgpu_device_handle  hDevice,
    uint64*               pStartVa,
    uint64*               pEndVa
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuQuerySharedAperture(hDevice,
                                                       pStartVa,
                                                       pEndVa);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuQuerySharedAperture,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuQuerySharedAperture(%p, %p, %p)\n",
        hDevice,
        pStartVa,
        pEndVa);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuBoGetPhysAddress(
    amdgpu_bo_handle  hBuffer,
    uint64*           pPhysAddress
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuBoGetPhysAddress(hBuffer,
                                                    pPhysAddress);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoGetPhysAddress,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoGetPhysAddress(%p, %p)\n",
        hBuffer,
        pPhysAddress);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsReservedVmid(
    amdgpu_device_handle  hDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsReservedVmid(hDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsReservedVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsReservedVmid(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsUnreservedVmid(
    amdgpu_device_handle  hDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsUnreservedVmid(hDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsUnreservedVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsUnreservedVmid(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSyncobj(
    amdgpu_device_handle  hDevice,
    uint32*               pSyncObj
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCreateSyncobj(hDevice,
                                                   pSyncObj);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCreateSyncobj,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCreateSyncobj(%p, %p)\n",
        hDevice,
        pSyncObj);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSyncobj2(
    amdgpu_device_handle  hDevice,
    uint32                flags,
    uint32*               pSyncObj
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCreateSyncobj2(hDevice,
                                                    flags,
                                                    pSyncObj);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCreateSyncobj2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCreateSyncobj2(%p, %x, %p)\n",
        hDevice,
        flags,
        pSyncObj);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySyncobj(
    amdgpu_device_handle  hDevice,
    uint32                syncObj
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsDestroySyncobj(hDevice,
                                                    syncObj);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsDestroySyncobj,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsDestroySyncobj(%p, %x)\n",
        hDevice,
        syncObj);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsExportSyncobj(
    amdgpu_device_handle  hDevice,
    uint32                syncObj,
    int32*                pSharedFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsExportSyncobj(hDevice,
                                                   syncObj,
                                                   pSharedFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsExportSyncobj,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsExportSyncobj(%p, %x, %p)\n",
        hDevice,
        syncObj,
        pSharedFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsImportSyncobj(
    amdgpu_device_handle  hDevice,
    int32                 sharedFd,
    uint32*               pSyncObj
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsImportSyncobj(hDevice,
                                                   sharedFd,
                                                   pSyncObj);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsImportSyncobj,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsImportSyncobj(%p, %x, %p)\n",
        hDevice,
        sharedFd,
        pSyncObj);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSubmitRaw2(
    amdgpu_device_handle          dev,
    amdgpu_context_handle         context,
    uint32_t                      bo_list_handle,
    int                           num_chunks,
    struct drm_amdgpu_cs_chunk *  chunks,
    uint64_t *                    seq_no
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSubmitRaw2(dev,
                                                context,
                                                bo_list_handle,
                                                num_chunks,
                                                chunks,
                                                seq_no);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSubmitRaw2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSubmitRaw2(%p, %p, %x, %x, %p, %p)\n",
        dev,
        context,
        bo_list_handle,
        num_chunks,
        chunks,
        seq_no);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnAmdgpuCsChunkFenceToDep(
    struct amdgpu_cs_fence*         pFence,
    struct drm_amdgpu_cs_chunk_dep  pDep
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnAmdgpuCsChunkFenceToDep(pFence,
                                         pDep);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsChunkFenceToDep,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsChunkFenceToDep(%p, %x)\n",
        pFence,
        pDep);
    m_paramLogger.Flush();
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnAmdgpuCsChunkFenceInfoToData(
    struct amdgpu_cs_fence_info       fenceInfo,
    struct drm_amdgpu_cs_chunk_data*  pData
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnAmdgpuCsChunkFenceInfoToData(fenceInfo,
                                              pData);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsChunkFenceInfoToData,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsChunkFenceInfoToData(%x, %p)\n",
        fenceInfo,
        pData);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjImportSyncFile(
    amdgpu_device_handle  hDevice,
    uint32                syncObj,
    int32                 syncFileFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile(hDevice,
                                                           syncObj,
                                                           syncFileFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjImportSyncFile,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjImportSyncFile(%p, %x, %x)\n",
        hDevice,
        syncObj,
        syncFileFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjImportSyncFile2(
    amdgpu_device_handle  hDevice,
    uint32                syncObj,
    uint64                point,
    int32                 syncFileFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile2(hDevice,
                                                            syncObj,
                                                            point,
                                                            syncFileFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjImportSyncFile2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjImportSyncFile2(%p, %x, %lx, %x)\n",
        hDevice,
        syncObj,
        point,
        syncFileFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjExportSyncFile(
    amdgpu_device_handle  hDevice,
    uint32                syncObj,
    int32*                pSyncFileFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile(hDevice,
                                                           syncObj,
                                                           pSyncFileFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjExportSyncFile,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjExportSyncFile(%p, %x, %p)\n",
        hDevice,
        syncObj,
        pSyncFileFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjExportSyncFile2(
    amdgpu_device_handle  hDevice,
    uint32                syncObj,
    uint64                point,
    uint32                flags,
    int32*                pSyncFileFd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile2(hDevice,
                                                            syncObj,
                                                            point,
                                                            flags,
                                                            pSyncFileFd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjExportSyncFile2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjExportSyncFile2(%p, %x, %lx, %x, %p)\n",
        hDevice,
        syncObj,
        point,
        flags,
        pSyncFileFd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjWait(
    amdgpu_device_handle  hDevice,
    uint32*               pHandles,
    uint32                numHandles,
    int64                 timeoutInNs,
    uint32                flags,
    uint32*               pFirstSignaled
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjWait(hDevice,
                                                 pHandles,
                                                 numHandles,
                                                 timeoutInNs,
                                                 flags,
                                                 pFirstSignaled);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjWait,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjWait(%p, %p, %x, %lx, %x, %p)\n",
        hDevice,
        pHandles,
        numHandles,
        timeoutInNs,
        flags,
        pFirstSignaled);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjTimelineWait(
    amdgpu_device_handle  hDevice,
    uint32*               pHandles,
    uint64*               points,
    uint32                numHandles,
    int64                 timeoutInNs,
    uint32                flags,
    uint32*               pFirstSignaled
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjTimelineWait(hDevice,
                                                         pHandles,
                                                         points,
                                                         numHandles,
                                                         timeoutInNs,
                                                         flags,
                                                         pFirstSignaled);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjTimelineWait,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjTimelineWait(%p, %p, %p, %x, %lx, %x, %p)\n",
        hDevice,
        pHandles,
        points,
        numHandles,
        timeoutInNs,
        flags,
        pFirstSignaled);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjReset(
    amdgpu_device_handle  hDevice,
    const uint32*         pHandles,
    uint32                numHandles
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjReset(hDevice,
                                                  pHandles,
                                                  numHandles);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjReset,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjReset(%p, %p, %x)\n",
        hDevice,
        pHandles,
        numHandles);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjSignal(
    amdgpu_device_handle  hDevice,
    const uint32*         pHandles,
    uint32                numHandles
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjSignal(hDevice,
                                                   pHandles,
                                                   numHandles);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjSignal,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjSignal(%p, %p, %x)\n",
        hDevice,
        pHandles,
        numHandles);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjTimelineSignal(
    amdgpu_device_handle  hDevice,
    const uint32*         pHandles,
    uint64*               points,
    uint32                numHandles
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjTimelineSignal(hDevice,
                                                           pHandles,
                                                           points,
                                                           numHandles);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjTimelineSignal,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjTimelineSignal(%p, %p, %p, %x)\n",
        hDevice,
        pHandles,
        points,
        numHandles);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjTransfer(
    amdgpu_device_handle  hDevice,
    uint32                dst_handle,
    uint64                dst_point,
    uint32                src_handle,
    uint64                src_point,
    uint32                flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjTransfer(hDevice,
                                                     dst_handle,
                                                     dst_point,
                                                     src_handle,
                                                     src_point,
                                                     flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjTransfer,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjTransfer(%p, %x, %lx, %x, %lx, %x)\n",
        hDevice,
        dst_handle,
        dst_point,
        src_handle,
        src_point,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjQuery(
    amdgpu_device_handle  hDevice,
    const uint32*         pHandles,
    uint64*               points,
    uint32                numHandles
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjQuery(hDevice,
                                                  pHandles,
                                                  points,
                                                  numHandles);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjQuery,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjQuery(%p, %p, %p, %x)\n",
        hDevice,
        pHandles,
        points,
        numHandles);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjQuery2(
    amdgpu_device_handle  hDevice,
    const uint32*         pHandles,
    uint64*               points,
    uint32                numHandles,
    uint32                flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSyncobjQuery2(hDevice,
                                                   pHandles,
                                                   points,
                                                   numHandles,
                                                   flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSyncobjQuery2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSyncobjQuery2(%p, %p, %p, %x, %x)\n",
        hDevice,
        pHandles,
        points,
        numHandles,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCtxCreate2(
    amdgpu_device_handle    hDevice,
    uint32                  priority,
    amdgpu_context_handle*  pContextHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCtxCreate2(hDevice,
                                                priority,
                                                pContextHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxCreate2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxCreate2(%p, %x, %p)\n",
        hDevice,
        priority,
        pContextHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsCtxCreate3(
    amdgpu_device_handle    hDevice,
    uint32                  priority,
    uint32_t                flags,
    amdgpu_context_handle*  pContextHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsCtxCreate3(hDevice,
                                                priority,
                                                flags,
                                                pContextHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxCreate3,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxCreate3(%p, %x, %x, %p)\n",
        hDevice,
        priority,
        flags,
        pContextHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmVersionPtr DrmLoaderFuncsProxy::pfnDrmGetVersion(
    int  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmVersionPtr ret = m_pFuncs->pfnDrmGetVersion(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetVersion(%x)\n",
        fd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmFreeVersion(
    drmVersionPtr  v
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeVersion(v);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmFreeVersion,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmFreeVersion(%p)\n",
        v);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmGetNodeTypeFromFd(
    int  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmGetNodeTypeFromFd(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetNodeTypeFromFd,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetNodeTypeFromFd(%x)\n",
        fd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
char* DrmLoaderFuncsProxy::pfnDrmGetRenderDeviceNameFromFd(
    int  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnDrmGetRenderDeviceNameFromFd(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetRenderDeviceNameFromFd,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetRenderDeviceNameFromFd(%x)\n",
        fd);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmGetDevices(
    drmDevicePtr*  pDevices,
    int32          maxDevices
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmGetDevices(pDevices,
                                           maxDevices);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetDevices,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetDevices(%p, %x)\n",
        pDevices,
        maxDevices);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmFreeDevices(
    drmDevicePtr*  pDevices,
    int32          count
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeDevices(pDevices,
                                count);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmFreeDevices,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmFreeDevices(%p, %x)\n",
        pDevices,
        count);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmGetDevice2(
    int            fd,
    uint32_t       flags,
    drmDevicePtr*  pDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmGetDevice2(fd,
                                           flags,
                                           pDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetDevice2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetDevice2(%x, %x, %p)\n",
        fd,
        flags,
        pDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmFreeDevice(
    drmDevicePtr*  pDevice
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeDevice(pDevice);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmFreeDevice,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmFreeDevice(%p)\n",
        pDevice);
    m_paramLogger.Flush();
}

// =====================================================================================================================
char* DrmLoaderFuncsProxy::pfnDrmGetBusid(
    int  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnDrmGetBusid(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetBusid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetBusid(%x)\n",
        fd);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmFreeBusid(
    const char*  pBusId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeBusid(pBusId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmFreeBusid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmFreeBusid(%p)\n",
        pBusId);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModeResPtr DrmLoaderFuncsProxy::pfnDrmModeGetResources(
    int  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeResPtr ret = m_pFuncs->pfnDrmModeGetResources(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetResources(%x)\n",
        fd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeResources(
    drmModeResPtr  ptr
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeResources(ptr);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeResources(%p)\n",
        ptr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModeConnectorPtr DrmLoaderFuncsProxy::pfnDrmModeGetConnector(
    int     fd,
    uint32  connectorId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeConnectorPtr ret = m_pFuncs->pfnDrmModeGetConnector(fd,
                                                               connectorId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetConnector,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetConnector(%x, %x)\n",
        fd,
        connectorId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeConnector(
    drmModeConnectorPtr  ptr
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeConnector(ptr);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeConnector,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeConnector(%p)\n",
        ptr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmGetCap(
    int      fd,
    uint64   capability,
    uint64*  pValue
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmGetCap(fd,
                                       capability,
                                       pValue);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetCap,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetCap(%x, %lx, %p)\n",
        fd,
        capability,
        pValue);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmSetClientCap(
    int     fd,
    uint64  capability,
    uint64  value
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmSetClientCap(fd,
                                             capability,
                                             value);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmSetClientCap,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmSetClientCap(%x, %lx, %lx)\n",
        fd,
        capability,
        value);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmSyncobjCreate(
    int      fd,
    uint32   flags,
    uint32*  pHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmSyncobjCreate(fd,
                                              flags,
                                              pHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmSyncobjCreate,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmSyncobjCreate(%x, %x, %p)\n",
        fd,
        flags,
        pHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreePlane(
    drmModePlanePtr  pPlanePtr
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreePlane(pPlanePtr);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreePlane,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreePlane(%p)\n",
        pPlanePtr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreePlaneResources(
    drmModePlaneResPtr  pPlaneResPtr
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreePlaneResources(pPlaneResPtr);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreePlaneResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreePlaneResources(%p)\n",
        pPlaneResPtr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModePlaneResPtr DrmLoaderFuncsProxy::pfnDrmModeGetPlaneResources(
    int32  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModePlaneResPtr ret = m_pFuncs->pfnDrmModeGetPlaneResources(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetPlaneResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetPlaneResources(%x)\n",
        fd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmModePlanePtr DrmLoaderFuncsProxy::pfnDrmModeGetPlane(
    int32   fd,
    uint32  planeId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModePlanePtr ret = m_pFuncs->pfnDrmModeGetPlane(fd,
                                                       planeId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetPlane,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetPlane(%x, %x)\n",
        fd,
        planeId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmDropMaster(
    int32  fd
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmDropMaster(fd);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmDropMaster,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmDropMaster(%x)\n",
        fd);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmPrimeFDToHandle(
    int32    fd,
    int32    primeFd,
    uint32*  pHandle
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmPrimeFDToHandle(fd,
                                                primeFd,
                                                pHandle);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmPrimeFDToHandle,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmPrimeFDToHandle(%x, %x, %p)\n",
        fd,
        primeFd,
        pHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmModeAddFB2(
    int32      fd,
    uint32     width,
    uint32     height,
    uint32     pixelFormat,
    uint32     boHandles[4],
    uint32     pitches[4],
    uint32     offsets[4],
    uint32*    pBufId,
    uint32     flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmModeAddFB2(fd,
                                           width,
                                           height,
                                           pixelFormat,
                                           boHandles,
                                           pitches,
                                           offsets,
                                           pBufId,
                                           flags);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeAddFB2,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeAddFB2(%x, %x, %x, %x, %x, %x, %x, %p, %x)\n",
        fd,
        width,
        height,
        pixelFormat,
        boHandles,
        pitches,
        offsets,
        pBufId,
        flags);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmModePageFlip(
    int32   fd,
    uint32  crtcId,
    uint32  fbId,
    uint32  flags,
    void*   pUserData
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmModePageFlip(fd,
                                             crtcId,
                                             fbId,
                                             flags,
                                             pUserData);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModePageFlip,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModePageFlip(%x, %x, %x, %x, %p)\n",
        fd,
        crtcId,
        fbId,
        flags,
        pUserData);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmModeEncoderPtr DrmLoaderFuncsProxy::pfnDrmModeGetEncoder(
    int32   fd,
    uint32  encoderId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeEncoderPtr ret = m_pFuncs->pfnDrmModeGetEncoder(fd,
                                                           encoderId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetEncoder,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetEncoder(%x, %x)\n",
        fd,
        encoderId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeEncoder(
    drmModeEncoderPtr  pEncoder
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeEncoder(pEncoder);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeEncoder,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeEncoder(%p)\n",
        pEncoder);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmModeSetCrtc(
    int32               fd,
    uint32              crtcId,
    uint32              bufferId,
    uint32              x,
    uint32              y,
    uint32*             pConnectors,
    int32               count,
    drmModeModeInfoPtr  pMode
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmModeSetCrtc(fd,
                                          crtcId,
                                          bufferId,
                                          x,
                                          y,
                                          pConnectors,
                                          count,
                                          pMode);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeSetCrtc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeSetCrtc(%x, %x, %x, %x, %x, %p, %x, %p)\n",
        fd,
        crtcId,
        bufferId,
        x,
        y,
        pConnectors,
        count,
        pMode);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmModeConnectorPtr DrmLoaderFuncsProxy::pfnDrmModeGetConnectorCurrent(
    int32   fd,
    uint32  connectorId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeConnectorPtr ret = m_pFuncs->pfnDrmModeGetConnectorCurrent(fd,
                                                                      connectorId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetConnectorCurrent,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetConnectorCurrent(%x, %x)\n",
        fd,
        connectorId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmModeCrtcPtr DrmLoaderFuncsProxy::pfnDrmModeGetCrtc(
    int32   fd,
    uint32  crtcId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeCrtcPtr ret = m_pFuncs->pfnDrmModeGetCrtc(fd,
                                                     crtcId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetCrtc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetCrtc(%x, %x)\n",
        fd,
        crtcId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeCrtc(
    drmModeCrtcPtr  pCrtc
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeCrtc(pCrtc);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeCrtc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeCrtc(%p)\n",
        pCrtc);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmCrtcGetSequence(
    int32    fd,
    uint32   crtcId,
    uint64*  pSequence,
    uint64*  pNs
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmCrtcGetSequence(fd,
                                                crtcId,
                                                pSequence,
                                                pNs);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmCrtcGetSequence,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmCrtcGetSequence(%x, %x, %p, %p)\n",
        fd,
        crtcId,
        pSequence,
        pNs);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmCrtcQueueSequence(
    int32    fd,
    uint32   crtcId,
    uint32   flags,
    uint64   sequence,
    uint64*  pSequenceQueued,
    uint64   userData
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmCrtcQueueSequence(fd,
                                                  crtcId,
                                                  flags,
                                                  sequence,
                                                  pSequenceQueued,
                                                  userData);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmCrtcQueueSequence,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmCrtcQueueSequence(%x, %x, %x, %lx, %p, %lx)\n",
        fd,
        crtcId,
        flags,
        sequence,
        pSequenceQueued,
        userData);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmHandleEvent(
    int32               fd,
    drmEventContextPtr  pEvctx
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmHandleEvent(fd,
                                            pEvctx);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmHandleEvent,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmHandleEvent(%x, %p)\n",
        fd,
        pEvctx);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int32 DrmLoaderFuncsProxy::pfnDrmIoctl(
    int32   fd,
    uint32  request,
    void*   pArg
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmIoctl(fd,
                                      request,
                                      pArg);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmIoctl,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmIoctl(%x, %x, %p)\n",
        fd,
        request,
        pArg);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
drmModePropertyPtr DrmLoaderFuncsProxy::pfnDrmModeGetProperty(
    int32   fd,
    uint32  propertyId
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModePropertyPtr ret = m_pFuncs->pfnDrmModeGetProperty(fd,
                                                             propertyId);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetProperty,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetProperty(%x, %x)\n",
        fd,
        propertyId);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeProperty(
    drmModePropertyPtr  pProperty
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeProperty(pProperty);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeProperty,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeProperty(%p)\n",
        pProperty);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModeObjectPropertiesPtr DrmLoaderFuncsProxy::pfnDrmModeObjectGetProperties(
    int     fd,
    uint32  object_id,
    uint32  object_type
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeObjectPropertiesPtr ret = m_pFuncs->pfnDrmModeObjectGetProperties(fd,
                                                                             object_id,
                                                                             object_type);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeObjectGetProperties,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeObjectGetProperties(%x, %x, %x)\n",
        fd,
        object_id,
        object_type);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreeObjectProperties(
    drmModeObjectPropertiesPtr  props
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeObjectProperties(props);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeObjectProperties,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeObjectProperties(%p)\n",
        props);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModePropertyBlobPtr DrmLoaderFuncsProxy::pfnDrmModeGetPropertyBlob(
    int     fd,
    uint32  blob_id
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModePropertyBlobPtr ret = m_pFuncs->pfnDrmModeGetPropertyBlob(fd,
                                                                     blob_id);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeGetPropertyBlob,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeGetPropertyBlob(%x, %x)\n",
        fd,
        blob_id);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeFreePropertyBlob(
    drmModePropertyBlobPtr  ptr
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreePropertyBlob(ptr);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreePropertyBlob,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreePropertyBlob(%p)\n",
        ptr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModeAtomicReqPtr DrmLoaderFuncsProxy::pfnDrmModeAtomicAlloc(    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    drmModeAtomicReqPtr ret = m_pFuncs->pfnDrmModeAtomicAlloc();
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeAtomicAlloc,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf("DrmModeAtomicAlloc()\n");
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
void DrmLoaderFuncsProxy::pfnDrmModeAtomicFree(
    drmModeAtomicReqPtr  req
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeAtomicFree(req);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeAtomicFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeAtomicFree(%p)\n",
        req);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmModeAtomicCommit(
    int                  fd,
    drmModeAtomicReqPtr  req,
    uint32               flags,
    void*                user_data
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmModeAtomicCommit(fd,
                                               req,
                                               flags,
                                               user_data);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeAtomicCommit,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeAtomicCommit(%x, %p, %x, %p)\n",
        fd,
        req,
        flags,
        user_data);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmModeCreatePropertyBlob(
    int          fd,
    const void*  data,
    size_t       length,
    uint32*      id
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmModeCreatePropertyBlob(fd,
                                                     data,
                                                     length,
                                                     id);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeCreatePropertyBlob,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeCreatePropertyBlob(%x, %p, %x, %p)\n",
        fd,
        data,
        length,
        id);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmModeDestroyPropertyBlob(
    int     fd,
    uint32  id
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmModeDestroyPropertyBlob(fd,
                                                      id);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeDestroyPropertyBlob,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeDestroyPropertyBlob(%x, %x)\n",
        fd,
        id);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmModeAtomicAddProperty(
    drmModeAtomicReqPtr  req,
    uint32               object_id,
    uint32               property_id,
    uint64               value
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmModeAtomicAddProperty(req,
                                                    object_id,
                                                    property_id,
                                                    value);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeAtomicAddProperty,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeAtomicAddProperty(%p, %x, %x, %lx)\n",
        req,
        object_id,
        property_id,
        value);
    m_paramLogger.Flush();

    return ret;
}

#endif

// =====================================================================================================================
DrmLoader::DrmLoader()
    :
    m_initialized(false)
{
    memset(&m_funcs, 0, sizeof(m_funcs));
}

// =====================================================================================================================
DrmLoader::~DrmLoader()
{
}

// =====================================================================================================================
Result DrmLoader::Init(
    Platform* pPlatform)
{
    Result           result      = Result::Success;
    constexpr uint32 LibNameSize = 64;
    char LibNames[DrmLoaderLibrariesCount][LibNameSize] = {
        "libdrm_amdgpu.so.1",
        "libdrm.so.2",
    };
    SpecializedInit(pPlatform, &LibNames[LibDrmAmdgpu][0]);
    if (m_initialized == false)
    {
        // resolve symbols from libdrm_amdgpu.so.1
        result = m_library[LibDrmAmdgpu].Load(LibNames[LibDrmAmdgpu]);
        PAL_ASSERT_MSG(result == Result::Success, "Failed to load LibDrmAmdgpu library");
        if (result == Result::Success)
        {
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_hw_ip_info", &m_funcs.pfnAmdgpuQueryHwIpInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_va_op", &m_funcs.pfnAmdgpuBoVaOp);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_va_op_raw", &m_funcs.pfnAmdgpuBoVaOpRaw);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_create_semaphore", &m_funcs.pfnAmdgpuCsCreateSemaphore);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_signal_semaphore", &m_funcs.pfnAmdgpuCsSignalSemaphore);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_wait_semaphore", &m_funcs.pfnAmdgpuCsWaitSemaphore);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_destroy_semaphore", &m_funcs.pfnAmdgpuCsDestroySemaphore);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_create_sem", &m_funcs.pfnAmdgpuCsCreateSem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_signal_sem", &m_funcs.pfnAmdgpuCsSignalSem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_wait_sem", &m_funcs.pfnAmdgpuCsWaitSem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_export_sem", &m_funcs.pfnAmdgpuCsExportSem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_import_sem", &m_funcs.pfnAmdgpuCsImportSem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_destroy_sem", &m_funcs.pfnAmdgpuCsDestroySem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_get_marketing_name", &m_funcs.pfnAmdgpuGetMarketingName);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_va_range_free", &m_funcs.pfnAmdgpuVaRangeFree);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_va_range_query", &m_funcs.pfnAmdgpuVaRangeQuery);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_va_range_alloc", &m_funcs.pfnAmdgpuVaRangeAlloc);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_vm_reserve_vmid", &m_funcs.pfnAmdgpuVmReserveVmid);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_vm_unreserve_vmid", &m_funcs.pfnAmdgpuVmUnreserveVmid);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_read_mm_registers", &m_funcs.pfnAmdgpuReadMmRegisters);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_device_initialize", &m_funcs.pfnAmdgpuDeviceInitialize);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_device_deinitialize", &m_funcs.pfnAmdgpuDeviceDeinitialize);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_alloc", &m_funcs.pfnAmdgpuBoAlloc);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_set_metadata", &m_funcs.pfnAmdgpuBoSetMetadata);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_query_info", &m_funcs.pfnAmdgpuBoQueryInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_export", &m_funcs.pfnAmdgpuBoExport);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_import", &m_funcs.pfnAmdgpuBoImport);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_create_bo_from_user_mem", &m_funcs.pfnAmdgpuCreateBoFromUserMem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_create_bo_from_phys_mem", &m_funcs.pfnAmdgpuCreateBoFromPhysMem);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_find_bo_by_cpu_mapping", &m_funcs.pfnAmdgpuFindBoByCpuMapping);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_free", &m_funcs.pfnAmdgpuBoFree);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_cpu_map", &m_funcs.pfnAmdgpuBoCpuMap);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_cpu_unmap", &m_funcs.pfnAmdgpuBoCpuUnmap);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_remap_secure", &m_funcs.pfnAmdgpuBoRemapSecure);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_wait_for_idle", &m_funcs.pfnAmdgpuBoWaitForIdle);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_list_create", &m_funcs.pfnAmdgpuBoListCreate);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_list_destroy", &m_funcs.pfnAmdgpuBoListDestroy);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_list_create_raw", &m_funcs.pfnAmdgpuBoListCreateRaw);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_list_destroy_raw", &m_funcs.pfnAmdgpuBoListDestroyRaw);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_ctx_create", &m_funcs.pfnAmdgpuCsCtxCreate);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_ctx_free", &m_funcs.pfnAmdgpuCsCtxFree);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_submit", &m_funcs.pfnAmdgpuCsSubmit);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_query_fence_status", &m_funcs.pfnAmdgpuCsQueryFenceStatus);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_wait_fences", &m_funcs.pfnAmdgpuCsWaitFences);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_ctx_stable_pstate", &m_funcs.pfnAmdgpuCsCtxStablePstate);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_buffer_size_alignment", &m_funcs.pfnAmdgpuQueryBufferSizeAlignment);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_firmware_version", &m_funcs.pfnAmdgpuQueryFirmwareVersion);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_video_caps_info", &m_funcs.pfnAmdgpuQueryVideoCapsInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_hw_ip_count", &m_funcs.pfnAmdgpuQueryHwIpCount);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_heap_info", &m_funcs.pfnAmdgpuQueryHeapInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_gpu_info", &m_funcs.pfnAmdgpuQueryGpuInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_sensor_info", &m_funcs.pfnAmdgpuQuerySensorInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_info", &m_funcs.pfnAmdgpuQueryInfo);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_private_aperture", &m_funcs.pfnAmdgpuQueryPrivateAperture);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_query_shared_aperture", &m_funcs.pfnAmdgpuQuerySharedAperture);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_bo_get_phys_address", &m_funcs.pfnAmdgpuBoGetPhysAddress);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_reserved_vmid", &m_funcs.pfnAmdgpuCsReservedVmid);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_unreserved_vmid", &m_funcs.pfnAmdgpuCsUnreservedVmid);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_create_syncobj", &m_funcs.pfnAmdgpuCsCreateSyncobj);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_create_syncobj2", &m_funcs.pfnAmdgpuCsCreateSyncobj2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_destroy_syncobj", &m_funcs.pfnAmdgpuCsDestroySyncobj);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_export_syncobj", &m_funcs.pfnAmdgpuCsExportSyncobj);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_import_syncobj", &m_funcs.pfnAmdgpuCsImportSyncobj);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_submit_raw2", &m_funcs.pfnAmdgpuCsSubmitRaw2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_chunk_fence_to_dep", &m_funcs.pfnAmdgpuCsChunkFenceToDep);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_chunk_fence_info_to_data", &m_funcs.pfnAmdgpuCsChunkFenceInfoToData);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_import_sync_file", &m_funcs.pfnAmdgpuCsSyncobjImportSyncFile);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_import_sync_file2", &m_funcs.pfnAmdgpuCsSyncobjImportSyncFile2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_export_sync_file", &m_funcs.pfnAmdgpuCsSyncobjExportSyncFile);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_export_sync_file2", &m_funcs.pfnAmdgpuCsSyncobjExportSyncFile2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_wait", &m_funcs.pfnAmdgpuCsSyncobjWait);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_timeline_wait", &m_funcs.pfnAmdgpuCsSyncobjTimelineWait);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_reset", &m_funcs.pfnAmdgpuCsSyncobjReset);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_signal", &m_funcs.pfnAmdgpuCsSyncobjSignal);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_timeline_signal", &m_funcs.pfnAmdgpuCsSyncobjTimelineSignal);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_transfer", &m_funcs.pfnAmdgpuCsSyncobjTransfer);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_query", &m_funcs.pfnAmdgpuCsSyncobjQuery);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_syncobj_query2", &m_funcs.pfnAmdgpuCsSyncobjQuery2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_ctx_create2", &m_funcs.pfnAmdgpuCsCtxCreate2);
            m_library[LibDrmAmdgpu].GetFunction("amdgpu_cs_ctx_create3", &m_funcs.pfnAmdgpuCsCtxCreate3);
        }

        // resolve symbols from libdrm.so.2
        result = m_library[LibDrm].Load(LibNames[LibDrm]);
        PAL_ASSERT_MSG(result == Result::Success, "Failed to load LibDrm library");
        if (result == Result::Success)
        {
            m_library[LibDrm].GetFunction("drmGetVersion", &m_funcs.pfnDrmGetVersion);
            m_library[LibDrm].GetFunction("drmFreeVersion", &m_funcs.pfnDrmFreeVersion);
            m_library[LibDrm].GetFunction("drmGetNodeTypeFromFd", &m_funcs.pfnDrmGetNodeTypeFromFd);
            m_library[LibDrm].GetFunction("drmGetRenderDeviceNameFromFd", &m_funcs.pfnDrmGetRenderDeviceNameFromFd);
            m_library[LibDrm].GetFunction("drmGetDevices", &m_funcs.pfnDrmGetDevices);
            m_library[LibDrm].GetFunction("drmFreeDevices", &m_funcs.pfnDrmFreeDevices);
            m_library[LibDrm].GetFunction("drmGetDevice2", &m_funcs.pfnDrmGetDevice2);
            m_library[LibDrm].GetFunction("drmFreeDevice", &m_funcs.pfnDrmFreeDevice);
            m_library[LibDrm].GetFunction("drmGetBusid", &m_funcs.pfnDrmGetBusid);
            m_library[LibDrm].GetFunction("drmFreeBusid", &m_funcs.pfnDrmFreeBusid);
            m_library[LibDrm].GetFunction("drmModeGetResources", &m_funcs.pfnDrmModeGetResources);
            m_library[LibDrm].GetFunction("drmModeFreeResources", &m_funcs.pfnDrmModeFreeResources);
            m_library[LibDrm].GetFunction("drmModeGetConnector", &m_funcs.pfnDrmModeGetConnector);
            m_library[LibDrm].GetFunction("drmModeFreeConnector", &m_funcs.pfnDrmModeFreeConnector);
            m_library[LibDrm].GetFunction("drmGetCap", &m_funcs.pfnDrmGetCap);
            m_library[LibDrm].GetFunction("drmSetClientCap", &m_funcs.pfnDrmSetClientCap);
            m_library[LibDrm].GetFunction("drmSyncobjCreate", &m_funcs.pfnDrmSyncobjCreate);
            m_library[LibDrm].GetFunction("drmModeFreePlane", &m_funcs.pfnDrmModeFreePlane);
            m_library[LibDrm].GetFunction("drmModeFreePlaneResources", &m_funcs.pfnDrmModeFreePlaneResources);
            m_library[LibDrm].GetFunction("drmModeGetPlaneResources", &m_funcs.pfnDrmModeGetPlaneResources);
            m_library[LibDrm].GetFunction("drmModeGetPlane", &m_funcs.pfnDrmModeGetPlane);
            m_library[LibDrm].GetFunction("drmDropMaster", &m_funcs.pfnDrmDropMaster);
            m_library[LibDrm].GetFunction("drmPrimeFDToHandle", &m_funcs.pfnDrmPrimeFDToHandle);
            m_library[LibDrm].GetFunction("drmModeAddFB2", &m_funcs.pfnDrmModeAddFB2);
            m_library[LibDrm].GetFunction("drmModePageFlip", &m_funcs.pfnDrmModePageFlip);
            m_library[LibDrm].GetFunction("drmModeGetEncoder", &m_funcs.pfnDrmModeGetEncoder);
            m_library[LibDrm].GetFunction("drmModeFreeEncoder", &m_funcs.pfnDrmModeFreeEncoder);
            m_library[LibDrm].GetFunction("drmModeSetCrtc", &m_funcs.pfnDrmModeSetCrtc);
            m_library[LibDrm].GetFunction("drmModeGetConnectorCurrent", &m_funcs.pfnDrmModeGetConnectorCurrent);
            m_library[LibDrm].GetFunction("drmModeGetCrtc", &m_funcs.pfnDrmModeGetCrtc);
            m_library[LibDrm].GetFunction("drmModeFreeCrtc", &m_funcs.pfnDrmModeFreeCrtc);
            m_library[LibDrm].GetFunction("drmCrtcGetSequence", &m_funcs.pfnDrmCrtcGetSequence);
            m_library[LibDrm].GetFunction("drmCrtcQueueSequence", &m_funcs.pfnDrmCrtcQueueSequence);
            m_library[LibDrm].GetFunction("drmHandleEvent", &m_funcs.pfnDrmHandleEvent);
            m_library[LibDrm].GetFunction("drmIoctl", &m_funcs.pfnDrmIoctl);
            m_library[LibDrm].GetFunction("drmModeGetProperty", &m_funcs.pfnDrmModeGetProperty);
            m_library[LibDrm].GetFunction("drmModeFreeProperty", &m_funcs.pfnDrmModeFreeProperty);
            m_library[LibDrm].GetFunction("drmModeObjectGetProperties", &m_funcs.pfnDrmModeObjectGetProperties);
            m_library[LibDrm].GetFunction("drmModeFreeObjectProperties", &m_funcs.pfnDrmModeFreeObjectProperties);
            m_library[LibDrm].GetFunction("drmModeGetPropertyBlob", &m_funcs.pfnDrmModeGetPropertyBlob);
            m_library[LibDrm].GetFunction("drmModeFreePropertyBlob", &m_funcs.pfnDrmModeFreePropertyBlob);
            m_library[LibDrm].GetFunction("drmModeAtomicAlloc", &m_funcs.pfnDrmModeAtomicAlloc);
            m_library[LibDrm].GetFunction("drmModeAtomicFree", &m_funcs.pfnDrmModeAtomicFree);
            m_library[LibDrm].GetFunction("drmModeAtomicCommit", &m_funcs.pfnDrmModeAtomicCommit);
            m_library[LibDrm].GetFunction("drmModeCreatePropertyBlob", &m_funcs.pfnDrmModeCreatePropertyBlob);
            m_library[LibDrm].GetFunction("drmModeDestroyPropertyBlob", &m_funcs.pfnDrmModeDestroyPropertyBlob);
            m_library[LibDrm].GetFunction("drmModeAtomicAddProperty", &m_funcs.pfnDrmModeAtomicAddProperty);
        }

        if (result == Result::Success)
        {
            m_initialized = true;
#if defined(PAL_DEBUG_PRINTS)
            m_proxy.SetFuncCalls(&m_funcs);
#endif
        }
    }
    return result;
}

void DrmLoader::SpecializedInit(
    Platform* pPlatform,
    char*     pDtifLibName)
{
}

} // Linux
} // Pal
