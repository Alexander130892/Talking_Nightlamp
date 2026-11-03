cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/logic"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/lamp_strip"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/lamp")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(13_lamp)
