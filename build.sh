#!/bin/bash

# Default to debug mode if no argument provided
BUILD_TYPE=${1:-Debug}
BUILD_DIR="cmake-build-${BUILD_TYPE,,}"  # Convert to lowercase

TMP_SUBDIR="${BUILD_DIR}/tmp"
mkdir -p "${TMP_SUBDIR}"
export TMPDIR="$(cd "${TMP_SUBDIR}" && pwd)"
export TMP="${TMPDIR}"
export TEMP="${TMPDIR}"

# 创建构建目录并进入
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}


cmake \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -G "CodeBlocks - Unix Makefiles" \
  .. 

# 执行编译
make

BUILD_STATUS=$?

if [ ${BUILD_STATUS} -eq 0 ]; then
  rm -rf "${TMPDIR}"
fi

exit ${BUILD_STATUS}

# 回到项目根目录
cd ..

if [ ! -d "./data" ]
then
   tar -xzvf data.tar.gz
fi
cd script