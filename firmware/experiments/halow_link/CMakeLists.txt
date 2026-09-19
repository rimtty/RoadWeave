cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(roadweave_halow_link)
if(CONFIG_RW_LINK_THROUGHPUT)
    # Per-packet SDK INFO logging changes the load being measured.
    target_compile_definitions(libmorse PRIVATE MMLOG_LEVEL_DEFAULT=4)
else()
    target_compile_definitions(libmorse PRIVATE MMLOG_LEVEL_DEFAULT=5)
endif()
