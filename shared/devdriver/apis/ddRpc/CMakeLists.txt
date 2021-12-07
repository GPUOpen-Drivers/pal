### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

devdriver_push_scope("Rpc")

### Shared Code #######################################################################################################
add_subdirectory(ddRpcShared)

### Client ############################################################################################################
add_subdirectory(ddRpcClient)

### Server ############################################################################################################
add_subdirectory(ddRpcServer)

### Unit Tests ########################################################################################################
if(DD_OPT_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
