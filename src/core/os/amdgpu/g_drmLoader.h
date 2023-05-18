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

#pragma once

#include "core/os/amdgpu/amdgpuHeaders.h"
#include "palFile.h"
#include "palLibrary.h"

namespace Pal
{
namespace Amdgpu
{
// symbols from libdrm_amdgpu.so.1
typedef int32 (*AmdgpuQueryHwIpInfo)(
            amdgpu_device_handle              hDevice,
            uint32                            type,
            uint32                            ipInstance,
            struct drm_amdgpu_info_hw_ip*     pInfo);

typedef int32 (*AmdgpuBoVaOp)(
            amdgpu_bo_handle  hBuffer,
            uint64            offset,
            uint64            size,
            uint64            address,
            uint64            flags,
            uint32            ops);

typedef int32 (*AmdgpuBoVaOpRaw)(
            amdgpu_device_handle  hDevice,
            amdgpu_bo_handle      hBuffer,
            uint64                offset,
            uint64                size,
            uint64                address,
            uint64                flags,
            uint32                ops);

typedef int32 (*AmdgpuCsCreateSemaphore)(
            amdgpu_semaphore_handle*  pSemaphore);

typedef int32 (*AmdgpuCsSignalSemaphore)(
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_semaphore_handle   hSemaphore);

typedef int32 (*AmdgpuCsWaitSemaphore)(
            amdgpu_context_handle     hConext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_semaphore_handle   hSemaphore);

typedef int32 (*AmdgpuCsDestroySemaphore)(
            amdgpu_semaphore_handle   hSemaphore);

typedef int32 (*AmdgpuCsCreateSem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle*    pSemaphore);

typedef int32 (*AmdgpuCsSignalSem)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_sem_handle         hSemaphore);

typedef int32 (*AmdgpuCsWaitSem)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_sem_handle         hSemaphore);

typedef int32 (*AmdgpuCsExportSem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore,
            int32*                pSharedFd);

typedef int32 (*AmdgpuCsImportSem)(
            amdgpu_device_handle  hDevice,
            int32                 fd,
            amdgpu_sem_handle*    pSemaphore);

typedef int32 (*AmdgpuCsDestroySem)(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore);

typedef const char* (*AmdgpuGetMarketingName)(
            amdgpu_device_handle  hDevice);

typedef int32 (*AmdgpuVaRangeFree)(
            amdgpu_va_handle  hVaRange);

typedef int32 (*AmdgpuVaRangeQuery)(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  type,
            uint64*                   pStart,
            uint64*                   pEnd);

typedef int32 (*AmdgpuVaRangeAlloc)(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  vaRangeType,
            uint64                    size,
            uint64                    vaBaseAlignment,
            uint64                    vaBaseRequired,
            uint64*                   pVaAllocated,
            amdgpu_va_handle*         pVaRange,
            uint64                    flags);

typedef int32 (*AmdgpuVmReserveVmid)(
            amdgpu_device_handle  hDevice,
            uint32                flags);

typedef int32 (*AmdgpuVmUnreserveVmid)(
            amdgpu_device_handle  hDevice,
            uint32                flags);

typedef int32 (*AmdgpuReadMmRegisters)(
            amdgpu_device_handle  hDevice,
            uint32                dwordOffset,
            uint32                count,
            uint32                instance,
            uint32                flags,
            uint32*               pValues);

typedef int32 (*AmdgpuDeviceInitialize)(
            int                       fd,
            uint32*                   pMajorVersion,
            uint32*                   pMinorVersion,
            amdgpu_device_handle*     pDeviceHandle);

typedef int32 (*AmdgpuDeviceDeinitialize)(
            amdgpu_device_handle  hDevice);

typedef int32 (*AmdgpuBoAlloc)(
            amdgpu_device_handle              hDevice,
            struct amdgpu_bo_alloc_request*   pAllocBuffer,
            amdgpu_bo_handle*                 pBufferHandle);

typedef int32 (*AmdgpuBoSetMetadata)(
            amdgpu_bo_handle              hBuffer,
            struct amdgpu_bo_metadata*    pInfo);

typedef int32 (*AmdgpuBoQueryInfo)(
            amdgpu_bo_handle          hBuffer,
            struct amdgpu_bo_info*    pInfo);

typedef int32 (*AmdgpuBoExport)(
            amdgpu_bo_handle              hBuffer,
            enum amdgpu_bo_handle_type    type,
            uint32*                       pFd);

typedef int32 (*AmdgpuBoImport)(
            amdgpu_device_handle              hDevice,
            enum amdgpu_bo_handle_type        type,
            uint32                            fd,
            struct amdgpu_bo_import_result*   pOutput);

typedef int32 (*AmdgpuCreateBoFromUserMem)(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle);

typedef int32 (*AmdgpuCreateBoFromPhysMem)(
            amdgpu_device_handle  hDevice,
            uint64                physAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle);

typedef int32 (*AmdgpuFindBoByCpuMapping)(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle,
            uint64*               pOffsetInBuffer);

typedef int32 (*AmdgpuBoFree)(
            amdgpu_bo_handle  hBuffer);

typedef int32 (*AmdgpuBoCpuMap)(
            amdgpu_bo_handle  hBuffer,
            void**            ppCpuAddress);

typedef int32 (*AmdgpuBoCpuUnmap)(
            amdgpu_bo_handle  hBuffer);

typedef int32 (*AmdgpuBoRemapSecure)(
            amdgpu_bo_handle  buf_handle,
            bool              secure_map);

typedef int32 (*AmdgpuBoWaitForIdle)(
            amdgpu_bo_handle  hBuffer,
            uint64            timeoutInNs,
            bool*             pBufferBusy);

typedef int32 (*AmdgpuBoListCreate)(
            amdgpu_device_handle      hDevice,
            uint32                    numberOfResources,
            amdgpu_bo_handle*         pResources,
            uint8*                    pResourcePriorities,
            amdgpu_bo_list_handle*    pBoListHandle);

typedef int32 (*AmdgpuBoListDestroy)(
            amdgpu_bo_list_handle     hBoList);

typedef int32 (*AmdgpuBoListCreateRaw)(
            amdgpu_device_handle              hDevice,
            uint32                            numberOfResources,
            struct drm_amdgpu_bo_list_entry*  pBoListEntry,
            uint32*                           pBoListHandle);

typedef int32 (*AmdgpuBoListDestroyRaw)(
            amdgpu_device_handle  hDevice,
            uint32                boListHandle);

typedef int32 (*AmdgpuCsQueryResetState)(
            amdgpu_context_handle     context,
            uint32_t *                state,
            uint32_t *                hangs);

typedef int32 (*AmdgpuCsQueryResetState2)(
            amdgpu_context_handle     hContext,
            uint64*                   flags);

typedef int32 (*AmdgpuCsCtxCreate)(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle*    pContextHandle);

typedef int32 (*AmdgpuCsCtxFree)(
            amdgpu_context_handle     hContext);

typedef int32 (*AmdgpuCsSubmit)(
            amdgpu_context_handle         hContext,
            uint64                        flags,
            struct amdgpu_cs_request*     pIbsRequest,
            uint32                        numberOfRequests);

typedef int32 (*AmdgpuCsQueryFenceStatus)(
            struct amdgpu_cs_fence*   pFence,
            uint64                    timeoutInNs,
            uint64                    flags,
            uint32*                   pExpired);

typedef int32 (*AmdgpuCsWaitFences)(
            struct amdgpu_cs_fence*   pFences,
            uint32                    fenceCount,
            bool                      waitAll,
            uint64                    timeoutInNs,
            uint32*                   pStatus,
            uint32*                   pFirst);

typedef int32 (*AmdgpuCsCtxStablePstate)(
            amdgpu_context_handle     context,
            uint32_t                  op,
            uint32_t                  flags,
            uint32_t *                out_flags);

typedef int32 (*AmdgpuQueryBufferSizeAlignment)(
            amdgpu_device_handle                      hDevice,
            struct amdgpu_buffer_size_alignments*     pInfo);

