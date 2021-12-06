### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddRpcServer #######################################################################################################
devdriver_library(ddRpcServer STATIC)
target_link_libraries(ddRpcServer
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        ddRpcShared
        ddSocket
        ddNet
        ddCore
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddRpcServerApi.h DESTINATION inc)
	install(TARGETS ddRpcServer          DESTINATION lib)
endif()

target_include_directories(ddRpcServer PUBLIC  inc)
target_include_directories(ddRpcServer PRIVATE src)

target_sources(ddRpcServer PRIVATE
    inc/ddRpcServerApi.h
    inc/ddRpcServer.h
    src/ddRpcServer.cpp
    src/rpcServer.h
    src/rpcServer.cpp
    src/rpcClientHandler.h
    src/rpcClientHandler.cpp
)
