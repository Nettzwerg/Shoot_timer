cmake_minimum_required(VERSION 3.16.0)
# APPEND statt set: PlatformIO trägt hier bereits seinen src-Ordner ein
list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(shot_timer)
