#!/bin/sh
# FreeBSD 14's base libc++ predates floating-point from_chars. Keep the release
# self-contained by linking a newer libc++ and libc++abi statically.
set -eu
prefix="$PWD/build/freebsd-libcxx"
version=20.1.8
if [ -f "$prefix/built-$version" ]; then
    exit 0
fi
source="$(mktemp -d)"
trap 'rm -rf "$source"' EXIT
git clone -q --depth 1 --filter=blob:none --sparse --branch "llvmorg-$version" \
    https://github.com/llvm/llvm-project "$source/llvm"
git -C "$source/llvm" sparse-checkout set runtimes libcxx libcxxabi libunwind cmake llvm/cmake llvm/utils third-party libc
cmake -S "$source/llvm/runtimes" -B "$source/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang19 -DCMAKE_CXX_COMPILER=clang++19 \
    "-DCMAKE_INSTALL_PREFIX=$prefix" '-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi' \
    -DLLVM_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
    -DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF \
    -DLIBCXX_LIBDIR_SUFFIX= -DLIBCXXABI_LIBDIR_SUFFIX= \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF
cmake --build "$source/build" --target install-cxx install-cxxabi --parallel 4
touch "$prefix/built-$version"
