cmake_minimum_required(VERSION 3.10)
project(zkCNN)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-mcmodel=large -O3 -pthread")

# ========== cnpy ==========
set(CNPY_ROOT $ENV{HOME}/local/cnpy)

find_path(CNPY_INCLUDE_DIR cnpy.h PATHS ${CNPY_ROOT}/include)
find_library(CNPY_LIBRARY cnpy PATHS ${CNPY_ROOT}/lib)

if (NOT CNPY_INCLUDE_DIR OR NOT CNPY_LIBRARY)
    message(FATAL_ERROR "cnpy not found")
endif()

# 导出给子目录
set(CNPY_INCLUDE_DIR ${CNPY_INCLUDE_DIR} CACHE INTERNAL "")
set(CNPY_LIBRARY     ${CNPY_LIBRARY}     CACHE INTERNAL "")

# ========== 子目录 ==========
add_subdirectory(3rd/mcl)
add_subdirectory(src)
