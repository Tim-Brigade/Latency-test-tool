#include "VideoDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace latency {

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder() {
    disconnect();
}

bool VideoDecoder::connect(const StreamConfig& config) {
    if (connected_) {
        disconnect();
    }

    lastError_.clear();
    diagnostics_ = ConnectionDiagnostics{};
    diagnostics_.url = config.url;

    // Detect protocol from URL if set to AUTO
    if (config.protocol == StreamProtocol::AUTO) {
        detectedProtocol_ = detectProtocol(config.url);
    } else {
        detectedProtocol_ = config.protocol;
    }
    diagnostics_.detectedProtocol = detectedProtocol_;

    // Build list of transports to try
    std::vector<TransportProtocol> transportsToTry;
    if (detectedProtocol_ == StreamProtocol::RTSP) {
        if (config.transport == TransportProtocol::AUTO) {
            transportsToTry = { TransportProtocol::TCP, TransportProtocol::UDP };
        } else {
            transportsToTry = { config.transport };
        }
    } else {
        // RTP: transport selection not applicable
        transportsToTry = { TransportProtocol::UDP };
    }

    // Try each transport
    for (auto transport : transportsToTry) {
        ConnectionAttempt attempt;
        attempt.transport = transport;

        if (tryConnect(config, transport, attempt)) {
            attempt.failedAt = ConnectionStage::Connected;
            diagnostics_.attempts.push_back(attempt);
            diagnostics_.succeeded = true;

            // Fill stream info
            AVStream* stream = formatCtx_->streams[videoStreamIndex_];
            streamInfo_.codecName = avcodec_get_name(codecCtx_->codec_id);
            streamInfo_.width = codecCtx_->width;
            streamInfo_.height = codecCtx_->height;
            streamInfo_.bitrate = static_cast<int>(formatCtx_->bit_rate);

            if (stream->avg_frame_rate.den > 0) {
                streamInfo_.fps = static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;
            }

            // Start decode thread
            connected_ = true;
            running_ = true;
            decodeThread_ = std::thread(&VideoDecoder::decodeThread, this);
            return true;
        }

        diagnostics_.attempts.push_back(attempt);
        cleanupConnection();
    }

    // All attempts failed
    buildDiagnosticSuggestions();
    return false;
}

bool VideoDecoder::tryConnect(const StreamConfig& config, TransportProtocol transport,
                               ConnectionAttempt& attempt) {
    // Allocate format context
    formatCtx_ = avformat_alloc_context();
    if (!formatCtx_) {
        attempt.failedAt = ConnectionStage::OpeningInput;
        attempt.ffmpegErrorString = "Failed to allocate format context";
        lastError_ = attempt.ffmpegErrorString;
        return false;
    }

    // Set protocol-specific and low-latency options
    AVDictionary* options = nullptr;

    if (detectedProtocol_ == StreamProtocol::RTSP) {
        av_dict_set(&options, "rtsp_transport",
                    transport == TransportProtocol::TCP ? "tcp" : "udp", 0);
        av_dict_set(&options, "stimeout",
                    std::to_string(config.connectionTimeoutMs * 1000).c_str(), 0);
    } else if (detectedProtocol_ == StreamProtocol::RTP) {
        av_dict_set(&options, "reorder_queue_size", "500", 0);
    }

    // Common low-latency flags
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    av_dict_set(&options, "max_delay", "0", 0);

    // Use config-driven probe size and analyze duration
    av_dict_set(&options, "probesize", std::to_string(config.probeSize).c_str(), 0);
    av_dict_set(&options, "analyzeduration", std::to_string(config.analyzeDurationUs).c_str(), 0);

    // Receive timeout
    av_dict_set(&options, "timeout", std::to_string(config.receiveTimeoutMs * 1000).c_str(), 0);

    // Stage: Opening input
    attempt.failedAt = ConnectionStage::OpeningInput;
    int ret = avformat_open_input(&formatCtx_, config.url.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        attempt.ffmpegErrorCode = ret;
        attempt.ffmpegErrorString = errBuf;
        lastError_ = "Failed to open stream: " + std::string(errBuf);
        formatCtx_ = nullptr;
        return false;
    }

    // Stage: Finding stream info
    attempt.failedAt = ConnectionStage::FindingStreamInfo;
    formatCtx_->max_analyze_duration = config.analyzeDurationUs;
    ret = avformat_find_stream_info(formatCtx_, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        attempt.ffmpegErrorCode = ret;
        attempt.ffmpegErrorString = errBuf;
        lastError_ = "Failed to find stream info: " + std::string(errBuf);
        return false;
    }

    // Stage: Finding video stream
    attempt.failedAt = ConnectionStage::FindingVideoStream;
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ < 0) {
        attempt.ffmpegErrorString = "No video stream found in container";
        lastError_ = "No video stream found";
        return false;
    }

    // Stage: Opening codec
    attempt.failedAt = ConnectionStage::OpeningCodec;
    if (!openCodec()) {
        attempt.ffmpegErrorString = lastError_;
        return false;
    }

    return true;
}

