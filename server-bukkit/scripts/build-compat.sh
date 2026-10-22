#!/usr/bin/env bash
# Compiles the add-ons of server-bukkit/compat/worldedit against WorldEdit family jars (inputs, never committed) into
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="$(realpath -m "${1:?out dir}")"
shift
for need in "$out/basalt-bukkit.jar" "$out/bukkit-api.jar"; do
    [ -s "$need" ] || { echo "$need is missing: run server-bukkit/scripts/build-java.sh first" >&2; exit 1; }
done
mkdir -p "$out/addons"
libs="$out/basalt-bukkit.jar:$out/bukkit-api.jar:$(ls "$out"/lib/*.jar | tr '\n' ':')"
value() { { grep -m1 "^$2=" "$1" || true; } | cut -d= -f2-; }
holds() { unzip -Z1 "$1" "${2//./\/}.class" >/dev/null 2>&1; }
for manifest in "$root"/server-bukkit/compat/*/*/basalt-addon.properties; do
    variant="$(dirname "$manifest")"
    spi="$(value "$manifest" spi)"
    requires="$(value "$manifest" requires)"
    excludes="$(value "$manifest" excludes)"
    id="$(value "$manifest" id)"
    source=""
    for jar in "$@"; do
        jar="$(realpath "$jar")"
        if holds "$jar" "$spi" && { [ -z "$requires" ] || holds "$jar" "$requires"; } \
            && { [ -z "$excludes" ] || ! holds "$jar" "$excludes"; }; then
            source="$jar"
            break
        fi
    done
    [ -n "$source" ] || continue
    work="$out/addon-$id"
    rm -rf "$work" && mkdir -p "$work"
    find "$variant/java" "$(dirname "$variant")/common/java" -name '*.java' >"$out/addon-$id-sources.txt"
    javac --release 25 -nowarn -proc:none -encoding UTF-8 -cp "$source:$libs" -d "$work" @"$out/addon-$id-sources.txt"
    cp "$manifest" "$work/"
    jar --create --file "$out/addons/$id.jar" -C "$work" .
    echo "built $out/addons/$id.jar from $(basename "$source")"
done
