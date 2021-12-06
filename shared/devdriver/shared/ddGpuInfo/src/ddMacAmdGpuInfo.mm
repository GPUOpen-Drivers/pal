/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include <ddAmdGpuInfo.h>

#include <ddPlatform.h>

#import <Metal/Metal.h>

// Protocol extension to access the AMD SPI implemented by our MTLDevice classes.
@protocol AmdDevDriverSPIPrivateDevice <MTLDevice>

// This private SPI provides data about the GPU, but only when the developer mode is active.
- (NSDictionary*)queryGpuInfo;

@end

// NSObject extension needed to unwrap the MTLDevice when it is wrapped by Apple's debug layer.
@interface NSObject (BaseObject)

// Accessor for the wrapped object.
-(id)baseObject;

@end


namespace DevDriver
{



Result QueryGpuInfo(const AllocCb& allocCb, Vector<AmdGpuInfo>* pGpus)
{
    Result result = Result::InvalidParameter;

    DD_UNUSED(allocCb);

    if (pGpus != nullptr)
    {
        result = Result::Success;

         NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
         if (devices)
         {
             for (id<MTLDevice> device in devices)
             {
                 AmdGpuInfo gpu = {};
                 bzero(&gpu, sizeof(gpu));

                 // Xcode's validation & capture layers wrap the MTLDevice and we have to progressively unwrap them using baseObject.
                 while([device respondsToSelector:@selector(baseObject)])
                 {
                     device = [((NSObject*)device) baseObject];
                 }

                 if ([device respondsToSelector:@selector(queryGpuInfo)])
                 {
                     id<AmdDevDriverSPIPrivateDevice> privDevice = (id<AmdDevDriverSPIPrivateDevice>)device;

                     // When the Metal driver is running in developer mode it will return a dictionary of data.
                     // This data describes the GPU & Metal driver. So far only the data needed to fill in AmdGpuInfo is used.
                     NSDictionary* dict = [privDevice queryGpuInfo];
                     if (dict)
                     {
                         NSNumber* version = [dict objectForKey:@"InfoVersion"];
                         if ((version != nil) && (version.unsignedIntValue > 0))
                         {
                             NSString* name = [dict objectForKey:@"Name"];
                             NSNumber* index = [dict objectForKey:@"Index"];
                             NSNumber* engine = [dict objectForKey:@"Engine"];
                             NSNumber* family = [dict objectForKey:@"Family"];
                             NSNumber* deviceId = [dict objectForKey:@"DeviceID"];
                             NSNumber* deviceRev = [dict objectForKey:@"DeviceRevision"];
                             NSNumber* revision = [dict objectForKey:@"Revision"];
                             NSNumber* memoryType = [dict objectForKey:@"MemoryType"];
                             NSNumber* memoryOps = [dict objectForKey:@"MemoryOps"];
                             NSNumber* busWidth = [dict objectForKey:@"BusWidth"];
                             NSNumber* vram = [dict objectForKey:@"VRAM"];
                             NSNumber* counterFreq = [dict objectForKey:@"CounterFreq"];
                             NSNumber* sysClockFreq = [dict objectForKey:@"SysClockFreq"];
                             NSNumber* memClockFreq = [dict objectForKey:@"MemClockFreq"];

                             /*
                              * The following data will also be exported and they are enough to fill in the RGP ASIC info block.
                              * Mac model & CPU data can be fetched directly by the DevDriver.

                             NSString* driverVersionString = [dict objectForKey:@"MetalDriverVersion"];
                             NSNumber* buildNumber = [dict objectForKey:@"MetalBuildNumber"];

                             NSNumber* rgpFlags = [dict objectForKey:@"RGPFlags"];
                             NSNumber* numVGPRS = [dict objectFey:@"VGPRS"];
                             NSNumber* numSGPRS = [dict objectForKey:@"SGPRS"];
                             NSNumber* shaderEngines = [dict objectForKey:@"ShaderEngines"];
                             NSNumber* computeUnitPerShaderEngine = [dict objectForKey:@"ComputeUnitPerShaderEngine"];
                             NSNumber* simdPerComputeUnit = [dict objectForKey:@"SimdPerComputeUnit"];
                             NSNumber* wavefrontsPerSimd = [dict objectForKey:@"WavefrontsPerSimd"];
                             NSNumber* minimumVgprAlloc = [dict objectForKey:@"MinimumVgprAlloc"];
                             NSNumber* vgprAllocGranularity = [dict objectForKey:@"VgprAllocGranularity"];
                             NSNumber* minimumSgprAlloc = [dict objectForKey:@"MinimumSgprAlloc"];
                             NSNumber* sgprAllocGranularity = [dict objectForKey:@"SgprAllocGranularity"];
                             NSNumber* hardwareContexts = [dict objectForKey:@"HardwareContexts"];
                             NSNumber* gpuType = [dict objectForKey:@"GpuType"];
                             NSNumber* gfxIpLevel = [dict objectForKey:@"GfxIpLevel"];
                             NSNumber* gdsSize = [dict objectForKey:@"GdsSize"];
                             NSNumber* gdsPerShaderEngineSize = [dict objectForKey:@"GdsPerShaderEngineSize"];
                             NSNumber* ceRamSize = [dict objectForKey:@"CeRamSize"];
                             NSNumber* ceRamSizeGraphics = [dict objectForKey:@"CeRamSizeGraphics"];
                             NSNumber* ceRamSizeCompute = [dict objectForKey:@"CeRamSizeCompute"];
                             NSNumber* maximumDedicatedCu = [dict objectForKey:@"MaximumDedicatedCu"];
                             NSNumber* level2CacheSize = [dict objectForKey:@"Level2CacheSize"];
                             NSNumber* level1CacheSize = [dict objectForKey:@"Level1CacheSize"];
                             NSNumber* ldsSize = [dict objectForKey:@"LdsSize"];
                             NSNumber* aluPerClock = [dict objectForKey:@"AluPerClock"];
                             NSNumber* texturesPerClock = [dict objectForKey:@"TexturesPerClock"];
                             NSNumber* primitivesPerClock = [dict objectForKey:@"PrimitivesPerClock"];
                             NSNumber* pixelsPerClock = [dict objectForKey:@"PixelsPerClock"];
                             NSNumber* ldsGranularity = [dict objectForKey:@"LdsGranularity"];
                             NSData* cuMask = [dict objectForKey:@"CuMask"];

                             // The cuMask object is an NSData whose content matches the below definition:
                             struct CuMaskStruct
                             {
                                 constexpr size_t RGP_MAX_NUM_SE = 32;
                                 constexpr size_t RGP_SA_PER_SE = 2;
                                 uint16_t mask[RGP_MAX_NUM_SE][RGP_SA_PER_SE];
                             };
                             */

                             DevDriver::Platform::Strncpy(gpu.name, name.UTF8String);

                             gpu.asic.gpuIndex = index.unsignedIntValue;
                             gpu.asic.ids.gfxEngineId = engine.unsignedIntValue;
                             gpu.asic.ids.family = family.unsignedIntValue;
                             gpu.asic.ids.deviceId = deviceId.unsignedIntValue;
                             gpu.asic.ids.revisionId = deviceRev.unsignedIntValue;
                             gpu.asic.ids.eRevId = revision.unsignedIntValue;

                             gpu.memory.type = (LocalMemoryType)memoryType.unsignedIntValue;
                             gpu.memory.memOpsPerClock = memoryOps.unsignedIntValue;
                             gpu.memory.busBitWidth = busWidth.unsignedIntValue;
                             gpu.memory.invisibleHeap.physAddr = 0;
                             gpu.memory.invisibleHeap.size = vram.unsignedLongValue;

                             gpu.memory.clocksHz.min = 0;
                             gpu.memory.clocksHz.max = memClockFreq.unsignedLongValue;

                             gpu.engineClocks.min = 0;
                             gpu.engineClocks.max = sysClockFreq.unsignedLongValue;

                             gpu.asic.gpuCounterFreq = counterFreq.unsignedLongValue;

                             pGpus->PushBack(gpu);
                         }
                     }
                 }
             }
             [devices release];
         }
    }
    return result;
}
} // namespace DevDriver
