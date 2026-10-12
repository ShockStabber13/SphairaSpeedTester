#include "sphaira_pipeline.hpp"
#include <switch.h>
#include <curl/curl.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <sys/stat.h>

namespace {
constexpr const char* DIR = "sdmc:/switch/SphairaSpeedTester";
constexpr const char* URL_FILE = "sdmc:/switch/SphairaSpeedTester/url.txt";

std::string loadUrl() {
    std::ifstream f(URL_FILE);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("https://", 0) == 0 || line.rfind("http://", 0) == 0)
            return line;
    }
    return {};
}
std::string hostOnly(const std::string& url) {
    auto pos = url.find("://");
    if (pos == std::string::npos) return "(none)";
    pos += 3;
    return url.substr(pos, url.find_first_of("/?#", pos) - pos);
}
double mib(std::uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}
void render(const std::string& url, const tester::Progress* p, bool sd) {
    std::printf("\x1b[2J\x1b[H");
    std::printf("SPHAIRA SPEED TESTER v0.1\n");
    std::printf("Sphaira-derived 64 KiB network + 2x4 MiB SD ring\n\n");
    std::printf("Source: %s\n", hostOnly(url).c_str());
    std::printf("Sample: 256 MiB (HTTP Range 0-268435455)\n");
    std::printf("URL file: /switch/SphairaSpeedTester/url.txt\n\n");
    if (url.empty()) std::printf("Missing URL. Add a direct HTTP(S) link in url.txt.\n\n");
    if (!p) {
        std::printf("A: HTTP-only speed test\n");
        std::printf("X: HTTP + SD write speed test\n");
        std::printf("+: Exit\n");
    } else {
        const auto downloaded = p->received.load();
        const auto saved = p->written.load();
        const auto elapsed = p->elapsedMs.load();
        std::printf("Mode: %s\n", sd ? "HTTP + SD" : "HTTP only");
        std::printf("Received: %.1f MiB | Consumed: %.1f MiB\n",
                    mib(downloaded), mib(saved));
        if (elapsed)
            std::printf("Overall: %.2f MiB/s | HTTP: %d\n",
                        mib(saved) * 1000.0 / elapsed, p->httpCode.load());
        else std::printf("Current bytes recorded; final speed on completion.\n");
        if (p->done.load())
            std::printf("\nResult: %s\n\nA/X: another test | +: Exit\n",
                        p->result.c_str());
        else std::printf("\nB: Cancel test\n");
    }
    consoleUpdate(nullptr);
}
} // namespace

int main(int, char**) {
    consoleInit(nullptr);
    const Result socket = socketInitializeDefault();
    if (R_FAILED(socket)) {
        std::printf("Network initialization failed: 0x%08X\n", socket);
        consoleUpdate(nullptr);
        // Keep display visible for the user rather than crashing.
        for (;;) {
            hidScanInput();
            if (hidKeysDown(CONTROLLER_P1_AUTO) & KEY_PLUS) break;
            svcSleepThread(100000000);
        }
        consoleExit(nullptr);
        return 1;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mkdir(DIR, 0777);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    std::string url = loadUrl();
    std::unique_ptr<tester::Progress> progress;
    std::thread worker;
    bool sd = false, running = true;
    auto lastUi = std::chrono::steady_clock::now();
    render(url, nullptr, false);

    while (appletMainLoop() && running) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (progress && progress->done.load() && worker.joinable()) worker.join();
        if ((down & HidNpadButton_Plus) && (!progress || progress->done.load())) {
            running = false;
        } else if ((down & HidNpadButton_B) && progress && !progress->done.load()) {
            progress->cancel.store(true);
        } else if ((down & (HidNpadButton_A | HidNpadButton_X)) &&
                   (!progress || progress->done.load())) {
            if (worker.joinable()) worker.join();
            url = loadUrl();
            if (!url.empty()) {
                sd = (down & HidNpadButton_X) != 0;
                progress = std::make_unique<tester::Progress>();
                auto* p = progress.get();
                worker = std::thread([url, sd, p] {
                    tester::runSample(url, sd, *p);
                });
            }
        }
        if (std::chrono::steady_clock::now() - lastUi >
            std::chrono::milliseconds(250)) {
            lastUi = std::chrono::steady_clock::now();
            if (progress && !progress->done.load()) {
                // Live wall-clock elapsed includes network and SD backpressure.
                // Avoid setting elapsedMs on this thread: worker stores final value.
            }
            render(url, progress.get(), sd);
        }
        svcSleepThread(16000000);
    }

    if (progress && !progress->done.load()) progress->cancel.store(true);
    if (worker.joinable()) worker.join();
    curl_global_cleanup();
    socketExit();
    consoleExit(nullptr);
    return 0;
}
