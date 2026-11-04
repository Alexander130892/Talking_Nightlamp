cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/minimp3"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/storage"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/audio"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/input")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(15_audio)
