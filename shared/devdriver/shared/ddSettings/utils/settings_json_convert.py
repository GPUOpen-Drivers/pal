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

def extract_build_filters(parent):
    """Extract and convert BuiltTypes macros.

    `parent` is a dict that contains a "BuildTypes" or "OrBuildTypes" field.
    """
    build_types = parent.get("BuildTypes", None)
    if build_types:
        cond = {"Macros": build_types}
        if len(build_types) > 1:
            cond["Relation"] = "AND"
        return cond

    or_build_types = parent.get("OrBuildTypes", None)
    if or_build_types:
        cond = {"Macros": or_build_types}
        if len(or_build_types) > 1:
            cond["Relation"] = "OR"
        return cond

    return None

def convert_to_settings2_type(type):
    assert type != "struct"

    if type == "size_t" or type == "gpusize":
        return "uint64"
    elif type == "enum":
        return "uint8"
    else:
        return type

def convert_to_settings2_tags(old_tags):
    new_tags_list = []
    for tag_i, tag in enumerate(old_tags):
        if isinstance(tag, str):
            new_tags_list.append(tag)
        elif isinstance(tag, dict):
            cond = extract_build_filters(tag)
            new_tag = {"Name": tag["Name"], "BuildFilters": cond}
            new_tags_list.append(new_tag)
        else:
            raise ValueError(
                f"The tag at index {tag_i} in top-level Tags list is neither a string nor an object."
            )

    return new_tags_list

def convert_to_settings2_defaults(defaults, type):
    def fix(default):
        if isinstance(default, str):
            default_lowercase = default.lower()
            if default_lowercase == "true":
                return True
            elif default_lowercase == "false":
                return False
            else:
                return default
        else:
            return default

    defaults = {
        "Default": fix(defaults["Default"]),
        "Type": convert_to_settings2_type(type),
    }

    default_win = defaults.get("WinDefault", None)
    if default_win:
        defaults["Windows"] = fix(default_win)

    default_linux = defaults.get("LnxDefault", None)
    if default_linux:
        defaults["Linux"] = fix(default_linux)

    return defaults

def validvalues_missing_name_fix(setting_name):
    if setting_name == "SEMask":
        return "ShaderEngineBitmask"
    elif setting_name == "CmdBufferLoggerAnnotations":
        return "CmdBufferLoggerAnnotations"
    elif setting_name == "BasePreset" or setting_name == "ElevatedPreset":
        return "PresetLogFlags"
    else:
        print(f'[WARN] ValidValues of {setting_name} has no "Name" field')

def convert_to_settings2_setting(setting: dict, group_name: str) -> dict:
    setting_name = setting["Name"]
    if group_name:
        setting_name = group_name + "_" + setting_name

    new_setting = {
        "Name": setting_name,
        "Description": setting["Description"],
    }

    if group_name is None:
        # If VariableName and Name only differ in capitalization of the first letter,
        # remove VariableName. Codegen script defaults to use Name with the first
        # letter converted to lowercase as VariableName.
        setting_var_name = setting["VariableName"]
        if (
            setting_var_name[0].upper() != setting_name[0]
            or setting_name[1:] != setting_var_name[1:]
        ):
            new_setting["VariableName"] = setting_var_name
    else:
        # For settings in a group, VariableNames are based on the newly
        # concatenanted Name, so no need to add them.
        pass

    tags = setting.get("Tags", None)
    if tags:
        new_setting["Tags"] = tags

    driver_states = setting.get("DriverState", None)
    if driver_states:
        new_setting["DriverStates"] = driver_states

    new_setting["Defaults"] = convert_to_settings2_defaults(
        setting["Defaults"], setting["Type"]
    )

    is_bitmask = False
    flags = setting.get("Flags", None)
    if flags:
        new_flags = {}
        if flags.get("IsHex", False):
            new_flags["IsHex"] = True
        if flags.get("IsPath", False):
            new_flags["IsDir"] = True
        if flags.get("IsFile", False):
            new_flags["IsFile"] = True
        if flags.get("RereadSetting", False):
            new_flags["ReadAfterOverride"] = True
        if flags.get("IsBitmask"):
            is_bitmask = True

        if len(new_flags) != 0:
            new_setting["Flags"] = new_flags

    registry_scope = setting.get("Scope", "PrivatePalKey")
    new_setting["Scope"] = registry_scope

    if setting["Type"] == "enum" and not is_bitmask:
        new_setting["Enum"] = setting["ValidValues"]["Name"]
    elif is_bitmask:
        valid_values = setting.get("ValidValues", None)
        assert (
            valid_values != None
        ), "[WARN] {} is a bitmask but has no ValidValues".format(setting_name)

        name = valid_values.get("Name", None)
        if name is None:
            new_setting["Bitmask"] = validvalues_missing_name_fix(setting_name)
        else:
            new_setting["Bitmask"] = name

    build_filters = extract_build_filters(setting)
    if build_filters:
        new_setting["BuildFilters"] = build_filters

    return new_setting

