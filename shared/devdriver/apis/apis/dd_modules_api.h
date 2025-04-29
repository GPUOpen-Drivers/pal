/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#ifndef DD_MODULES_API_H
#define DD_MODULES_API_H

#include "dd_common_api.h"
#include "dd_api_registry_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DD_MODULES_API_NAME "DD_MODULES_API"

#define DD_MODULES_API_VERSION_MAJOR 0
#define DD_MODULES_API_VERSION_MINOR 1
#define DD_MODULES_API_VERSION_PATCH 0

/// A DevDriver module is a C/C++ library (static or dynamic) that implements and exports the function
/// `void DDModuleLoad_xxx(DDApiRegistry* pRegistry);`, where "xxx" is the filename of the compiled module.
///
/// ```c++
/// DD_DECLARE_MODULE_LOAD_API(foo);
///
/// DD_RESULT DDModuleLoad_foo(DDApiRegistry* pRegistry) {
///    printf("foo module is loaded");
/// }
/// ```
///
/// It is recommended that modules register their APIs in DDModuleLoad_xxx(), but query others' APIs later
/// during module initialization.

    #define DD_DECLARE_MODULE_LOAD_API(name) \
        extern "C" __attribute__((visibility("default"))) DD_RESULT DDModuleLoad_ ## name (DDApiRegistry* pApiRegistry)

typedef struct DDModuleInstance DDModuleInstance;

/// This struct holds module level callback functions that each module can implement.
typedef struct DDModulesCallbacks
{
    /// An opaque pointer to a module instance.
    DDModuleInstance* pInstance;

    /// This function is called after __all__ modules (static and dynamic) have been loaded. The order at
    /// which this function is called for every module is not guaranteed.
    ///
    /// @param pInstance Must be \ref DDModulesCallbacks.pInstance.
    /// @return DD_RESULT_SUCCESS The module has been initialized successfully.
    DD_RESULT (*Initialize)(DDModuleInstance* pInstance);

    /// This function gives a module a chance to clean up their resources before the system shuts down.
    /// The order at which this function is called for every module is not guaranteed.
    ///
    /// @param pInstance Must be \ref DDModulesCallbacks.pInstance.
    void (*Destroy)(DDModuleInstance* pInstance);
} DDModulesCallbacks;

typedef struct DDModulesManagerInstance DDModulesManagerInstance;

/// This struct contains functions for DDModule.
typedef struct DDModulesApi
{
    /// A opaque pointer to an internal modules manager instance.
    DDModulesManagerInstance* pInstance;

    /// Add an implementation of \ref DDModulesCallbacks.
    ///
    /// @param pInstance Must be \ref DDModulesApi.pInstance.
    /// @param pCallback A pointer to a \ref DDModulesCallbacks object. This callback object must persist
    /// before the module is unloaded at the end of the program.
    /// @return DD_RESULT_SUCCESS If a callback implementation is added successfully.
    /// @return DD_RESULT_COMMON_INVALID_PARAMETER If either \param pInstance or \param pCallback is NULL.
    DD_RESULT (*AddModulesCallbacks)(DDModulesManagerInstance* pInstance, DDModulesCallbacks* pCallback);
} DDModulesApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
