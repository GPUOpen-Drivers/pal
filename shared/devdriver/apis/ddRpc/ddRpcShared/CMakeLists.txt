### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddRpcShared #######################################################################################################
devdriver_library(ddRpcShared STATIC)
target_link_libraries(ddRpcShared
    PUBLIC
        ddSocket
    PRIVATE
        ddCommon
        ddCore
        mpack
)

target_include_directories(ddRpcShared PUBLIC  inc)
target_include_directories(ddRpcShared PRIVATE src)

target_sources(ddRpcShared PRIVATE
    inc/ddRpcShared.h
    src/ddRpcShared.cpp
)
