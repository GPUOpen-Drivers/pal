### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ###

devdriver_push_scope("Event")

### Parser ############################################################################################################
add_subdirectory(ddEventParser)

### Server ############################################################################################################
add_subdirectory(ddEventServer)

### Client ############################################################################################################
add_subdirectory(ddEventClient)

### Unit Tests ########################################################################################################
if(DD_OPT_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
