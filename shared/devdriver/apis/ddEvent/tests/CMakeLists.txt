### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

devdriver_push_scope("Tests")

devdriver_executable(tests_ddEvent
    # Main test files
    src/ddEventTests.h
    src/ddEventTests.cpp
)

target_include_directories(tests_ddEvent
    PRIVATE
        src
)

target_link_libraries(tests_ddEvent
    PRIVATE
        ddTestUtil

        ddEventParser
        ddEventServer
        ddEventClient
)

set_target_properties(tests_ddEvent
    PROPERTIES
        FOLDER "${DEVDRIVER_TESTS_ROOT}"
)

devdriver_discover_tests(tests_ddEvent)
