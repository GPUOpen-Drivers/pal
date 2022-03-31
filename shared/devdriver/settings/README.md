# Settings

## APIs

At the highest level, Settings module is a set of APIs (defined over RPC
protocol) describing various operations regarding Settings. Any pair of client
and server implementing this the APIs can communicate with each other to perform
Settings related functions.

## C++ and Code-gen

For users who want to implement Settings servers in C++, a base class and a
code-gen script are provided to help facilitate the implementation.

## Setup and Run Code-gen Script

Note: This is only required if you are modifying the settings json, generated files, or making your own.

First, install the following dependencies:

    pip install jsonschema
    pip install jinja2

The test mock files were generated using:

     python3 .\settings\utils\settings_codegen.py -i .\tests\settings\settings_mockcore.json -g settings_mock_core -s mockCoreSettings.h -o .\tests\settings\

Use the help menu to see the latest arguments.

### Create a specific Settings class

You can create a child class deriving from `SettingsBase` and implement
necessary virtual functions.

### Define settings in JSON

You also need to define your individual settings in a JSON file that conforms to the
[Settings schema](./utils/settings_schema.json).

Run [settings_codegen.py](./utils/settings_codegen.py) to generate implmementations
for your Settings child class.
