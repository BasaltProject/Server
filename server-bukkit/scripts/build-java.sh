#!/usr/bin/env bash
# Compiles server-bukkit/java (and the generated base classes) into <out>/basalt-bukkit.jar.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${1:-$root/build/bukkit}"
if [ ! -s "$out/bukkit-api.jar" ]; then
    echo "bukkit-api.jar is missing in $out: fetch the Paper API first" >&2
    exit 1
fi
rm -rf "$out/runtime-classes" "$out/gen" && mkdir -p "$out/runtime-classes" "$out/gen"
cp="$out/bukkit-api.jar:$(ls "$out"/lib/*.jar | tr '\n' ':')"
if [ -f "$here/GenerateBases.java" ]; then
    javac --release 25 -d "$out/gen-tool" "$here/GenerateBases.java" 2>&1 || exit 1
    java -cp "$out/gen-tool:$cp" GenerateBases "$here/bases.txt" "$out/gen"
fi
if [ -f "$here/GenerateBlockData.java" ]; then
    javac --release 25 -nowarn -cp "$cp" -d "$out/gen-tool" "$here/GenerateBlockData.java" 2>&1 || exit 1
    java -cp "$out/gen-tool:$cp" GenerateBlockData "$out/gen"
fi
if [ -f "$here/GenerateRegistryTypes.java" ]; then
    javac --release 25 -nowarn -cp "$cp" -d "$out/gen-tool" "$here/GenerateRegistryTypes.java" 2>&1 || exit 1
    java -cp "$out/gen-tool:$cp" GenerateRegistryTypes "$out/gen"
fi
find "$root/server-bukkit/java" "$out/gen" -name '*.java' | sed 's/.*/"&"/' >"$out/runtime-sources.txt"
javac --release 25 -nowarn -proc:none -encoding UTF-8 -cp "$cp" -d "$out/runtime-classes" @"$out/runtime-sources.txt"
jar --create --file "$out/basalt-bukkit.jar" -C "$out/runtime-classes" . -C "$root/server-bukkit/resources" .
mkdir -p "$out/runtime-libs" && cp "$out/bukkit-api.jar" "$out"/lib/*.jar "$out/runtime-libs/"
echo "built $out/basalt-bukkit.jar"
