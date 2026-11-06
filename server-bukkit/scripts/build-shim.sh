#!/usr/bin/env bash
# Builds Basalt Shim into <out>: libjvm and classpath.jar. A selected Bukkit build requires both.
# Usage: build-shim.sh <shim source dir> <out dir> <wire zlib include dir>
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "${2:?out dir}"
src="$(cd "${1:?shim source dir}" && pwd)"
out="$(cd "$2" && pwd)"
zlib="$(cd "${3:?zlib include dir}" && pwd)"

skip() {
    echo "Shim: failed, $*" >&2
    exit 1
}

[ -f "$src/CMakeLists.txt" ] || skip "shim/ is empty (run: git submodule update --init)"
for tool in cmake clang clang++ javac jar; do
    command -v "$tool" >/dev/null || skip "$tool is not installed"
done
ext="$(uname -s | tr A-Z a-z)"
[ "$ext" = darwin ] && lib=libjvm.dylib || lib=libjvm.so
if [ "$ext" = darwin ] && [ "${BASALT_SHIM_ARCH:-}" = x86_64 ] && [ "$(uname -m)" = arm64 ]; then
    exec arch -x86_64 "$0" "$@"
fi
process=compile
# Apple Silicon forbids the legacy JIT's writable/executable memory mapping.
# Use Shim's interpreter rather than weakening the host's memory protections.
if [ "$ext" = darwin ] && [ "$(uname -m)" = arm64 ]; then
    process=interpret
fi
build="$out/shim-build"
mkdir -p "$build"
# The profiler walks native stacks through the frame pointer, and the Java machine is part of them.
flags=(-DCMAKE_BUILD_TYPE= -DBUILD_TESTING=OFF "-DSHIM_PROCESS=$process")
[ "$ext" != darwin ] && flags+=(-DSHIM_FRAME_POINTER=ON)
command -v ninja >/dev/null && flags+=(-G Ninja)
if [ "$ext" != darwin ] && [ ! -f /usr/include/zlib.h ]; then
    mkdir -p "$out/zlib-stub"
    for candidate in /usr/lib/x86_64-linux-gnu/libz.so.1 /usr/lib/aarch64-linux-gnu/libz.so.1 /usr/lib64/libz.so.1 /lib/x86_64-linux-gnu/libz.so.1; do
        [ -e "$candidate" ] && ln -sf "$candidate" "$out/zlib-stub/libz.so" && break
    done
    flags+=("-DZLIB_INCLUDE_DIR=$zlib" "-DZLIB_LIBRARY=$out/zlib-stub/libz.so")
fi
if ! { CC=clang CXX=clang++ cmake -S "$src" -B "$build" "${flags[@]}" \
    && cmake --build "$build" --parallel --target shim_jvm shim_classpath; } >"$out/shim-build.log" 2>&1; then
    tail -n 60 "$out/shim-build.log" >&2
    skip "the Shim build failed (see $out/shim-build.log)"
fi
[ -f "$build/$lib" ] && [ -f "$build/classpath.jar" ] || skip "the Shim build left no $lib or classpath.jar (see $out/shim-build.log)"
cp "$build/$lib" "$build/classpath.jar" "$out/"
echo "Shim built in $out"
