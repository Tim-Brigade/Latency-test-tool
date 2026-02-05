#include "ResultsManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <numeric>

namespace latency {

ResultsManager::ResultsManager() = default;

void ResultsManager::startTest(const std::string& streamUrl, const std::string& codec,
                                int width, int height) {
    clear();

    currentTest_.testId = generateTestId();
    currentTest_.streamUrl = streamUrl;
    currentTest_.codec = codec;
    currentTest_.resolutionWidth = width;
    currentTest_.resolutionHeight = height;

    testRunning_ = true;
}

void ResultsManager::addMeasurement(const LatencyMeasurement& measurement) {
    if (!testRunning_) return;

    currentTest_.framesAnalyzed++;

    if (measurement.valid) {
        latencySamples_.push_back(measurement.latencyMs);
    }
}

TestResult ResultsManager::endTest() {
    testRunning_ = false;

    currentTest_.statistics = computeStatistics();
    currentTest_.testDurationSec = latencySamples_.empty() ? 0 :
        static_cast<int>(latencySamples_.size() / 30);  // Approximate

    lastResult_ = currentTest_;
    return lastResult_;
}

LatencyStatistics ResultsManager::getCurrentStatistics() const {
    return computeStatistics();
}

LatencyStatistics ResultsManager::computeStatistics() const {
    LatencyStatistics stats;
    stats.validSamples = static_cast<int>(latencySamples_.size());
    stats.invalidSamples = currentTest_.framesAnalyzed - stats.validSamples;

    if (latencySamples_.empty()) {
        return stats;
    }

    // Create sorted copy for percentile calculations
    std::vector<int32_t> sorted = latencySamples_;
    std::sort(sorted.begin(), sorted.end());

    // Min/Max
    stats.minMs = sorted.front();
    stats.maxMs = sorted.back();

    // Average
    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    stats.avgMs = sum / sorted.size();

    // Standard deviation
    double sqSum = 0.0;
    for (int32_t val : sorted) {
        double diff = val - stats.avgMs;
        sqSum += diff * diff;
    }
    stats.stdDevMs = std::sqrt(sqSum / sorted.size());

    // Percentiles
    auto percentile = [&sorted](double p) -> int32_t {
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    stats.p50Ms = percentile(0.50);
    stats.p95Ms = percentile(0.95);
    stats.p99Ms = percentile(0.99);

    return stats;
}

bool ResultsManager::exportToJson(const std::string& filename) const {
    nlohmann::json j;

    j["test_id"] = lastResult_.testId;
    j["stream_url"] = lastResult_.streamUrl;
    j["codec"] = lastResult_.codec;
    j["resolution"] = {
        {"width", lastResult_.resolutionWidth},
        {"height", lastResult_.resolutionHeight}
    };
    j["test_duration_sec"] = lastResult_.testDurationSec;
    j["frames_analyzed"] = lastResult_.framesAnalyzed;

    j["statistics"] = {
        {"min_ms", lastResult_.statistics.minMs},
        {"max_ms", lastResult_.statistics.maxMs},
        {"avg_ms", lastResult_.statistics.avgMs},
        {"std_dev_ms", lastResult_.statistics.stdDevMs},
        {"p50_ms", lastResult_.statistics.p50Ms},
        {"p95_ms", lastResult_.statistics.p95Ms},
        {"p99_ms", lastResult_.statistics.p99Ms},
        {"valid_samples", lastResult_.statistics.validSamples},
        {"invalid_samples", lastResult_.statistics.invalidSamples}
    };

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    return true;
}

void ResultsManager::clear() {
    latencySamples_.clear();
    currentTest_ = TestResult();
    testRunning_ = false;
}

std::string ResultsManager::generateTestId() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

} // namespace latency
