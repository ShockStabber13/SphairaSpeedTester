# Minimal Sphaira Speed Tester

Benchmark engine source, HTTP, threading and original NanoVG UI remain unmodified.
Only a slim app shell is built; the original full Sphaira menus/installer/USB/audio systems are not compiled.

Source branch: benchmark/known-good-9mbs; base known-good commit: f53878a93a9bac42a8c09f76db29a95905a7aa92.

The original 256 MiB test runs from sdmc:/switch/SphairaSpeedTester/url.txt.
A: HTTP only. X: HTTP + SD writing. Plus: exit. Press B to cancel.

Build: cmake --preset Lite -DLTO=OFF && cmake --build --preset Lite
NRO: build/Lite/sphaira.nro

Do not treat a passing compile as proof of identical on-console speed. Verify on hardware before integrating into SwitchGamesBrowser.
