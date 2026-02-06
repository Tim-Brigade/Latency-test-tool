#pragma once

#include "Config.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <chrono>

// Forward declarations for FFmpeg types
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace latency {

struct VideoFrame {
    uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int pitch = 0;  // Bytes per row
    int64_t timestamp = 0;  // Presentation timestamp

    ~VideoFrame() {
        delete[] data;
    }
};

struct StreamInfo {
    std::string codecName;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrate = 0;
};

struct DecodeStats {
    // Decoding method
    std::string decoderName;           // Full decoder name (e.g., "h264", "h264_cuvid")
    bool isHardwareAccelerated = false;
    std::string hwAccelType;           // "None", "CUDA", "DXVA2", "D3D11VA", etc.

    // Timing stats (in microseconds)
    double avgDecodeTimeUs = 0.0;
    double minDecodeTimeUs = 0.0;
    double maxDecodeTimeUs = 0.0;
    double lastDecodeTimeUs = 0.0;

    // Frame stats
    uint64_t framesDecoded = 0;
    uint64_t framesDropped = 0;
    double actualFps = 0.0;            // Measured output FPS

    // Queue stats
    size_t queueDepth = 0;
    size_t maxQueueSize = 0;

    // Network/demux stats
    double avgDemuxTimeUs = 0.0;
    double avgConvertTimeUs = 0.0;     // RGB conversion time
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    // Connect to RTSP/RTP stream
    bool connect(const StreamConfig& config);
    void disconnect();
    bool isConnected() const { return connected_; }

    // Pause/resume decoding (stops frame processing without disconnecting)
    void setPaused(bool paused);
    bool isPaused() const { return paused_; }

    // Get next decoded frame (returns nullptr if none available)
    std::unique_ptr<VideoFrame> getFrame();

    // Get stream information
    const StreamInfo& getStreamInfo() const { return streamInfo_; }
    std::string getLastError() const { return lastError_; }

    // Get decode statistics (thread-safe copy)
    DecodeStats getDecodeStats() const;

    // Get detected stream protocol
    StreamProtocol getDetectedProtocol() const { return detectedProtocol_; }

private:
    // Detect protocol from URL scheme
    StreamProtocol detectProtocol(const std::string& url) const;
    void decodeThread();
    bool openCodec();
    std::unique_ptr<VideoFrame> convertFrame(AVFrame* frame);

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    int videoStreamIndex_ = -1;

    StreamInfo streamInfo_;
    std::string lastError_;
    StreamProtocol detectedProtocol_ = StreamProtocol::AUTO;

    std::thread decodeThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> paused_{false};

    std::queue<std::unique_ptr<VideoFrame>> frameQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    static constexpr size_t MAX_QUEUE_SIZE = 4;

    // Decode statistics
    DecodeStats decodeStats_;
    mutable std::mutex statsMutex_;
    std::chrono::steady_clock::time_point statsStartTime_;
    double totalDecodeTimeUs_ = 0.0;
    double totalDemuxTimeUs_ = 0.0;
    double totalConvertTimeUs_ = 0.0;
};

} // namespace latency
