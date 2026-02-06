#pragma once

#include <string>

namespace latency {

enum class TransportProtocol {
    TCP,
    UDP
};

enum class StreamProtocol {
    AUTO,   // Auto-detect from URL scheme
    RTSP,   // rtsp:// - Real Time Streaming Protocol
    RTP     // rtp:// - Real-time Transport Protocol (direct)
};

struct StreamConfig {
    std::string url;
    StreamProtocol protocol = StreamProtocol::AUTO;  // Auto-detect from URL
    TransportProtocol transport = TransportProtocol::TCP;  // For RTSP: TCP or UDP
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
