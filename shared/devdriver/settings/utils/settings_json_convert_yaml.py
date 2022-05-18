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
import json
import sys
from typing import List
import ruamel.yaml
from ruamel.yaml import YAML
from ruamel.yaml.comments import CommentedSeq, CommentedMap
from ruamel.yaml.tokens import CommentToken
from ruamel.yaml.error import CommentMark

def add_build_filter_comment(seq: CommentedSeq, index: int, filter_comment: str):
    '''Add surrounding build filter comments.
    '''
    assert ruamel.yaml.__version__ == '0.16.10', 'This function uses internals ' \
        'of ruamel.yaml, and was only tested with the version 0.16.10, beyond which ' \
        'nothing is guaranteed to work.'

    assert index >= 0

    ENDIF_STR = '\n#endif\n'

    def append_comment(comment_items, key, pos, comment_str):
        comment = comment_items.get(key, None)
        if comment:
            # Comments are attached to the items are appear before them. If
            # there is already a comment attached to this item, it must be
            # `ENDIF_STR`.
            comment_str = ENDIF_STR + comment_str
            comment[pos].reset()
        else:
            comment_items[key] = [None, None, None, None]

        comment_items[key][pos] = CommentToken(comment_str, CommentMark(0), CommentMark(0))

    if index == 0:
        seq.ca.comment = [
            None,
            [CommentToken(f'{filter_comment}\n', CommentMark(0), CommentMark(0))]
        ]
    else:
        if isinstance(seq[index], dict):
            last_key = next(reversed(seq[index - 1]))
            append_comment(seq[index - 1].ca.items, last_key, 2, f'\n{filter_comment}\n')
        else:
            append_comment(seq.ca.items, index - 1, 0, f'\n{filter_comment}\n')

    if isinstance(seq[index], dict):
        last_key = next(reversed(seq[index]))
        append_comment(seq[index].ca.items, last_key, 2, ENDIF_STR)
    else:
        append_comment(seq.ca.items, index, 0, ENDIF_STR)

def extract_build_types(parent: dict) -> str:
    '''Extract BuiltTypes/OrBuiltTypes macros.

    `parent` is a dict that contains a "BuildTypes" or "OrBuildTypes" field.

    Return a string in the format of "#if macro [|| macro]", or "#if macro [&& macro]".
    '''
    result = None

    build_types = parent.get('BuildTypes', None)
    if build_types:
        result = '#if '
        for i, macro in enumerate(build_types):
            if i > 0:
                result += ' && '
            result += macro

    or_build_types = parent.get('OrBuildTypes', None)
    if or_build_types:
        result = '#if '
        for i, macro in enumerate(build_types):
            if i > 0:
                result += ' || '
            result += macro

    return result

def extract_filtered_tags(tags: List) -> List[dict]:
    '''Return a list of tags that could be filtered.
    '''
    filtered_tags = []
    for tag in tags:
        if isinstance(tag, dict):
            filter_str = extract_build_types(tag)
            if filter_str:
                filtered_tags.append(
                    {
                        'Name': tag['Name'],
                        'Filter': filter_str
                    }
                )
    return filtered_tags

def convert_to_settings2_tags(
    old_tags: List[str],
    filtered_tags: List[dict]) -> CommentedSeq:
    '''Add filter comments to tags.
    '''
    new_tags_list = CommentedSeq()
    for tag in old_tags:
        new_tags_list.append(tag)

        for filtered_tag in filtered_tags:
            if tag == filtered_tag['Name']:
                add_build_filter_comment(new_tags_list, len(new_tags_list) - 1, filtered_tag['Filter'])

    return new_tags_list

def convert_to_settings2_type(type: str) -> str:
    if type == 'struct':
        raise AssertionError()
    if type in ['size_t', 'gpusize']:
        return 'uint64'
    elif type == 'enum':
        return 'uint8'
    else:
        return type

def convert_to_settings2_defaults(defaults: dict, type: str) -> dict:
    def fix(default):
        if isinstance(default, str):
            default_lowercase = default.lower()
            if default_lowercase == 'true':
                return True
            elif default_lowercase == 'false':
                return False
            else:
                return default
        else:
            return default

    defaults = {
        'Default': fix(defaults['Default']),
        'Type': convert_to_settings2_type(type)
    }

    default_win = defaults.get('WinDefault', None)
    if default_win:
        defaults['Windows'] = fix(default_win)

    default_linux = defaults.get('LnxDefault', None)
    if default_linux:
        defaults['Linux'] = fix(default_linux)

    return defaults

def validvalues_missing_name_fix(setting_name: str) -> str:
    if setting_name == 'SEMask':
        return 'ShaderEngineBitmask'
    elif setting_name == 'CmdBufferLoggerAnnotations':
        return 'CmdBufferLoggerAnnotations'
    elif setting_name in ['BasePreset', 'ElevatedPreset']:
        return 'PresetLogFlags'
    else:
        print(f'[WARN] ValidValues of {setting_name} has no "Name" field')