def convert_to_settings2_setting_or_struct(setting):
    converted_settings = []

    setting_struct = setting.get("Structure", None)
    if setting_struct:
        for subsetting in setting_struct:
            group_name = setting["Name"]
            new_setting = convert_to_settings2_setting(subsetting, group_name)
            converted_settings.append(new_setting)
    else:
        new_setting = convert_to_settings2_setting(setting, None)
        converted_settings.append(new_setting)

    return converted_settings

def convert_to_settings2(old_settings: dict) -> dict:
    settings2 = {}  # settings conforming to the new schema

    settings2["Version"] = 2
    settings2["ComponentName"] = old_settings["ComponentName"]
    settings2["DriverStates"] = old_settings["DriverState"]

    enums = []
    settings2["Enums"] = enums

    bitmasks = []
    settings2["Bitmasks"] = bitmasks

    settingItems = []
    settings2["Settings"] = settingItems

    settings2["Tags"] = convert_to_settings2_tags(old_settings["Tags"])

    for setting in old_settings["Settings"]:
        flags = setting.get("Flags", None)
        is_bitmask = False
        if flags:
            if flags.get("IsBitmask", False):
                is_bitmask = True
            elif flags.get("IsHex", False):
                valid_values = setting.get("ValidValues")
                if valid_values:
                    # fix for bad json
                    is_bitmask = valid_values.get("IsEnum", False)

        # Populate Enums
        # It's not an enum if IsBitmask is set to True
        if setting["Type"] == "enum" and not is_bitmask:
            valid_values = setting["ValidValues"]
            enum = {"Name": valid_values["Name"], "Description": setting["Description"]}
            variants = []
            for val in valid_values["Values"]:
                variant = {"Name": val["Name"], "Value": val["Value"]}

                desc = val.get("Description", None)
                if desc is None:
                    name = val["Name"]
                    valid_values_name = valid_values["Name"]
                    print(
                        f'[WARN] Enum field "{name}" of "{valid_values_name}" has no description'
                    )
                else:
                    variant["Description"] = desc

                build_filters = extract_build_filters(val)
                if build_filters:
                    variant["BuildFilters"] = build_filters
                variants.append(variant)
            enum["Variants"] = variants
            enums.append(enum)

        # Populate Bitmasks
        if is_bitmask:
            valid_values = setting["ValidValues"]
            bitmask = {
                "Name": valid_values["Name"],
                "Description": setting["Description"],
            }
            bits = []
            for val in valid_values["Values"]:
                bit = {
                    "Name": val["Name"],
                    "Value": val["Value"],
                    "Description": val["Description"],
                }
                build_filters = extract_build_filters(val)
                if build_filters:
                    bit["BuildFilters"] = build_filters
                bits.append(bit)
            bitmask["Bits"] = bits
            bitmasks.append(bitmask)

        # Populate Settings
        converted_settings = convert_to_settings2_setting_or_struct(setting)
        settingItems.extend(converted_settings)

    return settings2

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert a Settings json file to the one that conforms to the Settings 2.0 schema."
    )

    parser.add_argument(
        "old_file", help="A path to an existing Settings json file of the old schema."
    )
    parser.add_argument(
        "new_file", help="A file path to store the converted Settings json file."
    )

    args = parser.parse_args()

    json_file = open(args.old_file)
    assert json_file != None

    settings_json = json.load(json_file)
    assert settings_json != None
    json_file.close()

    settings2 = convert_to_settings2(settings_json)
    with open(args.new_file, "w") as file:
        json.dump(settings2, file, indent=2)

    return 0

if __name__ == "__main__":
    sys.exit(main())
