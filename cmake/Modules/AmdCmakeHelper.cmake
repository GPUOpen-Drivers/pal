# Set up global CMake properties ###################################################################
cmake_minimum_required(VERSION 3.5)

# Include Frequently Used Modules ##################################################################
include(CMakeDependentOption)

# Build Type Helper ################################################################################
set(CMAKE_BUILD_TYPE_DEBUG $<CONFIG:Debug>)
set(CMAKE_BUILD_TYPE_RELEASE $<CONFIG:Release>)
set(CMAKE_BUILD_TYPE_RELWITHDEBINFO $<CONFIG:RelWithDebInfo>)

# Options Helpers ##################################################################################
macro(dropdown_option _option _options)
    set_property(CACHE ${_option} PROPERTY STRINGS ${${_options}})

    list(FIND ${_options} ${${_option}} ${_option}_INDEX)
    if(${${_option}_INDEX} EQUAL -1)
        message(FATAL_ERROR "Option ${${_option}} not supported, valid entries are ${${_options}}")
    endif()
endmacro()

macro(mark_grouped_as_advanced _group)
    get_cmake_property(_groupVariables CACHE_VARIABLES)
    foreach(_groupVariable ${_groupVariables})
        if(_groupVariable MATCHES "^${_group}_.*")
            mark_as_advanced(FORCE ${_groupVariable})
        endif()
    endforeach()
endmacro()

# System Architecture Helpers ######################################################################
include(TestBigEndian)

function(get_system_architecture_endianess endianess)
    test_big_endian(architectureIsBigEndian)
    if (architectureIsBigEndian)
        set(${endianess} "BIG" PARENT_SCOPE)
    else()
        set(${endianess} "LITTLE" PARENT_SCOPE)
    endif()
endfunction()

function(get_system_architecture_bits bits)
    math(EXPR ${bits} "8*${CMAKE_SIZEOF_VOID_P}")
    set(${bits} ${${bits}} PARENT_SCOPE)
endfunction()

# Architecture Endianness ##########################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_ENDIANESS)
    get_system_architecture_endianess(TARGET_ARCHITECTURE_ENDIANESS)
    set(TARGET_ARCHITECTURE_ENDIANESS ${TARGET_ARCHITECTURE_ENDIANESS} CACHE STRING "Specify the target architecture endianess.")
    set(TARGET_ARCHITECTURE_ENDIANESS_OPTIONS "BIG" "LITTLE")
    dropdown_option(TARGET_ARCHITECTURE_ENDIANESS TARGET_ARCHITECTURE_ENDIANESS_OPTIONS)
endif()

# Architecture Bits ################################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
    get_system_architecture_bits(TARGET_ARCHITECTURE_BITS)
    set(TARGET_ARCHITECTURE_BITS ${TARGET_ARCHITECTURE_BITS} CACHE STRING "Specify the target architecture bits.")
    set(TARGET_ARCHITECTURE_BITS_OPTIONS "32" "64")
    dropdown_option(TARGET_ARCHITECTURE_BITS TARGET_ARCHITECTURE_BITS_OPTIONS)
endif()

# Visual Studio Filter Helper ######################################################################
macro(target_vs_filters _target)
    if(MSVC)
        get_target_property(${_target}_SOURCES ${_target} SOURCES)
        get_target_property(${_target}_INCLUDES_DIRS ${_target} INTERFACE_INCLUDE_DIRECTORIES)

        if(${_target}_INCLUDES_DIRS)
            foreach(_include_dir IN ITEMS ${${_target}_INCLUDES_DIRS})
                file(GLOB _include_files
                    LIST_DIRECTORIES false
                    "${_include_dir}/*.h*"
                )

                list(APPEND ${_target}_INCLUDES ${_include_files})
            endforeach()

            target_sources(${_target} PRIVATE ${${_target}_INCLUDES})
        endif()

        set(${_target}_FILES ${${_target}_SOURCES} ${${_target}_INCLUDES})

        foreach(_source IN ITEMS ${${_target}_FILES})
            set(_source ${_source})
            get_filename_component(_source_path "${_source}" ABSOLUTE)
            file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
            get_filename_component(_source_path_rel "${_source_path_rel}" DIRECTORY)
            string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
            source_group("${_group_path}" FILES "${_source}")
        endforeach()
    endif()
endmacro()

# Install Helper ###################################################################################
macro(target_install _target _destination)
    install(TARGET ${_target} DESTINATION ${_destination}/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})
endmacro()

# Visual Studio Specific Options ###################################################################
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

