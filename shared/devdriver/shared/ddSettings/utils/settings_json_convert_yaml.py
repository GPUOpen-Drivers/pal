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
from typing import List
from ruamel.yaml import YAML

def extract_build_types(parent: dict) -> List:
    build_types = parent.get("BuildTypes", None)
    or_build_types = parent.get("OrBuildTypes", None)

    assert (build_types is None) or (or_build_types is None), f"parent: {parent}"

    if build_types is not None:
        return build_types
    elif or_build_types is not None:
        return or_build_types
    else:
        return []

def convert_to_settings2_top_level_tags(tags: List) -> List:
    """Convert the top level 'Tags'."""
    new_tags = []
    for tag in tags:
        if isinstance(tag, dict):
            build_types = extract_build_types(tag)
            if len(build_types) > 0:
                new_tag = {"name": tag["Name"], "buildtypes": build_types}
            else:
                new_tag = tag["Name"]
            new_tags.append(new_tag)
        else:
            new_tags.append(tag)
    return new_tags

def convert_to_settings2_enum_helper(valid_values: dict) -> dict:
    name = valid_values["Name"]
    description = valid_values.get("Description", name)
    enum = {"name": name, "description": description}

    values = []
    for valid_val in valid_values["Values"]:
        value = dict({"name": valid_val["Name"], "value": valid_val["Value"]})

        desc = valid_val.get("Description", None)
        if desc is None:
            name = valid_val["Name"]
            valid_values_name = valid_values["Name"]
            print(
                f'[WARN] Enum field "{name}" of "{valid_values_name}" has no description'
            )
        else:
            value["description"] = desc

        build_types = extract_build_types(valid_val)
        if len(build_types) > 0:
            value["buildtypes"] = build_types

        values.append(value)

    assert len(values) > 0
    enum["values"] = values

    build_types = extract_build_types(valid_values)
    if len(build_types) > 0:
        enum["buildtypes"] = build_types

    return enum

def convert_to_settings2_type(type: str) -> str:
    type_lower = type.strip().lower()

    if type_lower == "struct":
        raise AssertionError()
    if type_lower in ["size_t", "gpusize"]:
        return "uint64"
    elif type_lower == "enum":
        return "uint32"
    elif type_lower == "string":
        return "str"
    else:
        return type_lower

def convert_to_settings2_defaults(defaults: dict, type: str) -> dict:
    def fix(default, type: str):
        fixed_default = default

        if type == "float" and isinstance(default, str):
            fixed_default = float(default.replace("f", ""))  # e.g. "1.25f" -> 1.25"
        elif type.startswith("uint") or type.startswith("int"):
            if isinstance(default, str):
                if default.isdigit():
                    fixed_default = int(default, 0)
                elif default.startswith("0x"):
                    fixed_default = int(default, 16)
                else:
                    # default is an enum name, we will convert it to its
                    # corresponding enum value later
                    pass
        elif isinstance(default, str):
            default_lowercase = default.lower()
            if default_lowercase == "true":
                fixed_default = True
            elif default_lowercase == "false":
                fixed_default = False
            else:
                fixed_default = default.replace("\\", "\\\\")

        return fixed_default

    new_type = convert_to_settings2_type(type)

    new_defaults = {
        "default": fix(defaults["Default"], new_type),
        "type": new_type,
    }

    default_win = defaults.get("WinDefault", None)
    if default_win is not None:
        new_defaults["windows"] = fix(default_win, new_type)

    default_linux = defaults.get("LnxDefault", None)
    if default_linux is not None:
        new_defaults["linux"] = fix(default_linux, new_type)

    return new_defaults

def validvalues_missing_name_fix(setting_name: str) -> str:
    if setting_name == "CmdBufferLoggerAnnotations":
        return "CmdBufferLoggerAnnotations"
    elif setting_name in ["BasePreset", "ElevatedPreset"]:
        return "PresetLogFlags"
    elif setting_name == "FileSettingsFlags":
        return "FileSettingsFlagsBitmask"
    elif setting_name == "FileAccessFlags":
        return "FileAccessFlagsBitmask"
    elif setting_name == "OrigTypeMask":
        return "OrigTypeMask"
    else:
        print(f'[WARN] ValidValues of {setting_name} has no "Name" field')

