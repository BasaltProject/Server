#!/usr/bin/env bash
set -euo pipefail

# Runs a command up to `tries` times, pausing longer after each failure. Package servers stall now and then.
retry() {
    local tries="$1" attempt=1
    shift
    until "$@"; do
        if [ "$attempt" -ge "$tries" ]; then
            echo "giving up on: $*" >&2
            return 1
        fi
        echo "attempt $attempt of $tries failed: $*" >&2
        sleep $((attempt * 15))
        attempt=$((attempt + 1))
    done
}

case "$(uname -s)" in
Linux)
    {
        echo 'Acquire::http::Timeout "20"; Acquire::https::Timeout "20"; Acquire::Retries "3";'
        echo 'Acquire::ForceIPv4 "true"; Dpkg::Use-Pty "0";'
    } | sudo tee /etc/apt/apt.conf.d/99basalt >/dev/null
    installed=0
    for attempt in 1 2 3 4 5; do
        # The Azure mirror is the one that stalls; from the second try on, go to Canonical's own servers.
        if [ "$attempt" -ge 2 ]; then
            sudo find /etc/apt -type f \( -name '*.list' -o -name '*.sources' \) \
                -exec sed -i 's#//azure\.archive\.ubuntu\.com/#//archive.ubuntu.com/#g' {} +
        fi
        sudo rm -rf /var/lib/apt/lists/partial
        if sudo timeout 180 apt-get update &&
            sudo timeout 420 apt-get install -y --no-install-recommends clang-19 clang-tools-19 lld-19 libclang-rt-19-dev; then
            installed=1
            break
        fi
        echo "apt attempt $attempt failed" >&2
        sudo apt-get clean
        sleep $((attempt * 10))
    done
    [ "$installed" = 1 ]
    echo "BASALT_CMAKE_ARGS=-DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19" >>"$GITHUB_ENV"
    ;;
Darwin)
    retry 4 brew install llvm
    llvm="$(brew --prefix llvm)"
    target=13.0
    echo "MACOSX_DEPLOYMENT_TARGET=$target" >>"$GITHUB_ENV"
    arch="${BASALT_TARGET_ARCH:-$(uname -m)}"
    cross=""
    if [ "$arch" = x86_64 ] && [ "$(uname -m)" = arm64 ]; then
        sudo softwareupdate --install-rosetta --agree-to-license || true
        cross="-DCMAKE_OSX_ARCHITECTURES=x86_64"
        echo "BASALT_SHIM_ARCH=x86_64" >>"$GITHUB_ENV"
    fi
    version="$(brew list --versions llvm | awk '{ print $2 }')"
    prefix="$HOME/libcxx-$arch"
    if [ ! -f "$prefix/built-$version-$target" ]; then
        rm -rf "$prefix" "$HOME/src-llvm"
        mkdir -p "$prefix"
        clone_llvm() {
            rm -rf "$HOME/src-llvm"
            git clone -q --depth 1 --filter=blob:none --sparse --branch "llvmorg-$version" https://github.com/llvm/llvm-project "$HOME/src-llvm"
        }
        retry 4 clone_llvm
        retry 4 git -C "$HOME/src-llvm" sparse-checkout set runtimes libcxx libcxxabi libunwind cmake llvm/cmake llvm/utils third-party libc
        cmake -S "$HOME/src-llvm/runtimes" -B "$HOME/src-llvm/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER="$llvm/bin/clang" -DCMAKE_CXX_COMPILER="$llvm/bin/clang++" \
            -DCMAKE_OSX_ARCHITECTURES="$arch" -DCMAKE_OSX_DEPLOYMENT_TARGET="$target" -DCMAKE_INSTALL_PREFIX="$prefix" \
            -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" -DLLVM_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_TESTS=OFF \
            -DLIBCXX_INCLUDE_BENCHMARKS=OFF -DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
            -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF
        cmake --build "$HOME/src-llvm/build" --target install-cxx install-cxxabi
        touch "$prefix/built-$version-$target"
    fi
    cxx="$(find "$prefix" -name libc++.a | head -1)"
    lipo -archs "$cxx" | grep -q "$arch"
    echo "BASALT_CMAKE_ARGS=-DCMAKE_C_COMPILER=$llvm/bin/clang -DCMAKE_CXX_COMPILER=$llvm/bin/clang++ -DCMAKE_OSX_DEPLOYMENT_TARGET=$target \"-DCMAKE_CXX_STANDARD_LIBRARIES=-lc++ $cxx\" -DCMAKE_CXX_FLAGS=-D_LIBCPP_DISABLE_AVAILABILITY $cross" >>"$GITHUB_ENV"
    ;;
MINGW* | MSYS* | CYGWIN*)
    args=""
    if [ "${PROCESSOR_ARCHITECTURE:-}" = ARM64 ] || [ "${PROCESSOR_ARCHITEW6432:-}" = ARM64 ]; then
        # An emulated x64 Clang targets x64 unless told otherwise.
        if ! command -v clang >/dev/null; then
            retry 4 choco install llvm --no-progress -y
            echo "C:\\Program Files\\LLVM\\bin" >>"$GITHUB_PATH"
            export PATH="/c/Program Files/LLVM/bin:$PATH"
        fi
        if ! command -v ninja >/dev/null; then
            retry 4 choco install ninja --no-progress -y
        fi
        args="-DCMAKE_C_COMPILER_TARGET=aarch64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=aarch64-pc-windows-msvc"
    fi
    echo "BASALT_CMAKE_ARGS=$args" >>"$GITHUB_ENV"
    ;;
*)
    echo "BASALT_CMAKE_ARGS=" >>"$GITHUB_ENV"
    ;;
esac