typedef int32 (*AmdgpuQueryFirmwareVersion)(
            amdgpu_device_handle  hDevice,
            uint32                fwType,
            uint32                ipInstance,
            uint32                index,
            uint32*               pVersion,
            uint32*               pFeature);

typedef int32 (*AmdgpuQueryVideoCapsInfo)(
            amdgpu_device_handle  hDevice,
            uint32                capType,
            uint32                size,
            void*                 pCaps);

typedef int32 (*AmdgpuQueryHwIpCount)(
            amdgpu_device_handle  hDevice,
            uint32                type,
            uint32*               pCount);

typedef int32 (*AmdgpuQueryHeapInfo)(
            amdgpu_device_handle      hDevice,
            uint32                    heap,
            uint32                    flags,
            struct amdgpu_heap_info*  pInfo);

typedef int32 (*AmdgpuQueryGpuInfo)(
            amdgpu_device_handle      hDevice,
            struct amdgpu_gpu_info*   pInfo);

typedef int32 (*AmdgpuQuerySensorInfo)(
            amdgpu_device_handle  hDevice,
            uint32                sensor_type,
            uint32                size,
            void*                 value);

typedef int32 (*AmdgpuQueryInfo)(
            amdgpu_device_handle  hDevice,
            uint32                infoId,
            uint32                size,
            void*                 pValue);

typedef int32 (*AmdgpuQueryPrivateAperture)(
            amdgpu_device_handle  hDevice,
            uint64*               pStartVa,
            uint64*               pEndVa);

typedef int32 (*AmdgpuQuerySharedAperture)(
            amdgpu_device_handle  hDevice,
            uint64*               pStartVa,
            uint64*               pEndVa);

typedef int32 (*AmdgpuBoGetPhysAddress)(
            amdgpu_bo_handle  hBuffer,
            uint64*           pPhysAddress);

typedef int32 (*AmdgpuCsReservedVmid)(
            amdgpu_device_handle  hDevice);

typedef int32 (*AmdgpuCsUnreservedVmid)(
            amdgpu_device_handle  hDevice);

typedef int32 (*AmdgpuCsCreateSyncobj)(
            amdgpu_device_handle  hDevice,
            uint32*               pSyncObj);

typedef int32 (*AmdgpuCsCreateSyncobj2)(
            amdgpu_device_handle  hDevice,
            uint32                flags,
            uint32*               pSyncObj);

typedef int32 (*AmdgpuCsDestroySyncobj)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj);

typedef int32 (*AmdgpuCsExportSyncobj)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32*                pSharedFd);

typedef int32 (*AmdgpuCsImportSyncobj)(
            amdgpu_device_handle  hDevice,
            int32                 sharedFd,
            uint32*               pSyncObj);

typedef int32 (*AmdgpuCsSubmitRaw2)(
            amdgpu_device_handle          dev,
            amdgpu_context_handle         context,
            uint32_t                      bo_list_handle,
            int                           num_chunks,
            struct drm_amdgpu_cs_chunk *  chunks,
            uint64_t *                    seq_no);

typedef void (*AmdgpuCsChunkFenceToDep)(
            struct amdgpu_cs_fence*           pFence,
            struct drm_amdgpu_cs_chunk_dep    pDep);

typedef void (*AmdgpuCsChunkFenceInfoToData)(
            struct amdgpu_cs_fence_info       fenceInfo,
            struct drm_amdgpu_cs_chunk_data*  pData);

typedef int32 (*AmdgpuCsSyncobjImportSyncFile)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32                 syncFileFd);

typedef int32 (*AmdgpuCsSyncobjImportSyncFile2)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            uint64                point,
            int32                 syncFileFd);

typedef int32 (*AmdgpuCsSyncobjExportSyncFile)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32*                pSyncFileFd);

typedef int32 (*AmdgpuCsSyncobjExportSyncFile2)(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            uint64                point,
            uint32                flags,
            int32*                pSyncFileFd);

typedef int32 (*AmdgpuCsSyncobjWait)(
            amdgpu_device_handle  hDevice,
            uint32*               pHandles,
            uint32                numHandles,
            int64                 timeoutInNs,
            uint32                flags,
            uint32*               pFirstSignaled);

typedef int32 (*AmdgpuCsSyncobjTimelineWait)(
            amdgpu_device_handle  hDevice,
            uint32*               pHandles,
            uint64*               points,
            uint32                numHandles,
            int64                 timeoutInNs,
            uint32                flags,
            uint32*               pFirstSignaled);

typedef int32 (*AmdgpuCsSyncobjReset)(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint32                numHandles);

typedef int32 (*AmdgpuCsSyncobjSignal)(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint32                numHandles);

typedef int32 (*AmdgpuCsSyncobjTimelineSignal)(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles);

typedef int32 (*AmdgpuCsSyncobjTransfer)(
            amdgpu_device_handle  hDevice,
            uint32                dst_handle,
            uint64                dst_point,
            uint32                src_handle,
            uint64                src_point,
            uint32                flags);

typedef int32 (*AmdgpuCsSyncobjQuery)(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles);

typedef int32 (*AmdgpuCsSyncobjQuery2)(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles,
            uint32                flags);

typedef int32 (*AmdgpuCsCtxCreate2)(
            amdgpu_device_handle      hDevice,
            uint32                    priority,
            amdgpu_context_handle*    pContextHandle);

typedef int32 (*AmdgpuCsCtxCreate3)(
            amdgpu_device_handle      hDevice,
            uint32                    priority,
            uint32_t                  flags,
            amdgpu_context_handle*    pContextHandle);

// symbols from libdrm.so.2
typedef drmVersionPtr (*DrmGetVersion)(
            int   fd);

typedef void (*DrmFreeVersion)(
            drmVersionPtr     v);

typedef int32 (*DrmGetNodeTypeFromFd)(
            int   fd);

typedef char* (*DrmGetRenderDeviceNameFromFd)(
            int   fd);

typedef int32 (*DrmGetDevices)(
            drmDevicePtr*     pDevices,
            int32             maxDevices);

typedef void (*DrmFreeDevices)(
            drmDevicePtr*     pDevices,
            int32             count);

typedef int32 (*DrmGetDevice2)(
            int               fd,
            uint32_t          flags,
            drmDevicePtr*     pDevice);

typedef void (*DrmFreeDevice)(
            drmDevicePtr*     pDevice);

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
            uint32    connectorId);

typedef void (*DrmModeFreeConnector)(
            drmModeConnectorPtr   ptr);

typedef int32 (*DrmGetCap)(
            int       fd,
            uint64    capability,
            uint64*   pValue);

typedef int32 (*DrmSetClientCap)(
            int       fd,
            uint64    capability,
            uint64    value);

typedef int32 (*DrmSyncobjCreate)(
            int       fd,
            uint32    flags,
            uint32*   pHandle);

typedef void (*DrmModeFreePlane)(
            drmModePlanePtr   pPlanePtr);

typedef void (*DrmModeFreePlaneResources)(
            drmModePlaneResPtr    pPlaneResPtr);

typedef drmModePlaneResPtr (*DrmModeGetPlaneResources)(
            int32     fd);

typedef drmModePlanePtr (*DrmModeGetPlane)(
            int32     fd,
            uint32    planeId);

typedef int32 (*DrmDropMaster)(
            int32     fd);

typedef int32 (*DrmPrimeFDToHandle)(
            int32     fd,
            int32     primeFd,
            uint32*   pHandle);

typedef int32 (*DrmModeAddFB2)(
            int32         fd,
            uint32        width,
            uint32        height,
            uint32        pixelFormat,
            uint32        boHandles[4],
            uint32        pitches[4],
            uint32        offsets[4],
            uint32*       pBufId,
            uint32        flags);

typedef int32 (*DrmModePageFlip)(
            int32     fd,
            uint32    crtcId,
            uint32    fbId,
            uint32    flags,
            void*     pUserData);

