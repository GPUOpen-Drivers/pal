/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "dd_registry_utils.h"

#include <Windows.h>
#include <SetupAPI.h>
// This must appear before ntddvdeo to link
#include <initguid.h>
#include <ntddvdeo.h>
#include <stack>

#pragma warning(push)
#pragma warning(disable : 4996) // Disable deprecated warning for codecvt
// Needed to convert widestring to string
#include <codecvt>

std::string ws2s(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

#pragma warning(pop)

namespace DevDriver
{
// This is a port of the code from LibAmd routine for finding the driver store
void GetRegistryPaths(std::set<std::string>* pRegistryPaths)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DISPLAY_DEVICE_ARRIVAL,
                                            nullptr,
                                            nullptr,
                                            DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (hDevInfo != INVALID_HANDLE_VALUE)
    {
        SP_DEVICE_INTERFACE_DATA devInterface = { };
        devInterface.cbSize = sizeof(devInterface);

        char buffer[sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA) + (MAX_PATH * sizeof(TCHAR))];

        // Walk through the list of devices Windows reports to us:
        for (uint32_t chain = 0;
            (SetupDiEnumDeviceInterfaces(hDevInfo,
                                         nullptr,
                                         &GUID_DISPLAY_DEVICE_ARRIVAL,
                                         chain,
                                         &devInterface) != 0);
                                    ++chain)
        {
            SP_DEVINFO_DATA devInfo = { };
            devInfo.cbSize = sizeof(devInfo);

            // The UNICODE version must be used to be compatible with the name of the device used to
            // open an adapter instance.
            SP_DEVICE_INTERFACE_DETAIL_DATA_W* const pDevInterfaceDetails =
                reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(&buffer[0]);
            pDevInterfaceDetails->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            // Use the wide-character version of this function as we use the device name to obtain an
            // adapter handle from Windows, and the "open adapter from device name" function requires a
            // wide-character device name.
            if (SetupDiGetDeviceInterfaceDetailW(hDevInfo,
                &devInterface,
                pDevInterfaceDetails,
                sizeof(buffer),
                nullptr,
                &devInfo))
            {
                DWORD requiredSize        = 0;
                wchar_t hardwareIds[2048] = {};
                if (SetupDiGetDeviceRegistryPropertyW(hDevInfo,
                    &devInfo,
                    SPDRP_HARDWAREID,
                    NULL,
                    reinterpret_cast<PBYTE>(&hardwareIds),
                    sizeof(hardwareIds),
                    &requiredSize))
                {
                    // Check our HW ID for PCI and 1002 (AMD).
                    std::wstring ws(hardwareIds);

                    if ((ws.find(L"PCI", 0) == 0) && (ws.find(L"VEN_1002", 0) != 0))
                    {
                        // We've found an AMD part.
                        wchar_t path[2048] = {};

                        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo,
                            &devInfo,
                            SPDRP_DRIVER,
                            NULL,
                            reinterpret_cast<PBYTE>(&path),
                            sizeof(path),
                            &requiredSize))
                        {
                            std::wstring registryPath(path);

                            registryPath = std::wstring(L"SYSTEM\\CurrentControlSet\\Control\\Class\\") + registryPath;
                            registryPath = registryPath + std::wstring(L"\\UMD");

                            std::wstring subkeys[] = { L"DXC", L"VULKAN", L"DXXP" };
                            for (const auto& subkey : subkeys)
                            {
                                HKEY hKey;
                                std::wstring fullPath = registryPath + L"\\" + subkey;
                                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, fullPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
                                {
                                    pRegistryPaths->insert(ws2s(fullPath));
                                    RegCloseKey(hKey);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

DD_RESULT EnumerateDriverRegistry(const std::string&                     rootKey,
                                  std::vector <DDSettingsRegistryInfo> & output)
{
    std::stack<std::string> keysToProcess;
    keysToProcess.push(rootKey);

    DD_RESULT result = DD_RESULT_SUCCESS;

    while (!keysToProcess.empty())
    {
        std::string currentKey = keysToProcess.top();
        keysToProcess.pop();

        HKEY hSubKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, currentKey.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS)
        {
            char  keyName[256];
            DWORD keyNameSize;
            DWORD index = 0;

            // Parse the parent component from the currentKey path
            size_t      pos    = currentKey.find_last_of("\\");
            std::string parent = (pos == std::string::npos) ? currentKey : currentKey.substr(pos + 1);

            // Enumerate subkeys
            while (true)
            {
                keyNameSize = sizeof(keyName);
                if (RegEnumKeyExA(hSubKey, index, keyName, &keyNameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                {
                    break;
                }
                keysToProcess.push(currentKey + "\\" + keyName);
                index++;
            }

            // Enumerate values
            index = 0;
            while (true)
            {
                keyNameSize = sizeof(keyName);
                DWORD valueType;
                if (RegEnumValueA(hSubKey, index, keyName, &keyNameSize, NULL, &valueType, NULL, NULL) != ERROR_SUCCESS)
                {
                    break;
                }
                DDSettingsRegistryInfo item = {};
                strncpy(item.registryComponentName, parent.c_str(), DD_SETTINGS_MAX_COMPONENT_NAME_SIZE - 1);
                item.registryComponentName[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE - 1] = '\0';

                std::string keyNameStr(keyName);
                // Settings stored as hashes start with "#"
                if (keyNameStr[0] == '#')
                {
                    // Skip the "#" and convert to our name hash
                    std::string numberStr = keyNameStr.substr(1);
                    char*       endPtr    = nullptr;
                    errno                 = 0; // Reset errno before the call
                    uint64_t number       = std::strtoull(numberStr.c_str(), &endPtr, 10);

                    if ((errno == 0) && (*endPtr == '\0') && (number <= UINT32_MAX))
                    {
                        item.nameHash     = static_cast<DD_SETTINGS_NAME_HASH>(number);
                        item.storedAsHash = true;
                    }
                    else
                    {
                        // Handle conversion error
                        item.nameHash     = static_cast<DD_SETTINGS_NAME_HASH>(-1);
                        item.storedAsHash = true;
                    }
                }
                else
                {
                    strncpy(item.settingNameStr, keyName, DD_SETTINGS_MAX_MISC_STRING_SIZE - 1);
                    item.settingNameStr[DD_SETTINGS_MAX_MISC_STRING_SIZE - 1] = '\0';
                    item.storedAsHash = false;
                }

                output.push_back(item);
                index++;
            }

            RegCloseKey(hSubKey);
        }
        else
        {
            result = DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR;
        }
    }

    return result;
}

DD_RESULT CheckAndDeleteValue(HKEY hKey, const DDSettingsRegistryInfo* pInfo)
{
    char nameHashStr[DD_SETTINGS_MAX_MISC_STRING_SIZE];
    if (pInfo->storedAsHash)
    {
        snprintf(nameHashStr, sizeof(nameHashStr), "#%u", pInfo->nameHash);
    }

    DWORD index = 0;
    char  valueName[DD_SETTINGS_MAX_MISC_STRING_SIZE];
    DWORD valueNameSize = sizeof(valueName);
    DWORD type;
    BYTE  data[1024];
    DWORD dataSize = sizeof(data);

    while (RegEnumValueA(hKey, index, valueName, &valueNameSize, NULL, &type, data, &dataSize) == ERROR_SUCCESS)
    {
        if ((pInfo->storedAsHash && (strcmp(valueName, nameHashStr) == 0)) ||
            (strcmp(valueName, pInfo->settingNameStr) == 0))
        {
            RegDeleteValueA(hKey, valueName);
            return DD_RESULT_SUCCESS;
        }
        index++;
        valueNameSize = sizeof(valueName);
        dataSize      = sizeof(data);
    }

    return DD_RESULT_DD_GENERIC_FILE_NOT_FOUND;
}

DD_RESULT DeleteRegistrySetting(const std::string& rootKeyStr, const DDSettingsRegistryInfo* pInfo)
{
    HKEY hRootKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, rootKeyStr.c_str(), 0, KEY_READ | KEY_WRITE, &hRootKey) != ERROR_SUCCESS)
    {
        return DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR;
    }

    std::vector<std::string> keysToCheck = { rootKeyStr };
    char                     subKeyName[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE];

    while (!keysToCheck.empty())
    {
        std::string currentKey = keysToCheck.back();
        keysToCheck.pop_back();

        HKEY hCurrentKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, currentKey.c_str(), 0, KEY_READ | KEY_WRITE, &hCurrentKey) !=
            ERROR_SUCCESS)
        {
            continue;
        }

        // Check if the current key is the one we are looking for:
        if (currentKey.substr(currentKey.find_last_of("\\") + 1) == pInfo->registryComponentName)
        {
            if (CheckAndDeleteValue(hCurrentKey, pInfo) == DD_RESULT_SUCCESS)
            {
                RegCloseKey(hCurrentKey);
                RegCloseKey(hRootKey);
                return DD_RESULT_SUCCESS;
            }
        }

        DWORD index          = 0;
        DWORD subKeyNameSize = sizeof(subKeyName);
        // Collect the subkeys to be checked next:
        while (RegEnumKeyExA(hCurrentKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            std::string subKeyPath = currentKey + "\\" + subKeyName;
            keysToCheck.push_back(subKeyPath);
            index++;
            subKeyNameSize = sizeof(subKeyName);
        }

        RegCloseKey(hCurrentKey);
    }

    RegCloseKey(hRootKey);
    return DD_RESULT_DD_GENERIC_FILE_NOT_FOUND;
}

}
