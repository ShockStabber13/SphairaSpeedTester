// Minimal, independently buildable adaptation of Sphaira's original network
// PushPullThreadData and threaded_file_transfer.cpp ring architecture.
// Upstream: https://github.com/NaGaa95/sphaira (GPL-3.0)
// This reproduces the relevant 64 KiB curl push/pull buffer and vector-swapping
// two-slot 4 MiB writer ring, rather than copying SwitchGamesBrowser's pipeline.
#include "sphaira_pipeline.hpp"
#include <switch.h>
#include <curl/curl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace tester {
namespace {
using clock_type = std::chrono::steady_clock;
constexpr std::size_t NET_BUFFER = 64 * 1024;
constexpr std::size_t BLOCK = 4 * 1024 * 1024;
constexpr const char* SD_FILE = "sdmc:/switch/SphairaSpeedTester/sample.part";
constexpr const char* FS_FILE = "/switch/SphairaSpeedTester/sample.part";

// Sphaira's 64 KiB PushPullThreadData buffering model: curl callback never
// blocks on the SD writer. Instead it returns CURL_WRITEFUNC_PAUSE when full.
struct PushPull {
    std::mutex mutex;
    std::condition_variable canPull;
    std::vector<unsigned char> buffer;
    CURL* curl{};
    Progress& progress;
    std::uint64_t accepted{};
    bool finished{};
    bool intentionalStop{};
    CURLcode result{CURLE_OK};
    long responseCode{};

    explicit PushPull(Progress& p) : progress(p) { buffer.reserve(NET_BUFFER); }

    static size_t onWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
        auto& self = *static_cast<PushPull*>(userdata);
        const std::size_t count = size * nmemb;
        if (!ptr || !count || self.progress.cancel.load()) return 0;
        std::lock_guard<std::mutex> lock(self.mutex);
        if (self.accepted >= SAMPLE_BYTES) {
            self.intentionalStop = true; // server ignored Range; sample done
            return 0;
        }
        const auto space = SAMPLE_BYTES - self.accepted;
        if (count > space) {
            // Last callback crossed the requested sample boundary. curl treats
            // a short callback as an error; record the deliberate sample stop.
            const std::size_t take = static_cast<std::size_t>(space);
            if (take > NET_BUFFER - self.buffer.size())
                return CURL_WRITEFUNC_PAUSE;
            self.buffer.insert(self.buffer.end(),
                               reinterpret_cast<unsigned char*>(ptr),
                               reinterpret_cast<unsigned char*>(ptr) + take);
            self.accepted += take;
            self.progress.received.fetch_add(take);
            self.intentionalStop = true;
            self.canPull.notify_one();
            return 0;
        }
        if (count > NET_BUFFER - self.buffer.size())
            return CURL_WRITEFUNC_PAUSE;
        self.buffer.insert(self.buffer.end(),
                           reinterpret_cast<unsigned char*>(ptr),
                           reinterpret_cast<unsigned char*>(ptr) + count);
        self.accepted += count;
        self.progress.received.fetch_add(count);
        self.canPull.notify_one();
        return count;
    }

    // Mirrors Sphaira's pause/unpause from the curl progress callback.
    static int onProgress(void* userdata, curl_off_t, curl_off_t downloaded,
                          curl_off_t, curl_off_t) {
        auto& self = *static_cast<PushPull*>(userdata);
        if (self.progress.cancel.load()) return 1;
        if (!downloaded) return 0;
        bool pause;
        {
            std::lock_guard<std::mutex> lock(self.mutex);
            pause = self.buffer.size() >= NET_BUFFER;
        }
        // Never call curl_easy_pause while holding the buffer mutex; unpause
        // can synchronously re-enter the curl write callback.
        return curl_easy_pause(self.curl, pause ? CURLPAUSE_ALL : CURLPAUSE_CONT)
                   == CURLE_OK ? 0 : 1;
    }

    std::size_t pull(unsigned char* dst, std::size_t request) {
        std::unique_lock<std::mutex> lock(mutex);
        std::size_t copied = 0;
        while (copied < request) {
            if (progress.cancel.load()) break;
            if (buffer.empty()) {
                if (finished) break;
                canPull.wait_for(lock, std::chrono::milliseconds(100), [&] {
                    return !buffer.empty() || finished || progress.cancel.load();
                });
                continue;
            }
            const auto n = std::min(request - copied, buffer.size());
            std::memcpy(dst + copied, buffer.data(), n);
            // Deliberately retain Sphaira's original vector.erase behavior.
            buffer.erase(buffer.begin(), buffer.begin() + n);
            copied += n;
        }
        return copied;
    }

    void perform(const std::string& url) {
        CURL* h = curl_easy_init();
        if (!h) {
            std::lock_guard<std::mutex> lock(mutex);
            result = CURLE_FAILED_INIT;
            finished = true;
            canPull.notify_all();
            return;
        }
        curl = h;
        const std::string range = "0-" + std::to_string(SAMPLE_BYTES - 1);
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_RANGE, range.c_str());
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(h, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(h, CURLOPT_USERAGENT, "SphairaSpeedTester/0.1");
        curl_easy_setopt(h, CURLOPT_BUFFERSIZE, static_cast<long>(NET_BUFFER));
        curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &PushPull::onWrite);
        curl_easy_setopt(h, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &PushPull::onProgress);
        curl_easy_setopt(h, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
        curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 20L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
        const CURLcode code = curl_easy_perform(h);
        long http = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http);
        {
            std::lock_guard<std::mutex> lock(mutex);
            result = code;
            responseCode = http;
            progress.httpCode.store(static_cast<int>(http));
            finished = true;
        }
        canPull.notify_all();
        curl_easy_cleanup(h);
        curl = nullptr;
    }
};

