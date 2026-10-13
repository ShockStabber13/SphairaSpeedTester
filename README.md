# SphairaSpeedTester — Minimal original-engine benchmark

This is the stripped-down version of the original `benchmark/known-good-9mbs` tester.
It keeps the **exact original tester UI, NanoVG progress box, HTTP stream, and
multithreaded HTTP/SD transfer implementation**; no SDL reimplementation.

Removed: unused Sphaira menu source files, file association presets, extra themes,
screenshots, unrelated USB/WebUSB tools, and unrelated CI workflows.
The Sphaira support runtime still supplies dependencies for the original popup,
HTTP/SD pipeline and rendering. It has deliberately **not** been rewritten.

## Build

```bash
cmake --preset Lite -DLTO=OFF
cmake --build --preset Lite --parallel 4
```

Output: `build/Lite/sphaira.nro` (rename to `SphairaSpeedTester.nro`
for the SD card). The build script `./build_release.sh` packages
`out/SphairaSpeedTester.nro`.

## Use

1. Place a direct HTTP(S) URL for a file of at least 256 MiB in
   `sdmc:/switch/SphairaSpeedTester/url.txt`.
2. Open the tester in application mode. Press **A** for HTTP only or **X**
   for HTTP + SD. Press **+** to exit.
3. Read `sdmc:/switch/SphairaSpeedTester-result.txt`.

A successful build does not prove Switch hardware throughput. Verify on your
console before regarding the reduction as validated.

## Source integrity

The GitHub Actions build checks SHA-1 Git blob hashes for the original
benchmark, HTTP, threaded transfer, progress popup, option popup and home
screen before compiling.
