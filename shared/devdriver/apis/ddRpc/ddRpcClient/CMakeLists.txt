### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddRpcClient #######################################################################################################
devdriver_library(ddRpcClient STATIC)
target_link_libraries(ddRpcClient
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        ddRpcShared
        ddSocket
        ddCore
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddRpcClientApi.h DESTINATION inc)
	install(TARGETS ddRpcClient          DESTINATION lib)
endif()

target_include_directories(ddRpcClient PUBLIC  inc)
target_include_directories(ddRpcClient PRIVATE src)

target_sources(ddRpcClient PRIVATE
    inc/ddRpcClientApi.h
    inc/ddRpcClient.h
    src/ddRpcClient.cpp
    src/rpcClient.h
    src/rpcClient.cpp
)
