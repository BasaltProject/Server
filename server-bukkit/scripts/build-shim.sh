#!/usr/bin/env bash
# Builds Basalt Shim (the shim/ submodule) out of tree into <out>: libjvm.so and classpath.jar. Never fails the build.
# Usage: build-shim.sh <shim source dir> <out dir> <wire zlib include dir>
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "${2:?out dir}"
src="$(cd "${1:?shim source dir}" && pwd)"
out="$(cd "$2" && pwd)"
zlib="$(cd "${3:?zlib include dir}" && pwd)"

skip() {
    echo "Shim: skipped, $*" >&2
    exit 0
}

[ -f "$src/GNUmakefile" ] || skip "shim/ is empty (run: git submodule update --init)"
for tool in make clang clang++ javac jar; do
    command -v "$tool" >/dev/null || skip "$tool is not installed"
done
ext="$(uname -s | tr A-Z a-z)"
[ "$ext" = darwin ] && lib=libjvm.dylib || lib=libjvm.so
if [ -f "$out/$lib" ] && [ -f "$out/classpath.jar" ] \
    && [ -z "$(find "$src" -newer "$out/$lib" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.java' -o -name '*.mk' \) -print -quit)" ]; then
    exit 0
fi
mkdir -p "$out"
flags=()
if [ ! -f /usr/include/zlib.h ]; then
    mkdir -p "$out/zlib-stub"
    for candidate in /usr/lib/x86_64-linux-gnu/libz.so.1 /usr/lib/aarch64-linux-gnu/libz.so.1 /usr/lib64/libz.so.1 /lib/x86_64-linux-gnu/libz.so.1; do
        [ -e "$candidate" ] && ln -sf "$candidate" "$out/zlib-stub/libz.so" && break
    done
    flags+=("extra-cflags=-I$zlib" "extra-lflags=-L$out/zlib-stub")
fi
# Its rules only work inside the source tree, whose build/ folder is ignored by that repo.
run_make() { make -C "$src" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" use-clang=true "${flags[@]}" "$@"; }
{ run_make build; } >"$out/shim-make.log" 2>&1 || skip "the Shim build failed (see $out/shim-make.log)"
built="$(dirname "$(ls -t "$src"/build/*/"$lib" 2>/dev/null | head -1)")"
[ -f "$built/$lib" ] && [ -f "$built/classpath.jar" ] || skip "the Shim build left no $lib or classpath.jar (see $out/shim-make.log)"
cp "$built/$lib" "$built/classpath.jar" "$out/"
echo "Shim built in $out"
