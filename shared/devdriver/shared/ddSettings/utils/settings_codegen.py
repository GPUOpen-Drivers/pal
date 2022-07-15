##
 #######################################################################################################################
 #
 #  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
import io
import time
import jsonschema
import sys
from os import path
from jinja2 import Environment as JinjaEnv, FileSystemLoader as JinjaLoader
from ruamel.yaml import YAML
from ruamel.yaml.comments import CommentedSeq
from ruamel.yaml.compat import ordereddict

def extract_yaml_comment(seq: CommentedSeq, index: int) -> str:
    """Extract the filetering comments (e.g. `#if ...`) of the `i`-th item in the sequence `seq`.

    For example, in the YAML data below, the comment `BUILD_APPLE` should be
    considered to be associated with the item "Apple".

    ```yaml
    Fruits:
      - Banana
    #if BUILD_APPLE
      - Apple
    #endif
      - Orange
    ```
    """
    import ruamel.yaml

    assert ruamel.yaml.__version__ == "0.16.10", (
        "This function uses internals of ruamel.yaml, and was only tested "
        "with the version 0.16.10, beyond which nothing is guaranteed to work."
    )

    last_line = lambda comment: comment.splitlines()[-1].lstrip()

    assert index >= 0

    comment = ""
    if index == 0:
        # If it's the first item, its comment is attached to the sequence
        # itself.
        if seq.ca.comment:
            comment = last_line(seq.ca.comment[1][0].value)
    else:
        if isinstance(seq[index], dict):
            # If items are of type `dict`, the comment is attached to the last
            # key-value pair of the previous item.
            if seq[index - 1].ca.items:
                last_key = next(reversed(seq[index - 1]))
                comment = last_line(seq[index - 1].ca.items[last_key][2].value)
        else:
            if seq.ca.items:
                comment = last_line(seq.ca.items[index - 1][0].value)

    return comment

def setup_build_filters(seq: CommentedSeq):
    """Check attached comment and add 'BuildFilters' field accordingly."""
    # Because current item's filtering comment is attached to the last field of
    # the previous item, directly adding 'BuildFilters' field to this item
    # would interfere with the parsing of the next item.
    build_filters = []
    for i in range(len(seq)):
        comment = extract_yaml_comment(seq, i)
        if comment.startswith("#if "):
            build_filters.append((i, comment))

    for i, comment in build_filters:
        seq[i]["BuildFilters"] = comment

def fnv1a(bytes: bytes):
    fnv_prime = 0x01000193
    hval = 0x811C9DC5
    uint32Mod = 0xFFFFFFFF

    for byte in bytes:
        hval = int(byte) ^ hval
        hval = hval * fnv_prime
        hval = hval & uint32Mod

    return hval

