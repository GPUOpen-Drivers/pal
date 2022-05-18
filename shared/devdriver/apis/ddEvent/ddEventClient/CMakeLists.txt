### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddEventClient #####################################################################################################
devdriver_library(ddEventClient STATIC)
target_link_libraries(ddEventClient
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
    install(FILES   inc/ddEventClientApi.h DESTINATION inc)
    install(TARGETS ddEventClient          DESTINATION lib)
endif()

target_include_directories(ddEventClient PUBLIC  inc)
target_include_directories(ddEventClient PRIVATE src)

target_sources(ddEventClient PRIVATE
    inc/ddEventClientApi.h
    inc/ddEventClient.h
    src/ddEventClient.cpp

    src/eventClient.h
    src/eventClient.cpp

    src/legacy/legacyEventClient.h
    src/legacy/legacyEventClient.cpp
)
