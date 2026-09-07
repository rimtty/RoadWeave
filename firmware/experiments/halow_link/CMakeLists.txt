cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(roadweave_halow_link)
target_compile_definitions(libmorse PRIVATE MMLOG_LEVEL_DEFAULT=5)
