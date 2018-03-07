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

#pragma once

#include "pal.h"
#include "core/os/lnx/lnxHeaders.h"
#include "palFile.h"
namespace Pal
{
namespace Linux
{
// symbols from libdrm_amdgpu.so.1
typedef int (*AmdgpuQueryHwIpInfo)(
            amdgpu_device_handle              hDevice,
            unsigned                          type,
            unsigned                          ipInstance,
            struct drm_amdgpu_info_hw_ip*     pInfo);

typedef int (*AmdgpuBoVaOp)(
            amdgpu_bo_handle  hBuffer,
            uint64_t          offset,
            uint64_t          size,
            uint64_t          address,
            uint64_t          flags,
            uint32_t          ops);

typedef int (*AmdgpuBoVaOpRaw)(
            amdgpu_device_handle  hDevice,
            amdgpu_bo_handle      hBuffer,
            uint64_t              offset,
            uint64_t              size,
            uint64_t              address,
            uint64_t              flags,
            uint32_t              ops);

typedef int (*AmdgpuCsCreateSemaphore)(
            amdgpu_semaphore_handle*  pSemaphore);

typedef int (*AmdgpuCsSignalSemaphore)(
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_semaphore_handle   hSemaphore);

typedef int (*AmdgpuCsWaitSemaphore)(
            amdgpu_context_handle     hConext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_semaphore_handle   hSemaphore);

typedef int (*AmdgpuCsDestroySemaphore)(
            amdgpu_semaphore_handle   hSemaphore);

typedef int (*AmdgpuCsCreateSem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle*    pSemaphore);

typedef int (*AmdgpuCsSignalSem)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_sem_handle         hSemaphore);

typedef int (*AmdgpuCsWaitSem)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_sem_handle         hSemaphore);

typedef int (*AmdgpuCsExportSem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore,
            int*                  pSharedFd);

typedef int (*AmdgpuCsImportSem)(
            amdgpu_device_handle  hDevice,
            int                   fd,
            amdgpu_sem_handle*    pSemaphore);

typedef int (*AmdgpuCsDestroySem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore);

typedef const char* (*AmdgpuGetMarketingName)(
            amdgpu_device_handle  hDevice);

typedef int (*AmdgpuVaRangeFree)(
            amdgpu_va_handle  hVaRange);

typedef int (*AmdgpuVaRangeQuery)(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  type,
            uint64_t*                 pStart,
            uint64_t*                 pEnd);

typedef int (*AmdgpuVaRangeAlloc)(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  vaRangeType,
            uint64_t                  size,
            uint64_t                  vaBaseAlignment,
            uint64_t                  vaBaseRequired,
            uint64_t*                 pVaAllocated,
            amdgpu_va_handle*         pVaRange,
            uint64_t                  flags);

typedef int (*AmdgpuReadMmRegisters)(
            amdgpu_device_handle  hDevice,
            unsigned              dwordOffset,
            unsigned              count,
            uint32_t              instance,
            uint32_t              flags,
            uint32_t*             pValues);

typedef int (*AmdgpuDeviceInitialize)(
            int                       fd,
            uint32_t*                 pMajorVersion,
            uint32_t*                 pMinorVersion,
            amdgpu_device_handle*     pDeviceHandle);

typedef int (*AmdgpuDeviceDeinitialize)(
            amdgpu_device_handle  hDevice);

typedef int (*AmdgpuBoAlloc)(
            amdgpu_device_handle              hDevice,
            struct amdgpu_bo_alloc_request*   pAllocBuffer,
            amdgpu_bo_handle*                 pBufferHandle);

typedef int (*AmdgpuBoSetMetadata)(
            amdgpu_bo_handle              hBuffer,
            struct amdgpu_bo_metadata*    pInfo);

typedef int (*AmdgpuBoQueryInfo)(
            amdgpu_bo_handle          hBuffer,
            struct amdgpu_bo_info*    pInfo);

typedef int (*AmdgpuBoExport)(
            amdgpu_bo_handle              hBuffer,
            enum amdgpu_bo_handle_type    type,
            uint32_t*                     pFd);

typedef int (*AmdgpuBoImport)(
            amdgpu_device_handle              hDevice,
            enum amdgpu_bo_handle_type        type,
            uint32_t                          fd,
            struct amdgpu_bo_import_result*   pOutput);

typedef int (*AmdgpuCreateBoFromUserMem)(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle);

typedef int (*AmdgpuCreateBoFromPhysMem)(
            amdgpu_device_handle  hDevice,
            uint64_t              physAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle);

typedef int (*AmdgpuFindBoByCpuMapping)(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle,
            uint64_t*             pOffsetInBuffer);

typedef int (*AmdgpuBoFree)(
            amdgpu_bo_handle  hBuffer);

typedef int (*AmdgpuBoCpuMap)(
            amdgpu_bo_handle  hBuffer,
            void**            ppCpuAddress);

typedef int (*AmdgpuBoCpuUnmap)(
            amdgpu_bo_handle  hBuffer);

typedef int (*AmdgpuBoWaitForIdle)(
            amdgpu_bo_handle  hBuffer,
            uint64_t          timeoutInNs,
            bool*             pBufferBusy);

typedef int (*AmdgpuBoListCreate)(
            amdgpu_device_handle      hDevice,
            uint32_t                  numberOfResources,
            amdgpu_bo_handle*         pResources,
            uint8_t*                  pResourcePriorities,
            amdgpu_bo_list_handle*    pBoListHandle);

typedef int (*AmdgpuBoListDestroy)(
            amdgpu_bo_list_handle     hBoList);

typedef int (*AmdgpuCsCtxCreate)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle*    pContextHandle);

typedef int (*AmdgpuCsCtxFree)(
            amdgpu_context_handle     hContext);

typedef int (*AmdgpuCsSubmit)(
            amdgpu_context_handle         hContext,
            uint64_t                      flags,
            struct amdgpu_cs_request*     pIbsRequest,
            uint32_t                      numberOfRequests);

typedef int (*AmdgpuCsQueryFenceStatus)(
            struct amdgpu_cs_fence*   pFence,
            uint64_t                  timeoutInNs,
            uint64_t                  flags,
            uint32_t*                 pExpired);

