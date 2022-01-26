### Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddSocket ##########################################################################################################
devdriver_library(ddSocket STATIC)
target_link_libraries(ddSocket
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddSocketApi.h DESTINATION inc)
	install(TARGETS ddSocket          DESTINATION lib)
endif()

target_include_directories(ddSocket PUBLIC  inc)
target_include_directories(ddSocket PRIVATE src)

target_sources(ddSocket PRIVATE
    inc/ddSocketApi.h
    inc/ddSocket.h
    src/ddSocket.cpp
    src/socketServer.h
    src/socketServer.cpp
)
