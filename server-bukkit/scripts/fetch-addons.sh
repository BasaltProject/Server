#!/usr/bin/env bash
# Downloads the pinned WorldEdit family jars the add-ons compile against into <dir>, each checked against its SHA-256.
here="$(cd "$(dirname "$0")" && pwd)"
dir="${1:?dir}"
mkdir -p "$dir"
while IFS='|' read -r name url sha; do
    case "$name" in '' | '#'*) continue ;; esac
    [ -s "$dir/$name" ] && continue
    if curl -fsSL --retry 2 --connect-timeout 15 -m 300 "$url" -o "$dir/$name.part" \
        && [ "$(sha256sum "$dir/$name.part" | cut -c1-64)" = "$sha" ]; then
        mv "$dir/$name.part" "$dir/$name"
        echo "fetched $name"
    else
        rm -f "$dir/$name.part"
        echo "Bukkit add-ons: could not fetch $name from $url (offline, or the hash does not match); its add-on is skipped" >&2
    fi
done <"$here/addons.txt"
exit 0