def setup_default(setting: dict, group_name: str) -> str:
    """A Jinja filter that renders settings default value assignment."""

    def assign_line(setting: dict, platform: str, group_name: str) -> str:
        setting_name = setting["VariableName"]
        if len(group_name) != 0:
            setting_name = f"{group_name}.{setting_name}"

        default = setting["Defaults"][platform]
        default_str = ""
        enum_name = setting.get("Enum", None)
        bitmask_name = setting.get("Bitmask", None)
        is_string = False
        if enum_name:
            if isinstance(default, str):
                default_str = f"{enum_name}::{default}"
            elif isinstance(default, int):
                default_str = str(default)
            else:
                raise ValueError(
                    f"{setting_name} is an Enum. Its default value must be either "
                    f"an integer or a string representing one of the variants of its "
                    f"Enum type, but it's {type(default)}."
                )

        elif bitmask_name:
            if isinstance(default, str):
                default_str = f"{bitmask_name}::{default}"
            elif isinstance(default, int):
                default_str = str(default)
            else:
                raise ValueError(
                    f"{setting_name} is an Enum. Its default value must be either "
                    f"an integer or a string representing one of the variants of its "
                    f"Enum type, but it's {type(default)}."
                )

        elif isinstance(default, bool):
            default_str = "true" if default else "false"

        elif isinstance(default, int):
            flags = setting.get("Flags", None)
            if flags and "IsHex" in flags:
                default_str = hex(default)
            else:
                default_str = str(default)

        elif isinstance(default, str):
            if setting["Defaults"]["Type"] == "float":
                default_str = default
                if default_str[-1:] != "f":
                    default_str += "f"
            else:
                default_str = f'"{default}"'
                is_string = True

        elif isinstance(default, float):
            default_str = f"{str(default)}f"

        else:
            raise ValueError(
                f'Invalid type of default value for the setting "{setting_name}"'
            )

        if is_string:
            string_len = setting["StringLength"]
            # Need to preserve this long line so the output C++ code is well formatted.
            return f"    memset(m_settings.{setting_name}, 0, {string_len});\n    strncpy(m_settings.{setting_name}, {default_str}, {string_len});\n"
        else:
            return f"    m_settings.{setting_name} = {default_str};\n"

    defaults = setting["Defaults"]

    default_win = defaults.get("Windows", None)
    default_linux = defaults.get("Linux", None)

    result = ""
    if default_win and default_linux:
        result += "#if defined(_WIN32)\n"
        result += assign_line(setting, "Windows", group_name)
        result += "#elif (__unix__)\n"
        result += assign_line(setting, "Linux", group_name)
        result += "#else\n"
        result += assign_line(setting, "Default", group_name)
        result += "#endif\n"
    elif default_win:
        result += "#if defined(_WIN32)\n"
        result += assign_line(setting, "Windows", group_name)
        result += "#else\n"
        result += assign_line(setting, "Default", group_name)
        result += "#endif\n"
    elif default_linux:
        result += "#if defined(__unix__)\n"
        result += assign_line(setting, "Linux", group_name)
        result += "#else\n"
        result += assign_line(setting, "Default", group_name)
        result += "#endif\n"
    else:
        result += assign_line(setting, "Default", group_name)

    return result

# A jinja filter that returns "Util::ValueType".
def setting_type_cpp(setting_type: str) -> str:
    if setting_type == "bool":
        return "Util::ValueType::Boolean"
    elif setting_type == "int8" or setting_type == "int16" or setting_type == "int32":
        return "Util::ValueType::Int"
    elif setting_type == "uint8":
        return "Util::ValueType::Uint8"
    elif setting_type == "uint16" or setting_type == "uint32":
        return "Util::ValueType::Uint"
    elif setting_type == "uint64":
        return "Util::ValueType::Uint64"
    elif setting_type == "float":
        return "Util::ValueType::Float"
    elif setting_type == "string":
        return "Util::ValueType::Str"
    else:
        return '"Invalid value for the JSON field Defaults::Type."'

# A jinja filter that returns "SettingType".
def setting_type_cpp2(setting_type: str) -> str:
    if setting_type == "bool":
        return "SettingType::Boolean"
    elif setting_type == "int8" or setting_type == "int16" or setting_type == "int32":
        return "SettingType::Int"
    elif setting_type == "uint8":
        return "SettingType::Uint8"
    elif setting_type == "uint16" or setting_type == "uint32":
        return "SettingType::Uint"
    elif setting_type == "uint64":
        return "SettingType::Uint64"
    elif setting_type == "int64":
        return "SettingType::Int64"
    elif setting_type == "float":
        return "SettingType::Float"
    elif setting_type == "string":
        return "SettingType::String"
    else:
        return '"Invalid value for the JSON field Defaults::Type."'

def gen_variable_name(name: str) -> str:
    """Generate C++ variable names.
    The variable name is generated by lowercasing the first character if
    it's followed by a lowercase character. If `name` starts with multiple
    uppercase characters, the first n-1 characters are lowercased, unless
    all characters in `name` are uppercase, in which case all are lowercased.

    For example:
    "PeerMemoryEnabled" -> "peerMemoryEnabled"
    "RISSharpness" -> "risSharpness"
    "TFQ" -> "tfq"
    """
    uppercase_len = 0
    for i in range(len(name)):
        if name[i].isupper():
            uppercase_len += 1
        else:
            break

    var_name = None
    if uppercase_len == len(name):
        var_name = name.lower()
    elif uppercase_len == 0:
        var_name = name
    elif uppercase_len == 1:
        var_name = name[0].lower() + name[1:]
    else:
        var_name = name[: uppercase_len - 1].lower() + name[uppercase_len - 1 :]

    return var_name

