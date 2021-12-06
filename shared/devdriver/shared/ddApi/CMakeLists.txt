### Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All rights reserved. ###

### ddApi #########################################################################################################
devdriver_library(ddApi INTERFACE)

if (DD_BP_INSTALL)
	install(FILES inc/ddApi.h DESTINATION inc)
endif()

target_include_directories(ddApi INTERFACE inc)