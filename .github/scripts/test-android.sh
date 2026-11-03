#!/usr/bin/env bash
set -euo pipefail
build="${1:?build directory}"
remote=/data/local/tmp/basalt-ci
adb shell "rm -rf $remote && mkdir -p $remote"
adb push .github/scripts/smoke-binary.sh "$remote/smoke-binary.sh"
for binary in "$build"/bin/* \
    "$build/wire/wire-common/wire-common-test" \
    "$build/wire/wire-io/wire-io-test" \
    "$build/wire/wire-auth/wire-auth-test" \
    "$build/server-random/server-random-test"; do
    adb push "$binary" "$remote/"
done
adb shell "chmod 755 $remote/*"
adb shell "cd $remote && TMPDIR=$remote sh ./smoke-binary.sh ./BasaltServer-*"
for test in wire-common-test wire-io-test wire-auth-test server-random-test; do
    adb shell "cd $remote && TMPDIR=$remote BASALT_TEST_ANDROID_DNS_HOST=piston-meta.mojang.com BASALT_TEST_ANDROID_HTTPS_URL=https://piston-meta.mojang.com/mc/game/version_manifest_v2.json ./$test"
done
