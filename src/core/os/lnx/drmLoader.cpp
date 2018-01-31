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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryHwIpInfo(
    amdgpu_device_handle           hDevice,
    unsigned                       type,
    unsigned                       ipInstance,
    struct drm_amdgpu_info_hw_ip*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryHwIpInfo(hDevice,
                                               type,
                                               ipInstance,
                                               pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoVaOp(
    amdgpu_bo_handle  hBuffer,
    uint64_t          offset,
    uint64_t          size,
    uint64_t          address,
    uint64_t          flags,
    uint32_t          ops
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoVaOp(hBuffer,
                                        offset,
                                        size,
                                        address,
                                        flags,
                                        ops);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoVaOpRaw(
    amdgpu_device_handle  hDevice,
    amdgpu_bo_handle      hBuffer,
    uint64_t              offset,
    uint64_t              size,
    uint64_t              address,
    uint64_t              flags,
    uint32_t              ops
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoVaOpRaw(hDevice,
                                           hBuffer,
                                           offset,
                                           size,
                                           address,
                                           flags,
                                           ops);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSemaphore(
    amdgpu_semaphore_handle*  pSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCreateSemaphore(pSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCreateSemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCreateSemaphore(%p)\n",
        pSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsSignalSemaphore(
    amdgpu_context_handle    hContext,
    uint32_t                 ipType,
    uint32_t                 ipInstance,
    uint32_t                 ring,
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSignalSemaphore(hContext,
                                                   ipType,
                                                   ipInstance,
                                                   ring,
                                                   hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsWaitSemaphore(
    amdgpu_context_handle    hConext,
    uint32_t                 ipType,
    uint32_t                 ipInstance,
    uint32_t                 ring,
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsWaitSemaphore(hConext,
                                                 ipType,
                                                 ipInstance,
                                                 ring,
                                                 hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySemaphore(
    amdgpu_semaphore_handle  hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsDestroySemaphore(hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsDestroySemaphore,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsDestroySemaphore(%p)\n",
        hSemaphore);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle*    pSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCreateSem(hDevice,
                                             pSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsSignalSem(
    amdgpu_device_handle   hDevice,
    amdgpu_context_handle  hContext,
    uint32_t               ipType,
    uint32_t               ipInstance,
    uint32_t               ring,
    amdgpu_sem_handle      hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSignalSem(hDevice,
                                             hContext,
                                             ipType,
                                             ipInstance,
                                             ring,
                                             hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsWaitSem(
    amdgpu_device_handle   hDevice,
    amdgpu_context_handle  hContext,
    uint32_t               ipType,
    uint32_t               ipInstance,
    uint32_t               ring,
    amdgpu_sem_handle      hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsWaitSem(hDevice,
                                           hContext,
                                           ipType,
                                           ipInstance,
                                           ring,
                                           hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsExportSem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle     hSemaphore,
    int*                  pSharedFd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsExportSem(hDevice,
                                             hSemaphore,
                                             pSharedFd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsImportSem(
    amdgpu_device_handle  hDevice,
    int                   fd,
    amdgpu_sem_handle*    pSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsImportSem(hDevice,
                                             fd,
                                             pSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySem(
    amdgpu_device_handle  hDevice,
    amdgpu_sem_handle     hSemaphore
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsDestroySem(hDevice,
                                              hSemaphore);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    const char* pRet = m_pFuncs->pfnAmdgpuGetMarketingName(hDevice);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuGetMarketingName,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuGetMarketingName(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuVaRangeFree(
    amdgpu_va_handle  hVaRange
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuVaRangeFree(hVaRange);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuVaRangeFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuVaRangeFree(%p)\n",
        hVaRange);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuVaRangeQuery(
    amdgpu_device_handle      hDevice,
    enum amdgpu_gpu_va_range  type,
    uint64_t*                 pStart,
    uint64_t*                 pEnd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuVaRangeQuery(hDevice,
                                              type,
                                              pStart,
                                              pEnd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuVaRangeAlloc(
    amdgpu_device_handle      hDevice,
    enum amdgpu_gpu_va_range  vaRangeType,
    uint64_t                  size,
    uint64_t                  vaBaseAlignment,
    uint64_t                  vaBaseRequired,
    uint64_t*                 pVaAllocated,
    amdgpu_va_handle*         pVaRange,
    uint64_t                  flags
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuVaRangeAlloc(hDevice,
                                              vaRangeType,
                                              size,
                                              vaBaseAlignment,
                                              vaBaseRequired,
                                              pVaAllocated,
                                              pVaRange,
                                              flags);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuReadMmRegisters(
    amdgpu_device_handle  hDevice,
    unsigned              dwordOffset,
    unsigned              count,
    uint32_t              instance,
    uint32_t              flags,
    uint32_t*             pValues
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuReadMmRegisters(hDevice,
                                                 dwordOffset,
                                                 count,
                                                 instance,
                                                 flags,
                                                 pValues);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuDeviceInitialize(
    int                    fd,
    uint32_t*              pMajorVersion,
    uint32_t*              pMinorVersion,
    amdgpu_device_handle*  pDeviceHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuDeviceInitialize(fd,
                                                  pMajorVersion,
                                                  pMinorVersion,
                                                  pDeviceHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuDeviceDeinitialize(
    amdgpu_device_handle  hDevice
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuDeviceDeinitialize(hDevice);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuDeviceDeinitialize,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuDeviceDeinitialize(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuBoAlloc(
    amdgpu_device_handle             hDevice,
    struct amdgpu_bo_alloc_request*  pAllocBuffer,
    amdgpu_bo_handle*                pBufferHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoAlloc(hDevice,
                                         pAllocBuffer,
                                         pBufferHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoSetMetadata(
    amdgpu_bo_handle            hBuffer,
    struct amdgpu_bo_metadata*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoSetMetadata(hBuffer,
                                               pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoQueryInfo(
    amdgpu_bo_handle        hBuffer,
    struct amdgpu_bo_info*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoQueryInfo(hBuffer,
                                             pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoExport(
    amdgpu_bo_handle            hBuffer,
    enum amdgpu_bo_handle_type  type,
    uint32_t*                   pFd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoExport(hBuffer,
                                          type,
                                          pFd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoImport(
    amdgpu_device_handle             hDevice,
    enum amdgpu_bo_handle_type       type,
    uint32_t                         fd,
    struct amdgpu_bo_import_result*  pOutput
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoImport(hDevice,
                                          type,
                                          fd,
                                          pOutput);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCreateBoFromUserMem(
    amdgpu_device_handle  hDevice,
    void*                 pCpuAddress,
    uint64_t              size,
    amdgpu_bo_handle*     pBufferHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCreateBoFromUserMem(hDevice,
                                                     pCpuAddress,
                                                     size,
                                                     pBufferHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCreateBoFromPhysMem(
    amdgpu_device_handle  hDevice,
    uint64_t              physAddress,
    uint64_t              size,
    amdgpu_bo_handle*     pBufferHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCreateBoFromPhysMem(hDevice,
                                                     physAddress,
                                                     size,
                                                     pBufferHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCreateBoFromPhysMem,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCreateBoFromPhysMem(%p, %p, %lx, %p)\n",
        hDevice,
        physAddress,
        size,
        pBufferHandle);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuFindBoByCpuMapping(
    amdgpu_device_handle  hDevice,
    void*                 pCpuAddress,
    uint64_t              size,
    amdgpu_bo_handle*     pBufferHandle,
    uint64_t*             pOffsetInBuffer
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuFindBoByCpuMapping(hDevice,
                                                    pCpuAddress,
                                                    size,
                                                    pBufferHandle,
                                                    pOffsetInBuffer);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoFree(
    amdgpu_bo_handle  hBuffer
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoFree(hBuffer);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoFree(%p)\n",
        hBuffer);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuBoCpuMap(
    amdgpu_bo_handle  hBuffer,
    void**            ppCpuAddress
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoCpuMap(hBuffer,
                                          ppCpuAddress);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoCpuUnmap(
    amdgpu_bo_handle  hBuffer
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoCpuUnmap(hBuffer);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoCpuUnmap,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoCpuUnmap(%p)\n",
        hBuffer);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuBoWaitForIdle(
    amdgpu_bo_handle  hBuffer,
    uint64_t          timeoutInNs,
    bool*             pBufferBusy
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoWaitForIdle(hBuffer,
                                               timeoutInNs,
                                               pBufferBusy);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoListCreate(
    amdgpu_device_handle    hDevice,
    uint32_t                numberOfResources,
    amdgpu_bo_handle*       pResources,
    uint8_t*                pResourcePriorities,
    amdgpu_bo_list_handle*  pBoListHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoListCreate(hDevice,
                                              numberOfResources,
                                              pResources,
                                              pResourcePriorities,
                                              pBoListHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoListDestroy(
    amdgpu_bo_list_handle  hBoList
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoListDestroy(hBoList);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuBoListDestroy,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuBoListDestroy(%p)\n",
        hBoList);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsCtxCreate(
    amdgpu_device_handle    hDevice,
    amdgpu_context_handle*  pContextHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCtxCreate(hDevice,
                                             pContextHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsCtxFree(
    amdgpu_context_handle  hContext
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCtxFree(hContext);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsCtxFree,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsCtxFree(%p)\n",
        hContext);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsSubmit(
    amdgpu_context_handle      hContext,
    uint64_t                   flags,
    struct amdgpu_cs_request*  pIbsRequest,
    uint32_t                   numberOfRequests
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSubmit(hContext,
                                          flags,
                                          pIbsRequest,
                                          numberOfRequests);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsQueryFenceStatus(
    struct amdgpu_cs_fence*  pFence,
    uint64_t                 timeoutInNs,
    uint64_t                 flags,
    uint32_t*                pExpired
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsQueryFenceStatus(pFence,
                                                    timeoutInNs,
                                                    flags,
                                                    pExpired);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsWaitFences(
    struct amdgpu_cs_fence*  pFences,
    uint32_t                 fenceCount,
    bool                     waitAll,
    uint64_t                 timeoutInNs,
    uint32_t*                pStatus,
    uint32_t*                pFirst
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsWaitFences(pFences,
                                              fenceCount,
                                              waitAll,
                                              timeoutInNs,
                                              pStatus,
                                              pFirst);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryBufferSizeAlignment(
    amdgpu_device_handle                   hDevice,
    struct amdgpu_buffer_size_alignments*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryBufferSizeAlignment(hDevice,
                                                          pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryFirmwareVersion(
    amdgpu_device_handle  hDevice,
    unsigned              fwType,
    unsigned              ipInstance,
    unsigned              index,
    uint32_t*             pVersion,
    uint32_t*             pFeature
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryFirmwareVersion(hDevice,
                                                      fwType,
                                                      ipInstance,
                                                      index,
                                                      pVersion,
                                                      pFeature);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryHwIpCount(
    amdgpu_device_handle  hDevice,
    unsigned              type,
    uint32_t*             pCount
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryHwIpCount(hDevice,
                                                type,
                                                pCount);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryHeapInfo(
    amdgpu_device_handle      hDevice,
    uint32_t                  heap,
    uint32_t                  flags,
    struct amdgpu_heap_info*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryHeapInfo(hDevice,
                                               heap,
                                               flags,
                                               pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryGpuInfo(
    amdgpu_device_handle     hDevice,
    struct amdgpu_gpu_info*  pInfo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryGpuInfo(hDevice,
                                              pInfo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryInfo(
    amdgpu_device_handle  hDevice,
    unsigned              infoId,
    unsigned              size,
    void*                 pValue
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryInfo(hDevice,
                                           infoId,
                                           size,
                                           pValue);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQueryPrivateAperture(
    amdgpu_device_handle  hDevice,
    uint64_t*             pStartVa,
    uint64_t*             pEndVa
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQueryPrivateAperture(hDevice,
                                                      pStartVa,
                                                      pEndVa);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuQuerySharedAperture(
    amdgpu_device_handle  hDevice,
    uint64_t*             pStartVa,
    uint64_t*             pEndVa
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuQuerySharedAperture(hDevice,
                                                     pStartVa,
                                                     pEndVa);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuBoGetPhysAddress(
    amdgpu_bo_handle    hBuffer,
    uint64_t*           pPhysAddress
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuBoGetPhysAddress(hBuffer,
                                                  pPhysAddress);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsReservedVmid(
    amdgpu_device_handle  hDevice
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsReservedVmid(hDevice);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsReservedVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsReservedVmid(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsUnreservedVmid(
    amdgpu_device_handle  hDevice
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsUnreservedVmid(hDevice);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsUnreservedVmid,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsUnreservedVmid(%p)\n",
        hDevice);
    m_paramLogger.Flush();

    return ret;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsCreateSyncobj(
    amdgpu_device_handle  hDevice,
    uint32_t*             pSyncObj
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCreateSyncobj(hDevice,
                                                 pSyncObj);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsDestroySyncobj(
    amdgpu_device_handle  hDevice,
    uint32_t              syncObj
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsDestroySyncobj(hDevice,
                                                  syncObj);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsExportSyncobj(
    amdgpu_device_handle  hDevice,
    uint32_t              syncObj,
    int*                  pSharedFd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsExportSyncobj(hDevice,
                                                 syncObj,
                                                 pSharedFd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsImportSyncobj(
    amdgpu_device_handle  hDevice,
    int                   sharedFd,
    uint32_t*             pSyncObj
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsImportSyncobj(hDevice,
                                                 sharedFd,
                                                 pSyncObj);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsSubmitRaw(
    amdgpu_device_handle         hDevice,
    amdgpu_context_handle        hContext,
    amdgpu_bo_list_handle        hBuffer,
    int                          numChunks,
    struct drm_amdgpu_cs_chunk*  pChunks,
    uint64_t*                    pSeqNo
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSubmitRaw(hDevice,
                                             hContext,
                                             hBuffer,
                                             numChunks,
                                             pChunks,
                                             pSeqNo);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnAmdgpuCsChunkFenceToDep(pFence,
                                         pDep);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnAmdgpuCsChunkFenceInfoToData(fenceInfo,
                                              pData);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("AmdgpuCsChunkFenceInfoToData,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "AmdgpuCsChunkFenceInfoToData(%x, %p)\n",
        fenceInfo,
        pData);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjImportSyncFile(
    amdgpu_device_handle  hDevice,
    uint32_t              syncObj,
    int                   syncFileFd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile(hDevice,
                                                         syncObj,
                                                         syncFileFd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsSyncobjExportSyncFile(
    amdgpu_device_handle  hDevice,
    uint32_t              syncObj,
    int*                  pSyncFileFd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile(hDevice,
                                                         syncObj,
                                                         pSyncFileFd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnAmdgpuCsCtxCreate2(
    amdgpu_device_handle    hDevice,
    uint32_t                priority,
    amdgpu_context_handle*  pContextHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnAmdgpuCsCtxCreate2(hDevice,
                                              priority,
                                              pContextHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnDrmGetNodeTypeFromFd(
    int  fd
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmGetNodeTypeFromFd(fd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnDrmGetRenderDeviceNameFromFd(fd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("DrmGetRenderDeviceNameFromFd,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmGetRenderDeviceNameFromFd(%x)\n",
        fd);
    m_paramLogger.Flush();

    return pRet;
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmGetDevices(
    drmDevicePtr*  pDevices,
    int            maxDevices
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmGetDevices(pDevices,
                                         maxDevices);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int            count
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeDevices(pDevices,
                                count);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    char* pRet = m_pFuncs->pfnDrmGetBusid(fd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmFreeBusid(pBusId);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    drmModeResPtr ret = m_pFuncs->pfnDrmModeGetResources(fd);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeResources(ptr);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeResources,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeResources(%p)\n",
        ptr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
drmModeConnectorPtr DrmLoaderFuncsProxy::pfnDrmModeGetConnector(
    int       fd,
    uint32_t  connectorId
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    drmModeConnectorPtr ret = m_pFuncs->pfnDrmModeGetConnector(fd,
                                                               connectorId);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
    int64 begin = Util::GetPerfCpuTime();
    m_pFuncs->pfnDrmModeFreeConnector(ptr);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
    m_timeLogger.Printf("DrmModeFreeConnector,%ld,%ld,%ld\n", begin, end, elapse);
    m_timeLogger.Flush();

    m_paramLogger.Printf(
        "DrmModeFreeConnector(%p)\n",
        ptr);
    m_paramLogger.Flush();
}

// =====================================================================================================================
int DrmLoaderFuncsProxy::pfnDrmGetCap(
    int        fd,
    uint64_t   capability,
    uint64_t*  pValue
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmGetCap(fd,
                                     capability,
                                     pValue);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
int DrmLoaderFuncsProxy::pfnDrmSyncobjCreate(
    int        fd,
    uint32_t   flags,
    uint32_t*  pHandle
    ) const
{
    int64 begin = Util::GetPerfCpuTime();
    int ret = m_pFuncs->pfnDrmSyncobjCreate(fd,
                                            flags,
                                            pHandle);
    int64 end = Util::GetPerfCpuTime();
    int64 elapse = end - begin;
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
        "libdrm.so.2"
    };

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

} //namespace Linux
} //namespace Pal
