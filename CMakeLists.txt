# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ./components/bsp_extra
    )

add_compile_options(-Wno-format)
project(lvgl_demo_v8)

# Managed components are downloaded and ignored by Git. Apply the local
# stability fixes after component resolution so fresh clones get them too.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/patch_managed_components.cmake)
