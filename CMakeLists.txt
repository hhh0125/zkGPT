cmake_minimum_required(VERSION 3.10)
project(zkCNN)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# Benchmarks are meaningless at -O0; default to an optimized build.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
option(ZKGPT_NATIVE "Tune for the build machine (-march=native)" OFF)
if (ZKGPT_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)
if (ENABLE_ASAN AND ENABLE_UBSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_UBSAN must use separate build trees")
endif()
if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()
if (ENABLE_UBSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
endif()

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
