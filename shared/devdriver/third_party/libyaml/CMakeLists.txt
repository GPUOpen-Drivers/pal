project(DD_LIBYAML VERSION 0.2.5 LANGUAGES C)

add_library(dd_libyaml
    STATIC
        src/api.c
        src/dumper.c
        src/emitter.c
        src/loader.c
        src/parser.c
        src/reader.c
        src/scanner.c
        src/writer.c
)

target_include_directories(dd_libyaml
    PUBLIC
        include
)

target_compile_definitions(dd_libyaml
    PRIVATE
      HAVE_CONFIG_H # Generate version info
    PUBLIC
      YAML_DECLARE_STATIC # Don't export/import functions in the header
)