def convert_to_settings2_setting(
    setting: dict,
    group_name: str,
    filtered_tags) -> dict:

    setting_name = setting['Name']
    new_setting = CommentedMap({
        'Name': setting_name,
        'Description': setting['Description'],
    })

    if group_name:
        new_setting['Group'] = group_name

    tags = setting.get('Tags', None)
    if tags:
        new_setting['Tags'] = convert_to_settings2_tags(tags, filtered_tags)

    driver_states = setting.get('DriverState', None)
    if driver_states:
        new_setting['DriverStates'] = driver_states

    new_setting['Defaults'] = convert_to_settings2_defaults(setting['Defaults'], setting['Type'])

    is_bitmask = False
    flags = setting.get('Flags', None)
    if flags:
        new_flags = {}
        if flags.get('IsHex', False):
            new_flags['IsHex'] = True
        if flags.get('IsPath', False):
            new_flags['IsDir'] = True
        if flags.get('IsFile', False):
            new_flags['IsFile'] = True
        if flags.get('RereadSetting', False):
            new_flags['ReadAfterOverride'] = True
        if flags.get('IsBitmask'):
            is_bitmask = True

        if len(new_flags) != 0:
            new_setting['Flags'] = new_flags

    registry_scope = setting.get('Scope', 'PrivatePalKey')
    new_setting['Scope'] = registry_scope

    if setting['Type'] == 'enum' and not is_bitmask:
        new_setting['Enum'] = setting['ValidValues']['Name']
    elif is_bitmask:
        valid_values = setting.get('ValidValues', None)
        assert valid_values is not None, '[WARN] {} is a bitmask but has no ValidValues'.format(setting_name)

        name = valid_values.get('Name', None)
        if name is None:
            new_setting['Bitmask'] = validvalues_missing_name_fix(setting_name)
        else:
            new_setting['Bitmask'] = name

    return new_setting

def convert_to_settings2_setting_or_struct(setting: dict, filtered_tags: List[dict]) -> dict:
    converted_settings = []

    setting_struct = setting.get('Structure', None)
    if setting_struct:
        for subsetting in setting_struct:
            group_name = setting['Name']
            new_setting = convert_to_settings2_setting(subsetting, group_name, filtered_tags)
            new_setting['BuildFilter'] = extract_build_types(subsetting)

            converted_settings.append(new_setting)
    else:
        new_setting = convert_to_settings2_setting(setting, None, filtered_tags)
        new_setting['BuildFilter'] = extract_build_types(setting)

        converted_settings.append(new_setting)

    return converted_settings

def convert_to_settings2(old_settings: dict) -> dict:
    settings2 = dict()

    settings2['Version'] = 2
    settings2['ComponentName'] = old_settings['ComponentName']
    settings2['DriverStates'] = old_settings['DriverState']

    top_level_filtered_tags = extract_filtered_tags(old_settings['Tags'])

    enums = []
    settings2['Enums'] = enums

    bitmasks = []
    settings2['Bitmasks'] = bitmasks

    new_settings_list = CommentedSeq()
    settings2['Settings'] = new_settings_list

    for setting in old_settings['Settings']:
        flags = setting.get('Flags', None)
        is_bitmask = False
        if flags:
            if flags.get('IsBitmask', False):
                is_bitmask = True
            elif flags.get('IsHex', False):
                valid_values = setting.get('ValidValues')
                if valid_values:
                    # fix for bad json
                    is_bitmask = valid_values.get('IsEnum', False)

        # Populate Enums
        # It's not an enum if IsBitmask is set to True
        if setting['Type'] == 'enum' and not is_bitmask:
            valid_values = setting['ValidValues']
            enum = {
                'Name': valid_values['Name'],
                'Description': setting['Description']
            }
            variants = CommentedSeq()
            for val in valid_values['Values']:
                variant = CommentedMap({
                    'Name': val['Name'],
                    'Value': val['Value']
                })

                desc = val.get('Description', None)
                if desc is None:
                    name = val['Name']
                    valid_values_name = valid_values['Name']
                    print(f'[WARN] Enum field "{name}" of "{valid_values_name}" has no description')
                else:
                    variant['Description'] = desc

                variants.append(variant)

                filter_str = extract_build_types(val)
                if filter_str:
                    add_build_filter_comment(variants, len(variants) - 1, filter_str)

            enum['Variants'] = variants
            enums.append(enum)

        # Populate Bitmasks
        if is_bitmask:
            valid_values = setting['ValidValues']
            bitmask = {
                'Name': valid_values['Name'],
                'Description': setting['Description']
            }
            bits = CommentedSeq()
            for val in valid_values['Values']:
                bit = CommentedMap({
                    'Name': val['Name'],
                    'Value': val['Value'],
                    'Description': val['Description']
                })
                bits.append(bit)

                filter_str = extract_build_types(val)
                if filter_str:
                    add_build_filter_comment(bits, len(bits) - 1, filter_str)

            bitmask['Bits'] = bits
            bitmasks.append(bitmask)

        # Populate Settings
        converted_settings = convert_to_settings2_setting_or_struct(setting, top_level_filtered_tags)
        new_settings_list.extend(converted_settings)

    for i, setting in enumerate(new_settings_list):
        filter_str = setting.pop('BuildFilter', None)
        if filter_str:
            add_build_filter_comment(new_settings_list, i, filter_str)

    return settings2

def main() -> int:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='Convert a Settings JSON file to a YAML file that conforms to the Settings 2.0 schema.'
    )

    parser.add_argument('old_file',
                        type=str,
                        help='A path to an existing Settings JSON file of the old schema.')
    parser.add_argument('new_file',
                        type=str,
                        help='A file path to store the converted Settings YAML file.')

    args = parser.parse_args()

    settings_json = None
    with open(args.old_file) as file:
        settings_json = json.load(file)
        assert settings_json is not None, '`old_file` invalid: empty JSON input file'

    settings2 = convert_to_settings2(settings_json)
    with open(args.new_file, 'w') as file:
        yaml=YAML()
        yaml.default_flow_style = False
        yaml.indent(mapping=2, sequence=4, offset=2)
        yaml.dump(settings2, file)

    return 0

if __name__ == '__main__':
    sys.exit(main())
