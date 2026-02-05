#pragma once

#include <string>

namespace latency {

enum class TransportProtocol {
    TCP,
    UDP
};

struct StreamConfig {
    std::string url;
    TransportProtocol transport = TransportProtocol::TCP;
    int connectionTimeoutMs = 5000;
    int receiveTimeoutMs = 2000;
};

struct TestConfig {
    int testDurationSec = 30;
    int warmupFrames = 30;  // Skip first N frames for decoder warmup
    bool autoDetectPatternRegion = true;
    int patternX = 0;       // Manual pattern region (if not auto-detect)
    int patternY = 0;
    int patternWidth = 0;
    int patternHeight = 0;
};

struct AppConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    int timestampPanelWidth = 400;
    std::string fontPath = "resources/fonts/RobotoMono-Bold.ttf";
    int fontSize = 48;
};

} // namespace latency
