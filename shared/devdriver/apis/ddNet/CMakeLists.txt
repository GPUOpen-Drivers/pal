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

### Unit tests
if(DD_OPT_ENABLE_TESTS)
    devdriver_executable(tests_ddNet
        tests/ddNetTests.h
        tests/ddNetTests.cpp
    )

    target_include_directories(tests_ddNet
        PRIVATE
            tests
    )

    target_link_libraries(tests_ddNet
        PRIVATE
            ddNet
            ddTestUtil
    )

    set_target_properties(tests_ddNet
        PROPERTIES
            FOLDER "${DEVDRIVER_TESTS_ROOT}"
    )

    devdriver_discover_tests(tests_ddNet)
endif()