def validate_settings_name(name: str):
    """Valid a "Name" field in JSON object.

    The generated C++ variable names are based on `name`. So we need to make
    sure they can compile.

    A name must start with an alphabetic letter and can only contain
    alphanumeric characters, plus underscore.
    """
    if not name[0].isalpha():
        raise ValueError(f'"{name}" does not start with an aphabetic letter.')
    name_cleaned = name.replace("_", "")
    if not name_cleaned.isalnum():
        raise ValueError(
            f'"{name}" contains character(s) other than alphanumeric ' "and underscore."
        )

def validate_tag(tag: str):
    """Validate a "Tag" field in JSON object.

    The maximum length of a tag string is 40. A tag must start with an
    alphabetic letter, and only contain alphanumeric characters, underscores
    ('_'), dashes ('-') and spaces (' '). There cannot be trailing spaces.
    """
    MAX_TAG_STR_LEN = 40
    if len(tag) > MAX_TAG_STR_LEN:
        raise ValueError(
            f'The tag "{tag}" exceeds the maximum lenght ({MAX_TAG_STR_LEN}).'
        )

    if not tag[0].isalpha():
        raise ValueError(f'The tag "{tag}" does not start with an aphabetic letter.')

    if tag[-1] == " ":
        raise ValueError(f'The tag "{tag}" has trailing space(s).')

    tag_cleaned = tag.replace("_", "").replace("-", "").replace(" ", "")
    if not tag_cleaned.isalnum:
        raise ValueError(
            f'The tag "{tag}" contains characters(s) other than '
            "alphanumeric underscores, dashes, and spaces"
        )

# Generate a encoded byte array of the settings json data.
def gen_settings_blob(settings: dict, magic_buf: bytes) -> bytearray:
    settings_stream = io.StringIO()
    yaml = YAML()
    yaml.dump(settings, settings_stream)
    settings_str = settings_stream.getvalue()
    settings_bytes = bytearray(settings_str.encode(encoding="utf-8"))

    # Pad settings_bytes to be 4-byte aligned.
    settings_bytes_len = len(settings_bytes)
    assert settings_bytes_len != 0, "Settings JSON binary buffer is empty."

    remainder4 = settings_bytes_len % 4
    if remainder4 != 0:
        for i in range(4 - remainder4):
            settings_bytes.append(0)
    settings_4byte_len = len(settings_bytes)

    # 4-byte aligned length
    magic_4byte_len = int(len(magic_buf) / 4) * 4

    # XOR all bytes of settings with that of the magic buffer.
    settings_bytes_encrypted = bytearray()
    settings_bytes_cnt = 0
    repeat = (settings_4byte_len - 1 + magic_4byte_len) % magic_4byte_len
    for _ in range(repeat):
        for i in range(0, magic_4byte_len, 4):
            if settings_bytes_cnt >= settings_4byte_len:
                break

            # Do it 4 bytes at a time.
            magic_int32 = int.from_bytes(
                magic_buf[i : (i + 4)], byteorder="little", signed=False
            )
            settings_int32 = int.from_bytes(
                settings_bytes[settings_bytes_cnt : (settings_bytes_cnt + 4)],
                byteorder="little",
                signed=False,
            )
            xor_res = settings_int32 ^ magic_int32
            settings_bytes_encrypted.extend(
                xor_res.to_bytes(4, byteorder="little", signed=False)
            )
            settings_bytes_cnt += 4

        if settings_bytes_cnt >= settings_4byte_len:
            break

    return settings_bytes_encrypted[:settings_bytes_len]

def gen_setting_name_hashes(settings: ordereddict):
    """Generate name hash for each setting.

    And add it as 'NameHash' field. This function also validates setting names
    and checks for duplicates.
    """
    setting_name_set = set()  # used to detect duplicate names
    for setting in settings:
        name = setting["Name"]
        validate_settings_name(name)

        if name not in setting_name_set:
            setting_name_set.add(name)
        else:
            raise ValueError(f"Duplicate setting name: {name}")

        # `setting` is an ordereddict, and appending a key-value pair would
        # interfere with how we extract filtering comments (e.g. `#if ...`).
        # So insert at the beginning of `setting`.
        setting.insert(0, "NameHash", fnv1a(bytes(name.encode(encoding="utf-8"))))

