# Sphaira Speed Tester — Original engine

The original Sphaira application, initialization, configured network sockets, CPU boost, `yati::source::Http`, `PushThreadData`, transfer threads and ring buffers are **unchanged**. This branch only adds two benchmark options and the small `speed_test.cpp` adapter.

Branch: `experiment/upstream-engine-benchmark` (same repository; `master` and previous experiment remain untouched).

1. Place one resolved, authorized **AllDebrid direct-file URL** at `sdmc:/switch/SphairaSpeedTester/url.txt`.
2. Launch `SphairaSpeedTester.nro` and press **A** for HTTP-only or **X** for HTTP + SD. **+** exits. This is now a single dedicated speed-test screen, with no Sphaira menu tabs, app store, file manager, or installer.
3. Read the persistent results at `sdmc:/switch/SphairaSpeedTester-result.txt`.

Each run tests up to **256 MiB** and requires a file at least that large supporting HTTP Range. HTTP-only discards bytes; HTTP+SD uses an automatically deleted temporary file. No installation occurs. The original source builds with the Lite CMake preset without LTO and uses an incremental GitHub Actions build cache.

## Dedicated interface

The original Sphaira `MainMenu` has been replaced with a minimal black/green benchmark screen while retaining the native Sphaira `App`, networking and transfer pipeline. The menu registry is empty, update checks and browser tabs are gone, and the NRO uses a neutral app icon rather than original Sphaira branding.

## A/B experiment 01 — CPU boost off

This commit differs from benchmark/known-good-9mbs by **one behavior only**: `App::SetBoostMode(true)` is ignored across startup and progress UI. `SetBoostMode(false)` continues to request normal mode. The original sockets, curl options, PushThreadData, yati::source::Http and transfer buffers are unchanged. Compare **A / HTTP-only** using the *same resolved AllDebrid file and same network* on both builds. Record 2–3 runs and their MiB/s. The historical known-good result is approximately 9 MB/s, but this new build is untested on real hardware. If it falls significantly, CPU boost is a contributor; if unchanged, boost is not required for that measured speed under these conditions.
