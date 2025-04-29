//===--- AMDHSAKernelDescriptor.h -----------------------------*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// AMDHSA kernel descriptor definitions. For more information, visit
/// https://llvm.org/docs/AMDGPUUsage.html#kernel-descriptor
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_AMDHSAKERNELDESCRIPTOR_H
#define LLVM_SUPPORT_AMDHSAKERNELDESCRIPTOR_H

#include <cstddef>
#include <cstdint>

// Gets offset of specified member in specified type.
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE*)0)->MEMBER)
#endif // offsetof

// Creates enumeration entries used for packing bits into integers. Enumeration
// entries include bit shift amount, bit width, and bit mask.
#ifndef AMDHSA_BITS_ENUM_ENTRY
#define AMDHSA_BITS_ENUM_ENTRY(NAME, SHIFT, WIDTH) \
  NAME ## _SHIFT = (SHIFT),                        \
  NAME ## _WIDTH = (WIDTH),                        \
  NAME = (((1 << (WIDTH)) - 1) << (SHIFT))
#endif // AMDHSA_BITS_ENUM_ENTRY

// Gets bits for specified bit mask from specified source.
#ifndef AMDHSA_BITS_GET
#define AMDHSA_BITS_GET(SRC, MSK) ((SRC & MSK) >> MSK ## _SHIFT)
#endif // AMDHSA_BITS_GET

