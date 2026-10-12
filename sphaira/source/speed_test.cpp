// Adapter to original, unmodified sphaira::yati::source::Http
// and original sphaira::thread::Transfer. No substitute network code.
#include "speed_test.hpp"
#include "app.hpp"
#include "defines.hpp"
#include "threaded_file_transfer.hpp"
#include "yati/source/http.hpp"
#include "ui/progress_box.hpp"
#include <switch.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

namespace sphaira::speedtest {
namespace {
constexpr s64 BYTES = 256LL * 1024 * 1024;
constexpr const char* URL = "sdmc:/switch/SphairaSpeedTester/url.txt";
constexpr const char* RESULT = "sdmc:/switch/SphairaSpeedTester-result.txt";
constexpr const char* TEMP = "/switch/SphairaSpeedTester-sample.part";
std::string ReadUrl() {
    std::ifstream f(URL);
    std::string s;
    while (std::getline(f, s)) {
        if (!s.empty() && s.back() == '\r') s.pop_back();
        if (s.rfind("https://", 0) == 0 || s.rfind("http://", 0) == 0) return s;
    }
    return {};
}
void Save(const std::string& msg) {
    if (FILE* f = std::fopen(RESULT, "w")) {
        std::fprintf(f, "%s\n", msg.c_str());
        std::fclose(f);
    }
}
}
void Start(bool writeSd) {
    const auto url = ReadUrl();
    if (url.empty()) {
        App::Notify("Set direct AllDebrid URL in /switch/SphairaSpeedTester/url.txt");
        return;
    }
    auto report = std::make_shared<std::string>("No result");
    App::Push<ui::ProgressBox>(0, "AllDebrid test",
        writeSd ? "Sphaira HTTP + SD" : "Sphaira HTTP only",
        [url, writeSd, report](ui::ProgressBox* pbox) -> Result {
            FsFileSystem* sd = nullptr;
            FsFile file{};
            bool opened = false;
            if (writeSd) {
                sd = fsdevGetDeviceFileSystem("sdmc:");
                if (!sd) {
                    *report = "SD filesystem unavailable";
                    Save(*report);
                    return Result_YatiHttpReadFailed;
                }
                fsFsDeleteFile(sd, TEMP);
                Result rc = fsFsCreateFile(sd, TEMP, BYTES, 0);
                if (R_FAILED(rc)) {
                    *report = "SD create failed";
                    Save(*report);
                    return rc;
                }
                rc = fsFsOpenFile(sd, TEMP, FsOpenMode_Write, &file);
                if (R_FAILED(rc)) {
                    fsFsDeleteFile(sd, TEMP);
                    fsFsCommit(sd);
                    *report = "SD open failed";
                    Save(*report);
                    return rc;
                }
                opened = true;
                fsFsCommit(sd);
            }
            const auto start = std::chrono::steady_clock::now();
            u64 received = 0, written = 0;
            yati::source::Http source(url, "", "", pbox->GetToken());
            const auto read = [&source, &received](void* data, s64 off,
                s64 size, u64* bytes) -> Result {
                const Result rc = source.Read(data, off, size, bytes);
                if (R_SUCCEEDED(rc)) received += *bytes;
                return rc;
            };
            const auto write = [&file, &written, writeSd](const void* data,
                s64 off, s64 size) -> Result {
                if (writeSd)
                    R_TRY(fsFileWrite(&file, off, data, size, FsWriteOption_None));
                written += size;
                R_SUCCEED();
            };
            const Result rc = thread::Transfer(pbox, BYTES, read, write,
                thread::Mode::MultiThreaded);
            const double seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            if (opened) {
                const Result flushed = fsFileFlush(&file);
                fsFileClose(&file);
                fsFsDeleteFile(sd, TEMP);
                fsFsCommit(sd);
                if (R_FAILED(flushed) && R_SUCCEEDED(rc)) {
                    *report = "SD flush failed";
                    Save(*report);
                    return flushed;
                }
            }
            char text[180]{};
            const double mib = double(written) / (1024.0 * 1024.0);
            std::snprintf(text, sizeof(text),
                "%s %.2f MiB/s; %.1f MiB in %.1f sec; received %llu; Result 0x%08X",
                writeSd ? "HTTP+SD" : "HTTP only",
                seconds > 0 ? mib / seconds : 0.0, mib, seconds,
                static_cast<unsigned long long>(received),
                static_cast<unsigned>(rc));
            *report = text;
            Save(*report);
            return rc;
        },
        [report](Result rc) {
            App::Notify(R_FAILED(rc)
                ? "Speed test failed; see SphairaSpeedTester-result.txt"
                : *report);
        }
    );
}
}
