This directory contains files from multiple locations that we require to implement the HSA ABI runtime.
* hsa.h, amd_hsa*.h: Define the core HSA/AQL types we need to emulate the HSA submission model. They were copied from
  [ROCR](https://github.com/RadeonOpenCompute/ROCR-Runtime/tree/master/src/inc).
* AMDHSAKernelDescriptor.h: Defines the HSA ABI types stored in an HSA code object binary. It was copied from
  [LLVM](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/Support/AMDHSAKernelDescriptor.h).