// Sets bits for specified bit mask in specified destination.
#ifndef AMDHSA_BITS_SET
#define AMDHSA_BITS_SET(DST, MSK, VAL)                                         \
  do {                                                                         \
    auto local = VAL;                                                          \
    DST &= ~MSK;                                                               \
    DST |= ((local << MSK##_SHIFT) & MSK);                                     \
  } while (0)
#endif // AMDHSA_BITS_SET

namespace llvm {
namespace amdhsa {

// Floating point rounding modes. Must match hardware definition.
enum : uint8_t {
  FLOAT_ROUND_MODE_NEAR_EVEN = 0,
  FLOAT_ROUND_MODE_PLUS_INFINITY = 1,
  FLOAT_ROUND_MODE_MINUS_INFINITY = 2,
  FLOAT_ROUND_MODE_ZERO = 3,
};

// Floating point denorm modes. Must match hardware definition.
enum : uint8_t {
  FLOAT_DENORM_MODE_FLUSH_SRC_DST = 0,
  FLOAT_DENORM_MODE_FLUSH_DST = 1,
  FLOAT_DENORM_MODE_FLUSH_SRC = 2,
  FLOAT_DENORM_MODE_FLUSH_NONE = 3,
};

// System VGPR workitem IDs. Must match hardware definition.
enum : uint8_t {
  SYSTEM_VGPR_WORKITEM_ID_X = 0,
  SYSTEM_VGPR_WORKITEM_ID_X_Y = 1,
  SYSTEM_VGPR_WORKITEM_ID_X_Y_Z = 2,
  SYSTEM_VGPR_WORKITEM_ID_UNDEFINED = 3,
};

// Kernel code properties. Must be kept backwards compatible.
#define KERNEL_CODE_PROPERTY(NAME, SHIFT, WIDTH) \
  AMDHSA_BITS_ENUM_ENTRY(KERNEL_CODE_PROPERTY_ ## NAME, SHIFT, WIDTH)
enum : int32_t {
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER, 0, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_DISPATCH_PTR, 1, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_QUEUE_PTR, 2, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_KERNARG_SEGMENT_PTR, 3, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_DISPATCH_ID, 4, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_FLAT_SCRATCH_INIT, 5, 1),
  KERNEL_CODE_PROPERTY(ENABLE_SGPR_PRIVATE_SEGMENT_SIZE, 6, 1),
  KERNEL_CODE_PROPERTY(RESERVED0, 7, 3),
  KERNEL_CODE_PROPERTY(ENABLE_WAVEFRONT_SIZE32, 10, 1), // GFX10+
  KERNEL_CODE_PROPERTY(USES_DYNAMIC_STACK, 11, 1),
  KERNEL_CODE_PROPERTY(ENABLE_WAVEGROUP, 12, 1), //# GFX13+
  KERNEL_CODE_PROPERTY(RESERVED1, 13, 3),
};
#undef KERNEL_CODE_PROPERTY

// Kernarg preload specification.
#define KERNARG_PRELOAD_SPEC(NAME, SHIFT, WIDTH)                               \
  AMDHSA_BITS_ENUM_ENTRY(KERNARG_PRELOAD_SPEC_##NAME, SHIFT, WIDTH)
enum : int32_t {
  KERNARG_PRELOAD_SPEC(LENGTH, 0, 7),
  KERNARG_PRELOAD_SPEC(OFFSET, 7, 9),
};
#undef KERNARG_PRELOAD_SPEC

// Kernel descriptor. Must be kept backwards compatible.
struct kernel_descriptor_t {
  uint32_t group_segment_fixed_size;
  uint32_t private_segment_fixed_size;
  uint32_t kernarg_size;
  uint8_t reserved0[4];
  int64_t kernel_code_entry_byte_offset;
  uint8_t reserved1[16];
  uint32_t laneshared_segment_fixed_size; //# GFX13+
  uint32_t compute_pgm_rsrc3;             // GFX10+ and GFX90A+
  uint32_t compute_pgm_rsrc1;
  uint32_t compute_pgm_rsrc2;
  uint16_t kernel_code_properties;
  uint16_t kernarg_preload;
  uint8_t reserved3[4];
};

enum : uint32_t {
  GROUP_SEGMENT_FIXED_SIZE_OFFSET = 0,
  PRIVATE_SEGMENT_FIXED_SIZE_OFFSET = 4,
  KERNARG_SIZE_OFFSET = 8,
  RESERVED0_OFFSET = 12,
  KERNEL_CODE_ENTRY_BYTE_OFFSET_OFFSET = 16,
  RESERVED1_OFFSET = 24,
  LANESHARED_SEGMENT_FIXED_SIZE_OFFSET = 40,
  COMPUTE_PGM_RSRC3_OFFSET = 44,
  COMPUTE_PGM_RSRC1_OFFSET = 48,
  COMPUTE_PGM_RSRC2_OFFSET = 52,
  KERNEL_CODE_PROPERTIES_OFFSET = 56,
  KERNARG_PRELOAD_OFFSET = 58,
  RESERVED3_OFFSET = 60
};

static_assert(
    sizeof(kernel_descriptor_t) == 64,
    "invalid size for kernel_descriptor_t");
static_assert(offsetof(kernel_descriptor_t, group_segment_fixed_size) ==
                  GROUP_SEGMENT_FIXED_SIZE_OFFSET,
              "invalid offset for group_segment_fixed_size");
static_assert(offsetof(kernel_descriptor_t, private_segment_fixed_size) ==
                  PRIVATE_SEGMENT_FIXED_SIZE_OFFSET,
              "invalid offset for private_segment_fixed_size");
static_assert(offsetof(kernel_descriptor_t, kernarg_size) ==
                  KERNARG_SIZE_OFFSET,
              "invalid offset for kernarg_size");
static_assert(offsetof(kernel_descriptor_t, reserved0) == RESERVED0_OFFSET,
              "invalid offset for reserved0");
static_assert(offsetof(kernel_descriptor_t, kernel_code_entry_byte_offset) ==
                  KERNEL_CODE_ENTRY_BYTE_OFFSET_OFFSET,
              "invalid offset for kernel_code_entry_byte_offset");
static_assert(offsetof(kernel_descriptor_t, reserved1) == RESERVED1_OFFSET,
              "invalid offset for reserved1");
static_assert(offsetof(kernel_descriptor_t, laneshared_segment_fixed_size) ==
                  LANESHARED_SEGMENT_FIXED_SIZE_OFFSET,
              "invalid offset for laneshared_segment_fixed_size");
static_assert(offsetof(kernel_descriptor_t, compute_pgm_rsrc3) ==
                  COMPUTE_PGM_RSRC3_OFFSET,
              "invalid offset for compute_pgm_rsrc3");
static_assert(offsetof(kernel_descriptor_t, compute_pgm_rsrc1) ==
                  COMPUTE_PGM_RSRC1_OFFSET,
              "invalid offset for compute_pgm_rsrc1");
static_assert(offsetof(kernel_descriptor_t, compute_pgm_rsrc2) ==
                  COMPUTE_PGM_RSRC2_OFFSET,
              "invalid offset for compute_pgm_rsrc2");
static_assert(offsetof(kernel_descriptor_t, kernel_code_properties) ==
                  KERNEL_CODE_PROPERTIES_OFFSET,
              "invalid offset for kernel_code_properties");
static_assert(offsetof(kernel_descriptor_t, kernarg_preload) ==
                  KERNARG_PRELOAD_OFFSET,
              "invalid offset for kernarg_preload");
static_assert(offsetof(kernel_descriptor_t, reserved3) == RESERVED3_OFFSET,
              "invalid offset for reserved3");

} // end namespace amdhsa
} // end namespace llvm

#endif // LLVM_SUPPORT_AMDHSAKERNELDESCRIPTOR_H
