#!/usr/bin/env bash
set -euo pipefail
make -C "$(dirname "$0")/speedtester" -j2
echo "Output: speedtester/SphairaSpeedTester.nro"
