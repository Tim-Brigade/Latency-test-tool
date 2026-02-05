#pragma once

#include "Config.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

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

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    // Connect to RTSP/RTP stream
    bool connect(const StreamConfig& config);
    void disconnect();
    bool isConnected() const { return connected_; }

    // Get next decoded frame (returns nullptr if none available)
    std::unique_ptr<VideoFrame> getFrame();

    // Get stream information
    const StreamInfo& getStreamInfo() const { return streamInfo_; }
    std::string getLastError() const { return lastError_; }

private:
    void decodeThread();
    bool openCodec();
    std::unique_ptr<VideoFrame> convertFrame(AVFrame* frame);

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    int videoStreamIndex_ = -1;

    StreamInfo streamInfo_;
    std::string lastError_;

    std::thread decodeThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    std::queue<std::unique_ptr<VideoFrame>> frameQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    static constexpr size_t MAX_QUEUE_SIZE = 4;
};

} // namespace latency
