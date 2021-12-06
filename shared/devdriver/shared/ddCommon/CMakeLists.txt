### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddCommon ##########################################################################################################
devdriver_library(ddCommon STATIC)
target_link_libraries(ddCommon
    PUBLIC
        ddApi
        ddCore
)

target_include_directories(ddCommon PUBLIC  inc)
target_include_directories(ddCommon PRIVATE src)

target_sources(ddCommon PRIVATE
    inc/ddCommon.h
    src/ddCommon.cpp
)
