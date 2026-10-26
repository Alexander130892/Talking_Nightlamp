cmake_minimum_required(VERSION 3.16)

# Reuse the real, host-tested logic component (tag_format) from the firmware.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/logic")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(05_read)
