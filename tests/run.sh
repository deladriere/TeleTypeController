#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
node tests/protocol.test.cjs
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/teletype-tests.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -I tests/firmware/stubs tests/firmware/controls.cpp -o "$build_dir/controls"
"$build_dir/controls"
