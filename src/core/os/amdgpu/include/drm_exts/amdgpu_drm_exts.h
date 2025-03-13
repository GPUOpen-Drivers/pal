/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

// This header contains kernel interfaces that either have yet to be upstreamed
// or are defunct but kept around for compatibility. All new kernel/drm header
// changes should be added here first, leaving the headers in drm/ as an
// exact copy of public upstream headers.

#pragma once

// =====================================================================================================================
// New IOCTLs
/**
 * Request GPU access to physical memory from 3rd party device.
 *
 * \param dev - [in] Device handle. See #amdgpu_device_initialize()
 * \param phys_address - [in] Physical address from 3rd party device which
 * we want to map to GPU address space (make GPU accessible)
 * (This address must be correctly aligned).
 * \param size - [in] Size of allocation (must be correctly aligned)
 * \param buf_handle - [out] Buffer handle for the userptr memory
 * resource on submission and be used in other operations.
 *
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
 * \note
 * This call should guarantee that such memory will be persistently
 * "locked" / make non-pageable. The purpose of this call is to provide
 * opportunity for GPU get access to this resource during submission.
 *
 *
 * Supported (theoretical) max. size of mapping is restricted only by
 * capability.direct_gma_size. See #amdgpu_query_capability()
 *
 * It is responsibility of caller to correctly specify physical_address
*/
int amdgpu_create_bo_from_phys_mem(amdgpu_device_handle dev,
				uint64_t phys_address, uint64_t size,
				amdgpu_bo_handle *buf_handle);

/**
 * Get physical address from BO
 *
 * \param buf_handle - [in] Buffer handle for the physical address.
 * \param phys_address - [out] Physical address of this BO.
 *
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_bo_get_phys_address(amdgpu_bo_handle buf_handle,
					uint64_t *phys_address);

/**
 * Remap between the non-secure buffer and secure buffer
 *
 * \param   buf_handle         - \c [in] Buffer handle
 * \param   secure_map	       - \c [in] flag for indentifying map to secure buffer or non-secure buffer
 *
 * \return   0 on success
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_bo_remap_secure(amdgpu_bo_handle buf_handle, bool secure_map);

/**
 * Create GPU execution Context
 *
 * For the purpose of GPU Scheduler and GPU Robustness extensions it is
 * necessary to have information/identify rendering/compute contexts.
 * It also may be needed to associate some specific requirements with such
 * contexts.  Kernel driver will guarantee that submission from the same
 * context will always be executed in order (first come, first serve).
 *
 *
 * \param   dev      - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   priority - \c [in] Context creation flags. See AMDGPU_CTX_PRIORITY_*
 * \param   flags    - \c [in] Context creation flags. See AMDGPU_CTX_FLAG_*
 * \param   context  - \c [out] GPU Context handle
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_cs_ctx_free()
 *
*/
int amdgpu_cs_ctx_create3(amdgpu_device_handle dev,
             uint32_t priority,
             uint32_t flags,
             amdgpu_context_handle *context);

/**
 * Query hardware or driver capabilities.
 *
 *
 * \param   dev     - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   value   - \c [out] Pointer to the return value.
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX error code
 *
*/
int amdgpu_query_capability(amdgpu_device_handle dev,
			     struct drm_amdgpu_capability *cap);

/**
 * Query private aperture range
 *
 * \param dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param start - \c [out] Start of private aperture
 * \param end    - \c [out] End of private aperture
 *
 * \return  0 on success\n
 *         <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_private_aperture(amdgpu_device_handle dev,
			uint64_t *start,
			uint64_t *end);

/**
 * Query shared aperture range
 *
 * \param dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param start - \c [out] Start of shared aperture
 * \param end    - \c [out] End of shared aperture
 *
 * \return 0 on success\n
 *    <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_shared_aperture(amdgpu_device_handle dev,
			uint64_t *start,
			uint64_t *end);

// =====================================================================================================================
// Legacy IOCTLs

/**
 * Define handle for sem file
 */
typedef uint32_t amdgpu_sem_handle;

/**
 *  create sem
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param   sem	   - \c [out] sem handle
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_cs_create_sem(amdgpu_device_handle dev,
			 amdgpu_sem_handle *sem);

/**
 *  signal sem
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param   context        - \c [in] GPU Context
 * \param   ip_type        - \c [in] Hardware IP block type = AMDGPU_HW_IP_*
 * \param   ip_instance    - \c [in] Index of the IP block of the same type
 * \param   ring           - \c [in] Specify ring index of the IP
 * \param   sem	   - \c [out] sem handle
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
 */
int amdgpu_cs_signal_sem(amdgpu_device_handle dev,
			 amdgpu_context_handle ctx,
			 uint32_t ip_type,
			 uint32_t ip_instance,
			 uint32_t ring,
			 amdgpu_sem_handle sem);

/**
 *  wait sem
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param   context        - \c [in] GPU Context
 * \param   ip_type        - \c [in] Hardware IP block type = AMDGPU_HW_IP_*
 * \param   ip_instance    - \c [in] Index of the IP block of the same type
 * \param   ring           - \c [in] Specify ring index of the IP
 * \param   sem	   - \c [out] sem handle
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_cs_wait_sem(amdgpu_device_handle dev,
		       amdgpu_context_handle ctx,
		       uint32_t ip_type,
		       uint32_t ip_instance,
		       uint32_t ring,
		       amdgpu_sem_handle sem);

int amdgpu_cs_export_sem(amdgpu_device_handle dev,
			  amdgpu_sem_handle sem,
			  int *shared_handle);

int amdgpu_cs_import_sem(amdgpu_device_handle dev,
			  int shared_handle,
			  amdgpu_sem_handle *sem);

/**
 *  destroy sem
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 * \param   sem	   - \c [out] sem handle
 *
 * \return   0 on success\n
 *          <0 - Negative POSIX Error code
 *
 */
