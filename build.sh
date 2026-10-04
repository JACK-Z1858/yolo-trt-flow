#!/usr/bin/env bash

set -euo pipefail

build_dir="${BUILD_DIR:-build}"
build_type="${BUILD_TYPE:-Release}"

cmake_args=(
    -S .
    -B "$build_dir"
    "-DCMAKE_BUILD_TYPE=$build_type"
)

if [[ -n "${TensorRT_ROOT:-}" ]]; then
    cmake_args+=("-DTensorRT_ROOT=$TensorRT_ROOT")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config "$build_type" -j
