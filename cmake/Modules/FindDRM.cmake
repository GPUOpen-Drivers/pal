set(_DRM_SEARCHES
    /opt/amdgpu-pro/include
    /opt/amdgpu-pro/include/libdrm
   )

find_path(XF86_INCLUDE_PATH xf86drm.h ${_DRM_SEARCHES})
find_path(AMDGPU_INCLUDE_PATH amdgpu.h ${_DRM_SEARCHES})

if(XF86_INCLUDE_PATH AND AMDGPU_INCLUDE_PATH)
    set(DRM_FOUND 1)
    set(DRM_INCLUDE_DIRS ${XF86_INCLUDE_PATH} ${AMDGPU_INCLUDE_PATH})
    message(STATUS "Found DRM: ${DRM_INCLUDE_DIRS}")
endif ()
