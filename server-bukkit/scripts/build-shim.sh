#!/usr/bin/env bash
# Builds Basalt Shim into <out>: libjvm and classpath.jar. A selected Bukkit build requires both.
# Usage: build-shim.sh <shim source dir> <out dir> <wire zlib include dir> [cross args file]
# A cross args file holds one -D option per line and makes Shim build for that target; the tools it runs
# while building are built for this machine first. On Windows, BASALT_SHIM_MINGW_BIN names the MinGW-w64
# Clang folder, because Shim does not build with the MSVC ABI.
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "${2:?out dir}"
src="$(cd "${1:?shim source dir}" && pwd)"
out="$(cd "$2" && pwd)"
zlib="$(cd "${3:?zlib include dir}" && pwd)"
cross="${4:-}"

skip() {
    echo "Shim: failed, $*" >&2
    exit 1
}

[ -f "$src/CMakeLists.txt" ] || skip "shim/ is empty (run: git submodule update --init)"
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    [ -n "${BASALT_SHIM_MINGW_BIN:-}" ] || skip "BASALT_SHIM_MINGW_BIN must name the MinGW-w64 Clang bin folder"
    export PATH="$(cygpath -u "$BASALT_SHIM_MINGW_BIN"):$PATH"
    windows=1
    ;;
*) windows=0 ;;
esac
for tool in cmake "${CC:-clang}" "${CXX:-clang++}" javac jar; do
    command -v "$tool" >/dev/null || skip "$tool is not installed"
done
ext="$(uname -s | tr A-Z a-z)"
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
generator=()
command -v ninja >/dev/null && generator=(-G Ninja)
flags=(-DCMAKE_BUILD_TYPE= -DBUILD_TESTING=OFF "-DSHIM_PROCESS=$process")
[ "$ext" != darwin ] && [ "$windows" = 0 ] && flags+=(-DSHIM_FRAME_POINTER=ON)
if [ "$windows" = 1 ]; then
    flags+=("-DSHIM_JAVA_HOME=$(cygpath -m "${JAVA_HOME:?JAVA_HOME}")")
fi
if [ "$windows" = 0 ] && [ -z "${JAVA_HOME:-}" ]; then
    # FreeBSD installs javac as a wrapper outside the JDK folder, so Shim cannot find the JDK from it
    for jdk in /usr/local/openjdk*; do
        [ -f "$jdk/include/jni.h" ] && flags+=("-DSHIM_JAVA_HOME=$jdk") && break
    done
fi
if [ -n "$cross" ]; then
    host="$out/shim-host-build"
    mkdir -p "$host"
    if ! { CC="${CC:-clang}" CXX="${CXX:-clang++}" cmake -S "$src" -B "$host" -DCMAKE_BUILD_TYPE= ${generator[@]+"${generator[@]}"} \
        && cmake --build "$host" --parallel --target shim_binary_to_object shim_type_generator; } >"$out/shim-host-build.log" 2>&1; then
        tail -n 60 "$out/shim-host-build.log" >&2
        skip "the build tools for this machine failed (see $out/shim-host-build.log)"
    fi
    flags+=("-DSHIM_HOST_TOOLS_DIR=$host")
    while IFS= read -r line; do
        [ -n "$line" ] && flags+=("$line")
    done <"$cross"
fi
flags+=(${generator[@]+"${generator[@]}"})
if [ "$ext" != darwin ] && [ "$windows" = 0 ] && [ -z "$cross" ] && [ ! -f /usr/include/zlib.h ]; then
    mkdir -p "$out/zlib-stub"
    for candidate in /usr/lib/x86_64-linux-gnu/libz.so.1 /usr/lib/aarch64-linux-gnu/libz.so.1 /usr/lib64/libz.so.1 /lib/x86_64-linux-gnu/libz.so.1; do
        [ -e "$candidate" ] && ln -sf "$candidate" "$out/zlib-stub/libz.so" && break
    done
    flags+=("-DZLIB_INCLUDE_DIR=$zlib" "-DZLIB_LIBRARY=$out/zlib-stub/libz.so")
fi
if ! { CC="${CC:-clang}" CXX="${CXX:-clang++}" cmake -S "$src" -B "$build" "${flags[@]}" \
    && cmake --build "$build" --parallel --target shim_jvm shim_classpath; } >"$out/shim-build.log" 2>&1; then
    tail -n 60 "$out/shim-build.log" >&2
    skip "the Shim build failed (see $out/shim-build.log)"
fi
lib=""
for candidate in libjvm.dylib libjvm.so jvm.dll; do
    [ -f "$build/$candidate" ] && lib="$candidate" && break
done
[ -n "$lib" ] && [ -f "$build/classpath.jar" ] || skip "the Shim build left no libjvm or classpath.jar (see $out/shim-build.log)"
cp "$build/$lib" "$build/classpath.jar" "$out/"
echo "Shim built in $out"
