# Minimal SphairaSpeedTester — known-good engine preserved

Branch: `benchmark/known-good-9mbs` in `ShockStabber13/SphairaSpeedTester`.

The tester now contains a minimal **resource and source-tree subset** of the
known-good benchmark. The benchmark entry point, original HTTP engine,
PushThreadData, threaded transfer, NanoVG progress popup, option popup, and
tester main menu remain **byte-for-byte unchanged**.

A number of original Sphaira utility modules are still compiled by the
native Sphaira application runtime. Removing those without adapting their
callers and proving a successful build would risk breaking the tester.

Deleted unrelated menu source files, file associations, unused screenshots,
alternative themes, unrelated USB/WebUSB tooling and CI workflows.

## Running

1. Put a resolved HTTP(S) direct-file URL to a file **at least 256 MiB**
   in `sdmc:/switch/SphairaSpeedTester/url.txt`.
2. On Switch, press **A** for HTTP only or **X** for HTTP + SD write.
   Press **+** to exit; **B** cancels an active transfer.
3. Results: `sdmc:/switch/SphairaSpeedTester-result.txt`.

The benchmark makes ranged requests and tests up to 256 MiB. HTTP-only
discards downloaded data. HTTP+SD writes to a temporary file, then removes it.

## Building

```bash
./build_release.sh
```

Output: `out/SphairaSpeedTester.nro`.

Equivalent:
```bash
cmake --preset Lite -DLTO=OFF
cmake --build --preset Lite --parallel 4
```

The equivalent build output is `build/Lite/sphaira.nro`.

GitHub Actions also compiles with the Lite preset, checks source hashes
against the known-good files, verifies NRO header and uploads the artifact.

**Verification status:** Source hashes have been verified. Successful CI
compilation and correct execution/throughput on an actual Nintendo Switch
remain unconfirmed. The benchmark should only be called known-good after
hardware testing of this stripped commit.
