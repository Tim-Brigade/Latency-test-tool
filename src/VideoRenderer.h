#pragma once

#include "VideoDecoder.h"
#include <SDL.h>
#include <memory>

namespace latency {

class VideoRenderer {
public:
    VideoRenderer();
    ~VideoRenderer();

    bool init(SDL_Renderer* renderer);
    void render(int x, int y, int width, int height);

    // Update with new frame
    void updateFrame(std::unique_ptr<VideoFrame> frame);

    // Get current frame data for analysis
    const VideoFrame* getCurrentFrame() const { return currentFrame_.get(); }

    // Get video dimensions
    int getVideoWidth() const { return currentFrame_ ? currentFrame_->width : 0; }
    int getVideoHeight() const { return currentFrame_ ? currentFrame_->height : 0; }

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    std::unique_ptr<VideoFrame> currentFrame_;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
};

} // namespace latency