typedef drmModeEncoderPtr (*DrmModeGetEncoder)(
            int32     fd,
            uint32    encoderId);

typedef void (*DrmModeFreeEncoder)(
            drmModeEncoderPtr     pEncoder);

typedef int (*DrmModeSetCrtc)(
            int32                 fd,
            uint32                crtcId,
            uint32                bufferId,
            uint32                x,
            uint32                y,
            uint32*               pConnectors,
            int32                 count,
            drmModeModeInfoPtr    pMode);

typedef drmModeConnectorPtr (*DrmModeGetConnectorCurrent)(
            int32     fd,
            uint32    connectorId);

typedef drmModeCrtcPtr (*DrmModeGetCrtc)(
            int32     fd,
            uint32    crtcId);

typedef void (*DrmModeFreeCrtc)(
            drmModeCrtcPtr    pCrtc);

typedef int32 (*DrmCrtcGetSequence)(
            int32     fd,
            uint32    crtcId,
            uint64*   pSequence,
            uint64*   pNs);

typedef int32 (*DrmCrtcQueueSequence)(
            int32     fd,
            uint32    crtcId,
            uint32    flags,
            uint64    sequence,
            uint64*   pSequenceQueued,
            uint64    userData);

typedef int32 (*DrmHandleEvent)(
            int32                 fd,
            drmEventContextPtr    pEvctx);

typedef int32 (*DrmIoctl)(
            int32     fd,
            uint32    request,
            void*     pArg);

typedef drmModePropertyPtr (*DrmModeGetProperty)(
            int32     fd,
            uint32    propertyId);

typedef void (*DrmModeFreeProperty)(
            drmModePropertyPtr    pProperty);

typedef drmModeObjectPropertiesPtr (*DrmModeObjectGetProperties)(
            int       fd,
            uint32    object_id,
            uint32    object_type);

typedef void (*DrmModeFreeObjectProperties)(
            drmModeObjectPropertiesPtr    props);

typedef drmModePropertyBlobPtr (*DrmModeGetPropertyBlob)(
            int       fd,
            uint32    blob_id);

typedef void (*DrmModeFreePropertyBlob)(
            drmModePropertyBlobPtr    ptr);

typedef drmModeAtomicReqPtr (*DrmModeAtomicAlloc)(void);

typedef void (*DrmModeAtomicFree)(
            drmModeAtomicReqPtr   req);

typedef int (*DrmModeAtomicCommit)(
            int                   fd,
            drmModeAtomicReqPtr   req,
            uint32                flags,
            void*                 user_data);

typedef int (*DrmModeCreatePropertyBlob)(
            int           fd,
            const void*   data,
            size_t        length,
            uint32*       id);

typedef int (*DrmModeDestroyPropertyBlob)(
            int       fd,
            uint32    id);