typedef int (*AmdgpuCsWaitFences)(
            struct amdgpu_cs_fence*   pFences,
            uint32_t                  fenceCount,
            bool                      waitAll,
            uint64_t                  timeoutInNs,
            uint32_t*                 pStatus,
            uint32_t*                 pFirst);

typedef int (*AmdgpuQueryBufferSizeAlignment)(
            amdgpu_device_handle                      hDevice,
            struct amdgpu_buffer_size_alignments*     pInfo);

typedef int (*AmdgpuQueryFirmwareVersion)(
            amdgpu_device_handle  hDevice,
            unsigned              fwType,
            unsigned              ipInstance,
            unsigned              index,
            uint32_t*             pVersion,
            uint32_t*             pFeature);

typedef int (*AmdgpuQueryHwIpCount)(
            amdgpu_device_handle  hDevice,
            unsigned              type,
            uint32_t*             pCount);

typedef int (*AmdgpuQueryHeapInfo)(
            amdgpu_device_handle      hDevice,
            uint32_t                  heap,
            uint32_t                  flags,
            struct amdgpu_heap_info*  pInfo);

typedef int (*AmdgpuQueryGpuInfo)(
            amdgpu_device_handle      hDevice,
            struct amdgpu_gpu_info*   pInfo);

typedef int (*AmdgpuQuerySensorInfo)(
            amdgpu_device_handle  hDevice,
            unsigned              sensor_type,
            unsigned              size,
            void*                 value);

typedef int (*AmdgpuQueryInfo)(
            amdgpu_device_handle  hDevice,
            unsigned              infoId,
            unsigned              size,
            void*                 pValue);

typedef int (*AmdgpuQueryPrivateAperture)(
            amdgpu_device_handle  hDevice,
            uint64_t*             pStartVa,
            uint64_t*             pEndVa);

typedef int (*AmdgpuQuerySharedAperture)(
            amdgpu_device_handle  hDevice,
            uint64_t*             pStartVa,
            uint64_t*             pEndVa);

typedef int (*AmdgpuBoGetPhysAddress)(
            amdgpu_bo_handle  hBuffer,
            uint64_t*         pPhysAddress);

typedef int (*AmdgpuCsReservedVmid)(
            amdgpu_device_handle  hDevice);

typedef int (*AmdgpuCsUnreservedVmid)(
            amdgpu_device_handle  hDevice);

typedef int (*AmdgpuCsCreateSyncobj)(
            amdgpu_device_handle  hDevice,
            uint32_t*             pSyncObj);

typedef int (*AmdgpuCsDestroySyncobj)(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj);

typedef int (*AmdgpuCsExportSyncobj)(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int*                  pSharedFd);

typedef int (*AmdgpuCsImportSyncobj)(
            amdgpu_device_handle  hDevice,
            int                   sharedFd,
            uint32_t*             pSyncObj);

typedef int (*AmdgpuCsSubmitRaw)(
            amdgpu_device_handle          hDevice,
            amdgpu_context_handle         hContext,
            amdgpu_bo_list_handle         hBuffer,
            int                           numChunks,
            struct drm_amdgpu_cs_chunk*   pChunks,
            uint64_t*                     pSeqNo);

typedef void (*AmdgpuCsChunkFenceToDep)(
            struct amdgpu_cs_fence*           pFence,
            struct drm_amdgpu_cs_chunk_dep    pDep);

typedef void (*AmdgpuCsChunkFenceInfoToData)(
            struct amdgpu_cs_fence_info       fenceInfo,
            struct drm_amdgpu_cs_chunk_data*  pData);

typedef int (*AmdgpuCsSyncobjImportSyncFile)(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int                   syncFileFd);

typedef int (*AmdgpuCsSyncobjExportSyncFile)(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int*                  pSyncFileFd);

typedef int (*AmdgpuCsSyncobjWait)(
            amdgpu_device_handle  hDevice,
            uint32_t*             pHandles,
            unsigned              numHandles,
            int64_t               timeoutInNs,
            unsigned              flags,
            uint32_t*             pFirstSignaled);

typedef int (*AmdgpuCsSyncobjReset)(
            amdgpu_device_handle  hDevice,
            const uint32_t*       pHandles,
            uint32_t              numHandles);

typedef int (*AmdgpuCsCtxCreate2)(
            amdgpu_device_handle      hDevice,
            uint32_t                  priority,
            amdgpu_context_handle*    pContextHandle);

// symbols from libdrm.so.2
typedef int (*DrmGetNodeTypeFromFd)(
            int   fd);

typedef char* (*DrmGetRenderDeviceNameFromFd)(
            int   fd);

typedef int (*DrmGetDevices)(
            drmDevicePtr*     pDevices,
            int               maxDevices);

typedef void (*DrmFreeDevices)(
            drmDevicePtr*     pDevices,
            int               count);

typedef char* (*DrmGetBusid)(
            int   fd);

typedef void (*DrmFreeBusid)(
            const char*   pBusId);

typedef drmModeResPtr (*DrmModeGetResources)(
            int   fd);

typedef void (*DrmModeFreeResources)(
            drmModeResPtr     ptr);

typedef drmModeConnectorPtr (*DrmModeGetConnector)(
            int       fd,
            uint32_t  connectorId);

typedef void (*DrmModeFreeConnector)(
            drmModeConnectorPtr   ptr);

typedef int (*DrmGetCap)(
            int           fd,
            uint64_t      capability,
            uint64_t*     pValue);

typedef int (*DrmSyncobjCreate)(
            int           fd,
            uint32_t      flags,
            uint32_t*     pHandle);

enum DrmLoaderLibraries : uint32
{
    LibDrmAmdgpu = 0,
    LibDrm = 1,
    DrmLoaderLibrariesCount = 2
};

struct DrmLoaderFuncs
{
    AmdgpuQueryHwIpInfo               pfnAmdgpuQueryHwIpInfo;
    bool pfnAmdgpuQueryHwIpInfoisValid() const
    {
        return (pfnAmdgpuQueryHwIpInfo != nullptr);
    }

    AmdgpuBoVaOp                      pfnAmdgpuBoVaOp;
    bool pfnAmdgpuBoVaOpisValid() const
    {
        return (pfnAmdgpuBoVaOp != nullptr);
    }

