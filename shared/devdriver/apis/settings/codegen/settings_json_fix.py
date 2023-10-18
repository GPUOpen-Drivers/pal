##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

import argparse
import json
import sys

def add_missing_enum_name(setting: dict):
    valid_values = setting.get("ValidValues", None)
    if valid_values and valid_values.get("IsEnum", False):
        if "Name" not in valid_values:
            valid_values["Name"] = setting["Name"]

def remove_fields(setting: dict):
    if "DependsOn" in setting:
        del setting["DependsOn"]

    flags = setting.get("Flags", None)
    if flags and ("RereadSetting" in flags):
        del flags["RereadSetting"]

    if "Size" in setting:
        del setting["Size"]

    if "VariableName" in setting:
        del setting["VariableName"]

def convert_types(setting: dict):
    if "Type" in setting:
        type_str = setting["Type"]
        if type_str in ["gpusize", "size_t"]:
            setting["Type"] = "uint64"

def convert_is_path_to_is_dir(setting: dict):
    flags = setting.get("Flags", None)
    if flags and ("IsPath" in flags):
        flags["IsDir"] = flags["IsPath"]
        del flags["IsPath"]

def convert_default_names(setting: dict):
    defaults = setting.get("Defaults", None)
    if defaults:
        if "WinDefault" in defaults:
            defaults["Windows"] = defaults["WinDefault"]
            del defaults["WinDefault"]
        if "LnxDefault" in defaults:
            defaults["Linux"] = defaults["LnxDefault"]
            del defaults["LnxDefault"]

def fix(setting: dict):
    add_missing_enum_name(setting)
    remove_fields(setting)
    convert_types(setting)
    convert_is_path_to_is_dir(setting)
    convert_default_names(setting)

def main() -> int:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Fix a settings JSON file to conform to the settings 2.0 schema.",
    )

    parser.add_argument(
        "old_file",
        type=str,
        help="A path to an existing settings JSON file.",
    )
    parser.add_argument(
        "new_file",
        type=str,
        help="A file path to store the fixed settings JSON file.",
    )

    args = parser.parse_args()

    with open(args.old_file) as file:
        settings_json = json.load(file)

    if "settings" in settings_json:
        settings_json["Settings"] = settings_json["settings"]
        del settings_json["settings"]

    for setting in settings_json["Settings"]:
        structure = setting.get("Structure", None)
        if structure:
            for subsetting in structure:
                fix(subsetting)

            # Remove "Type" from "Structure" setting.
            if "Type" in setting:
                del setting["Type"]
        fix(setting)

    with open(args.new_file, "w") as file:
        json.dump(settings_json, file, indent=2)

    return 0

if __name__ == "__main__":
    sys.exit(main())