typedef int (*DrmModeAtomicAddProperty)(
            drmModeAtomicReqPtr   req,
            uint32                object_id,
            uint32                property_id,
            uint64                value);

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

    AmdgpuVmReserveVmid               pfnAmdgpuVmReserveVmid;
    bool pfnAmdgpuVmReserveVmidisValid() const
    {
        return (pfnAmdgpuVmReserveVmid != nullptr);
    }

    AmdgpuVmUnreserveVmid             pfnAmdgpuVmUnreserveVmid;
    bool pfnAmdgpuVmUnreserveVmidisValid() const
    {
        return (pfnAmdgpuVmUnreserveVmid != nullptr);
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

    AmdgpuBoRemapSecure               pfnAmdgpuBoRemapSecure;
    bool pfnAmdgpuBoRemapSecureisValid() const
    {
        return (pfnAmdgpuBoRemapSecure != nullptr);
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

    AmdgpuBoListCreateRaw             pfnAmdgpuBoListCreateRaw;
    bool pfnAmdgpuBoListCreateRawisValid() const
    {
        return (pfnAmdgpuBoListCreateRaw != nullptr);
    }

    AmdgpuBoListDestroyRaw            pfnAmdgpuBoListDestroyRaw;
    bool pfnAmdgpuBoListDestroyRawisValid() const
    {
        return (pfnAmdgpuBoListDestroyRaw != nullptr);
    }

    AmdgpuCsQueryResetState           pfnAmdgpuCsQueryResetState;
    bool pfnAmdgpuCsQueryResetStateisValid() const
    {
        return (pfnAmdgpuCsQueryResetState != nullptr);
    }

    AmdgpuCsQueryResetState2          pfnAmdgpuCsQueryResetState2;
    bool pfnAmdgpuCsQueryResetState2isValid() const
    {
        return (pfnAmdgpuCsQueryResetState2 != nullptr);
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

    AmdgpuCsCtxStablePstate           pfnAmdgpuCsCtxStablePstate;
    bool pfnAmdgpuCsCtxStablePstateisValid() const
    {
        return (pfnAmdgpuCsCtxStablePstate != nullptr);
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

    AmdgpuQueryVideoCapsInfo          pfnAmdgpuQueryVideoCapsInfo;
    bool pfnAmdgpuQueryVideoCapsInfoisValid() const
    {
        return (pfnAmdgpuQueryVideoCapsInfo != nullptr);
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

    AmdgpuCsCreateSyncobj2            pfnAmdgpuCsCreateSyncobj2;
    bool pfnAmdgpuCsCreateSyncobj2isValid() const
    {
        return (pfnAmdgpuCsCreateSyncobj2 != nullptr);
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

    AmdgpuCsSubmitRaw2                pfnAmdgpuCsSubmitRaw2;
    bool pfnAmdgpuCsSubmitRaw2isValid() const
    {
        return (pfnAmdgpuCsSubmitRaw2 != nullptr);
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

    AmdgpuCsSyncobjImportSyncFile2    pfnAmdgpuCsSyncobjImportSyncFile2;
    bool pfnAmdgpuCsSyncobjImportSyncFile2isValid() const
    {
        return (pfnAmdgpuCsSyncobjImportSyncFile2 != nullptr);
    }

    AmdgpuCsSyncobjExportSyncFile     pfnAmdgpuCsSyncobjExportSyncFile;
    bool pfnAmdgpuCsSyncobjExportSyncFileisValid() const
    {
        return (pfnAmdgpuCsSyncobjExportSyncFile != nullptr);
    }

    AmdgpuCsSyncobjExportSyncFile2    pfnAmdgpuCsSyncobjExportSyncFile2;
    bool pfnAmdgpuCsSyncobjExportSyncFile2isValid() const
    {
        return (pfnAmdgpuCsSyncobjExportSyncFile2 != nullptr);
    }

    AmdgpuCsSyncobjWait               pfnAmdgpuCsSyncobjWait;
    bool pfnAmdgpuCsSyncobjWaitisValid() const
    {
        return (pfnAmdgpuCsSyncobjWait != nullptr);
    }

    AmdgpuCsSyncobjTimelineWait       pfnAmdgpuCsSyncobjTimelineWait;
    bool pfnAmdgpuCsSyncobjTimelineWaitisValid() const
    {
        return (pfnAmdgpuCsSyncobjTimelineWait != nullptr);
    }

    AmdgpuCsSyncobjReset              pfnAmdgpuCsSyncobjReset;
    bool pfnAmdgpuCsSyncobjResetisValid() const
    {
        return (pfnAmdgpuCsSyncobjReset != nullptr);
    }

    AmdgpuCsSyncobjSignal             pfnAmdgpuCsSyncobjSignal;
    bool pfnAmdgpuCsSyncobjSignalisValid() const
    {
        return (pfnAmdgpuCsSyncobjSignal != nullptr);
    }

    AmdgpuCsSyncobjTimelineSignal     pfnAmdgpuCsSyncobjTimelineSignal;
    bool pfnAmdgpuCsSyncobjTimelineSignalisValid() const
    {
        return (pfnAmdgpuCsSyncobjTimelineSignal != nullptr);
    }

    AmdgpuCsSyncobjTransfer           pfnAmdgpuCsSyncobjTransfer;
    bool pfnAmdgpuCsSyncobjTransferisValid() const
    {
        return (pfnAmdgpuCsSyncobjTransfer != nullptr);
    }

    AmdgpuCsSyncobjQuery              pfnAmdgpuCsSyncobjQuery;
    bool pfnAmdgpuCsSyncobjQueryisValid() const
    {
        return (pfnAmdgpuCsSyncobjQuery != nullptr);
    }

    AmdgpuCsSyncobjQuery2             pfnAmdgpuCsSyncobjQuery2;
    bool pfnAmdgpuCsSyncobjQuery2isValid() const
    {
        return (pfnAmdgpuCsSyncobjQuery2 != nullptr);
    }

    AmdgpuCsCtxCreate2                pfnAmdgpuCsCtxCreate2;
    bool pfnAmdgpuCsCtxCreate2isValid() const
    {
        return (pfnAmdgpuCsCtxCreate2 != nullptr);
    }

    AmdgpuCsCtxCreate3                pfnAmdgpuCsCtxCreate3;
    bool pfnAmdgpuCsCtxCreate3isValid() const
    {
        return (pfnAmdgpuCsCtxCreate3 != nullptr);
    }

    DrmGetVersion                     pfnDrmGetVersion;
    bool pfnDrmGetVersionisValid() const
    {
        return (pfnDrmGetVersion != nullptr);
    }

    DrmFreeVersion                    pfnDrmFreeVersion;
    bool pfnDrmFreeVersionisValid() const
    {
        return (pfnDrmFreeVersion != nullptr);
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

    DrmGetDevice2                     pfnDrmGetDevice2;
    bool pfnDrmGetDevice2isValid() const
    {
        return (pfnDrmGetDevice2 != nullptr);
    }

    DrmFreeDevice                     pfnDrmFreeDevice;
    bool pfnDrmFreeDeviceisValid() const
    {
        return (pfnDrmFreeDevice != nullptr);
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

    DrmSetClientCap                   pfnDrmSetClientCap;
    bool pfnDrmSetClientCapisValid() const
    {
        return (pfnDrmSetClientCap != nullptr);
    }

    DrmSyncobjCreate                  pfnDrmSyncobjCreate;
    bool pfnDrmSyncobjCreateisValid() const
    {
        return (pfnDrmSyncobjCreate != nullptr);
    }

    DrmModeFreePlane                  pfnDrmModeFreePlane;
    bool pfnDrmModeFreePlaneisValid() const
    {
        return (pfnDrmModeFreePlane != nullptr);
    }

    DrmModeFreePlaneResources         pfnDrmModeFreePlaneResources;
    bool pfnDrmModeFreePlaneResourcesisValid() const
    {
        return (pfnDrmModeFreePlaneResources != nullptr);
    }

    DrmModeGetPlaneResources          pfnDrmModeGetPlaneResources;
    bool pfnDrmModeGetPlaneResourcesisValid() const
    {
        return (pfnDrmModeGetPlaneResources != nullptr);
    }

    DrmModeGetPlane                   pfnDrmModeGetPlane;
    bool pfnDrmModeGetPlaneisValid() const
    {
        return (pfnDrmModeGetPlane != nullptr);
    }

    DrmDropMaster                     pfnDrmDropMaster;
    bool pfnDrmDropMasterisValid() const
    {
        return (pfnDrmDropMaster != nullptr);
    }

    DrmPrimeFDToHandle                pfnDrmPrimeFDToHandle;
    bool pfnDrmPrimeFDToHandleisValid() const
    {
        return (pfnDrmPrimeFDToHandle != nullptr);
    }

    DrmModeAddFB2                     pfnDrmModeAddFB2;
    bool pfnDrmModeAddFB2isValid() const
    {
        return (pfnDrmModeAddFB2 != nullptr);
    }

    DrmModePageFlip                   pfnDrmModePageFlip;
    bool pfnDrmModePageFlipisValid() const
    {
        return (pfnDrmModePageFlip != nullptr);
    }

    DrmModeGetEncoder                 pfnDrmModeGetEncoder;
    bool pfnDrmModeGetEncoderisValid() const
    {
        return (pfnDrmModeGetEncoder != nullptr);
    }

    DrmModeFreeEncoder                pfnDrmModeFreeEncoder;
    bool pfnDrmModeFreeEncoderisValid() const
    {
        return (pfnDrmModeFreeEncoder != nullptr);
    }

    DrmModeSetCrtc                    pfnDrmModeSetCrtc;
    bool pfnDrmModeSetCrtcisValid() const
    {
        return (pfnDrmModeSetCrtc != nullptr);
    }

    DrmModeGetConnectorCurrent        pfnDrmModeGetConnectorCurrent;
    bool pfnDrmModeGetConnectorCurrentisValid() const
    {
        return (pfnDrmModeGetConnectorCurrent != nullptr);
    }

    DrmModeGetCrtc                    pfnDrmModeGetCrtc;
    bool pfnDrmModeGetCrtcisValid() const
    {
        return (pfnDrmModeGetCrtc != nullptr);
    }

    DrmModeFreeCrtc                   pfnDrmModeFreeCrtc;
    bool pfnDrmModeFreeCrtcisValid() const
    {
        return (pfnDrmModeFreeCrtc != nullptr);
    }

    DrmCrtcGetSequence                pfnDrmCrtcGetSequence;
    bool pfnDrmCrtcGetSequenceisValid() const
    {
        return (pfnDrmCrtcGetSequence != nullptr);
    }

    DrmCrtcQueueSequence              pfnDrmCrtcQueueSequence;
    bool pfnDrmCrtcQueueSequenceisValid() const
    {
        return (pfnDrmCrtcQueueSequence != nullptr);
    }

    DrmHandleEvent                    pfnDrmHandleEvent;
    bool pfnDrmHandleEventisValid() const
    {
        return (pfnDrmHandleEvent != nullptr);
    }

    DrmIoctl                          pfnDrmIoctl;
    bool pfnDrmIoctlisValid() const
    {
        return (pfnDrmIoctl != nullptr);
    }

    DrmModeGetProperty                pfnDrmModeGetProperty;
    bool pfnDrmModeGetPropertyisValid() const
    {
        return (pfnDrmModeGetProperty != nullptr);
    }

    DrmModeFreeProperty               pfnDrmModeFreeProperty;
    bool pfnDrmModeFreePropertyisValid() const
    {
        return (pfnDrmModeFreeProperty != nullptr);
    }

    DrmModeObjectGetProperties        pfnDrmModeObjectGetProperties;
    bool pfnDrmModeObjectGetPropertiesisValid() const
    {
        return (pfnDrmModeObjectGetProperties != nullptr);
    }

    DrmModeFreeObjectProperties       pfnDrmModeFreeObjectProperties;
    bool pfnDrmModeFreeObjectPropertiesisValid() const
    {
        return (pfnDrmModeFreeObjectProperties != nullptr);
    }

    DrmModeGetPropertyBlob            pfnDrmModeGetPropertyBlob;
    bool pfnDrmModeGetPropertyBlobisValid() const
    {
        return (pfnDrmModeGetPropertyBlob != nullptr);
    }

    DrmModeFreePropertyBlob           pfnDrmModeFreePropertyBlob;
    bool pfnDrmModeFreePropertyBlobisValid() const
    {
        return (pfnDrmModeFreePropertyBlob != nullptr);
    }

    DrmModeAtomicAlloc                pfnDrmModeAtomicAlloc;
    bool pfnDrmModeAtomicAllocisValid() const
    {
        return (pfnDrmModeAtomicAlloc != nullptr);
    }

    DrmModeAtomicFree                 pfnDrmModeAtomicFree;
    bool pfnDrmModeAtomicFreeisValid() const
    {
        return (pfnDrmModeAtomicFree != nullptr);
    }

    DrmModeAtomicCommit               pfnDrmModeAtomicCommit;
    bool pfnDrmModeAtomicCommitisValid() const
    {
        return (pfnDrmModeAtomicCommit != nullptr);
    }

    DrmModeCreatePropertyBlob         pfnDrmModeCreatePropertyBlob;
    bool pfnDrmModeCreatePropertyBlobisValid() const
    {
        return (pfnDrmModeCreatePropertyBlob != nullptr);
    }

    DrmModeDestroyPropertyBlob        pfnDrmModeDestroyPropertyBlob;
    bool pfnDrmModeDestroyPropertyBlobisValid() const
    {
        return (pfnDrmModeDestroyPropertyBlob != nullptr);
    }

    DrmModeAtomicAddProperty          pfnDrmModeAtomicAddProperty;
    bool pfnDrmModeAtomicAddPropertyisValid() const
    {
        return (pfnDrmModeAtomicAddProperty != nullptr);
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

    int32 pfnAmdgpuQueryHwIpInfo(
            amdgpu_device_handle              hDevice,
            uint32                            type,
            uint32                            ipInstance,
            struct drm_amdgpu_info_hw_ip*     pInfo) const;

    bool pfnAmdgpuQueryHwIpInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHwIpInfo != nullptr);
    }

    int32 pfnAmdgpuBoVaOp(
            amdgpu_bo_handle  hBuffer,
            uint64            offset,
            uint64            size,
            uint64            address,
            uint64            flags,
            uint32            ops) const;

    bool pfnAmdgpuBoVaOpisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoVaOp != nullptr);
    }

    int32 pfnAmdgpuBoVaOpRaw(
            amdgpu_device_handle  hDevice,
            amdgpu_bo_handle      hBuffer,
            uint64                offset,
            uint64                size,
            uint64                address,
            uint64                flags,
            uint32                ops) const;

    bool pfnAmdgpuBoVaOpRawisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoVaOpRaw != nullptr);
    }

    int32 pfnAmdgpuCsCreateSemaphore(
            amdgpu_semaphore_handle*  pSemaphore) const;

    bool pfnAmdgpuCsCreateSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSemaphore != nullptr);
    }

    int32 pfnAmdgpuCsSignalSemaphore(
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsSignalSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSignalSemaphore != nullptr);
    }

    int32 pfnAmdgpuCsWaitSemaphore(
            amdgpu_context_handle     hConext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsWaitSemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitSemaphore != nullptr);
    }

    int32 pfnAmdgpuCsDestroySemaphore(
            amdgpu_semaphore_handle   hSemaphore) const;

    bool pfnAmdgpuCsDestroySemaphoreisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsDestroySemaphore != nullptr);
    }

    int32 pfnAmdgpuCsCreateSem(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle*    pSemaphore) const;

    bool pfnAmdgpuCsCreateSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSem != nullptr);
    }

    int32 pfnAmdgpuCsSignalSem(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_sem_handle         hSemaphore) const;

    bool pfnAmdgpuCsSignalSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSignalSem != nullptr);
    }

    int32 pfnAmdgpuCsWaitSem(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle     hContext,
            uint32                    ipType,
            uint32                    ipInstance,
            uint32                    ring,
            amdgpu_sem_handle         hSemaphore) const;

    bool pfnAmdgpuCsWaitSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitSem != nullptr);
    }

    int32 pfnAmdgpuCsExportSem(
            amdgpu_device_handle  hDevice,
            amdgpu_sem_handle     hSemaphore,
            int32*                pSharedFd) const;

    bool pfnAmdgpuCsExportSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsExportSem != nullptr);
    }

    int32 pfnAmdgpuCsImportSem(
            amdgpu_device_handle  hDevice,
            int32                 fd,
            amdgpu_sem_handle*    pSemaphore) const;

    bool pfnAmdgpuCsImportSemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsImportSem != nullptr);
    }

    int32 pfnAmdgpuCsDestroySem(
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

    int32 pfnAmdgpuVaRangeFree(
            amdgpu_va_handle  hVaRange) const;

    bool pfnAmdgpuVaRangeFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeFree != nullptr);
    }

    int32 pfnAmdgpuVaRangeQuery(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  type,
            uint64*                   pStart,
            uint64*                   pEnd) const;

    bool pfnAmdgpuVaRangeQueryisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeQuery != nullptr);
    }

    int32 pfnAmdgpuVaRangeAlloc(
            amdgpu_device_handle      hDevice,
            enum amdgpu_gpu_va_range  vaRangeType,
            uint64                    size,
            uint64                    vaBaseAlignment,
            uint64                    vaBaseRequired,
            uint64*                   pVaAllocated,
            amdgpu_va_handle*         pVaRange,
            uint64                    flags) const;

    bool pfnAmdgpuVaRangeAllocisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVaRangeAlloc != nullptr);
    }

    int32 pfnAmdgpuVmReserveVmid(
            amdgpu_device_handle  hDevice,
            uint32                flags) const;

    bool pfnAmdgpuVmReserveVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVmReserveVmid != nullptr);
    }

    int32 pfnAmdgpuVmUnreserveVmid(
            amdgpu_device_handle  hDevice,
            uint32                flags) const;

    bool pfnAmdgpuVmUnreserveVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuVmUnreserveVmid != nullptr);
    }

    int32 pfnAmdgpuReadMmRegisters(
            amdgpu_device_handle  hDevice,
            uint32                dwordOffset,
            uint32                count,
            uint32                instance,
            uint32                flags,
            uint32*               pValues) const;

    bool pfnAmdgpuReadMmRegistersisValid() const
    {
        return (m_pFuncs->pfnAmdgpuReadMmRegisters != nullptr);
    }

    int32 pfnAmdgpuDeviceInitialize(
            int                       fd,
            uint32*                   pMajorVersion,
            uint32*                   pMinorVersion,
            amdgpu_device_handle*     pDeviceHandle) const;

    bool pfnAmdgpuDeviceInitializeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuDeviceInitialize != nullptr);
    }

    int32 pfnAmdgpuDeviceDeinitialize(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuDeviceDeinitializeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuDeviceDeinitialize != nullptr);
    }

    int32 pfnAmdgpuBoAlloc(
            amdgpu_device_handle              hDevice,
            struct amdgpu_bo_alloc_request*   pAllocBuffer,
            amdgpu_bo_handle*                 pBufferHandle) const;

    bool pfnAmdgpuBoAllocisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoAlloc != nullptr);
    }

    int32 pfnAmdgpuBoSetMetadata(
            amdgpu_bo_handle              hBuffer,
            struct amdgpu_bo_metadata*    pInfo) const;

    bool pfnAmdgpuBoSetMetadataisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoSetMetadata != nullptr);
    }

    int32 pfnAmdgpuBoQueryInfo(
            amdgpu_bo_handle          hBuffer,
            struct amdgpu_bo_info*    pInfo) const;

    bool pfnAmdgpuBoQueryInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoQueryInfo != nullptr);
    }

    int32 pfnAmdgpuBoExport(
            amdgpu_bo_handle              hBuffer,
            enum amdgpu_bo_handle_type    type,
            uint32*                       pFd) const;

    bool pfnAmdgpuBoExportisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoExport != nullptr);
    }

    int32 pfnAmdgpuBoImport(
            amdgpu_device_handle              hDevice,
            enum amdgpu_bo_handle_type        type,
            uint32                            fd,
            struct amdgpu_bo_import_result*   pOutput) const;

    bool pfnAmdgpuBoImportisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoImport != nullptr);
    }

    int32 pfnAmdgpuCreateBoFromUserMem(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle) const;

    bool pfnAmdgpuCreateBoFromUserMemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCreateBoFromUserMem != nullptr);
    }

    int32 pfnAmdgpuCreateBoFromPhysMem(
            amdgpu_device_handle  hDevice,
            uint64                physAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle) const;

    bool pfnAmdgpuCreateBoFromPhysMemisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCreateBoFromPhysMem != nullptr);
    }

    int32 pfnAmdgpuFindBoByCpuMapping(
            amdgpu_device_handle  hDevice,
            void*                 pCpuAddress,
            uint64                size,
            amdgpu_bo_handle*     pBufferHandle,
            uint64*               pOffsetInBuffer) const;

    bool pfnAmdgpuFindBoByCpuMappingisValid() const
    {
        return (m_pFuncs->pfnAmdgpuFindBoByCpuMapping != nullptr);
    }

    int32 pfnAmdgpuBoFree(
            amdgpu_bo_handle  hBuffer) const;

    bool pfnAmdgpuBoFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoFree != nullptr);
    }

    int32 pfnAmdgpuBoCpuMap(
            amdgpu_bo_handle  hBuffer,
            void**            ppCpuAddress) const;

    bool pfnAmdgpuBoCpuMapisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoCpuMap != nullptr);
    }

    int32 pfnAmdgpuBoCpuUnmap(
            amdgpu_bo_handle  hBuffer) const;

    bool pfnAmdgpuBoCpuUnmapisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoCpuUnmap != nullptr);
    }

    int32 pfnAmdgpuBoRemapSecure(
            amdgpu_bo_handle  buf_handle,
            bool              secure_map) const;

    bool pfnAmdgpuBoRemapSecureisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoRemapSecure != nullptr);
    }

    int32 pfnAmdgpuBoWaitForIdle(
            amdgpu_bo_handle  hBuffer,
            uint64            timeoutInNs,
            bool*             pBufferBusy) const;

    bool pfnAmdgpuBoWaitForIdleisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoWaitForIdle != nullptr);
    }

    int32 pfnAmdgpuBoListCreate(
            amdgpu_device_handle      hDevice,
            uint32                    numberOfResources,
            amdgpu_bo_handle*         pResources,
            uint8*                    pResourcePriorities,
            amdgpu_bo_list_handle*    pBoListHandle) const;

    bool pfnAmdgpuBoListCreateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListCreate != nullptr);
    }

    int32 pfnAmdgpuBoListDestroy(
            amdgpu_bo_list_handle     hBoList) const;

    bool pfnAmdgpuBoListDestroyisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListDestroy != nullptr);
    }

    int32 pfnAmdgpuBoListCreateRaw(
            amdgpu_device_handle              hDevice,
            uint32                            numberOfResources,
            struct drm_amdgpu_bo_list_entry*  pBoListEntry,
            uint32*                           pBoListHandle) const;

    bool pfnAmdgpuBoListCreateRawisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListCreateRaw != nullptr);
    }

    int32 pfnAmdgpuBoListDestroyRaw(
            amdgpu_device_handle  hDevice,
            uint32                boListHandle) const;

    bool pfnAmdgpuBoListDestroyRawisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoListDestroyRaw != nullptr);
    }

    int32 pfnAmdgpuCsQueryResetState(
            amdgpu_context_handle     context,
            uint32_t *                state,
            uint32_t *                hangs) const;

    bool pfnAmdgpuCsQueryResetStateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsQueryResetState != nullptr);
    }

    int32 pfnAmdgpuCsQueryResetState2(
            amdgpu_context_handle     hContext,
            uint64*                   flags) const;

    bool pfnAmdgpuCsQueryResetState2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsQueryResetState2 != nullptr);
    }

    int32 pfnAmdgpuCsCtxCreate(
            amdgpu_device_handle      hDevice,
            amdgpu_context_handle*    pContextHandle) const;

    bool pfnAmdgpuCsCtxCreateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxCreate != nullptr);
    }

    int32 pfnAmdgpuCsCtxFree(
            amdgpu_context_handle     hContext) const;

    bool pfnAmdgpuCsCtxFreeisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxFree != nullptr);
    }

    int32 pfnAmdgpuCsSubmit(
            amdgpu_context_handle         hContext,
            uint64                        flags,
            struct amdgpu_cs_request*     pIbsRequest,
            uint32                        numberOfRequests) const;

    bool pfnAmdgpuCsSubmitisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSubmit != nullptr);
    }

    int32 pfnAmdgpuCsQueryFenceStatus(
            struct amdgpu_cs_fence*   pFence,
            uint64                    timeoutInNs,
            uint64                    flags,
            uint32*                   pExpired) const;

    bool pfnAmdgpuCsQueryFenceStatusisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsQueryFenceStatus != nullptr);
    }

    int32 pfnAmdgpuCsWaitFences(
            struct amdgpu_cs_fence*   pFences,
            uint32                    fenceCount,
            bool                      waitAll,
            uint64                    timeoutInNs,
            uint32*                   pStatus,
            uint32*                   pFirst) const;

    bool pfnAmdgpuCsWaitFencesisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsWaitFences != nullptr);
    }

    int32 pfnAmdgpuCsCtxStablePstate(
            amdgpu_context_handle     context,
            uint32_t                  op,
            uint32_t                  flags,
            uint32_t *                out_flags) const;

    bool pfnAmdgpuCsCtxStablePstateisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxStablePstate != nullptr);
    }

    int32 pfnAmdgpuQueryBufferSizeAlignment(
            amdgpu_device_handle                      hDevice,
            struct amdgpu_buffer_size_alignments*     pInfo) const;

    bool pfnAmdgpuQueryBufferSizeAlignmentisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryBufferSizeAlignment != nullptr);
    }

    int32 pfnAmdgpuQueryFirmwareVersion(
            amdgpu_device_handle  hDevice,
            uint32                fwType,
            uint32                ipInstance,
            uint32                index,
            uint32*               pVersion,
            uint32*               pFeature) const;

    bool pfnAmdgpuQueryFirmwareVersionisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryFirmwareVersion != nullptr);
    }

    int32 pfnAmdgpuQueryVideoCapsInfo(
            amdgpu_device_handle  hDevice,
            uint32                capType,
            uint32                size,
            void*                 pCaps) const;

    bool pfnAmdgpuQueryVideoCapsInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryVideoCapsInfo != nullptr);
    }

    int32 pfnAmdgpuQueryHwIpCount(
            amdgpu_device_handle  hDevice,
            uint32                type,
            uint32*               pCount) const;

    bool pfnAmdgpuQueryHwIpCountisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHwIpCount != nullptr);
    }

    int32 pfnAmdgpuQueryHeapInfo(
            amdgpu_device_handle      hDevice,
            uint32                    heap,
            uint32                    flags,
            struct amdgpu_heap_info*  pInfo) const;

    bool pfnAmdgpuQueryHeapInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryHeapInfo != nullptr);
    }

    int32 pfnAmdgpuQueryGpuInfo(
            amdgpu_device_handle      hDevice,
            struct amdgpu_gpu_info*   pInfo) const;

    bool pfnAmdgpuQueryGpuInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryGpuInfo != nullptr);
    }

    int32 pfnAmdgpuQuerySensorInfo(
            amdgpu_device_handle  hDevice,
            uint32                sensor_type,
            uint32                size,
            void*                 value) const;

    bool pfnAmdgpuQuerySensorInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQuerySensorInfo != nullptr);
    }

    int32 pfnAmdgpuQueryInfo(
            amdgpu_device_handle  hDevice,
            uint32                infoId,
            uint32                size,
            void*                 pValue) const;

    bool pfnAmdgpuQueryInfoisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryInfo != nullptr);
    }

    int32 pfnAmdgpuQueryPrivateAperture(
            amdgpu_device_handle  hDevice,
            uint64*               pStartVa,
            uint64*               pEndVa) const;

    bool pfnAmdgpuQueryPrivateApertureisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQueryPrivateAperture != nullptr);
    }

    int32 pfnAmdgpuQuerySharedAperture(
            amdgpu_device_handle  hDevice,
            uint64*               pStartVa,
            uint64*               pEndVa) const;

    bool pfnAmdgpuQuerySharedApertureisValid() const
    {
        return (m_pFuncs->pfnAmdgpuQuerySharedAperture != nullptr);
    }

    int32 pfnAmdgpuBoGetPhysAddress(
            amdgpu_bo_handle  hBuffer,
            uint64*           pPhysAddress) const;

    bool pfnAmdgpuBoGetPhysAddressisValid() const
    {
        return (m_pFuncs->pfnAmdgpuBoGetPhysAddress != nullptr);
    }

    int32 pfnAmdgpuCsReservedVmid(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuCsReservedVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsReservedVmid != nullptr);
    }

    int32 pfnAmdgpuCsUnreservedVmid(
            amdgpu_device_handle  hDevice) const;

    bool pfnAmdgpuCsUnreservedVmidisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsUnreservedVmid != nullptr);
    }

    int32 pfnAmdgpuCsCreateSyncobj(
            amdgpu_device_handle  hDevice,
            uint32*               pSyncObj) const;

    bool pfnAmdgpuCsCreateSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSyncobj != nullptr);
    }

    int32 pfnAmdgpuCsCreateSyncobj2(
            amdgpu_device_handle  hDevice,
            uint32                flags,
            uint32*               pSyncObj) const;

    bool pfnAmdgpuCsCreateSyncobj2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCreateSyncobj2 != nullptr);
    }

    int32 pfnAmdgpuCsDestroySyncobj(
            amdgpu_device_handle  hDevice,
            uint32                syncObj) const;

    bool pfnAmdgpuCsDestroySyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsDestroySyncobj != nullptr);
    }

    int32 pfnAmdgpuCsExportSyncobj(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32*                pSharedFd) const;

    bool pfnAmdgpuCsExportSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsExportSyncobj != nullptr);
    }

    int32 pfnAmdgpuCsImportSyncobj(
            amdgpu_device_handle  hDevice,
            int32                 sharedFd,
            uint32*               pSyncObj) const;

    bool pfnAmdgpuCsImportSyncobjisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsImportSyncobj != nullptr);
    }

    int32 pfnAmdgpuCsSubmitRaw2(
            amdgpu_device_handle          dev,
            amdgpu_context_handle         context,
            uint32_t                      bo_list_handle,
            int                           num_chunks,
            struct drm_amdgpu_cs_chunk *  chunks,
            uint64_t *                    seq_no) const;

    bool pfnAmdgpuCsSubmitRaw2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSubmitRaw2 != nullptr);
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

    int32 pfnAmdgpuCsSyncobjImportSyncFile(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32                 syncFileFd) const;

    bool pfnAmdgpuCsSyncobjImportSyncFileisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjImportSyncFile2(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            uint64                point,
            int32                 syncFileFd) const;

    bool pfnAmdgpuCsSyncobjImportSyncFile2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjImportSyncFile2 != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjExportSyncFile(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            int32*                pSyncFileFd) const;

    bool pfnAmdgpuCsSyncobjExportSyncFileisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjExportSyncFile2(
            amdgpu_device_handle  hDevice,
            uint32                syncObj,
            uint64                point,
            uint32                flags,
            int32*                pSyncFileFd) const;

    bool pfnAmdgpuCsSyncobjExportSyncFile2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjExportSyncFile2 != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjWait(
            amdgpu_device_handle  hDevice,
            uint32*               pHandles,
            uint32                numHandles,
            int64                 timeoutInNs,
            uint32                flags,
            uint32*               pFirstSignaled) const;

    bool pfnAmdgpuCsSyncobjWaitisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjWait != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjTimelineWait(
            amdgpu_device_handle  hDevice,
            uint32*               pHandles,
            uint64*               points,
            uint32                numHandles,
            int64                 timeoutInNs,
            uint32                flags,
            uint32*               pFirstSignaled) const;

    bool pfnAmdgpuCsSyncobjTimelineWaitisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjTimelineWait != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjReset(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint32                numHandles) const;

    bool pfnAmdgpuCsSyncobjResetisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjReset != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjSignal(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint32                numHandles) const;

    bool pfnAmdgpuCsSyncobjSignalisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjSignal != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjTimelineSignal(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles) const;

    bool pfnAmdgpuCsSyncobjTimelineSignalisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjTimelineSignal != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjTransfer(
            amdgpu_device_handle  hDevice,
            uint32                dst_handle,
            uint64                dst_point,
            uint32                src_handle,
            uint64                src_point,
            uint32                flags) const;

    bool pfnAmdgpuCsSyncobjTransferisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjTransfer != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjQuery(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles) const;

    bool pfnAmdgpuCsSyncobjQueryisValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjQuery != nullptr);
    }

    int32 pfnAmdgpuCsSyncobjQuery2(
            amdgpu_device_handle  hDevice,
            const uint32*         pHandles,
            uint64*               points,
            uint32                numHandles,
            uint32                flags) const;

    bool pfnAmdgpuCsSyncobjQuery2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsSyncobjQuery2 != nullptr);
    }

    int32 pfnAmdgpuCsCtxCreate2(
            amdgpu_device_handle      hDevice,
            uint32                    priority,
            amdgpu_context_handle*    pContextHandle) const;

    bool pfnAmdgpuCsCtxCreate2isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxCreate2 != nullptr);
    }

    int32 pfnAmdgpuCsCtxCreate3(
            amdgpu_device_handle      hDevice,
            uint32                    priority,
            uint32_t                  flags,
            amdgpu_context_handle*    pContextHandle) const;

    bool pfnAmdgpuCsCtxCreate3isValid() const
    {
        return (m_pFuncs->pfnAmdgpuCsCtxCreate3 != nullptr);
    }

    drmVersionPtr pfnDrmGetVersion(
            int   fd) const;

    bool pfnDrmGetVersionisValid() const
    {
        return (m_pFuncs->pfnDrmGetVersion != nullptr);
    }

    void pfnDrmFreeVersion(
            drmVersionPtr     v) const;

    bool pfnDrmFreeVersionisValid() const
    {
        return (m_pFuncs->pfnDrmFreeVersion != nullptr);
    }

    int32 pfnDrmGetNodeTypeFromFd(
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

    int32 pfnDrmGetDevices(
            drmDevicePtr*     pDevices,
            int32             maxDevices) const;

    bool pfnDrmGetDevicesisValid() const
    {
        return (m_pFuncs->pfnDrmGetDevices != nullptr);
    }

    void pfnDrmFreeDevices(
            drmDevicePtr*     pDevices,
            int32             count) const;

    bool pfnDrmFreeDevicesisValid() const
    {
        return (m_pFuncs->pfnDrmFreeDevices != nullptr);
    }

    int32 pfnDrmGetDevice2(
            int               fd,
            uint32_t          flags,
            drmDevicePtr*     pDevice) const;

    bool pfnDrmGetDevice2isValid() const
    {
        return (m_pFuncs->pfnDrmGetDevice2 != nullptr);
    }

    void pfnDrmFreeDevice(
            drmDevicePtr*     pDevice) const;

    bool pfnDrmFreeDeviceisValid() const
    {
        return (m_pFuncs->pfnDrmFreeDevice != nullptr);
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
            uint32    connectorId) const;

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

    int32 pfnDrmGetCap(
            int       fd,
            uint64    capability,
            uint64*   pValue) const;

    bool pfnDrmGetCapisValid() const
    {
        return (m_pFuncs->pfnDrmGetCap != nullptr);
    }

    int32 pfnDrmSetClientCap(
            int       fd,
            uint64    capability,
            uint64    value) const;

    bool pfnDrmSetClientCapisValid() const
    {
        return (m_pFuncs->pfnDrmSetClientCap != nullptr);
    }

    int32 pfnDrmSyncobjCreate(
            int       fd,
            uint32    flags,
            uint32*   pHandle) const;

    bool pfnDrmSyncobjCreateisValid() const
    {
        return (m_pFuncs->pfnDrmSyncobjCreate != nullptr);
    }

    void pfnDrmModeFreePlane(
            drmModePlanePtr   pPlanePtr) const;

    bool pfnDrmModeFreePlaneisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreePlane != nullptr);
    }

    void pfnDrmModeFreePlaneResources(
            drmModePlaneResPtr    pPlaneResPtr) const;

    bool pfnDrmModeFreePlaneResourcesisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreePlaneResources != nullptr);
    }

    drmModePlaneResPtr pfnDrmModeGetPlaneResources(
            int32     fd) const;

    bool pfnDrmModeGetPlaneResourcesisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetPlaneResources != nullptr);
    }

    drmModePlanePtr pfnDrmModeGetPlane(
            int32     fd,
            uint32    planeId) const;

    bool pfnDrmModeGetPlaneisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetPlane != nullptr);
    }

    int32 pfnDrmDropMaster(
            int32     fd) const;

    bool pfnDrmDropMasterisValid() const
    {
        return (m_pFuncs->pfnDrmDropMaster != nullptr);
    }

    int32 pfnDrmPrimeFDToHandle(
            int32     fd,
            int32     primeFd,
            uint32*   pHandle) const;

    bool pfnDrmPrimeFDToHandleisValid() const
    {
        return (m_pFuncs->pfnDrmPrimeFDToHandle != nullptr);
    }

    int32 pfnDrmModeAddFB2(
            int32         fd,
            uint32        width,
            uint32        height,
            uint32        pixelFormat,
            uint32        boHandles[4],
            uint32        pitches[4],
            uint32        offsets[4],
            uint32*       pBufId,
            uint32        flags) const;

    bool pfnDrmModeAddFB2isValid() const
    {
        return (m_pFuncs->pfnDrmModeAddFB2 != nullptr);
    }

    int32 pfnDrmModePageFlip(
            int32     fd,
            uint32    crtcId,
            uint32    fbId,
            uint32    flags,
            void*     pUserData) const;

    bool pfnDrmModePageFlipisValid() const
    {
        return (m_pFuncs->pfnDrmModePageFlip != nullptr);
    }

    drmModeEncoderPtr pfnDrmModeGetEncoder(
            int32     fd,
            uint32    encoderId) const;

    bool pfnDrmModeGetEncoderisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetEncoder != nullptr);
    }

    void pfnDrmModeFreeEncoder(
            drmModeEncoderPtr     pEncoder) const;

    bool pfnDrmModeFreeEncoderisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeEncoder != nullptr);
    }

    int pfnDrmModeSetCrtc(
            int32                 fd,
            uint32                crtcId,
            uint32                bufferId,
            uint32                x,
            uint32                y,
            uint32*               pConnectors,
            int32                 count,
            drmModeModeInfoPtr    pMode) const;

    bool pfnDrmModeSetCrtcisValid() const
    {
        return (m_pFuncs->pfnDrmModeSetCrtc != nullptr);
    }

    drmModeConnectorPtr pfnDrmModeGetConnectorCurrent(
            int32     fd,
            uint32    connectorId) const;

    bool pfnDrmModeGetConnectorCurrentisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetConnectorCurrent != nullptr);
    }

    drmModeCrtcPtr pfnDrmModeGetCrtc(
            int32     fd,
            uint32    crtcId) const;

    bool pfnDrmModeGetCrtcisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetCrtc != nullptr);
    }

    void pfnDrmModeFreeCrtc(
            drmModeCrtcPtr    pCrtc) const;

    bool pfnDrmModeFreeCrtcisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeCrtc != nullptr);
    }

    int32 pfnDrmCrtcGetSequence(
            int32     fd,
            uint32    crtcId,
            uint64*   pSequence,
            uint64*   pNs) const;

    bool pfnDrmCrtcGetSequenceisValid() const
    {
        return (m_pFuncs->pfnDrmCrtcGetSequence != nullptr);
    }

    int32 pfnDrmCrtcQueueSequence(
            int32     fd,
            uint32    crtcId,
            uint32    flags,
            uint64    sequence,
            uint64*   pSequenceQueued,
            uint64    userData) const;

    bool pfnDrmCrtcQueueSequenceisValid() const
    {
        return (m_pFuncs->pfnDrmCrtcQueueSequence != nullptr);
    }

    int32 pfnDrmHandleEvent(
            int32                 fd,
            drmEventContextPtr    pEvctx) const;

    bool pfnDrmHandleEventisValid() const
    {
        return (m_pFuncs->pfnDrmHandleEvent != nullptr);
    }

    int32 pfnDrmIoctl(
            int32     fd,
            uint32    request,
            void*     pArg) const;

    bool pfnDrmIoctlisValid() const
    {
        return (m_pFuncs->pfnDrmIoctl != nullptr);
    }

    drmModePropertyPtr pfnDrmModeGetProperty(
            int32     fd,
            uint32    propertyId) const;

    bool pfnDrmModeGetPropertyisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetProperty != nullptr);
    }

    void pfnDrmModeFreeProperty(
            drmModePropertyPtr    pProperty) const;

    bool pfnDrmModeFreePropertyisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeProperty != nullptr);
    }

    drmModeObjectPropertiesPtr pfnDrmModeObjectGetProperties(
            int       fd,
            uint32    object_id,
            uint32    object_type) const;

    bool pfnDrmModeObjectGetPropertiesisValid() const
    {
        return (m_pFuncs->pfnDrmModeObjectGetProperties != nullptr);
    }

    void pfnDrmModeFreeObjectProperties(
            drmModeObjectPropertiesPtr    props) const;

    bool pfnDrmModeFreeObjectPropertiesisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreeObjectProperties != nullptr);
    }

    drmModePropertyBlobPtr pfnDrmModeGetPropertyBlob(
            int       fd,
            uint32    blob_id) const;

    bool pfnDrmModeGetPropertyBlobisValid() const
    {
        return (m_pFuncs->pfnDrmModeGetPropertyBlob != nullptr);
    }

    void pfnDrmModeFreePropertyBlob(
            drmModePropertyBlobPtr    ptr) const;

    bool pfnDrmModeFreePropertyBlobisValid() const
    {
        return (m_pFuncs->pfnDrmModeFreePropertyBlob != nullptr);
    }

    drmModeAtomicReqPtr pfnDrmModeAtomicAlloc(void) const;

    void pfnDrmModeAtomicFree(
            drmModeAtomicReqPtr   req) const;

    bool pfnDrmModeAtomicFreeisValid() const
    {
        return (m_pFuncs->pfnDrmModeAtomicFree != nullptr);
    }

    int pfnDrmModeAtomicCommit(
            int                   fd,
            drmModeAtomicReqPtr   req,
            uint32                flags,
            void*                 user_data) const;

    bool pfnDrmModeAtomicCommitisValid() const
    {
        return (m_pFuncs->pfnDrmModeAtomicCommit != nullptr);
    }

    int pfnDrmModeCreatePropertyBlob(
            int           fd,
            const void*   data,
            size_t        length,
            uint32*       id) const;

    bool pfnDrmModeCreatePropertyBlobisValid() const
    {
        return (m_pFuncs->pfnDrmModeCreatePropertyBlob != nullptr);
    }

    int pfnDrmModeDestroyPropertyBlob(
            int       fd,
            uint32    id) const;

    bool pfnDrmModeDestroyPropertyBlobisValid() const
    {
        return (m_pFuncs->pfnDrmModeDestroyPropertyBlob != nullptr);
    }

    int pfnDrmModeAtomicAddProperty(
            drmModeAtomicReqPtr   req,
            uint32                object_id,
            uint32                property_id,
            uint64                value) const;

    bool pfnDrmModeAtomicAddPropertyisValid() const
    {
        return (m_pFuncs->pfnDrmModeAtomicAddProperty != nullptr);
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
// the class is responsible for resolving all external symbols that required by the Dri3WindowSystem.
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
    void   SpecializedInit(Platform* pPlatform, char*  pDtifLibName);

private:
    Util::Library m_library[DrmLoaderLibrariesCount];
    bool          m_initialized;

    DrmLoaderFuncs      m_funcs;
#if defined(PAL_DEBUG_PRINTS)
    DrmLoaderFuncsProxy m_proxy;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(DrmLoader);
};

} // Amdgpu
} // Pal
