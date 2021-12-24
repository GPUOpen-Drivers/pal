### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###
target_include_directories(pal PRIVATE .)

target_sources(pal PRIVATE
    amdgpu.h
    amdgpu_drm.h
    CMakeLists.txt
    drm.h
    drm_fourcc.h
    drm_mode.h
    drm_sarea.h
    xf86drm.h
    xf86drmMode.h
)