def validvalue_missing_name_fix(valid_values: dict):
    """This fixes the missing name of individual values of a valid_values object."""
    for value in valid_values["Values"]:
        if "Name" not in value:
            if valid_values["Name"] == "PresetLogFlags":
                description = value["Description"]
                value["Name"] = description[: description.index(":")]
            else:
                value["Name"] = value["Description"]

            print(
                "[WARN] Missing value name in ValidValues {}. Set it to {}".format(
                    valid_values["Name"], value["Name"]
                )
            )

def convert_to_settings2_enum(setting: dict, enums: List[dict]):
    flags = setting.get("Flags", None)
    is_bitmask = False
    valid_values = setting.get("ValidValues")
    if flags:
        if flags.get("IsBitmask", False):
            is_bitmask = True
        elif flags.get("IsHex", False):
            if valid_values:
                # fix for bad json
                is_bitmask = valid_values.get("IsEnum", False)

    if setting["Type"] == "enum" or (is_bitmask and valid_values):
        valid_values = setting["ValidValues"]
        if "Values" in valid_values:
            if "Name" not in valid_values:
                valid_values["Name"] = validvalues_missing_name_fix(setting["Name"])

            validvalue_missing_name_fix(valid_values)

            enum = convert_to_settings2_enum_helper(valid_values)

            if "buildtypes" not in enum:
                # check buildtypes associated with this setting
                build_types = list(extract_build_types(setting))
                if len(build_types) > 0:
                    enum["buildtypes"] = build_types

            if not any(map(lambda x: x["name"] == enum["name"], enums)):
                enums.append(enum)
        else:
            # if "Values" is a not field, then its "Name" field references an
            # already defined ValidValues
            pass

def fix_setting_name(name: str) -> str:
    assert name[0].isalpha(), "Setting name must start with an alphabet letter."

    name = name[0].upper() + name[1:]
    return name

def convert_to_settings2_setting(setting: dict, group_name: str) -> dict:
    setting_name = setting["Name"]
    new_setting = {
        "name": fix_setting_name(setting_name),
        "description": setting["Description"],
    }

    if group_name:
        new_setting["group"] = group_name

    tags = setting.get("Tags", None)
    if tags:
        new_setting["tags"] = tags

    is_bitmask = False
    flags = setting.get("Flags", None)
    if flags:
        new_flags = {}
        if flags.get("IsHex", False):
            new_flags["isHex"] = True
        if flags.get("IsPath", False):
            new_flags["isDir"] = True
        if flags.get("IsFile", False):
            new_flags["isFile"] = True
        if flags.get("RereadSetting", False):
            new_flags["ReadAfterOverride"] = True
        if flags.get("IsBitmask"):
            is_bitmask = True

        if len(new_flags) != 0:
            new_setting["flags"] = new_flags

    new_setting["defaults"] = convert_to_settings2_defaults(
        setting["Defaults"], setting["Type"]
    )

    registry_scope = setting.get("Scope", "PrivatePalKey")
    new_setting["scope"] = registry_scope

    if setting["Type"] == "enum" and not is_bitmask:
        new_setting["enum"] = setting["ValidValues"]["Name"]
    elif is_bitmask:
        valid_values = setting.get("ValidValues", None)
        if valid_values:
            name = valid_values.get("Name", None)
            if name is None:
                new_setting["enum"] = validvalues_missing_name_fix(setting_name)
            else:
                new_setting["enum"] = name
        else:
            print("[WARN] {} is a bitmask but has no ValidValues.".format(setting_name))

    build_types = extract_build_types(setting)
    if len(build_types) > 0:
        new_setting["buildtypes"] = build_types

    return new_setting

