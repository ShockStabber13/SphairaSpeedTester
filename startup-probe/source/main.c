#include <switch.h>
#include <stdio.h>

// Tiny C-only NRO with no libcurl, C++ runtime or extra dependencies.
// Record first startup point in the existing /switch folder, not a new folder.
#define DIAG "sdmc:/switch/SphairaStartupProbe.log"

static void mark(const char *s) {
    FILE *f = fopen(DIAG, "a");
    if (f) {
        fprintf(f, "probe: %s\n", s);
        fflush(f);
        fclose(f);
    }
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mark("entered main before consoleInit");
    consoleInit(NULL);
    mark("consoleInit succeeded");
    printf("SPHAIRA STARTUP PROBE\n\n");
    printf("This test does NOT start networking or allocate 4 MiB buffers.\n");
    printf("If you can read this screen, basic NRO startup works.\n\n");
    printf("Log: sdmc:/switch/SphairaStartupProbe.log\n\n");
    printf("Press PLUS to exit.\n");
    consoleUpdate(NULL);
    mark("initial console frame displayed");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    mark("controller initialized");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        svcSleepThread(16000000);
    }
    mark("exiting normally");
    consoleExit(NULL);
    return 0;
}
