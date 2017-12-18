#find_depth
if(NOT DEFINED GLOBAL_ROOT_SRC_DIR)
    execute_process(
        COMMAND find_depth
        OUTPUT_VARIABLE GLOBAL_ROOT_SRC_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(GLOBAL_ROOT_SRC_DIR ${CMAKE_SOURCE_DIR}/${GLOBAL_ROOT_SRC_DIR} CACHE PATH "Global root source directory..")
endif()