def prepare_enums(settings_root: dict) -> set:
    """Prepare enums in the top-level 'Enums' list.

    - Add 'BuildFilters' field to enum variants from its attached comments.
    - Return a set of all enum names. ValueError exception is raised when
      duplicate names are found.
    """
    enum_names = []
    enums = settings_root.get("Enums", None)
    if enums:
        for enum in enums:
            name = enum["Name"]
            validate_settings_name(name)
            enum_names.append(name)

            variants = enum["Variants"]
            setup_build_filters(variants)

    enum_names_unique = set(enum_names)

    if len(enum_names) != len(enum_names_unique):
        duplicates = [x for x in enum_names_unique if enum_names.count(x) > 1]
        raise ValueError(f"Duplicate Enum names: {duplicates}")

    return enum_names_unique

def prepare_settings_meta(
    settings_root: dict, magic_buf: bytes, codegen_header: str, settings_header: str
):
    """Prepare settings meta data for code generation.

    Meta data are anything that's not under the "Settings" field in the YAML.

    settings_root:
        The root of Settings YAML object.

    magic_buf:
        An array of randomly generated bytes.

    codegen_header:
        The name of the auto-generated header file by this script.

    settings_header:
        The name of the header file that contains a subclass of `SettingsBase`.
    """

    # Generating the blob before adding anything else to settings root object.
    settings_blob = gen_settings_blob(settings_root, magic_buf)

    # Compute settings blob hash. Blob hashes are used for consistency check
    # between tools and drivers.
    settings_root["SettingsBlobHash"] = hash(bytes(settings_blob))

    settings_root["SettingsBlob"] = ",".join(map(str, settings_blob))
    settings_root["SettingsBlobSize"] = len(settings_blob)

    settings_root["NumSettings"] = len(settings_root["Settings"])

    settings_root["CodeGenHeader"] = codegen_header
    settings_root["SettingsHeader"] = settings_header

    component_name = settings_root["ComponentName"]
    validate_settings_name(component_name)
    settings_root["ComponentNameLower"] = component_name[0].lower() + component_name[1:]

def prepare_settings_list(settings: dict, enum_name_set: set):

    setup_build_filters(settings)

    group_settings_indices = []

    for setting_idx, setting in enumerate(settings):
        setting_name = setting["Name"]

        # Settings 2.0 schema no longer contains 'VariableName'. It's generated
        # from 'Name'.
        setting["VariableName"] = gen_variable_name(setting_name)

        # Add "VariableType" field.
        enum_name = setting.get("Enum", None)
        if enum_name and (enum_name not in enum_name_set):
            raise ValueError(
                f'Unknown enum name "{enum_name}" of the setting "{setting_name}". '
                "An individual setting's enum name must match one in the Enums list."
            )

        bitmask_name = setting.get("Bitmask", None)
        if enum_name and (enum_name not in enum_name_set):
            raise ValueError(
                f'Unknown bitmask name "{enum_name}" of the setting "{setting_name}". '
                "An individual setting's bitmask name must match one in the Enums list."
            )

        if enum_name:
            setting["VariableType"] = enum_name
        elif bitmask_name:
            setting["VariableType"] = "uint32_t"
        else:
            defaults = setting["Defaults"]
            type_str = defaults["Type"]
            if type_str == "string":
                # We only support fixed-size string.
                setting["VariableType"] = "char"
            elif type_str.startswith("int") or type_str.startswith("uint"):
                setting["VariableType"] = type_str + "_t"
            else:
                setting["VariableType"] = type_str

        # If a string, add a field "StringLength".
        if setting["Defaults"]["Type"] == "string":
            flags = setting["Flags"]
            if flags.get("IsDir", False) or flags.get("IsFile", False):
                setting["StringLength"] = "DevDriver::kSettingsMaxPathStrLen"
            else:
                # Constant `SettingsMaxMiscStrLen` is defined in asettingsBase.h.
                setting["StringLength"] = "DevDriver::kSettingsMaxMiscStrLen"

        # Record group settings for later processing.
        if "Group" in setting:
            group_settings_indices.append(setting_idx)

    # Transform group settings (settings with a 'Group' field) into subsettings.
    setting_groups = {}  # a map of group_name -> subsettings
    for idx in reversed(group_settings_indices):
        setting = settings.pop(idx)
        group_name = setting["Group"]
        del setting["Group"]
        subsettings = setting_groups.get(group_name, None)
        if subsettings is None:
            subsettings = []
            setting_groups[group_name] = subsettings
        subsettings.append(setting)
    # Add setting groups into settings list.
    for group_name, subsettings in setting_groups.items():
        settings.append(
            {
                "GroupName": group_name,
                "GroupVariableName": group_name[:1].lower() + group_name[1:],
                "Subsettings": subsettings,
            }
        )

