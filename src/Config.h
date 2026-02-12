#pragma once

#include <string>
#include <vector>

namespace latency {

enum class TransportProtocol {
    AUTO,   // Try TCP first, fallback to UDP
    TCP,
    UDP
};

enum class StreamProtocol {
    AUTO,   // Auto-detect from URL scheme
    RTSP,   // rtsp:// - Real Time Streaming Protocol
    RTP     // rtp:// - Real-time Transport Protocol (direct)
};

enum class ConnectionStage {
    NotStarted,
    OpeningInput,       // avformat_open_input
    FindingStreamInfo,  // avformat_find_stream_info
    FindingVideoStream, // Scanning for video stream
    OpeningCodec,       // avcodec_open2
    Connected           // Success
};

struct ConnectionAttempt {
    TransportProtocol transport = TransportProtocol::TCP;
    ConnectionStage failedAt = ConnectionStage::NotStarted;
    int ffmpegErrorCode = 0;
    std::string ffmpegErrorString;
};

struct ConnectionDiagnostics {
    std::string url;
    StreamProtocol detectedProtocol = StreamProtocol::AUTO;
    std::vector<ConnectionAttempt> attempts;
    bool succeeded = false;
    std::vector<std::string> suggestions;
    std::string summary;
};

struct StreamConfig {
    std::string url;
    StreamProtocol protocol = StreamProtocol::AUTO;
    TransportProtocol transport = TransportProtocol::AUTO;
    int connectionTimeoutMs = 10000;
    int receiveTimeoutMs = 5000;
    int probeSize = 524288;          // 512KB (reasonable for most cameras)
    int analyzeDurationUs = 2000000; // 2 seconds
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