void VideoDecoder::cleanupConnection() {
    if (swsCtx_) { sws_freeContext(swsCtx_); swsCtx_ = nullptr; }
    if (codecCtx_) { avcodec_free_context(&codecCtx_); codecCtx_ = nullptr; }
    if (formatCtx_) { avformat_close_input(&formatCtx_); formatCtx_ = nullptr; }
    videoStreamIndex_ = -1;
}

void VideoDecoder::buildDiagnosticSuggestions() {
    diagnostics_.suggestions.clear();

    if (diagnostics_.attempts.empty()) return;

    const auto& lastAttempt = diagnostics_.attempts.back();

    // Stage-based summary
    switch (lastAttempt.failedAt) {
        case ConnectionStage::OpeningInput:
            diagnostics_.summary = "Could not connect to the stream URL.";
            break;
        case ConnectionStage::FindingStreamInfo:
            diagnostics_.summary = "Connected but could not detect stream format.";
            break;
        case ConnectionStage::FindingVideoStream:
            diagnostics_.summary = "Stream opened but contains no video track.";
            break;
        case ConnectionStage::OpeningCodec:
            diagnostics_.summary = "Video found but the codec could not be initialized.";
            break;
        default:
            diagnostics_.summary = "Connection failed.";
    }

    // Analyze error patterns across all attempts
    bool hadTimeout = false;
    bool hadConnectionRefused = false;
    bool hadAuth = false;
    bool triedTcp = false, triedUdp = false;

    for (const auto& att : diagnostics_.attempts) {
        if (att.transport == TransportProtocol::TCP) triedTcp = true;
        if (att.transport == TransportProtocol::UDP) triedUdp = true;

        const auto& err = att.ffmpegErrorString;
        if (err.find("Connection refused") != std::string::npos ||
            err.find("connection refused") != std::string::npos) {
            hadConnectionRefused = true;
        }
        if (err.find("timed out") != std::string::npos ||
            err.find("Timeout") != std::string::npos ||
            err.find("timeout") != std::string::npos) {
            hadTimeout = true;
        }
        if (err.find("401") != std::string::npos ||
            err.find("Unauthorized") != std::string::npos) {
            hadAuth = true;
        }
    }

    // Generate actionable suggestions
    if (hadConnectionRefused) {
        diagnostics_.suggestions.push_back("Check that the camera is powered on and reachable on the network.");
        diagnostics_.suggestions.push_back("Verify the IP address and port number in the URL.");
    }
    if (hadTimeout) {
        diagnostics_.suggestions.push_back("The camera did not respond in time.");
        diagnostics_.suggestions.push_back("Check firewall settings and network connectivity.");
    }
    if (hadAuth) {
        diagnostics_.suggestions.push_back("The camera requires authentication. Include credentials in the URL:");
        diagnostics_.suggestions.push_back("  rtsp://username:password@<ip>:<port>/path");
    }
    if (lastAttempt.failedAt == ConnectionStage::FindingStreamInfo) {
        diagnostics_.suggestions.push_back("The camera may use a format that needs longer analysis time.");
        diagnostics_.suggestions.push_back("Verify the stream works in VLC media player first.");
    }
    if (lastAttempt.failedAt == ConnectionStage::OpeningCodec) {
        diagnostics_.suggestions.push_back("The video codec may not be supported by this build of FFmpeg.");
    }
    if (!triedTcp || !triedUdp) {
        std::string untried = !triedUdp ? "UDP" : "TCP";
        diagnostics_.suggestions.push_back("Try switching transport to " + untried + " using the [P] key.");
    }
    if (diagnostics_.url.find("rtsp://") != 0 && diagnostics_.url.find("rtp://") != 0 &&
        diagnostics_.url.find("rtsps://") != 0) {
        diagnostics_.suggestions.push_back("URL does not start with rtsp:// or rtp:// - check the URL format.");
    }
    diagnostics_.suggestions.push_back("Verify the stream path (common paths: /stream, /live, /Streaming/Channels/1).");
}