def main():
    timer_start = time.monotonic()

    parser = argparse.ArgumentParser(
        description="Generate Settings files from a YAML files."
    )

    subparsers = parser.add_subparsers(
        help="Settings codegen commands.", dest="command"
    )

    fnv1a_parser = subparsers.add_parser(
        "fnv1a", help="Hash a string using FNV1-a algorithm."
    )
    fnv1a_parser.add_argument("str", type=str, help="A string to be hashed.")

    codegen_parser = subparsers.add_parser(
        "gen", help="Generate Settings files from a YAML files."
    )
    codegen_parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="A path to a Settings YAML file that conforms to Settings 2.0 schema",
    )
    codegen_parser.add_argument(
        "-g",
        "--generated-filename",
        required=True,
        help="The name for both generated header and source files.\n"
        "The final names will be prefixed with 'g_' and suffixed with '.h'\n"
        "or '.cpp' for the final filenames.",
    )
    codegen_parser.add_argument(
        "-s",
        "--settings-filename",
        required=True,
        help="The name of the header file that contains the definition\n"
        "of a specific Settings class inheriting from SettingsBase.",
    )
    codegen_parser.add_argument(
        "-o",
        "--outdir",
        required=True,
        help="The directory to put the generated files.",
    )

    args = parser.parse_args()

    if args.command == "fnv1a":
        print(fnv1a(args.str.encode(encoding="utf-8")))
    elif args.command == "gen":
        assert not args.generated_filename.endswith(
            ".h"
        ) and not args.generated_filename.endswith(
            ".cpp"
        ), 'The argument "generated-filename" should not contain extension'

        g_header_path = path.join(args.outdir, f"g_{args.generated_filename}.h")
        g_source_path = path.join(args.outdir, f"g_{args.generated_filename}.cpp")

        running_script_dir = sys.path[0]

        with open(args.input) as file:
            yaml = YAML()
            settings_root = yaml.load(file)

        with open(path.join(running_script_dir, "settings_schema.yaml")) as file:
            yaml = YAML()
            settings_schema = yaml.load(file)

        jsonschema.validate(settings_root, schema=settings_schema)

        with open(path.join(running_script_dir, "magic_buffer"), "rb") as magic_file:
            magic_buf = magic_file.read()

        gen_setting_name_hashes(settings_root["Settings"])

        prepare_settings_meta(
            settings_root,
            magic_buf,
            f"g_{args.generated_filename}.h",
            args.settings_filename,
        )

        prepare_settings_list(settings_root["Settings"], prepare_enums(settings_root))

        ### Set up Jinja Environment.

        jinja_env = JinjaEnv(
            trim_blocks=True,
            lstrip_blocks=True,
            keep_trailing_newline=True,
            # Jinja2 templates should be in the same directory as this script.
            loader=JinjaLoader(running_script_dir),
        )
        jinja_env.filters["setup_default"] = setup_default
        jinja_env.filters["setting_type_cpp"] = setting_type_cpp
        jinja_env.filters["setting_type_cpp2"] = setting_type_cpp2

        header_template = jinja_env.get_template("settings.h.jinja2")
        source_template = jinja_env.get_template("settings.cpp.jinja2")

        ### Render template.

        with open(g_header_path, "w") as generated_file:
            generated_file.write(header_template.render(settings_root))

        with open(g_source_path, "w") as generated_file:
            generated_file.write(source_template.render(settings_root))

        execution_time = time.monotonic() - timer_start
        print(
            "Settings C++ code generated successfully, in {} milliseconds.".format(
                execution_time * 1000
            )
        )

    return 0

if __name__ == "__main__":
    sys.exit(main())