def convert_to_settings2_setting_or_struct(setting: dict, enums: List[dict]) -> dict:
    converted_settings = []

    setting_struct = setting.get("Structure", None)
    struct_build_types = extract_build_types(setting)
    struct_tags = setting.get("Tags", [])
    if setting_struct:
        for subsetting in setting_struct:
            # Extract build types from struct and attach them to subsettings.
            build_types = extract_build_types(subsetting)
            if len(build_types) == 0 and len(struct_build_types) > 0:
                subsetting["BuildTypes"] = list(struct_build_types)
            elif len(build_types) > 0:
                for buildtype in struct_build_types:
                    if buildtype not in build_types:
                        build_types.append(buildtype)

            # Extract tag from struct and attach them to subsettings.
            subsetting_tags = subsetting.get("Tags", [])
            if len(subsetting_tags) == 0 and len(struct_tags) > 0:
                subsetting["Tags"] = list(struct_tags)
            elif len(subsetting_tags) > 0:
                for tag in struct_tags:
                    if tag not in subsetting_tags:
                        subsetting_tags.append(tag)

            convert_to_settings2_enum(subsetting, enums)

            group_name = setting["Name"]
            new_setting = convert_to_settings2_setting(subsetting, group_name)
            converted_settings.append(new_setting)
    else:
        convert_to_settings2_enum(setting, enums)
        new_setting = convert_to_settings2_setting(setting, None)
        converted_settings.append(new_setting)

    return converted_settings

def convert_to_settings2(old_settings: dict) -> dict:
    settings2 = dict()

    settings2["version"] = 2
    settings2["component"] = old_settings["ComponentName"]

    tags = convert_to_settings2_top_level_tags(old_settings["Tags"])
    if len(tags) > 0:
        settings2["tags"] = tags

    enums = []
    if "Enums" in old_settings:
        enums = [convert_to_settings2_enum_helper(v) for v in old_settings["Enums"]]
    settings2["enums"] = enums

    new_settings_list = []
    settings2["settings"] = new_settings_list

    for setting in old_settings["Settings"]:
        # Populate top-level Enums list
        convert_to_settings2_enum(setting, enums)

        # Populate Settings
        converted_settings = convert_to_settings2_setting_or_struct(setting, enums)
        new_settings_list.extend(converted_settings)

    return settings2

def convert_settings2_enum_value_to_integer(settings2: dict):
    """Convert default values of enum settings to integers.

    If they are strings of enum variants, use the corresponding values of those
    variants.
    """

    def variant_name_to_int(variant_name: str, enum: dict) -> int:
        result = 0
        if variant_name.startswith("("):
            variant_name = variant_name[1 : variant_name.index(")")]

        variant_name_list = variant_name.split("|")

        for name in variant_name_list:
            val = next(v["value"] for v in enum["values"] if v["name"] == name.strip())
            if isinstance(val, str):
                val = int(val, 0)
            result |= int(val)
        return result

    enums = settings2["enums"]

    for setting in settings2["settings"]:
        enum_name = setting.get("enum", None)
        if enum_name:
            enum = next(e for e in enums if e["name"] == enum_name)
            assert enum is not None, "Setting {} references an unknown enum {}.".format(
                setting["name"], enum_name
            )

            defaults = setting["defaults"]
            if isinstance(defaults["default"], str):
                defaults["default"] = variant_name_to_int(defaults["default"], enum)

            if "windows" in defaults:
                defaults["windows"] = variant_name_to_int(defaults["windows"], enum)

            if "linux" in defaults:
                defaults["linux"] = variant_name_to_int(defaults["linux"], enum)

def main() -> int:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Convert a Settings JSON file to a YAML file that conforms to the Settings 2.0 schema.",
    )

    parser.add_argument(
        "old_file",
        type=str,
        help="A path to an existing Settings JSON file of the old schema.",
    )
    parser.add_argument(
        "new_file",
        type=str,
        help="A file path to store the converted Settings YAML file.",
    )

    args = parser.parse_args()

    settings_json = None
    with open(args.old_file) as file:
        settings_json = json.load(file)
        assert settings_json is not None, "`old_file` invalid: empty JSON input file"

    settings2 = convert_to_settings2(settings_json)
    convert_settings2_enum_value_to_integer(settings2)

    with open(args.new_file, "w") as file:
        yaml = YAML()
        yaml.default_flow_style = False
        yaml.indent(mapping=2, sequence=4, offset=2)
        yaml.dump(settings2, file)

    return 0

if __name__ == "__main__":
    sys.exit(main())
