cmake_minimum_required(VERSION 3.16)

# Use the firmware's vendored minimp3 component (ADR 0013).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/minimp3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(08b_mp3)