    AmdgpuBoVaOpRaw                   pfnAmdgpuBoVaOpRaw;
    bool pfnAmdgpuBoVaOpRawisValid() const
    {
        return (pfnAmdgpuBoVaOpRaw != nullptr);
    }

    AmdgpuCsCreateSemaphore           pfnAmdgpuCsCreateSemaphore;
    bool pfnAmdgpuCsCreateSemaphoreisValid() const
    {
        return (pfnAmdgpuCsCreateSemaphore != nullptr);
    }

    AmdgpuCsSignalSemaphore           pfnAmdgpuCsSignalSemaphore;
    bool pfnAmdgpuCsSignalSemaphoreisValid() const
    {
        return (pfnAmdgpuCsSignalSemaphore != nullptr);
    }

    AmdgpuCsWaitSemaphore             pfnAmdgpuCsWaitSemaphore;
    bool pfnAmdgpuCsWaitSemaphoreisValid() const
    {
        return (pfnAmdgpuCsWaitSemaphore != nullptr);
    }

    AmdgpuCsDestroySemaphore          pfnAmdgpuCsDestroySemaphore;
    bool pfnAmdgpuCsDestroySemaphoreisValid() const
    {
        return (pfnAmdgpuCsDestroySemaphore != nullptr);
    }

    AmdgpuCsCreateSem                 pfnAmdgpuCsCreateSem;
    bool pfnAmdgpuCsCreateSemisValid() const
    {
        return (pfnAmdgpuCsCreateSem != nullptr);
    }

    AmdgpuCsSignalSem                 pfnAmdgpuCsSignalSem;
    bool pfnAmdgpuCsSignalSemisValid() const
    {
        return (pfnAmdgpuCsSignalSem != nullptr);
    }

    AmdgpuCsWaitSem                   pfnAmdgpuCsWaitSem;
    bool pfnAmdgpuCsWaitSemisValid() const
    {
        return (pfnAmdgpuCsWaitSem != nullptr);
    }

    AmdgpuCsExportSem                 pfnAmdgpuCsExportSem;
    bool pfnAmdgpuCsExportSemisValid() const
    {
        return (pfnAmdgpuCsExportSem != nullptr);
    }

    AmdgpuCsImportSem                 pfnAmdgpuCsImportSem;
    bool pfnAmdgpuCsImportSemisValid() const
    {
        return (pfnAmdgpuCsImportSem != nullptr);
    }

    AmdgpuCsDestroySem                pfnAmdgpuCsDestroySem;
    bool pfnAmdgpuCsDestroySemisValid() const
    {
        return (pfnAmdgpuCsDestroySem != nullptr);
    }

    AmdgpuGetMarketingName            pfnAmdgpuGetMarketingName;
    bool pfnAmdgpuGetMarketingNameisValid() const
    {
        return (pfnAmdgpuGetMarketingName != nullptr);
    }

    AmdgpuVaRangeFree                 pfnAmdgpuVaRangeFree;
    bool pfnAmdgpuVaRangeFreeisValid() const
    {
        return (pfnAmdgpuVaRangeFree != nullptr);
    }

    AmdgpuVaRangeQuery                pfnAmdgpuVaRangeQuery;
    bool pfnAmdgpuVaRangeQueryisValid() const
    {
        return (pfnAmdgpuVaRangeQuery != nullptr);
    }

    AmdgpuVaRangeAlloc                pfnAmdgpuVaRangeAlloc;
    bool pfnAmdgpuVaRangeAllocisValid() const
    {
        return (pfnAmdgpuVaRangeAlloc != nullptr);
    }

    AmdgpuReadMmRegisters             pfnAmdgpuReadMmRegisters;
    bool pfnAmdgpuReadMmRegistersisValid() const
    {
        return (pfnAmdgpuReadMmRegisters != nullptr);
    }

    AmdgpuDeviceInitialize            pfnAmdgpuDeviceInitialize;
    bool pfnAmdgpuDeviceInitializeisValid() const
    {
        return (pfnAmdgpuDeviceInitialize != nullptr);
    }

    AmdgpuDeviceDeinitialize          pfnAmdgpuDeviceDeinitialize;
    bool pfnAmdgpuDeviceDeinitializeisValid() const
    {
        return (pfnAmdgpuDeviceDeinitialize != nullptr);
    }

    AmdgpuBoAlloc                     pfnAmdgpuBoAlloc;
    bool pfnAmdgpuBoAllocisValid() const
    {
        return (pfnAmdgpuBoAlloc != nullptr);
    }

    AmdgpuBoSetMetadata               pfnAmdgpuBoSetMetadata;
    bool pfnAmdgpuBoSetMetadataisValid() const
    {
        return (pfnAmdgpuBoSetMetadata != nullptr);
    }

    AmdgpuBoQueryInfo                 pfnAmdgpuBoQueryInfo;
    bool pfnAmdgpuBoQueryInfoisValid() const
    {
        return (pfnAmdgpuBoQueryInfo != nullptr);
    }

    AmdgpuBoExport                    pfnAmdgpuBoExport;
    bool pfnAmdgpuBoExportisValid() const
    {
        return (pfnAmdgpuBoExport != nullptr);
    }

    AmdgpuBoImport                    pfnAmdgpuBoImport;
    bool pfnAmdgpuBoImportisValid() const
    {
        return (pfnAmdgpuBoImport != nullptr);
    }

    AmdgpuCreateBoFromUserMem         pfnAmdgpuCreateBoFromUserMem;
    bool pfnAmdgpuCreateBoFromUserMemisValid() const
    {
        return (pfnAmdgpuCreateBoFromUserMem != nullptr);
    }

    AmdgpuCreateBoFromPhysMem         pfnAmdgpuCreateBoFromPhysMem;
    bool pfnAmdgpuCreateBoFromPhysMemisValid() const
    {
        return (pfnAmdgpuCreateBoFromPhysMem != nullptr);
    }

    AmdgpuFindBoByCpuMapping          pfnAmdgpuFindBoByCpuMapping;
    bool pfnAmdgpuFindBoByCpuMappingisValid() const
    {
        return (pfnAmdgpuFindBoByCpuMapping != nullptr);
    }

    AmdgpuBoFree                      pfnAmdgpuBoFree;
    bool pfnAmdgpuBoFreeisValid() const
    {
        return (pfnAmdgpuBoFree != nullptr);
    }

