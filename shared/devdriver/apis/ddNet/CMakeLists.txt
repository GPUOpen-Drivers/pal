### Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddNet #############################################################################################################
devdriver_library(ddNet STATIC)
target_link_libraries(ddNet
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddNetApi.h DESTINATION inc)
	install(TARGETS ddNet          DESTINATION lib)
endif()

target_include_directories(ddNet PUBLIC  inc)
target_include_directories(ddNet PRIVATE src)

target_sources(ddNet PRIVATE
    inc/ddNetApi.h
    inc/ddNet.h
    src/ddNet.cpp
)
