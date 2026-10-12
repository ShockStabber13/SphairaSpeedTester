# Sphaira Speed Tester — Original engine

The original Sphaira application, initialization, configured network sockets, CPU boost, `yati::source::Http`, `PushThreadData`, transfer threads and ring buffers are **unchanged**. This branch only adds two benchmark options and the small `speed_test.cpp` adapter.

Branch: `experiment/upstream-engine-benchmark` (same repository; `master` and previous experiment remain untouched).

1. Place one resolved, authorized **AllDebrid direct-file URL** at `sdmc:/switch/SphairaSpeedTester/url.txt`.
2. Launch the built `SphairaSpeedTester.nro` and select **Y → Network → AllDebrid: HTTP only** or **AllDebrid: HTTP + SD**.
3. Read the persistent results at `sdmc:/switch/SphairaSpeedTester-result.txt`.

Each run tests up to **256 MiB** and requires a file at least that large supporting HTTP Range. HTTP-only discards bytes; HTTP+SD uses an automatically deleted temporary file. No installation occurs. The original source builds with the Lite CMake preset without LTO and uses an incremental GitHub Actions build cache.
