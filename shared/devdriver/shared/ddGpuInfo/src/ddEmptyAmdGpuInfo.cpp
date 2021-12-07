
#include <ddAmdGpuInfo.h>

namespace DevDriver
{

// Provide an empty version of this function for configurations that haven't otherwise provided it
Result QueryGpuInfo(const AllocCb& allocCb, Vector<AmdGpuInfo>* pGpus)
{
    DD_UNUSED(allocCb);
    DD_UNUSED(pGpus);

    DD_PRINT(LogLevel::Error, "QueryGpuInfo() is not implemented for your platform/configuration.");

    return Result::Unavailable;
}

} // namespace DevDriver