    AmdgpuBoCpuMap                    pfnAmdgpuBoCpuMap;
    bool pfnAmdgpuBoCpuMapisValid() const
    {
        return (pfnAmdgpuBoCpuMap != nullptr);
    }

    AmdgpuBoCpuUnmap                  pfnAmdgpuBoCpuUnmap;
    bool pfnAmdgpuBoCpuUnmapisValid() const
    {
        return (pfnAmdgpuBoCpuUnmap != nullptr);
    }

    AmdgpuBoWaitForIdle               pfnAmdgpuBoWaitForIdle;
    bool pfnAmdgpuBoWaitForIdleisValid() const
    {
        return (pfnAmdgpuBoWaitForIdle != nullptr);
    }

    AmdgpuBoListCreate                pfnAmdgpuBoListCreate;
    bool pfnAmdgpuBoListCreateisValid() const
    {
        return (pfnAmdgpuBoListCreate != nullptr);
    }

    AmdgpuBoListDestroy               pfnAmdgpuBoListDestroy;
    bool pfnAmdgpuBoListDestroyisValid() const
    {
        return (pfnAmdgpuBoListDestroy != nullptr);
    }

    AmdgpuCsCtxCreate                 pfnAmdgpuCsCtxCreate;
    bool pfnAmdgpuCsCtxCreateisValid() const
    {
        return (pfnAmdgpuCsCtxCreate != nullptr);
    }

    AmdgpuCsCtxFree                   pfnAmdgpuCsCtxFree;
    bool pfnAmdgpuCsCtxFreeisValid() const
    {
        return (pfnAmdgpuCsCtxFree != nullptr);
    }

    AmdgpuCsSubmit                    pfnAmdgpuCsSubmit;
    bool pfnAmdgpuCsSubmitisValid() const
    {
        return (pfnAmdgpuCsSubmit != nullptr);
    }

    AmdgpuCsQueryFenceStatus          pfnAmdgpuCsQueryFenceStatus;
    bool pfnAmdgpuCsQueryFenceStatusisValid() const
    {
        return (pfnAmdgpuCsQueryFenceStatus != nullptr);
    }

    AmdgpuCsWaitFences                pfnAmdgpuCsWaitFences;
    bool pfnAmdgpuCsWaitFencesisValid() const
    {
        return (pfnAmdgpuCsWaitFences != nullptr);
    }

    AmdgpuQueryBufferSizeAlignment    pfnAmdgpuQueryBufferSizeAlignment;
    bool pfnAmdgpuQueryBufferSizeAlignmentisValid() const
    {
        return (pfnAmdgpuQueryBufferSizeAlignment != nullptr);
    }

    AmdgpuQueryFirmwareVersion        pfnAmdgpuQueryFirmwareVersion;
    bool pfnAmdgpuQueryFirmwareVersionisValid() const
    {
        return (pfnAmdgpuQueryFirmwareVersion != nullptr);
    }

    AmdgpuQueryHwIpCount              pfnAmdgpuQueryHwIpCount;
    bool pfnAmdgpuQueryHwIpCountisValid() const
    {
        return (pfnAmdgpuQueryHwIpCount != nullptr);
    }

    AmdgpuQueryHeapInfo               pfnAmdgpuQueryHeapInfo;
    bool pfnAmdgpuQueryHeapInfoisValid() const
    {
        return (pfnAmdgpuQueryHeapInfo != nullptr);
    }

    AmdgpuQueryGpuInfo                pfnAmdgpuQueryGpuInfo;
    bool pfnAmdgpuQueryGpuInfoisValid() const
    {
        return (pfnAmdgpuQueryGpuInfo != nullptr);
    }

    AmdgpuQuerySensorInfo             pfnAmdgpuQuerySensorInfo;
    bool pfnAmdgpuQuerySensorInfoisValid() const
    {
        return (pfnAmdgpuQuerySensorInfo != nullptr);
    }

    AmdgpuQueryInfo                   pfnAmdgpuQueryInfo;
    bool pfnAmdgpuQueryInfoisValid() const
    {
        return (pfnAmdgpuQueryInfo != nullptr);
    }

    AmdgpuQueryPrivateAperture        pfnAmdgpuQueryPrivateAperture;
    bool pfnAmdgpuQueryPrivateApertureisValid() const
    {
        return (pfnAmdgpuQueryPrivateAperture != nullptr);
    }

    AmdgpuQuerySharedAperture         pfnAmdgpuQuerySharedAperture;
    bool pfnAmdgpuQuerySharedApertureisValid() const
    {
        return (pfnAmdgpuQuerySharedAperture != nullptr);
    }

    AmdgpuBoGetPhysAddress            pfnAmdgpuBoGetPhysAddress;
    bool pfnAmdgpuBoGetPhysAddressisValid() const
    {
        return (pfnAmdgpuBoGetPhysAddress != nullptr);
    }

    AmdgpuCsReservedVmid              pfnAmdgpuCsReservedVmid;
    bool pfnAmdgpuCsReservedVmidisValid() const
    {
        return (pfnAmdgpuCsReservedVmid != nullptr);
    }

    AmdgpuCsUnreservedVmid            pfnAmdgpuCsUnreservedVmid;
    bool pfnAmdgpuCsUnreservedVmidisValid() const
    {
        return (pfnAmdgpuCsUnreservedVmid != nullptr);
    }

    AmdgpuCsCreateSyncobj             pfnAmdgpuCsCreateSyncobj;
    bool pfnAmdgpuCsCreateSyncobjisValid() const
    {
        return (pfnAmdgpuCsCreateSyncobj != nullptr);
    }

    AmdgpuCsDestroySyncobj            pfnAmdgpuCsDestroySyncobj;
    bool pfnAmdgpuCsDestroySyncobjisValid() const
    {
        return (pfnAmdgpuCsDestroySyncobj != nullptr);
    }

    AmdgpuCsExportSyncobj             pfnAmdgpuCsExportSyncobj;
    bool pfnAmdgpuCsExportSyncobjisValid() const
    {
        return (pfnAmdgpuCsExportSyncobj != nullptr);
    }

    AmdgpuCsImportSyncobj             pfnAmdgpuCsImportSyncobj;
    bool pfnAmdgpuCsImportSyncobjisValid() const
    {
        return (pfnAmdgpuCsImportSyncobj != nullptr);
    }

