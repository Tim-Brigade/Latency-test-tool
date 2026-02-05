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

    // Allocate format context
    formatCtx_ = avformat_alloc_context();
    if (!formatCtx_) {
        lastError_ = "Failed to allocate format context";
        return false;
    }

    // Set low-latency options
    AVDictionary* options = nullptr;

    // Transport protocol
    if (config.transport == TransportProtocol::TCP) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
    } else {
        av_dict_set(&options, "rtsp_transport", "udp", 0);
    }

    // Low-latency flags
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);
    av_dict_set(&options, "max_delay", "0", 0);
    av_dict_set(&options, "probesize", "32768", 0);  // Small probe size for faster startup
    av_dict_set(&options, "analyzeduration", "0", 0);

    // Timeouts
    av_dict_set(&options, "stimeout", std::to_string(config.connectionTimeoutMs * 1000).c_str(), 0);
    av_dict_set(&options, "timeout", std::to_string(config.receiveTimeoutMs * 1000).c_str(), 0);

    // Open input
    int ret = avformat_open_input(&formatCtx_, config.url.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        lastError_ = "Failed to open stream: " + std::string(errBuf);
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
        return false;
    }

    // Find stream info (with short timeout for low latency)
    formatCtx_->max_analyze_duration = 500000;  // 0.5 seconds
    ret = avformat_find_stream_info(formatCtx_, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        lastError_ = "Failed to find stream info: " + std::string(errBuf);
        avformat_close_input(&formatCtx_);
        return false;
    }

    // Find video stream
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ < 0) {
        lastError_ = "No video stream found";
        avformat_close_input(&formatCtx_);
        return false;
    }

    // Open codec
    if (!openCodec()) {
        avformat_close_input(&formatCtx_);
        return false;
    }

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

void VideoDecoder::disconnect() {
    running_ = false;

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

        // Skip non-video packets
        if (packet->stream_index != videoStreamIndex_) {
            av_packet_unref(packet);
            continue;
        }

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

            // Convert frame to RGB
            auto videoFrame = convertFrame(frame);
            if (videoFrame) {
                std::unique_lock<std::mutex> lock(queueMutex_);

                // Wait if queue is full (discard old frames for low latency)
                while (frameQueue_.size() >= MAX_QUEUE_SIZE && running_) {
                    frameQueue_.pop();  // Drop oldest frame
                }

                frameQueue_.push(std::move(videoFrame));
                lock.unlock();
                queueCv_.notify_one();
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

} // namespace latency
