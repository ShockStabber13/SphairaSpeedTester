#!/usr/bin/env bash
set -euo pipefail
cmake --preset Lite -DLTO=OFF
cmake --build --preset Lite --parallel 4
mkdir -p out
cp build/Lite/sphaira.nro out/SphairaSpeedTester.nro
echo "Output: out/SphairaSpeedTester.nro"