    AmdgpuCsSubmitRaw                 pfnAmdgpuCsSubmitRaw;
    bool pfnAmdgpuCsSubmitRawisValid() const
    {
        return (pfnAmdgpuCsSubmitRaw != nullptr);
    }

    AmdgpuCsChunkFenceToDep           pfnAmdgpuCsChunkFenceToDep;
    bool pfnAmdgpuCsChunkFenceToDepisValid() const
    {
        return (pfnAmdgpuCsChunkFenceToDep != nullptr);
    }

    AmdgpuCsChunkFenceInfoToData      pfnAmdgpuCsChunkFenceInfoToData;
    bool pfnAmdgpuCsChunkFenceInfoToDataisValid() const
    {
        return (pfnAmdgpuCsChunkFenceInfoToData != nullptr);
    }

    AmdgpuCsSyncobjImportSyncFile     pfnAmdgpuCsSyncobjImportSyncFile;
    bool pfnAmdgpuCsSyncobjImportSyncFileisValid() const
    {
        return (pfnAmdgpuCsSyncobjImportSyncFile != nullptr);
    }

    AmdgpuCsSyncobjExportSyncFile     pfnAmdgpuCsSyncobjExportSyncFile;
    bool pfnAmdgpuCsSyncobjExportSyncFileisValid() const
    {
        return (pfnAmdgpuCsSyncobjExportSyncFile != nullptr);
    }

    AmdgpuCsSyncobjWait               pfnAmdgpuCsSyncobjWait;
    bool pfnAmdgpuCsSyncobjWaitisValid() const
    {
        return (pfnAmdgpuCsSyncobjWait != nullptr);
    }

    AmdgpuCsSyncobjReset              pfnAmdgpuCsSyncobjReset;
    bool pfnAmdgpuCsSyncobjResetisValid() const
    {
        return (pfnAmdgpuCsSyncobjReset != nullptr);
    }

    AmdgpuCsCtxCreate2                pfnAmdgpuCsCtxCreate2;
    bool pfnAmdgpuCsCtxCreate2isValid() const
    {
        return (pfnAmdgpuCsCtxCreate2 != nullptr);
    }

    DrmGetNodeTypeFromFd              pfnDrmGetNodeTypeFromFd;
    bool pfnDrmGetNodeTypeFromFdisValid() const
    {
        return (pfnDrmGetNodeTypeFromFd != nullptr);
    }

    DrmGetRenderDeviceNameFromFd      pfnDrmGetRenderDeviceNameFromFd;
    bool pfnDrmGetRenderDeviceNameFromFdisValid() const
    {
        return (pfnDrmGetRenderDeviceNameFromFd != nullptr);
    }

    DrmGetDevices                     pfnDrmGetDevices;
    bool pfnDrmGetDevicesisValid() const
    {
        return (pfnDrmGetDevices != nullptr);
    }

    DrmFreeDevices                    pfnDrmFreeDevices;
    bool pfnDrmFreeDevicesisValid() const
    {
        return (pfnDrmFreeDevices != nullptr);
    }

    DrmGetBusid                       pfnDrmGetBusid;
    bool pfnDrmGetBusidisValid() const
    {
        return (pfnDrmGetBusid != nullptr);
    }

    DrmFreeBusid                      pfnDrmFreeBusid;
    bool pfnDrmFreeBusidisValid() const
    {
        return (pfnDrmFreeBusid != nullptr);
    }

    DrmModeGetResources               pfnDrmModeGetResources;
    bool pfnDrmModeGetResourcesisValid() const
    {
        return (pfnDrmModeGetResources != nullptr);
    }

    DrmModeFreeResources              pfnDrmModeFreeResources;
    bool pfnDrmModeFreeResourcesisValid() const
    {
        return (pfnDrmModeFreeResources != nullptr);
    }

    DrmModeGetConnector               pfnDrmModeGetConnector;
    bool pfnDrmModeGetConnectorisValid() const
    {
        return (pfnDrmModeGetConnector != nullptr);
    }

    DrmModeFreeConnector              pfnDrmModeFreeConnector;
    bool pfnDrmModeFreeConnectorisValid() const
    {
        return (pfnDrmModeFreeConnector != nullptr);
    }

    DrmGetCap                         pfnDrmGetCap;
    bool pfnDrmGetCapisValid() const
    {
        return (pfnDrmGetCap != nullptr);
    }

    DrmSyncobjCreate                  pfnDrmSyncobjCreate;
    bool pfnDrmSyncobjCreateisValid() const
    {
        return (pfnDrmSyncobjCreate != nullptr);
    }

};

// =====================================================================================================================
// the class serves as a proxy layer to add more functionality to wrapped callbacks.
#if defined(PAL_DEBUG_PRINTS)
class DrmLoaderFuncsProxy
{
public:
    DrmLoaderFuncsProxy() { }
    ~DrmLoaderFuncsProxy() { }

    void SetFuncCalls(DrmLoaderFuncs* pFuncs) { m_pFuncs = pFuncs; }

    void Init(const char* pPath);

    int pfnAmdgpuQueryHwIpInfo(
            amdgpu_device_handle              hDevice,
            unsigned                          type,
            unsigned                          ipInstance,
            struct drm_amdgpu_info_hw_ip*     pInfo) const;

    bool pfnAmdgpuQueryHwIpInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHwIpInfo != nullptr);
    }

    int pfnAmdgpuBoVaOp(
            amdgpu_bo_handle  hBuffer,
            uint64_t          offset,
            uint64_t          size,
            uint64_t          address,
            uint64_t          flags,
            uint32_t          ops) const;

    bool pfnAmdgpuBoVaOpisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoVaOp != nullptr);
    }

    int pfnAmdgpuBoVaOpRaw(
            amdgpu_device_handle  hDevice,
            amdgpu_bo_handle      hBuffer,
            uint64_t              offset,
            uint64_t              size,
            uint64_t              address,
            uint64_t              flags,
            uint32_t              ops) const;

    bool pfnAmdgpuBoVaOpRawisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoVaOpRaw != nullptr);
    }

    int pfnAmdgpuCsCreateSemaphore(
            amdgpu_semaphore_handle*  pSemaphore) const;

    bool pfnAmdgpuCsCreateSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSemaphore != nullptr);
    }

    int pfnAmdgpuCsSignalSemaphore(
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsSignalSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSignalSemaphore != nullptr);
    }

    int pfnAmdgpuCsWaitSemaphore(
            amdgpu_context_handle     hConext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsWaitSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitSemaphore != nullptr);
    }

    int pfnAmdgpuCsDestroySemaphore(
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsDestroySemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsDestroySemaphore != nullptr);
    }

    int pfnAmdgpuCsCreateSem(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle*    pSemaphore) const;

    bool pfnAmdgpuCsCreateSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSem != nullptr);
    }

    int pfnAmdgpuCsSignalSem(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_sem_handle         hSemaphore) const;

    bool pfnAmdgpuCsSignalSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSignalSem != nullptr);
    }

    int pfnAmdgpuCsWaitSem(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32_t                  ipType,
            uint32_t                  ipInstance,
            uint32_t                  ring,
            amdgpu_sem_handle         hSemaphore) const;

    bool pfnAmdgpuCsWaitSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitSem != nullptr);
    }

    int pfnAmdgpuCsExportSem(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore,
            int*                  pSharedFd) const;

    bool pfnAmdgpuCsExportSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsExportSem != nullptr);
    }

    int pfnAmdgpuCsImportSem(
            amdgpu_device_handle  hDevice,
            int                   fd,
            amdgpu_sem_handle*    pSemaphore) const;

    bool pfnAmdgpuCsImportSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsImportSem != nullptr);
    }

    int pfnAmdgpuCsDestroySem(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore) const;

    bool pfnAmdgpuCsDestroySemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsDestroySem != nullptr);
    }

    const char* pfnAmdgpuGetMarketingName(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuGetMarketingNameisValid() const
    {
        return (m_pFuncs->pfnAmdgpuGetMarketingName != nullptr);
    }

    int pfnAmdgpuVaRangeFree(
            amdgpu_va_handle  hVaRange) const;

    bool pfnAmdgpuVaRangeFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeFree != nullptr);
    }

    int pfnAmdgpuVaRangeQuery(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  type,
            uint64_t*                 pStart,
            uint64_t*                 pEnd) const;

    bool pfnAmdgpuVaRangeQueryisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeQuery != nullptr);
    }

    int pfnAmdgpuVaRangeAlloc(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  vaRangeType,
            uint64_t                  size,
            uint64_t                  vaBaseAlignment,
            uint64_t                  vaBaseRequired,
            uint64_t*                 pVaAllocated,
            amdgpu_va_handle*         pVaRange,
            uint64_t                  flags) const;

    bool pfnAmdgpuVaRangeAllocisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeAlloc != nullptr);
    }

    int pfnAmdgpuReadMmRegisters(
            amdgpu_device_handle  hDevice,
            unsigned              dwordOffset,
            unsigned              count,
            uint32_t              instance,
            uint32_t              flags,
            uint32_t*             pValues) const;

    bool pfnAmdgpuReadMmRegistersisValid() const
    {
        return (m_pFuncs->pfnAmdgpuReadMmRegisters != nullptr);
    }

    int pfnAmdgpuDeviceInitialize(
            int                       fd,
            uint32_t*                 pMajorVersion,
            uint32_t*                 pMinorVersion,
            amdgpu_device_handle*     pDeviceHandle) const;

    bool pfnAmdgpuDeviceInitializeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuDeviceInitialize != nullptr);
    }

    int pfnAmdgpuDeviceDeinitialize(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuDeviceDeinitializeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuDeviceDeinitialize != nullptr);
    }

    int pfnAmdgpuBoAlloc(
            amdgpu_device_handle              hDevice,
            struct amdgpu_bo_alloc_request*   pAllocBuffer,
            amdgpu_bo_handle*                 pBufferHandle) const;

    bool pfnAmdgpuBoAllocisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoAlloc != nullptr);
    }

    int pfnAmdgpuBoSetMetadata(
            amdgpu_bo_handle              hBuffer,
            struct amdgpu_bo_metadata*    pInfo) const;

    bool pfnAmdgpuBoSetMetadataisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoSetMetadata != nullptr);
    }

    int pfnAmdgpuBoQueryInfo(
            amdgpu_bo_handle          hBuffer,
            struct amdgpu_bo_info*    pInfo) const;

    bool pfnAmdgpuBoQueryInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoQueryInfo != nullptr);
    }

    int pfnAmdgpuBoExport(
            amdgpu_bo_handle              hBuffer,
            enum amdgpu_bo_handle_type    type,
            uint32_t*                     pFd) const;

    bool pfnAmdgpuBoExportisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoExport != nullptr);
    }

    int pfnAmdgpuBoImport(
            amdgpu_device_handle              hDevice,
            enum amdgpu_bo_handle_type        type,
            uint32_t                          fd,
            struct amdgpu_bo_import_result*   pOutput) const;

    bool pfnAmdgpuBoImportisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoImport != nullptr);
    }

    int pfnAmdgpuCreateBoFromUserMem(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle) const;

    bool pfnAmdgpuCreateBoFromUserMemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCreateBoFromUserMem != nullptr);
    }

    int pfnAmdgpuCreateBoFromPhysMem(
            amdgpu_device_handle  hDevice,
            uint64_t              physAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle) const;

    bool pfnAmdgpuCreateBoFromPhysMemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCreateBoFromPhysMem != nullptr);
    }

    int pfnAmdgpuFindBoByCpuMapping(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64_t              size,
            amdgpu_bo_handle*     pBufferHandle,
            uint64_t*             pOffsetInBuffer) const;

    bool pfnAmdgpuFindBoByCpuMappingisValid() const
    {
        return (m_pFuncs->pfnAmdgpuFindBoByCpuMapping != nullptr);
    }

    int pfnAmdgpuBoFree(
            amdgpu_bo_handle  hBuffer) const;

    bool pfnAmdgpuBoFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoFree != nullptr);
    }

    int pfnAmdgpuBoCpuMap(
            amdgpu_bo_handle  hBuffer,
            void**            ppCpuAddress) const;

    bool pfnAmdgpuBoCpuMapisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoCpuMap != nullptr);
    }

    int pfnAmdgpuBoCpuUnmap(
            amdgpu_bo_handle  hBuffer) const;

    bool pfnAmdgpuBoCpuUnmapisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoCpuUnmap != nullptr);
    }

    int pfnAmdgpuBoWaitForIdle(
            amdgpu_bo_handle  hBuffer,
            uint64_t          timeoutInNs,
            bool*             pBufferBusy) const;

    bool pfnAmdgpuBoWaitForIdleisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoWaitForIdle != nullptr);
    }

    int pfnAmdgpuBoListCreate(
            amdgpu_device_handle      hDevice,
            uint32_t                  numberOfResources,
            amdgpu_bo_handle*         pResources,
            uint8_t*                  pResourcePriorities,
            amdgpu_bo_list_handle*    pBoListHandle) const;

    bool pfnAmdgpuBoListCreateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListCreate != nullptr);
    }

    int pfnAmdgpuBoListDestroy(
            amdgpu_bo_list_handle     hBoList) const;

    bool pfnAmdgpuBoListDestroyisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListDestroy != nullptr);
    }

    int pfnAmdgpuCsCtxCreate(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle*    pContextHandle) const;

    bool pfnAmdgpuCsCtxCreateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxCreate != nullptr);
    }

    int pfnAmdgpuCsCtxFree(
            amdgpu_context_handle     hContext) const;

    bool pfnAmdgpuCsCtxFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxFree != nullptr);
    }

    int pfnAmdgpuCsSubmit(
            amdgpu_context_handle         hContext,
            uint64_t                      flags,
            struct amdgpu_cs_request*     pIbsRequest,
            uint32_t                      numberOfRequests) const;

    bool pfnAmdgpuCsSubmitisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSubmit != nullptr);
    }

    int pfnAmdgpuCsQueryFenceStatus(
            struct amdgpu_cs_fence*   pFence,
            uint64_t                  timeoutInNs,
            uint64_t                  flags,
            uint32_t*                 pExpired) const;

    bool pfnAmdgpuCsQueryFenceStatusisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsQueryFenceStatus != nullptr);
    }

    int pfnAmdgpuCsWaitFences(
            struct amdgpu_cs_fence*   pFences,
            uint32_t                  fenceCount,
            bool                      waitAll,
            uint64_t                  timeoutInNs,
            uint32_t*                 pStatus,
            uint32_t*                 pFirst) const;

    bool pfnAmdgpuCsWaitFencesisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitFences != nullptr);
    }

    int pfnAmdgpuQueryBufferSizeAlignment(
            amdgpu_device_handle                      hDevice,
            struct amdgpu_buffer_size_alignments*     pInfo) const;

    bool pfnAmdgpuQueryBufferSizeAlignmentisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryBufferSizeAlignment != nullptr);
    }

    int pfnAmdgpuQueryFirmwareVersion(
            amdgpu_device_handle  hDevice,
            unsigned              fwType,
            unsigned              ipInstance,
            unsigned              index,
            uint32_t*             pVersion,
            uint32_t*             pFeature) const;

    bool pfnAmdgpuQueryFirmwareVersionisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryFirmwareVersion != nullptr);
    }

    int pfnAmdgpuQueryHwIpCount(
            amdgpu_device_handle  hDevice,
            unsigned              type,
            uint32_t*             pCount) const;

    bool pfnAmdgpuQueryHwIpCountisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHwIpCount != nullptr);
    }

    int pfnAmdgpuQueryHeapInfo(
            amdgpu_device_handle      hDevice,
            uint32_t                  heap,
            uint32_t                  flags,
            struct amdgpu_heap_info*  pInfo) const;

    bool pfnAmdgpuQueryHeapInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHeapInfo != nullptr);
    }

    int pfnAmdgpuQueryGpuInfo(
            amdgpu_device_handle      hDevice,
            struct amdgpu_gpu_info*   pInfo) const;

    bool pfnAmdgpuQueryGpuInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryGpuInfo != nullptr);
    }

    int pfnAmdgpuQuerySensorInfo(
            amdgpu_device_handle  hDevice,
            unsigned              sensor_type,
            unsigned              size,
            void*                 value) const;

    bool pfnAmdgpuQuerySensorInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQuerySensorInfo != nullptr);
    }

    int pfnAmdgpuQueryInfo(
            amdgpu_device_handle  hDevice,
            unsigned              infoId,
            unsigned              size,
            void*                 pValue) const;

    bool pfnAmdgpuQueryInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryInfo != nullptr);
    }

    int pfnAmdgpuQueryPrivateAperture(
            amdgpu_device_handle  hDevice,
            uint64_t*             pStartVa,
            uint64_t*             pEndVa) const;

    bool pfnAmdgpuQueryPrivateApertureisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryPrivateAperture != nullptr);
    }

    int pfnAmdgpuQuerySharedAperture(
            amdgpu_device_handle  hDevice,
            uint64_t*             pStartVa,
            uint64_t*             pEndVa) const;

    bool pfnAmdgpuQuerySharedApertureisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQuerySharedAperture != nullptr);
    }

    int pfnAmdgpuBoGetPhysAddress(
            amdgpu_bo_handle  hBuffer,
            uint64_t*         pPhysAddress) const;

    bool pfnAmdgpuBoGetPhysAddressisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoGetPhysAddress != nullptr);
    }

    int pfnAmdgpuCsReservedVmid(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuCsReservedVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsReservedVmid != nullptr);
    }

    int pfnAmdgpuCsUnreservedVmid(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuCsUnreservedVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsUnreservedVmid != nullptr);
    }

    int pfnAmdgpuCsCreateSyncobj(
            amdgpu_device_handle  hDevice,
            uint32_t*             pSyncObj) const;

    bool pfnAmdgpuCsCreateSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSyncobj != nullptr);
    }

    int pfnAmdgpuCsDestroySyncobj(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj) const;

    bool pfnAmdgpuCsDestroySyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsDestroySyncobj != nullptr);
    }

    int pfnAmdgpuCsExportSyncobj(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int*                  pSharedFd) const;

    bool pfnAmdgpuCsExportSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsExportSyncobj != nullptr);
    }

    int pfnAmdgpuCsImportSyncobj(
            amdgpu_device_handle  hDevice,
            int                   sharedFd,
            uint32_t*             pSyncObj) const;

    bool pfnAmdgpuCsImportSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsImportSyncobj != nullptr);
    }

    int pfnAmdgpuCsSubmitRaw(
            amdgpu_device_handle          hDevice,
            amdgpu_context_handle         hContext,
            amdgpu_bo_list_handle         hBuffer,
            int                           numChunks,
            struct drm_amdgpu_cs_chunk*   pChunks,
            uint64_t*                     pSeqNo) const;

    bool pfnAmdgpuCsSubmitRawisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSubmitRaw != nullptr);
    }

    void pfnAmdgpuCsChunkFenceToDep(
            struct amdgpu_cs_fence*           pFence,
            struct drm_amdgpu_cs_chunk_dep    pDep) const;

    bool pfnAmdgpuCsChunkFenceToDepisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsChunkFenceToDep != nullptr);
    }

    void pfnAmdgpuCsChunkFenceInfoToData(
            struct amdgpu_cs_fence_info       fenceInfo,
            struct drm_amdgpu_cs_chunk_data*  pData) const;

    bool pfnAmdgpuCsChunkFenceInfoToDataisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsChunkFenceInfoToData != nullptr);
    }

    int pfnAmdgpuCsSyncobjImportSyncFile(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int                   syncFileFd) const;

    bool pfnAmdgpuCsSyncobjImportSyncFileisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile != nullptr);
    }

    int pfnAmdgpuCsSyncobjExportSyncFile(
            amdgpu_device_handle  hDevice,
            uint32_t              syncObj,
            int*                  pSyncFileFd) const;

    bool pfnAmdgpuCsSyncobjExportSyncFileisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile != nullptr);
    }

    int pfnAmdgpuCsSyncobjWait(
            amdgpu_device_handle  hDevice,
            uint32_t*             pHandles,
            unsigned              numHandles,
            int64_t               timeoutInNs,
            unsigned              flags,
            uint32_t*             pFirstSignaled) const;

    bool pfnAmdgpuCsSyncobjWaitisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjWait != nullptr);
    }

    int pfnAmdgpuCsSyncobjReset(
            amdgpu_device_handle  hDevice,
            const uint32_t*       pHandles,
            uint32_t              numHandles) const;

    bool pfnAmdgpuCsSyncobjResetisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjReset != nullptr);
    }

    int pfnAmdgpuCsCtxCreate2(
            amdgpu_device_handle      hDevice,
            uint32_t                  priority,
            amdgpu_context_handle*    pContextHandle) const;

    bool pfnAmdgpuCsCtxCreate2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxCreate2 != nullptr);
    }

    int pfnDrmGetNodeTypeFromFd(
            int   fd) const;

    bool pfnDrmGetNodeTypeFromFdisValid() const
    {
        return (m_pFuncs->pfnDrmGetNodeTypeFromFd != nullptr);
    }

    char* pfnDrmGetRenderDeviceNameFromFd(
            int   fd) const;

    bool pfnDrmGetRenderDeviceNameFromFdisValid() const
    {
        return (m_pFuncs->pfnDrmGetRenderDeviceNameFromFd != nullptr);
    }

    int pfnDrmGetDevices(
            drmDevicePtr*     pDevices,
            int               maxDevices) const;

    bool pfnDrmGetDevicesisValid() const
    {
        return (m_pFuncs->pfnDrmGetDevices != nullptr);
    }

    void pfnDrmFreeDevices(
            drmDevicePtr*     pDevices,
            int               count) const;

    bool pfnDrmFreeDevicesisValid() const
    {
        return (m_pFuncs->pfnDrmFreeDevices != nullptr);
    }

    char* pfnDrmGetBusid(
            int   fd) const;

    bool pfnDrmGetBusidisValid() const
    {
        return (m_pFuncs->pfnDrmGetBusid != nullptr);
    }

    void pfnDrmFreeBusid(
            const char*   pBusId) const;

    bool pfnDrmFreeBusidisValid() const
    {
        return (m_pFuncs->pfnDrmFreeBusid != nullptr);
    }

    drmModeResPtr pfnDrmModeGetResources(
            int   fd) const;

    bool pfnDrmModeGetResourcesisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetResources != nullptr);
    }

    void pfnDrmModeFreeResources(
            drmModeResPtr     ptr) const;

    bool pfnDrmModeFreeResourcesisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeResources != nullptr);
    }

    drmModeConnectorPtr pfnDrmModeGetConnector(
            int       fd,
            uint32_t  connectorId) const;

    bool pfnDrmModeGetConnectorisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetConnector != nullptr);
    }

    void pfnDrmModeFreeConnector(
            drmModeConnectorPtr   ptr) const;

    bool pfnDrmModeFreeConnectorisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeConnector != nullptr);
    }

    int pfnDrmGetCap(
            int           fd,
            uint64_t      capability,
            uint64_t*     pValue) const;

    bool pfnDrmGetCapisValid() const
    {
        return (m_pFuncs->pfnDrmGetCap != nullptr);
    }

    int pfnDrmSyncobjCreate(
            int           fd,
            uint32_t      flags,
            uint32_t*     pHandle) const;

    bool pfnDrmSyncobjCreateisValid() const
    {
        return (m_pFuncs->pfnDrmSyncobjCreate != nullptr);
    }

private:
    Util::File  m_timeLogger;
    Util::File  m_paramLogger;
    DrmLoaderFuncs* m_pFuncs;

    PAL_DISALLOW_COPY_AND_ASSIGN(DrmLoaderFuncsProxy);
};
#endif

class Platform;
// =====================================================================================================================
// the class is responsible to resolve all external symbols that required by the Dri3WindowSystem.
class DrmLoader
{
public:
    DrmLoader();
    ~DrmLoader();
    bool   Initialized() { return m_initialized; }
    const DrmLoaderFuncs& GetProcsTable()const { return m_funcs; }
#if defined(PAL_DEBUG_PRINTS)
    const DrmLoaderFuncsProxy& GetProcsTableProxy()const { return m_proxy; }
    void SetLogPath(const char* pPath) { m_proxy.Init(pPath); }
#endif
    Result Init(Platform* pPlatform);

private:
    void* m_libraryHandles[DrmLoaderLibrariesCount];
    bool  m_initialized;
    DrmLoaderFuncs m_funcs;
#if defined(PAL_DEBUG_PRINTS)
    DrmLoaderFuncsProxy m_proxy;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(DrmLoader);
};

} //namespace Linux
} //namespace Pal
