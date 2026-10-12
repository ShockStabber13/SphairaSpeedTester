#pragma once
#include <atomic>
#include <cstdint>
#include <string>

namespace tester {
constexpr std::uint64_t SAMPLE_BYTES = 256ULL * 1024 * 1024;
struct Progress {
    std::atomic<bool> cancel{false};
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> received{0};
    std::atomic<std::uint64_t> written{0};
    std::atomic<std::uint64_t> elapsedMs{0};
    std::atomic<int> httpCode{0};
    // Set by the worker before done=true; read on the UI thread only after done.
    std::string result;
};
void runSample(const std::string& url, bool writeSd, Progress& progress);
} // namespace tester