int amdgpu_cs_destroy_sem(amdgpu_device_handle dev,
			  amdgpu_sem_handle sem);

/**
 *  reserve vmid for this process
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 */
int amdgpu_cs_reserved_vmid(amdgpu_device_handle dev);

/**
 *  unreserve vmid for this process
 *
 * \param   dev    - [in] Device handle. See #amdgpu_device_initialize()
 */
int amdgpu_cs_unreserved_vmid(amdgpu_device_handle dev);

// =====================================================================================================================
// Memory alloc flags
#if PAL_BUILD_GFX12
/* Set PTE.D and recompress during GTT->VRAM moves according to TILING flags. */
#define AMDGPU_GEM_CREATE_GFX12_DCC		(1 << 16)
#endif
/* hybrid specific */
/* Flag that the memory should be in SPARSE resource */
#define AMDGPU_GEM_CREATE_SPARSE		(1ULL << 29)
/* Flag that the memory allocation should be from top of domain */
#define AMDGPU_GEM_CREATE_TOP_DOWN		(1ULL << 30)
/* Flag that the memory allocation should be pinned */
#define AMDGPU_GEM_CREATE_NO_EVICT		(1ULL << 31)

/* hybrid specific */
#define AMDGPU_GEM_DOMAIN_DGMA		0x400
#define AMDGPU_GEM_DOMAIN_DGMA_IMPORT	0x800

// =====================================================================================================================
// Command submit flags
/* Set Flag to 1 if perfCounter is active */
#define AMDGPU_IB_FLAG_PERF_COUNTER (1 << 7)
/* Set flag to 1 if SQTT is active */
#define AMDGPU_IB_FLAG_SQ_THREAD_TRACE (1 << 8)

// =====================================================================================================================
// Queue create flags
#define AMDGPU_CTX_FLAGS_IFH            (1<<0)
#define AMDGPU_CTX_FLAGS_SECURE         (1<<1)

// =====================================================================================================================
// Queries
/* gpu capability */
#define AMDGPU_INFO_CAPABILITY			0x50
/* virtual range */
#define AMDGPU_INFO_VIRTUAL_RANGE		0x51
/* query pin memory capability */
#define AMDGPU_CAPABILITY_PIN_MEM_FLAG  (1 << 0)
/* query direct gma capability */
#define AMDGPU_CAPABILITY_DIRECT_GMA_FLAG	(1 << 1)
/**
 *  Definition of System Unified Address (SUA) apertures
 */
#define AMDGPU_SUA_APERTURE_PRIVATE    1
#define AMDGPU_SUA_APERTURE_SHARED     2
struct drm_amdgpu_virtual_range {
	uint64_t start;
	uint64_t end;
};
struct drm_amdgpu_capability {
	__u32 flag;
	__u32 direct_gma_size;
};

// =====================================================================================================================
// DRM modifier updates
#define AMDGPU_TILING_DCC_MAX_COMPRESSED_BLOCK_SIZE_SHIFT    45
#define AMDGPU_TILING_DCC_MAX_COMPRESSED_BLOCK_SIZE_MASK     0x3
#define AMDGPU_TILING_DCC_MAX_UNCOMPRESSED_BLOCK_SIZE_SHIFT  47
#define AMDGPU_TILING_DCC_MAX_UNCOMPRESSED_BLOCK_SIZE_MASK   0x3

#if PAL_BUILD_GFX12
/* GFX12 and later: */
#define AMDGPU_TILING_GFX12_SWIZZLE_MODE_SHIFT			0
#define AMDGPU_TILING_GFX12_SWIZZLE_MODE_MASK			0x7
/* These are DCC recompression setting for memory management: */
#define AMDGPU_TILING_GFX12_DCC_MAX_COMPRESSED_BLOCK_SHIFT	3
#define AMDGPU_TILING_GFX12_DCC_MAX_COMPRESSED_BLOCK_MASK	0x3 /* 0:64B, 1:128B, 2:256B */
#define AMDGPU_TILING_GFX12_DCC_NUMBER_TYPE_SHIFT		5
#define AMDGPU_TILING_GFX12_DCC_NUMBER_TYPE_MASK		0x7 /* CB_COLOR0_INFO.NUMBER_TYPE */
#define AMDGPU_TILING_GFX12_DCC_DATA_FORMAT_SHIFT		8
#define AMDGPU_TILING_GFX12_DCC_DATA_FORMAT_MASK		0x3f /* [0:4]:CB_COLOR0_INFO.FORMAT, [5]:MM */

#define AMD_FMT_MOD_TILE_VER_GFX12 5
/* Gfx12 swizzle modes:
 *    0 - LINEAR
 *    1 - 256B_2D  - 2D block dimensions
 *    2 - 4KB_2D
 *    3 - 64KB_2D
 *    4 - 256KB_2D
 *    5 - 4KB_3D   - 3D block dimensions
 *    6 - 64KB_3D
 *    7 - 256KB_3D
 */
/*
 * 64K_D_2D on GFX12 is identical to 64K_D on GFX11.
 */
#define AMD_FMT_MOD_TILE_GFX12_64K_2D 3
#endif
