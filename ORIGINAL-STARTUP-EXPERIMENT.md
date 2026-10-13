# Original Sphaira startup / minimal benchmark experiment

Based on the 91-file tester build at `b9f3af74340fff794586c093c2e392ecf91b957e`.

**Controlled change:** restore the complete, unmodified Sphaira `app.cpp`, its
matching `app.hpp`, and required supporting source files from
`d0781c287a8a59e4c1e41d07a7c3714598f96304`. Keep `speed_test.cpp`,
`yati/source/http.cpp`, `threaded_file_transfer.cpp`, original
`ProgressBox` and `main_menu.cpp` byte-for-byte unchanged.

The full Sphaira App initialization cannot operate with just the 91 files;
it calls functions in additional compiled components. **Do not interpret
a larger source tree as additional features visible to the tester.**

Use `cmake --preset Lite -DLTO=OFF` and `cmake --build --preset Lite`.
Check speeds on the *same* URL, SD card, and network conditions as the
previous benchmark. Passing CI tests compilation only.