bool VideoDecoder::openCodec() {
    AVStream* stream = formatCtx_->streams[videoStreamIndex_];

    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        lastError_ = "Unsupported codec";
        return false;
    }

    // Allocate codec context
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        lastError_ = "Failed to allocate codec context";
        return false;
    }

    // Copy codec parameters
    if (avcodec_parameters_to_context(codecCtx_, stream->codecpar) < 0) {
        lastError_ = "Failed to copy codec parameters";
        avcodec_free_context(&codecCtx_);
        return false;
    }

    // Low-latency decoding options
    codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx_->flags2 |= AV_CODEC_FLAG2_FAST;
    codecCtx_->thread_count = 2;  // Limit threads for lower latency

    // Open codec
    AVDictionary* codecOpts = nullptr;
    av_dict_set(&codecOpts, "threads", "2", 0);

    int ret = avcodec_open2(codecCtx_, codec, &codecOpts);
    av_dict_free(&codecOpts);

    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        lastError_ = "Failed to open codec: " + std::string(errBuf);
        avcodec_free_context(&codecCtx_);
        return false;
    }

    // Initialize decode statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        decodeStats_ = DecodeStats{};
        decodeStats_.decoderName = codec->name;
        decodeStats_.maxQueueSize = MAX_QUEUE_SIZE;

        // Detect hardware acceleration type
        decodeStats_.isHardwareAccelerated = false;
        decodeStats_.hwAccelType = "Software";

        // Check if this is a hardware decoder by name convention
        std::string codecName = codec->name;
        if (codecName.find("cuvid") != std::string::npos ||
            codecName.find("nvdec") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "NVIDIA CUDA/NVDEC";
        } else if (codecName.find("qsv") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "Intel QuickSync";
        } else if (codecName.find("d3d11va") != std::string::npos ||
                   codecName.find("dxva2") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "DirectX VA";
        } else if (codecName.find("vaapi") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "VA-API";
        } else if (codecName.find("videotoolbox") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "VideoToolbox";
        } else if (codecName.find("amf") != std::string::npos) {
            decodeStats_.isHardwareAccelerated = true;
            decodeStats_.hwAccelType = "AMD AMF";
        }

        // Reset timing accumulators
        totalDecodeTimeUs_ = 0.0;
        totalDemuxTimeUs_ = 0.0;
        totalConvertTimeUs_ = 0.0;
        statsStartTime_ = std::chrono::steady_clock::now();
    }

    // Initialize scaler for RGB conversion
    swsCtx_ = sws_getContext(
        codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt,
        codecCtx_->width, codecCtx_->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx_) {
        lastError_ = "Failed to initialize scaler";
        avcodec_free_context(&codecCtx_);
        return false;
    }

    return true;
}

void VideoDecoder::setPaused(bool paused) {
    paused_ = paused;
}

void VideoDecoder::disconnect() {
    running_ = false;
    paused_ = false;

    if (decodeThread_.joinable()) {
        queueCv_.notify_all();
        decodeThread_.join();
    }

    connected_ = false;

    // Clear frame queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!frameQueue_.empty()) {
            frameQueue_.pop();
        }
    }

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }

    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = nullptr;
    }

    videoStreamIndex_ = -1;
}

