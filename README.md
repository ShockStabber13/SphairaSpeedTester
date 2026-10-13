# Sphaira Speed Tester (minimal)

Dedicated Nintendo Switch HTTP/SD speed tester, with its **original known-good**
HTTP, ring-buffer transfer, benchmark and NanoVG popup implementations preserved.

## Build

```sh
cmake --preset Lite -DLTO=OFF
cmake --build --preset Lite --parallel 2
```

Output: `build/Lite/sphaira.nro`. The GitHub Actions workflow publishes
`SphairaSpeedTester.nro` inside its ZIP artifact.

## Run

Put an authorized, resolved HTTP(S) direct-file URL in
`sdmc:/switch/SphairaSpeedTester/url.txt` (file at least 256 MiB; HTTP Range
support required). Launch the NRO, press **A** for HTTP-only, **X** for HTTP+SD,
**+** to exit, or **B** to cancel via the original progress/confirmation popup.
Results are saved to `sdmc:/switch/SphairaSpeedTester-result.txt`.

This build removes the Sphaira installer, file browser, appstore, update checker,
USB, FTP, MTP, and unrelated menus. It keeps NanoVG/deko3d, the original
256 MiB benchmark, network transfer, buffering, cancellation, and progress UI.
A passing GitHub Actions compile does not prove identical speed or stability
on real Switch hardware. Test before integrating into SwitchGamesBrowser.

Original Sphaira source and license remain credited via `LICENSE`.
