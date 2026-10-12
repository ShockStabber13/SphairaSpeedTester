# Retained upstream Sphaira transfer sources

These are **unaltered original reference files**, retained for comparisons. They are NOT compiled into the minimal NRO, which builds only the isolated \`speedtester/\` code.

- \`sphaira/source/utils/devoptab_common.cpp\`: original 64 KiB \`PushPullThreadData\`, curl pause/unpause
- \`sphaira/include/utils/devoptab_common.hpp\`: corresponding interface
- \`sphaira/source/threaded_file_transfer.cpp\`: original 4 MiB \`RingBuf<2>\` and separated worker threads
- \`sphaira/include/threaded_file_transfer.hpp\`: transfer interface
- \`sphaira/source/yati/source/http.cpp\`, \`sphaira/include/yati/source/http.hpp\`: original HTTP source/reader
- \`sphaira/source/download.cpp\`, \`sphaira/include/download.hpp\`: original curl options and standalone downloader

All other unneeded upstream features, assets, workflows, UI and installers are removed from **this experiment branch only**. The full original fork remains available on \`master\` and the original project at https://github.com/NaGaa95/sphaira. GPL-3.0 license remains at repository root.
