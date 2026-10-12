# Sphaira Speed Tester

A **minimal Nintendo Switch NRO speed-test harness** forked from [NaGaa95/sphaira](https://github.com/NaGaa95/sphaira). It isolates Sphaira's network push/pull buffering and two-slot storage-pipeline design without the game browser, installer, downloader UI, themes, or Nintendo content handling.

**Upstream Sphaira remains intact on \`master\`.** The experiment is on \`experiment/minimal-speed-tester\`.

## Build

The minimal build requires devkitPro/devkitA64, libnx, and \`switch-curl\`:

\`\`\`sh
make -C speedtester -j2
\`\`\`

Output: \`speedtester/SphairaSpeedTester.nro\`. A GitHub Actions build is included.

## Run

1. Copy \`SphairaSpeedTester.nro\` to \`sdmc:/switch/SphairaSpeedTester.nro\`.
2. Create \`sdmc:/switch/SphairaSpeedTester/url.txt\` containing **one directly downloadable** \`http://\` or \`https://\` URL (first non-comment line).
3. Use a resolved AllDebrid or TorBox HTTPS link, or your own HTTP test file. The tester **does not authenticate to either debrid API**; this avoids mixing API or catalog latency into network throughput.
4. Launch with the Homebrew Menu. **A = HTTP-only**, **X = HTTP + SD**, **B = cancel**, **+ = exit**.

Each run requests up to **256 MiB** using one HTTP Range. SD tests preallocate a 256 MiB temporary sample and delete it afterward. No installs occur. The URL is deliberately never printed or uploaded; only the host is displayed.

Run the same link twice, once per mode. Real download speed depends on Wi-Fi, provider, TLS, HTTP Range support, SD and memory limitations. **Do not use expiring URLs after their expiry**.

## Architecture / provenance

**This is an isolated adaptation, not a byte-for-byte link of the original Sphaira app.** The network stage mirrors the original \`PushPullThreadData\` from \`sphaira/source/utils/devoptab_common.cpp\` (64 KiB buffer, curl pause/unpause) and staging mirrors \`RingBuf<2>\` from \`sphaira/source/threaded_file_transfer.cpp\` (4 MiB blocks swapped without per-block copies), with dedicated network, producer and writer threads.

Differences from upstream: console-only UI, no decompression or install, bounded 256 MiB Range request, full TLS certificate verification, optional discard sink, native preallocated SD test file, no automatic resume. This is intended to identify whether those core transfer mechanisms improve speed, not to claim identical behavior to unmodified Sphaira.

Upstream copyright and GPL-3.0 terms are retained. See \`LICENSE\`.

## Diagnosing a Switch crash

The tester writes startup markers to `sdmc:/switch/SphairaSpeedTester/debug.log` (only during setup, start/end, not inside the benchmark hot path). If the NRO closes unexpectedly, check the last marker and the Atmosphere fatal report, if generated. Tell us whether the crash happens **on launch**, after **A (HTTP-only)**, or after **X (HTTP+SD)**. Run through **Homebrew Menu with full RAM** (hold R while launching a game) if applet memory limits are involved. No benchmarks have been measured on actual hardware yet.

## Crash isolation and incremental builds

The UI now starts **without initializing network services**. If it opens but fails after pressing A/X, check `debug.log` for `Network: ...` markers. An immediate crash before the menu is a separate startup issue; share the Atmosphere crash report and `debug.log` to pinpoint it.

GitHub Actions builds only the tiny speed tester (never full Sphaira), runs once per push, and caches build outputs by the exact source/Makefile hash. Changed files trigger a fresh clean-environment build; local `make` automatically rebuilds only modified sources.


## v0.2 launch-crash probe

If the main tester closes immediately, try the separate \`SphairaStartupProbe.nro\` artifact first. This tiny C-only homebrew **does not load curl or allocate the download buffers**. It should display a simple console and record \`sdmc:/switch/SphairaStartupProbe.log\`, directly in the existing \`switch\` folder.

The main tester now writes \`sdmc:/switch/SphairaSpeedTester-startup.log\` **before** console initialization. It records whether app directory creation worked and remains independent of the absent app folder. Note: the SD card is mounted during libnx startup, before \`main\`. A missing app folder alone does not prove a pre-main crash.

Use the distinct probe filename and main tester filename; don't overwrite one with the other. If the probe itself closes immediately and produces no root log, the problem is at or before minimal homebrew initialization.

## Longer, selectable benchmark size

Use **Left/Right** on the idle menu to select **256 MiB / 512 MiB / 1024 MiB / 2048 MiB**. The default is **1024 MiB (1 GiB)**, which normally runs about four times longer than the old 256 MiB test at the same transfer speed. Both A (HTTP only) and X (HTTP + SD) use the selected size and show live MiB/s. The source file must contain at least as many bytes as the chosen size for a complete sample. SD mode temporarily preallocates that much free SD space, then deletes its sample file. Network/staging RAM buffer sizes are unchanged.\n