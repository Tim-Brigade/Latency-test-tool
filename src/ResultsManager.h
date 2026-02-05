#pragma once

#include "LatencyMeasurer.h"
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace latency {

struct LatencyStatistics {
    int32_t minMs = 0;
    int32_t maxMs = 0;
    double avgMs = 0.0;
    double stdDevMs = 0.0;
    int32_t p50Ms = 0;   // Median
    int32_t p95Ms = 0;
    int32_t p99Ms = 0;
    int validSamples = 0;
    int invalidSamples = 0;
};

struct TestResult {
    std::string testId;
    std::string streamUrl;
    std::string codec;
    int resolutionWidth = 0;
    int resolutionHeight = 0;
    int testDurationSec = 0;
    int framesAnalyzed = 0;
    LatencyStatistics statistics;
};

class ResultsManager {
public:
    ResultsManager();
    ~ResultsManager() = default;

    // Start a new test
    void startTest(const std::string& streamUrl, const std::string& codec,
                   int width, int height);

    // Add a measurement
    void addMeasurement(const LatencyMeasurement& measurement);

    // End the test and compute statistics
    TestResult endTest();

    // Export results to JSON file
    bool exportToJson(const std::string& filename) const;

    // Get current statistics (live update during test)
    LatencyStatistics getCurrentStatistics() const;

    // Get latest test result
    const TestResult& getLastResult() const { return lastResult_; }

    // Clear all data
    void clear();

private:
    LatencyStatistics computeStatistics() const;
    static std::string generateTestId();

    std::vector<int32_t> latencySamples_;
    TestResult currentTest_;
    TestResult lastResult_;
    bool testRunning_ = false;
};

} // namespace latency
