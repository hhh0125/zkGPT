#!/bin/bash

set -ex

# Check if debug mode is enabled
DEBUG_MODE=${1:-false}

if [ "$DEBUG_MODE" = "debug" ] || [ "$DEBUG_MODE" = "gdb" ]; then
    echo "Building in Debug mode for gdb debugging..."
    ./build.sh Debug
    BUILD_DIR="cmake-build-debug"
else
    echo "Building in Release mode..."
    ./build.sh Release
    BUILD_DIR="cmake-build-release"
fi

# 将编译过程的中间结果放入本目录下的临时目录
TMP_BUILD_DIR="./${BUILD_DIR}"
TMP_SUBDIR="${TMP_BUILD_DIR}/tmp"
mkdir -p "${TMP_SUBDIR}"
export TMPDIR="$(cd "${TMP_SUBDIR}" && pwd)"
export TMP="${TMPDIR}"
export TEMP="${TMPDIR}"

/usr/bin/cmake --build ./${BUILD_DIR} --target demo_llm_run -- -j 6

# 删除编译过程的中间文件
BUILD_STATUS=$?

if [ ${BUILD_STATUS} -eq 0 ]; then
    rm -rf "${TMPDIR}"
fi

if [ ${BUILD_STATUS} -ne 0 ]; then
    exit ${BUILD_STATUS}
fi


run_file=./${BUILD_DIR}/src/demo_llm_run

if [ -z "${ZKGPT_PARAMETER_DIR:-}" ] && [ -d "src/data/gpt2_int" ]; then
    export ZKGPT_PARAMETER_DIR="src/data/gpt2_int"
fi

if [ "$DEBUG_MODE" = "debug" ] || [ "$DEBUG_MODE" = "gdb" ]; then
    echo "Running with gdb debugger..."
    gdb --args ${run_file}
else
    echo "Running normally..."
    ${run_file}
fi 
