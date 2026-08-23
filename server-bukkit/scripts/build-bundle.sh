#!/usr/bin/env bash
# Assembles the Bukkit runtime folder staged into the server binary.
# Usage: build-bundle.sh <work dir> <runtime dir> [shim build dir]
# Once Bukkit is selected, missing prerequisites must fail instead of shipping an incomplete runtime.
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
work="${1:?work dir}"
dest="${2:?runtime dir}"
shim="${3:-}"

sha1_of() { if command -v sha1sum >/dev/null; then sha1sum "$1"; else shasum -a 1 "$1"; fi; }

skip() {
    echo "Bukkit runtime: failed, $*" >&2
    exit 1
}

for tool in javac jar curl; do
    command -v "$tool" >/dev/null || skip "$tool is not installed"
done
mkdir -p "$work" "$dest/lib" "$dest/shim"

stale=1
if [ -s "$work/basalt-bukkit.jar" ] && [ -s "$work/bukkit-api.jar" ] \
    && [ -z "$(find "$root/server-bukkit/java" "$root/server-bukkit/resources" "$here" -newer "$work/basalt-bukkit.jar" \( -name '*.java' -o -name '*.sh' -o -name '*.txt' -o -path '*services*' \) -print -quit)" ]; then
    stale=0
fi

if [ "$stale" = 1 ]; then
    "$here/fetch-deps.sh" "$work" >"$work/fetch.log" 2>&1
    while IFS=: read -r _ artifact version; do
        [ -s "$work/lib/$artifact-$version.jar" ] || skip "could not download $artifact-$version (see $work/fetch.log)"
    done <"$here/deps.txt"
    if [ ! -s "$work/bukkit-api.jar" ]; then
        IFS=: read -r _ artifact version <"$here/api.txt"
        url="https://repo.papermc.io/repository/maven-public/io/papermc/paper/$artifact/$version/$artifact-$version.jar"
        curl -fsSL "$url" -o "$work/bukkit-api.part" || skip "could not download $url"
        [ "$(curl -fsSL "$url.sha1" | cut -c1-40)" = "$(sha1_of "$work/bukkit-api.part" | cut -c1-40)" ] \
            || skip "the SHA-1 of $url does not match"
        mv "$work/bukkit-api.part" "$work/bukkit-api.jar"
        touch "$work/bukkit-api.fetched"
    fi
    "$here/build-java.sh" "$work" >"$work/java.log" 2>&1 || skip "the Java half did not compile (see $work/java.log)"
fi

cp "$work/basalt-bukkit.jar" "$dest/"
cp "$work"/lib/*.jar "$dest/lib/"
if [ -e "$work/bukkit-api.fetched" ]; then
    IFS=: read -r _ artifact version <"$here/api.txt"
    cp "$work/bukkit-api.jar" "$dest/lib/$artifact-$version.jar"
    cat "$here/api.txt" "$here/deps.txt" >"$dest/libs.txt"
else
    cp "$work/bukkit-api.jar" "$dest/lib/"
    cp "$here/deps.txt" "$dest/libs.txt"
fi

lib=libjvm.so
[ "$(uname -s)" = Darwin ] && lib=libjvm.dylib
if [ -n "$shim" ] && [ -f "$shim/$lib" ] && [ -f "$shim/classpath.jar" ]; then
    for file in "$lib" classpath.jar; do
        cmp -s "$shim/$file" "$dest/shim/$file" || cp "$shim/$file" "$dest/shim/$file"
    done
else
    echo "Bukkit runtime: no Basalt Shim build found (run git submodule update --init, or set BASALT_SHIM_DIR); the server will say so if a Bukkit jar is used" >&2
fi
echo "Bukkit runtime ready in $dest"
