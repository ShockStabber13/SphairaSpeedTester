#include "sphaira_pipeline.hpp"
#include <switch.h>
#include <curl/curl.h>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <sys/stat.h>

namespace {
constexpr const char* DIR = "sdmc:/switch/SphairaSpeedTester";
constexpr const char* URL_FILE = "sdmc:/switch/SphairaSpeedTester/url.txt";
constexpr const char* LOG_FILE = "sdmc:/switch/SphairaSpeedTester/debug.log";
// Write to the already-present /switch folder so diagnostics survive even
// when our application's own directory could not be created.
constexpr const char* ROOT_LOG = "sdmc:/switch/SphairaSpeedTester-startup.log";

void logStage(const char* stage) {
    for (const char* path : {ROOT_LOG, LOG_FILE}) {
        FILE* file = std::fopen(path, "a");
        if (file) {
            std::fprintf(file, "tester: %s\n", stage);
            std::fflush(file);
            std::fclose(file);
        }
    }
}

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
void render(const std::string& url, const tester::Progress* p, bool sd,
            double runningSeconds = 0) {
    std::printf("\x1b[2J\x1b[H");
    std::printf("SPHAIRA SPEED TESTER v0.2 (diagnostics)\n");
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
        const double seconds = (p->done.load() && elapsed)
            ? elapsed / 1000.0 : runningSeconds;
        std::printf("Mode: %s\n", sd ? "HTTP + SD" : "HTTP only");
        std::printf("Received: %.1f MiB (%.0f%%) | Consumed: %.1f MiB\n",
                    mib(downloaded), 100.0 * downloaded / tester::SAMPLE_BYTES,
                    mib(saved));
        if (seconds > 0)
            std::printf("HTTP: %.2f MiB/s | Pipeline: %.2f MiB/s\n",
                        mib(downloaded) / seconds, mib(saved) / seconds);
        std::printf("HTTP status: %d\n", p->httpCode.load());
        if (p->done.load())
            std::printf("\nResult: %s\n\nA/X: another test | +: Exit\n",
                        p->result.c_str());
        else std::printf("\nB: Cancel test\n");
    }
    consoleUpdate(nullptr);
}
} // namespace

int main(int, char**) {
    logStage("entered main (before consoleInit)");
    consoleInit(nullptr);
    logStage("console initialized");
    const int dirResult = mkdir(DIR, 0777);
    const int dirErrno = errno;
    char dirNote[96];
    std::snprintf(dirNote, sizeof(dirNote),
                  "mkdir directory result=%d errno=%d", dirResult, dirErrno);
    logStage(dirNote);
    logStage("Startup: console initialized (network NOT initialized)");
    // No BSD sockets, SSL services, or curl during app startup. This
    // isolates launch crashes from the download pipeline.
    bool networkReady = false;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    logStage("Startup: controllers initialized");
    std::string url = loadUrl();
    logStage("Startup: URL configuration read");
    std::unique_ptr<tester::Progress> progress;
    std::string startupError;
    std::thread worker;
    bool sd = false, running = true;
    auto lastUi = std::chrono::steady_clock::now();
    auto started = lastUi;
    render(url, nullptr, false);
    logStage("Startup: ready at main menu");

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
                if (!networkReady) {
                    logStage("Network: initializing on user request");
                    const Result socketResult = socketInitializeDefault();
                    if (R_FAILED(socketResult)) {
                        char code[32];
                        std::snprintf(code, sizeof(code), "0x%08X",
                                      static_cast<unsigned>(socketResult));
                        startupError = std::string("Socket init failed: ") + code;
                        logStage("Network: socket init FAILED");
                    } else if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
                        startupError = "curl initialization failed";
                        logStage("Network: curl init FAILED");
                        socketExit();
                    } else {
                        networkReady = true;
                        startupError.clear();
                        logStage("Network: ready");
                    }
                }
                if (!startupError.empty()) {
                    logStage("Network: blocked test after init failure");
                    continue;
                }
                sd = (down & HidNpadButton_X) != 0;
                progress = std::make_unique<tester::Progress>();
                started = std::chrono::steady_clock::now();
                auto* p = progress.get();
                logStage(sd ? "Sample: started HTTP + SD" :
                              "Sample: started HTTP only");
                try {
                    worker = std::thread([url, sd, p] {
                        try {
                            tester::runSample(url, sd, *p);
                        } catch (const std::exception& e) {
                            p->result = std::string("Worker: ") + e.what();
                            p->done.store(true);
                        } catch (...) {
                            p->result = "Worker failed unexpectedly";
                            p->done.store(true);
                        }
                    });
                } catch (const std::exception& e) {
                    progress->result = std::string("Cannot start test: ") + e.what();
                    progress->done.store(true);
                }
            }
        }
        if (std::chrono::steady_clock::now() - lastUi >
            std::chrono::milliseconds(250)) {
            lastUi = std::chrono::steady_clock::now();
            const double seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            render(url, progress.get(), sd, seconds);
            if (!startupError.empty()) {
                std::printf("\nNetwork initialization: %s\n", startupError.c_str());
                consoleUpdate(nullptr);
            }
        }
        svcSleepThread(16000000);
    }

    if (progress && !progress->done.load()) progress->cancel.store(true);
    if (worker.joinable()) worker.join();
    logStage("Shutdown: normal");
    if (networkReady) {
        curl_global_cleanup();
        socketExit();
    }
    consoleExit(nullptr);
    return 0;
}
