### Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved. ###

### ddEventStreamer #####################################################################################################
devdriver_library(ddEventStreamer STATIC)
target_link_libraries(ddEventStreamer
    PUBLIC
        ddApi
    PRIVATE
        ddEventClient
        ddEventParser
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
    install(FILES   inc/ddEventStreamerApi.h DESTINATION inc)
    install(TARGETS ddEventStreamer          DESTINATION lib)
endif()

target_include_directories(ddEventStreamer PUBLIC  inc)
target_include_directories(ddEventStreamer PRIVATE src)

target_sources(ddEventStreamer PRIVATE
    inc/ddEventStreamerApi.h
    inc/ddEventStreamer.h
    src/ddEventStreamer.cpp
    src/eventStreamer.h
    src/eventStreamer.cpp
)
