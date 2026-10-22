#!/usr/bin/env bash
# JOBS=8 bash run_msm/build_bn254_msm_backend.sh
# ./run_msm/bin/msm_v7_split   --scalars ./vi_0729.npy   --out ./run_msm/out_v7_split.npy   --stats ./run_msm/out_v7_split.stats.json   --K 16384   --n 16384    --c 8   --scalar-bits 76   --scalar-format bn254-balanced   --points-mode random   --point-seed test1   --threads 12   --mode tile-mask-fast64   --tile-size 64   --fast-min-gain 1   --fast-min-group-count 2    --fast-multiscale 1   --split-base-tile-size 32
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$script_dir/.." && pwd)"

mcl_candidates=()
if [[ -n "${MCL_DIR:-}" ]]; then
  mcl_candidates+=("$MCL_DIR")
fi
mcl_candidates+=(
  "$root/3rd/mcl"
  "$root/zkCNN/3rd/hyrax-bls12-381/3rd/mcl"
)
mcl_dir=""
for candidate in "${mcl_candidates[@]}"; do
  if [[ -f "$candidate/CMakeLists.txt" && -d "$candidate/include/mcl" ]]; then
    mcl_dir="$candidate"
    break
  fi
done
if [[ -z "$mcl_dir" ]]; then
  echo "[build] error: could not find mcl source tree" >&2
  exit 1
fi

msm_src="$script_dir/msm_v7_split.cpp"
msm_out_name="msm_v7_split"
# if [[ ! -f "$msm_src" && -f "$script_dir/msm_v7_split.cpp" ]]; then
#   msm_src="$script_dir/msm_v7_split.cpp"
#   msm_out_name="msm_v7_split"
# fi
g1_src="$script_dir/bn254_g1_tools.cpp"
out_dir="$script_dir/bin"
mcl_build_dir="${MCL_BUILD_DIR:-$script_dir/build/mcl}"
mcl_lib="$mcl_build_dir/lib/libmcl.a"
msm_out="$out_dir/$msm_out_name"
g1_out="$out_dir/bn254_g1_tools"

mkdir -p "$out_dir" "$mcl_build_dir"

jobs="${JOBS:-}"
if [[ -z "$jobs" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    jobs="$(nproc)"
  else
    jobs="4"
  fi
fi

cxx="${CXX:-g++}"
cmake="${CMAKE:-cmake}"
if ! command -v "$cmake" >/dev/null 2>&1; then
  echo "[build] error: cmake command not found: $cmake" >&2
  exit 1
fi

echo "[build] configuring mcl static library"
"$cmake" -S "$mcl_dir" -B "$mcl_build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="$cxx" \
  -DMCL_MAX_BIT_SIZE:STRING=384 \
  -DMCL_BUILD_SAMPLE=OFF \
  -DMCL_BUILD_TESTING=OFF \
  -DMCL_TEST_WITH_GMP=OFF

echo "[build] building mcl static library"
"$cmake" --build "$mcl_build_dir" --target mcl_st --parallel "$jobs"
if [[ ! -f "$mcl_lib" ]]; then
  echo "[build] error: mcl build did not produce $mcl_lib" >&2
  exit 1
fi

if [[ ! -f "$msm_src" ]]; then
  echo "[build] error: could not find BN254 MSM source file" >&2
  exit 1
fi

common_flags=(
  -std=c++17
  -O3
  -march=native
  -mtune=native
  -DNDEBUG
  -pthread
  -DMCL_DONT_USE_OPENSSL
  -DMCL_USE_VINT
  -DMCL_SIZEOF_UNIT=8
  -DMCL_VINT_FIXED_BUFFER
  -DMCL_MAX_BIT_SIZE=384
  -I"$mcl_dir/include"
)
common_libs=(
  "$mcl_lib"
)

echo "[build] compiling BN254 Pippenger MSM backend with $cxx"
"$cxx" "${common_flags[@]}" "$msm_src" "${common_libs[@]}" -o "$msm_out"

if [[ -f "$g1_src" ]]; then
  echo "[build] compiling BN254 G1 tools with $cxx"
  "$cxx" "${common_flags[@]}" "$g1_src" "${common_libs[@]}" -o "$g1_out"
  echo "[build] g1 tools ready: $g1_out"
else
  echo "[build] skipping BN254 G1 tools: $g1_src not found"
fi

echo "[build] backend ready: $msm_out"
