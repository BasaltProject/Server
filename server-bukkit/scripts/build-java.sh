#!/usr/bin/env bash
# Compiles server-bukkit/java (and the generated base classes) into <out>/basalt-bukkit.jar.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${1:-$root/build/bukkit}"
# Native Windows tools want Windows paths and ';' between classpath entries; the rest of the world takes ':'
sep=:
native() { printf '%s' "$1"; }
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    sep=';'
    export MSYS2_ARG_CONV_EXCL='*'
    native() { cygpath -m "$1"; }
    ;;
esac
out="$(native "$out")"
here="$(native "$here")"
root="$(native "$root")"
if [ ! -s "$out/bukkit-api.jar" ]; then
    echo "bukkit-api.jar is missing in $out: fetch the Paper API first" >&2
    exit 1
fi
rm -rf "$out/runtime-classes" "$out/gen" && mkdir -p "$out/runtime-classes" "$out/gen"
cp="$out/bukkit-api.jar"
for jar in "$out"/lib/*.jar; do cp="$cp$sep$jar"; done
if [ -f "$here/GenerateBases.java" ]; then
    javac --release 25 -nowarn -cp "$cp" -d "$out/gen-tool" "$here/GenerateBases.java" "$here/GenerateEntities.java" 2>&1 || exit 1
    java -cp "$out/gen-tool$sep$cp" GenerateBases "$here/bases.txt" "$out/gen" "$root/server-bukkit/java/org/basaltmc/bukkit/entity/ops"
    java -cp "$out/gen-tool$sep$cp" GenerateEntities "$root/server-bukkit/java/org/basaltmc/bukkit/entity/ops" "$out/gen"
fi
if [ -f "$here/GenerateBlockData.java" ]; then
    javac --release 25 -nowarn -cp "$cp" -d "$out/gen-tool" "$here/GenerateBlockData.java" 2>&1 || exit 1
    java -cp "$out/gen-tool$sep$cp" GenerateBlockData "$out/gen"
fi
if [ -f "$here/GenerateRegistryTypes.java" ]; then
    javac --release 25 -nowarn -cp "$cp" -d "$out/gen-tool" "$here/GenerateRegistryTypes.java" 2>&1 || exit 1
    java -cp "$out/gen-tool$sep$cp" GenerateRegistryTypes "$out/gen"
fi
if [ -f "$here/NmsManifest.java" ]; then
    nms_x="--add-exports jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED --add-exports jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED --add-exports jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED"
    javac $nms_x -nowarn -d "$out/gen-tool" "$here/NmsManifest.java" 2>&1 || exit 1
    rm -rf "$out/gen-nms" "$out/gen-nms-split"
    java $nms_x ${INERT_REPORT:+-DinertReport=$INERT_REPORT} -cp "$out/gen-tool$sep$cp" NmsManifest stubs "$here/nms-manifest" "$out/gen-nms" "$root/server-bukkit/java"
fi
find "$root/server-bukkit/java" "$out/gen" "$out/gen-nms" -name '*.java' | sed 's/.*/"&"/' >"$out/runtime-sources.txt"
javac --release 25 -nowarn -proc:none -encoding UTF-8 -cp "$cp" -d "$out/runtime-classes" @"$out/runtime-sources.txt"
jar --create --file "$out/basalt-bukkit.jar" -C "$out/runtime-classes" . -C "$root/server-bukkit/resources" .
mkdir -p "$out/runtime-libs" && cp "$out/bukkit-api.jar" "$out"/lib/*.jar "$out/runtime-libs/"
echo "built $out/basalt-bukkit.jar"
