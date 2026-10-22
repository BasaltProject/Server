#!/usr/bin/env bash
# Downloads the jars the Bukkit API needs into <out>/lib (never into git), each checked against its published SHA-1.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/../../build/bukkit}"
sha1_of() { if command -v sha1sum >/dev/null; then sha1sum "$1"; else shasum -a 1 "$1"; fi; }
mkdir -p "$out/lib"
while read -r coord; do
    [ -z "$coord" ] && continue
    IFS=: read -r group artifact version <<<"$coord"
    jar="$out/lib/$artifact-$version.jar"
    [ -s "$jar" ] && continue
    base="https://repo1.maven.org/maven2"
    [ "$group" = "com.mojang" ] && base="https://libraries.minecraft.net"
    url="$base/${group//.//}/$artifact/$version/$artifact-$version.jar"
    echo "fetch $artifact-$version"
    if curl -fsSL "$url" -o "$jar.part" \
        && [ "$(curl -fsSL "$url.sha1" | cut -c1-40)" = "$(sha1_of "$jar.part" | cut -c1-40)" ]; then
        mv "$jar.part" "$jar"
    else
        echo "FAILED $url" >&2
        rm -f "$jar.part"
    fi
done <"$here/deps.txt"
