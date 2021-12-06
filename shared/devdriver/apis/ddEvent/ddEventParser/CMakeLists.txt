### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddEventParser #####################################################################################################
devdriver_library(ddEventParser STATIC)
target_link_libraries(ddEventParser
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddEventParserApi.h DESTINATION inc)
	install(TARGETS ddEventParser          DESTINATION lib)
endif()

target_include_directories(ddEventParser PUBLIC  inc)
target_include_directories(ddEventParser PRIVATE src)

target_sources(ddEventParser PRIVATE
    inc/ddEventParserApi.h
    inc/ddEventParser.h
    src/ddEventParser.cpp

    src/eventParser.h
    src/eventParser.cpp

    src/legacy/legacyEventParser.h
    src/legacy/legacyEventParser.cpp
)
