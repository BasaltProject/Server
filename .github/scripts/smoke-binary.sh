#!/bin/sh
# Exercise the real entry point without accepting the EULA or downloading game assets.
set -eu
binary="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
cd "$scratch"
status=0
"$binary" >startup.log 2>&1 || status=$?
cat startup.log
[ "$status" -eq 78 ]
grep -q 'Basalt Server ' startup.log
grep -q 'eula = false' eula.toml
