### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

set(CMAKE_FOLDER "${DEVDRIVER_TESTS_ROOT}")

devdriver_executable(tests_ddRpc
    # Main test files
    src/ddRpcTests.h
    src/ddRpcTests.cpp
)

target_include_directories(tests_ddRpc
    PRIVATE
        src
)

target_link_libraries(tests_ddRpc
    PRIVATE
        ddRpcClient
        ddRpcServer
        ddTestUtil
)

set_target_properties(tests_ddRpc
    PROPERTIES
        FOLDER "${DEVDRIVER_TESTS_ROOT}"
)

devdriver_discover_tests(tests_ddRpc)
