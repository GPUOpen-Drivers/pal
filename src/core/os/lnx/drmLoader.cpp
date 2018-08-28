/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Modify the procsAnalysis.py and drmLoader.py in the tools/generate directory OR drmLoader.proc instead
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <dlfcn.h>
#include <time.h>
#include <string.h>
#include "core/os/lnx/lnxPlatform.h"
#include "core/os/lnx/drmLoader.h"
#include "palAssert.h"
#include "palSysUtil.h"

using namespace Util;
namespace Pal
{
namespace Linux
{
class Platform;

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
int32 DrmLoaderFuncsProxy::pfnAmdgpuCsSubmitRaw(
    amdgpu_device_handle         hDevice,
    amdgpu_context_handle        hContext,
    amdgpu_bo_list_handle        hBuffer,
    int32                        numChunks,
    struct drm_amdgpu_cs_chunk*  pChunks,
    uint64*                      pSeqNo
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnAmdgpuCsSubmitRaw(hDevice,
                                               hContext,
                                               hBuffer,
                                               numChunks,
                                               pChunks,
                                               pSeqNo);
    const int64 end = Util::GetPerfCpuTime();
    const int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsSubmitRaw,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsSubmitRaw(%p, %p, %p, %x, %p, %p)\n",
        hDevice,
        hContext,
        hBuffer,
        numChunks,
        pChunks,
        pSeqNo);
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
    int32    fd,
    uint32   width,
    uint32   height,
    uint32   pixelFormat,
    uint32   boHandles[4],
    uint32   pitches[4],
    uint32   offsets[4],
    uint32*  pBufId,
    uint32   flags
    ) const
{
    const int64 begin = Util::GetPerfCpuTime();
    int32 ret = m_pFuncs->pfnDrmModeAddFB2(fd,
                                           width,
                                           height,
                                           pixelFormat,
                                           boHandles[4],
                                           pitches[4],
                                           offsets[4],
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
        boHandles[4],
        pitches[4],
        offsets[4],
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

#endif

// =====================================================================================================================
DrmLoader::DrmLoader()
    :
    m_initialized(false)
{
    memset(m_libraryHandles, 0, sizeof(m_libraryHandles));
    memset(&m_funcs, 0, sizeof(m_funcs));
}

// =====================================================================================================================
DrmLoader::~DrmLoader()
{
    if (m_libraryHandles[LibDrmAmdgpu] != nullptr)
    {
        dlclose(m_libraryHandles[LibDrmAmdgpu]);
    }
    if (m_libraryHandles[LibDrm] != nullptr)
    {
        dlclose(m_libraryHandles[LibDrm]);
    }
}

// =====================================================================================================================
Result DrmLoader::Init(
    Platform* pPlatform)
{
    Result result                   = Result::Success;
    constexpr uint32_t LibNameSize  = 64;
    char LibNames[DrmLoaderLibrariesCount][LibNameSize] = {
        "libdrm_amdgpu.so.1",
        "libdrm.so.2",
    };

    SpecializedInit(pPlatform, &LibNames[LibDrmAmdgpu][0]);
    if (m_initialized == false)
    {
        // resolve symbols from libdrm_amdgpu.so.1
        m_libraryHandles[LibDrmAmdgpu] = dlopen(LibNames[LibDrmAmdgpu], RTLD_LAZY);
        if (m_libraryHandles[LibDrmAmdgpu] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnAmdgpuQueryHwIpInfo = reinterpret_cast<AmdgpuQueryHwIpInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_hw_ip_info"));
            m_funcs.pfnAmdgpuBoVaOp = reinterpret_cast<AmdgpuBoVaOp>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_va_op"));
            m_funcs.pfnAmdgpuBoVaOpRaw = reinterpret_cast<AmdgpuBoVaOpRaw>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_va_op_raw"));
            m_funcs.pfnAmdgpuCsCreateSemaphore = reinterpret_cast<AmdgpuCsCreateSemaphore>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_create_semaphore"));
            m_funcs.pfnAmdgpuCsSignalSemaphore = reinterpret_cast<AmdgpuCsSignalSemaphore>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_signal_semaphore"));
            m_funcs.pfnAmdgpuCsWaitSemaphore = reinterpret_cast<AmdgpuCsWaitSemaphore>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_wait_semaphore"));
            m_funcs.pfnAmdgpuCsDestroySemaphore = reinterpret_cast<AmdgpuCsDestroySemaphore>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_destroy_semaphore"));
            m_funcs.pfnAmdgpuCsCreateSem = reinterpret_cast<AmdgpuCsCreateSem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_create_sem"));
            m_funcs.pfnAmdgpuCsSignalSem = reinterpret_cast<AmdgpuCsSignalSem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_signal_sem"));
            m_funcs.pfnAmdgpuCsWaitSem = reinterpret_cast<AmdgpuCsWaitSem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_wait_sem"));
            m_funcs.pfnAmdgpuCsExportSem = reinterpret_cast<AmdgpuCsExportSem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_export_sem"));
            m_funcs.pfnAmdgpuCsImportSem = reinterpret_cast<AmdgpuCsImportSem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_import_sem"));
            m_funcs.pfnAmdgpuCsDestroySem = reinterpret_cast<AmdgpuCsDestroySem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_destroy_sem"));
            m_funcs.pfnAmdgpuGetMarketingName = reinterpret_cast<AmdgpuGetMarketingName>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_get_marketing_name"));
            m_funcs.pfnAmdgpuVaRangeFree = reinterpret_cast<AmdgpuVaRangeFree>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_va_range_free"));
            m_funcs.pfnAmdgpuVaRangeQuery = reinterpret_cast<AmdgpuVaRangeQuery>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_va_range_query"));
            m_funcs.pfnAmdgpuVaRangeAlloc = reinterpret_cast<AmdgpuVaRangeAlloc>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_va_range_alloc"));
            m_funcs.pfnAmdgpuReadMmRegisters = reinterpret_cast<AmdgpuReadMmRegisters>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_read_mm_registers"));
            m_funcs.pfnAmdgpuDeviceInitialize = reinterpret_cast<AmdgpuDeviceInitialize>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_device_initialize"));
            m_funcs.pfnAmdgpuDeviceDeinitialize = reinterpret_cast<AmdgpuDeviceDeinitialize>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_device_deinitialize"));
            m_funcs.pfnAmdgpuBoAlloc = reinterpret_cast<AmdgpuBoAlloc>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_alloc"));
            m_funcs.pfnAmdgpuBoSetMetadata = reinterpret_cast<AmdgpuBoSetMetadata>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_set_metadata"));
            m_funcs.pfnAmdgpuBoQueryInfo = reinterpret_cast<AmdgpuBoQueryInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_query_info"));
            m_funcs.pfnAmdgpuBoExport = reinterpret_cast<AmdgpuBoExport>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_export"));
            m_funcs.pfnAmdgpuBoImport = reinterpret_cast<AmdgpuBoImport>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_import"));
            m_funcs.pfnAmdgpuCreateBoFromUserMem = reinterpret_cast<AmdgpuCreateBoFromUserMem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_create_bo_from_user_mem"));
            m_funcs.pfnAmdgpuCreateBoFromPhysMem = reinterpret_cast<AmdgpuCreateBoFromPhysMem>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_create_bo_from_phys_mem"));
            m_funcs.pfnAmdgpuFindBoByCpuMapping = reinterpret_cast<AmdgpuFindBoByCpuMapping>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_find_bo_by_cpu_mapping"));
            m_funcs.pfnAmdgpuBoFree = reinterpret_cast<AmdgpuBoFree>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_free"));
            m_funcs.pfnAmdgpuBoCpuMap = reinterpret_cast<AmdgpuBoCpuMap>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_cpu_map"));
            m_funcs.pfnAmdgpuBoCpuUnmap = reinterpret_cast<AmdgpuBoCpuUnmap>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_cpu_unmap"));
            m_funcs.pfnAmdgpuBoWaitForIdle = reinterpret_cast<AmdgpuBoWaitForIdle>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_wait_for_idle"));
            m_funcs.pfnAmdgpuBoListCreate = reinterpret_cast<AmdgpuBoListCreate>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_list_create"));
            m_funcs.pfnAmdgpuBoListDestroy = reinterpret_cast<AmdgpuBoListDestroy>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_list_destroy"));
            m_funcs.pfnAmdgpuCsCtxCreate = reinterpret_cast<AmdgpuCsCtxCreate>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_ctx_create"));
            m_funcs.pfnAmdgpuCsCtxFree = reinterpret_cast<AmdgpuCsCtxFree>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_ctx_free"));
            m_funcs.pfnAmdgpuCsSubmit = reinterpret_cast<AmdgpuCsSubmit>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_submit"));
            m_funcs.pfnAmdgpuCsQueryFenceStatus = reinterpret_cast<AmdgpuCsQueryFenceStatus>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_query_fence_status"));
            m_funcs.pfnAmdgpuCsWaitFences = reinterpret_cast<AmdgpuCsWaitFences>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_wait_fences"));
            m_funcs.pfnAmdgpuQueryBufferSizeAlignment = reinterpret_cast<AmdgpuQueryBufferSizeAlignment>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_buffer_size_alignment"));
            m_funcs.pfnAmdgpuQueryFirmwareVersion = reinterpret_cast<AmdgpuQueryFirmwareVersion>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_firmware_version"));
            m_funcs.pfnAmdgpuQueryHwIpCount = reinterpret_cast<AmdgpuQueryHwIpCount>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_hw_ip_count"));
            m_funcs.pfnAmdgpuQueryHeapInfo = reinterpret_cast<AmdgpuQueryHeapInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_heap_info"));
            m_funcs.pfnAmdgpuQueryGpuInfo = reinterpret_cast<AmdgpuQueryGpuInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_gpu_info"));
            m_funcs.pfnAmdgpuQuerySensorInfo = reinterpret_cast<AmdgpuQuerySensorInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_sensor_info"));
            m_funcs.pfnAmdgpuQueryInfo = reinterpret_cast<AmdgpuQueryInfo>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_info"));
            m_funcs.pfnAmdgpuQueryPrivateAperture = reinterpret_cast<AmdgpuQueryPrivateAperture>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_private_aperture"));
            m_funcs.pfnAmdgpuQuerySharedAperture = reinterpret_cast<AmdgpuQuerySharedAperture>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_query_shared_aperture"));
            m_funcs.pfnAmdgpuBoGetPhysAddress = reinterpret_cast<AmdgpuBoGetPhysAddress>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_bo_get_phys_address"));
            m_funcs.pfnAmdgpuCsReservedVmid = reinterpret_cast<AmdgpuCsReservedVmid>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_reserved_vmid"));
            m_funcs.pfnAmdgpuCsUnreservedVmid = reinterpret_cast<AmdgpuCsUnreservedVmid>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_unreserved_vmid"));
            m_funcs.pfnAmdgpuCsCreateSyncobj = reinterpret_cast<AmdgpuCsCreateSyncobj>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_create_syncobj"));
            m_funcs.pfnAmdgpuCsCreateSyncobj2 = reinterpret_cast<AmdgpuCsCreateSyncobj2>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_create_syncobj2"));
            m_funcs.pfnAmdgpuCsDestroySyncobj = reinterpret_cast<AmdgpuCsDestroySyncobj>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_destroy_syncobj"));
            m_funcs.pfnAmdgpuCsExportSyncobj = reinterpret_cast<AmdgpuCsExportSyncobj>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_export_syncobj"));
            m_funcs.pfnAmdgpuCsImportSyncobj = reinterpret_cast<AmdgpuCsImportSyncobj>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_import_syncobj"));
            m_funcs.pfnAmdgpuCsSubmitRaw = reinterpret_cast<AmdgpuCsSubmitRaw>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_submit_raw"));
            m_funcs.pfnAmdgpuCsChunkFenceToDep = reinterpret_cast<AmdgpuCsChunkFenceToDep>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_chunk_fence_to_dep"));
            m_funcs.pfnAmdgpuCsChunkFenceInfoToData = reinterpret_cast<AmdgpuCsChunkFenceInfoToData>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_chunk_fence_info_to_data"));
            m_funcs.pfnAmdgpuCsSyncobjImportSyncFile = reinterpret_cast<AmdgpuCsSyncobjImportSyncFile>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_syncobj_import_sync_file"));
            m_funcs.pfnAmdgpuCsSyncobjExportSyncFile = reinterpret_cast<AmdgpuCsSyncobjExportSyncFile>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_syncobj_export_sync_file"));
            m_funcs.pfnAmdgpuCsSyncobjWait = reinterpret_cast<AmdgpuCsSyncobjWait>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_syncobj_wait"));
            m_funcs.pfnAmdgpuCsSyncobjReset = reinterpret_cast<AmdgpuCsSyncobjReset>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_syncobj_reset"));
            m_funcs.pfnAmdgpuCsSyncobjSignal = reinterpret_cast<AmdgpuCsSyncobjSignal>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_syncobj_signal"));
            m_funcs.pfnAmdgpuCsCtxCreate2 = reinterpret_cast<AmdgpuCsCtxCreate2>(dlsym(
                        m_libraryHandles[LibDrmAmdgpu],
                        "amdgpu_cs_ctx_create2"));
        }

        // resolve symbols from libdrm.so.2
        m_libraryHandles[LibDrm] = dlopen(LibNames[LibDrm], RTLD_LAZY);
        if (m_libraryHandles[LibDrm] == nullptr)
        {
            result = Result::ErrorUnavailable;
        }
        else
        {
            m_funcs.pfnDrmGetNodeTypeFromFd = reinterpret_cast<DrmGetNodeTypeFromFd>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmGetNodeTypeFromFd"));
            m_funcs.pfnDrmGetRenderDeviceNameFromFd = reinterpret_cast<DrmGetRenderDeviceNameFromFd>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmGetRenderDeviceNameFromFd"));
            m_funcs.pfnDrmGetDevices = reinterpret_cast<DrmGetDevices>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmGetDevices"));
            m_funcs.pfnDrmFreeDevices = reinterpret_cast<DrmFreeDevices>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmFreeDevices"));
            m_funcs.pfnDrmGetBusid = reinterpret_cast<DrmGetBusid>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmGetBusid"));
            m_funcs.pfnDrmFreeBusid = reinterpret_cast<DrmFreeBusid>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmFreeBusid"));
            m_funcs.pfnDrmModeGetResources = reinterpret_cast<DrmModeGetResources>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetResources"));
            m_funcs.pfnDrmModeFreeResources = reinterpret_cast<DrmModeFreeResources>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreeResources"));
            m_funcs.pfnDrmModeGetConnector = reinterpret_cast<DrmModeGetConnector>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetConnector"));
            m_funcs.pfnDrmModeFreeConnector = reinterpret_cast<DrmModeFreeConnector>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreeConnector"));
            m_funcs.pfnDrmGetCap = reinterpret_cast<DrmGetCap>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmGetCap"));
            m_funcs.pfnDrmSyncobjCreate = reinterpret_cast<DrmSyncobjCreate>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmSyncobjCreate"));
            m_funcs.pfnDrmModeFreePlane = reinterpret_cast<DrmModeFreePlane>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreePlane"));
            m_funcs.pfnDrmModeFreePlaneResources = reinterpret_cast<DrmModeFreePlaneResources>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreePlaneResources"));
            m_funcs.pfnDrmModeGetPlaneResources = reinterpret_cast<DrmModeGetPlaneResources>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetPlaneResources"));
            m_funcs.pfnDrmModeGetPlane = reinterpret_cast<DrmModeGetPlane>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetPlane"));
            m_funcs.pfnDrmDropMaster = reinterpret_cast<DrmDropMaster>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmDropMaster"));
            m_funcs.pfnDrmPrimeFDToHandle = reinterpret_cast<DrmPrimeFDToHandle>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmPrimeFDToHandle"));
            m_funcs.pfnDrmModeAddFB2 = reinterpret_cast<DrmModeAddFB2>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeAddFB2"));
            m_funcs.pfnDrmModePageFlip = reinterpret_cast<DrmModePageFlip>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModePageFlip"));
            m_funcs.pfnDrmModeGetEncoder = reinterpret_cast<DrmModeGetEncoder>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetEncoder"));
            m_funcs.pfnDrmModeFreeEncoder = reinterpret_cast<DrmModeFreeEncoder>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreeEncoder"));
            m_funcs.pfnDrmModeSetCrtc = reinterpret_cast<DrmModeSetCrtc>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeSetCrtc"));
            m_funcs.pfnDrmModeGetConnectorCurrent = reinterpret_cast<DrmModeGetConnectorCurrent>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetConnectorCurrent"));
            m_funcs.pfnDrmModeGetCrtc = reinterpret_cast<DrmModeGetCrtc>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetCrtc"));
            m_funcs.pfnDrmModeFreeCrtc = reinterpret_cast<DrmModeFreeCrtc>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreeCrtc"));
            m_funcs.pfnDrmCrtcGetSequence = reinterpret_cast<DrmCrtcGetSequence>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmCrtcGetSequence"));
            m_funcs.pfnDrmCrtcQueueSequence = reinterpret_cast<DrmCrtcQueueSequence>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmCrtcQueueSequence"));
            m_funcs.pfnDrmHandleEvent = reinterpret_cast<DrmHandleEvent>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmHandleEvent"));
            m_funcs.pfnDrmIoctl = reinterpret_cast<DrmIoctl>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmIoctl"));
            m_funcs.pfnDrmModeGetProperty = reinterpret_cast<DrmModeGetProperty>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeGetProperty"));
            m_funcs.pfnDrmModeFreeProperty = reinterpret_cast<DrmModeFreeProperty>(dlsym(
                        m_libraryHandles[LibDrm],
                        "drmModeFreeProperty"));
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

void
DrmLoader::SpecializedInit(Platform* pPlatform, char* pDtifLibName)
{
}

} //namespace Linux
} //namespace Pal
