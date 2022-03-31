devdriver_library(ddGpuInfo STATIC)

target_link_libraries(ddGpuInfo
    PUBLIC
        # Link against ddCore to get standard devdriver defs
        ddCore
        devdriver_gpuinfo
)

# This library contains exactly one cpp file, chosen based on platform:
if (CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    if (ANDROID)
        # These flags allow CMake to search beyond the scope of the Android SDK. Libdrm will
        # not be picked up without this change.
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
    endif()

    # If this call fails install both libdrm-dev:amd64 and libdrm-dev:i386
    find_package(Libdrm REQUIRED)

    target_sources(ddGpuInfo PRIVATE src/ddLinuxAmdGpuInfo.cpp)
    target_include_directories(ddGpuInfo PRIVATE ${Libdrm_INCLUDE_DIRS})
#if DD_ACCESS_INTERNAL
# messagelib provides a Windows implementation of QueryGpuInfo.
# This is only available when messagelib is.
elseif (WIN32)
    if (TARGET devdriver_message)
        target_link_libraries(ddGpuInfo PRIVATE devdriver_message)
        target_sources(ddGpuInfo PRIVATE src/ddWinFillerAmdGpuInfo.cpp)
    else()
        # When messagelib is not available, fall back to an empty implementation and print a unique warning message about this.
        message(WARNING "No QueryGpuInfo() implementation available on Windows with GPUOPEN_BUILD_MESSAGEBUS_TRANSPORT=OFF")
        target_sources(ddGpuInfo PRIVATE src/ddEmptyAmdGpuInfo.cpp)
    endif()
#endif // DD_ACCESS_INTERNAL
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_sources(ddGpuInfo PRIVATE src/ddMacAmdGpuInfo.mm)
    target_link_libraries(ddGpuInfo PUBLIC "-framework Cocoa -framework Metal")
else()
    message(WARNING "No QueryGpuInfo() implementation available on ${CMAKE_SYSTEM_NAME} with current configuration")
    target_sources(ddGpuInfo PRIVATE src/ddEmptyAmdGpuInfo.cpp)
endif()