void VideoDecoder::decodeThread() {
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    if (!packet || !frame) {
        running_ = false;
        if (packet) av_packet_free(&packet);
        if (frame) av_frame_free(&frame);
        return;
    }

    while (running_) {
        // If paused, sleep and skip processing
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Measure demux time
        auto demuxStart = std::chrono::steady_clock::now();

        // Read packet
        int ret = av_read_frame(formatCtx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            // Error or disconnection
            break;
        }

        auto demuxEnd = std::chrono::steady_clock::now();
        double demuxTimeUs = std::chrono::duration<double, std::micro>(demuxEnd - demuxStart).count();

        // Skip non-video packets
        if (packet->stream_index != videoStreamIndex_) {
            av_packet_unref(packet);
            continue;
        }

        // Measure decode time
        auto decodeStart = std::chrono::steady_clock::now();

        // Send packet to decoder
        ret = avcodec_send_packet(codecCtx_, packet);
        av_packet_unref(packet);

        if (ret < 0) {
            continue;
        }

        // Receive decoded frames
        while (ret >= 0 && running_) {
            ret = avcodec_receive_frame(codecCtx_, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                break;
            }

            auto decodeEnd = std::chrono::steady_clock::now();
            double decodeTimeUs = std::chrono::duration<double, std::micro>(decodeEnd - decodeStart).count();

            // Measure RGB conversion time
            auto convertStart = std::chrono::steady_clock::now();

            // Convert frame to RGB
            auto videoFrame = convertFrame(frame);

            auto convertEnd = std::chrono::steady_clock::now();
            double convertTimeUs = std::chrono::duration<double, std::micro>(convertEnd - convertStart).count();

            if (videoFrame) {
                std::unique_lock<std::mutex> lock(queueMutex_);

                bool dropped = false;
                // Wait if queue is full (discard old frames for low latency)
                while (frameQueue_.size() >= MAX_QUEUE_SIZE && running_) {
                    frameQueue_.pop();  // Drop oldest frame
                    dropped = true;
                }

                size_t currentQueueDepth = frameQueue_.size();
                frameQueue_.push(std::move(videoFrame));
                lock.unlock();
                queueCv_.notify_one();

                // Update statistics
                {
                    std::lock_guard<std::mutex> statsLock(statsMutex_);
                    decodeStats_.framesDecoded++;
                    if (dropped) {
                        decodeStats_.framesDropped++;
                    }

                    // Update timing stats
                    totalDecodeTimeUs_ += decodeTimeUs;
                    totalDemuxTimeUs_ += demuxTimeUs;
                    totalConvertTimeUs_ += convertTimeUs;

                    decodeStats_.lastDecodeTimeUs = decodeTimeUs;
                    decodeStats_.avgDecodeTimeUs = totalDecodeTimeUs_ / decodeStats_.framesDecoded;
                    decodeStats_.avgDemuxTimeUs = totalDemuxTimeUs_ / decodeStats_.framesDecoded;
                    decodeStats_.avgConvertTimeUs = totalConvertTimeUs_ / decodeStats_.framesDecoded;

                    // Track min/max
                    if (decodeStats_.framesDecoded == 1) {
                        decodeStats_.minDecodeTimeUs = decodeTimeUs;
                        decodeStats_.maxDecodeTimeUs = decodeTimeUs;
                    } else {
                        if (decodeTimeUs < decodeStats_.minDecodeTimeUs) {
                            decodeStats_.minDecodeTimeUs = decodeTimeUs;
                        }
                        if (decodeTimeUs > decodeStats_.maxDecodeTimeUs) {
                            decodeStats_.maxDecodeTimeUs = decodeTimeUs;
                        }
                    }

                    // Calculate actual FPS
                    auto now = std::chrono::steady_clock::now();
                    double elapsedSec = std::chrono::duration<double>(now - statsStartTime_).count();
                    if (elapsedSec > 0.1) {  // Avoid division by zero / early jitter
                        decodeStats_.actualFps = decodeStats_.framesDecoded / elapsedSec;
                    }

                    decodeStats_.queueDepth = currentQueueDepth + 1;  // +1 for the frame we just added
                }
            }

            av_frame_unref(frame);
        }
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
}

std::unique_ptr<VideoFrame> VideoDecoder::convertFrame(AVFrame* frame) {
    auto videoFrame = std::make_unique<VideoFrame>();
    videoFrame->width = frame->width;
    videoFrame->height = frame->height;
    videoFrame->pitch = frame->width * 3;  // RGB24
    videoFrame->timestamp = frame->pts;
    videoFrame->data = new uint8_t[videoFrame->pitch * videoFrame->height];

    uint8_t* dstData[1] = { videoFrame->data };
    int dstLinesize[1] = { videoFrame->pitch };

    sws_scale(swsCtx_,
              frame->data, frame->linesize,
              0, frame->height,
              dstData, dstLinesize);

    return videoFrame;
}

std::unique_ptr<VideoFrame> VideoDecoder::getFrame() {
    std::lock_guard<std::mutex> lock(queueMutex_);

    if (frameQueue_.empty()) {
        return nullptr;
    }

    auto frame = std::move(frameQueue_.front());
    frameQueue_.pop();
    return frame;
}

DecodeStats VideoDecoder::getDecodeStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return decodeStats_;
}

StreamProtocol VideoDecoder::detectProtocol(const std::string& url) const {
    // Check URL scheme prefix
    if (url.find("rtsp://") == 0 || url.find("rtsps://") == 0) {
        return StreamProtocol::RTSP;
    }
    if (url.find("rtp://") == 0) {
        return StreamProtocol::RTP;
    }
    // Default to RTSP for unknown schemes
    return StreamProtocol::RTSP;
}

} // namespace latency