// Sphaira's RingBuf<2> idea: a 4 MiB read buffer swaps with each fixed slot.
// No per-block heap copy, no dynamic deque allocation in the hot path.
class TwoSlotRing {
    struct Slot {
        std::vector<unsigned char> bytes;
        std::uint64_t offset{};
        Slot() { bytes.reserve(BLOCK); }
    };
    std::array<Slot, 2> slots;
    unsigned readIndex{}, writeIndex{}, count{};
    std::mutex mutex;
    std::condition_variable canRead, canWrite;
    bool closed{}, failed{};
public:
    bool push(std::vector<unsigned char>& buf, std::uint64_t offset,
              const Progress& p) {
        std::unique_lock<std::mutex> lock(mutex);
        canWrite.wait(lock, [&] {
            return count < 2 || failed || p.cancel.load();
        });
        if (failed || p.cancel.load()) return false;
        auto& dst = slots[writeIndex];
        dst.bytes.swap(buf);
        dst.offset = offset;
        writeIndex = (writeIndex + 1) % 2;
        ++count;
        canRead.notify_one();
        return true;
    }
    bool pop(std::vector<unsigned char>& buf, std::uint64_t& offset) {
        std::unique_lock<std::mutex> lock(mutex);
        canRead.wait(lock, [&] { return count || closed || failed; });
        if (failed || !count) return false;
        auto& src = slots[readIndex];
        src.bytes.swap(buf);
        offset = src.offset;
        readIndex = (readIndex + 1) % 2;
        --count;
        canWrite.notify_one();
        return true;
    }
    void finish() {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        canRead.notify_all();
        canWrite.notify_all();
    }
    void abort() {
        std::lock_guard<std::mutex> lock(mutex);
        failed = true;
        canRead.notify_all();
        canWrite.notify_all();
    }
};

std::string hexResult(Result rc) {
    char b[32];
    std::snprintf(b, sizeof(b), "0x%08X", static_cast<unsigned>(rc));
    return b;
}
} // namespace

void runSample(const std::string& url, bool writeSd, Progress& progress) {
    FsFile sdFile{};
    FsFileSystem* sd = nullptr;
    bool sdOpened = false;
    std::string failure;
    try {
        if (writeSd) {
            sd = fsdevGetDeviceFileSystem("sdmc:");
            if (!sd) throw std::runtime_error("SD card filesystem unavailable");
            fsFsDeleteFile(sd, FS_FILE); // previous aborted test, if any
            Result rc = fsFsCreateFile(sd, FS_FILE,
                                       static_cast<s64>(SAMPLE_BYTES), 0);
            if (R_FAILED(rc)) throw std::runtime_error("SD allocate: " + hexResult(rc));
            rc = fsFsOpenFile(sd, FS_FILE, FsOpenMode_Write, &sdFile);
            if (R_FAILED(rc)) throw std::runtime_error("SD open: " + hexResult(rc));
            sdOpened = true;
        }

        const auto start = clock_type::now();
        PushPull pipe(progress);
        TwoSlotRing ring;
        std::string writerError;
        std::thread network([&] { pipe.perform(url); });
        std::thread writer([&] {
            std::vector<unsigned char> buffer;
            buffer.reserve(BLOCK);
            std::uint64_t offset = 0;
            while (ring.pop(buffer, offset)) {
                if (progress.cancel.load()) break;
                if (writeSd) {
                    const Result rc = fsFileWrite(&sdFile, static_cast<s64>(offset),
                                                  buffer.data(), buffer.size(),
                                                  FsWriteOption_None);
                    if (R_FAILED(rc)) {
                        writerError = "SD write: " + hexResult(rc);
                        progress.cancel.store(true);
                        ring.abort();
                        break;
                    }
                }
                progress.written.fetch_add(buffer.size());
                buffer.clear();
            }
        });

        std::vector<unsigned char> buffer;
        buffer.resize(BLOCK);
        std::uint64_t offset = 0;
        while (offset < SAMPLE_BYTES && !progress.cancel.load()) {
            const auto desired = static_cast<std::size_t>(
                std::min<std::uint64_t>(BLOCK, SAMPLE_BYTES - offset));
            const std::size_t count = pipe.pull(buffer.data(), desired);
            if (!count) break;
            buffer.resize(count);
            if (!ring.push(buffer, offset, progress)) break;
            offset += count;
            buffer.resize(BLOCK);
        }
        ring.finish();
        writer.join();
        network.join();

        if (sdOpened) {
            if (!progress.cancel.load()) {
                const Result rc = fsFileFlush(&sdFile);
                if (R_FAILED(rc)) writerError = "SD flush: " + hexResult(rc);
            }
            fsFileClose(&sdFile);
            sdOpened = false;
        }
        progress.elapsedMs.store(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                clock_type::now() - start).count()));

        if (!writerError.empty()) failure = writerError;
        else if (progress.cancel.load()) failure = "Cancelled";
        else if (pipe.responseCode != 200 && pipe.responseCode != 206)
            failure = "HTTP error " + std::to_string(pipe.responseCode);
        else if (pipe.result != CURLE_OK && !pipe.intentionalStop)
            failure = std::string("curl: ") + curl_easy_strerror(pipe.result);
        else if (!progress.received.load())
            failure = "No HTTP data received";
        else if (progress.written.load() != progress.received.load())
            failure = "Buffer/write byte mismatch";
        else progress.result = "Complete";
    } catch (const std::exception& e) {
        failure = e.what();
    } catch (...) {
        failure = "Unknown transfer failure";
    }
    if (sdOpened) fsFileClose(&sdFile);
    if (sd) fsFsDeleteFile(sd, FS_FILE); // never retain a partially sampled game
    if (!failure.empty()) progress.result = failure;
    progress.done.store(true);
}
} // namespace tester
