### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddEventServer #####################################################################################################
devdriver_library(ddEventServer STATIC)
target_link_libraries(ddEventServer
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddEventServerApi.h DESTINATION inc)
	install(TARGETS ddEventServer          DESTINATION lib)
endif()

target_include_directories(ddEventServer PUBLIC  inc)
target_include_directories(ddEventServer PRIVATE src)

target_sources(ddEventServer PRIVATE
    inc/ddEventServerApi.h
    inc/ddEventServer.h
    src/ddEventServer.cpp

    src/eventServer.h
    src/eventServer.cpp
    src/eventProvider.h
    src/eventProvider.cpp
    src/eventShared.h
)
