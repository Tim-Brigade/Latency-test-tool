#include "VideoRenderer.h"

namespace latency {

VideoRenderer::VideoRenderer() = default;

VideoRenderer::~VideoRenderer() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
    }
}

bool VideoRenderer::init(SDL_Renderer* renderer) {
    renderer_ = renderer;
    return renderer_ != nullptr;
}

void VideoRenderer::updateFrame(std::unique_ptr<VideoFrame> frame) {
    if (!frame) return;

    // Recreate texture if dimensions changed
    if (frame->width != textureWidth_ || frame->height != textureHeight_) {
        if (texture_) {
            SDL_DestroyTexture(texture_);
        }

        texture_ = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,
            frame->width,
            frame->height
        );

        textureWidth_ = frame->width;
        textureHeight_ = frame->height;
    }

    // Update texture with frame data
    if (texture_) {
        SDL_UpdateTexture(texture_, nullptr, frame->data, frame->pitch);
    }

    currentFrame_ = std::move(frame);
}

void VideoRenderer::render(int x, int y, int width, int height) {
    // Background
    SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 255);
    SDL_Rect bgRect = {x, y, width, height};
    SDL_RenderFillRect(renderer_, &bgRect);

    if (!texture_ || !currentFrame_) {
        // No video - show placeholder
        SDL_SetRenderDrawColor(renderer_, 60, 60, 60, 255);
        SDL_Rect placeholder = {
            x + width / 4,
            y + height / 4,
            width / 2,
            height / 2
        };
        SDL_RenderFillRect(renderer_, &placeholder);
        return;
    }

    // Calculate aspect-ratio-preserving destination rect
    float videoAspect = static_cast<float>(currentFrame_->width) / currentFrame_->height;
    float areaAspect = static_cast<float>(width) / height;

    SDL_Rect dstRect;
    if (videoAspect > areaAspect) {
        // Video is wider - fit to width
        dstRect.w = width;
        dstRect.h = static_cast<int>(width / videoAspect);
        dstRect.x = x;
        dstRect.y = y + (height - dstRect.h) / 2;
    } else {
        // Video is taller - fit to height
        dstRect.h = height;
        dstRect.w = static_cast<int>(height * videoAspect);
        dstRect.x = x + (width - dstRect.w) / 2;
        dstRect.y = y;
    }

    SDL_RenderCopy(renderer_, texture_, nullptr, &dstRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer_, &bgRect);
}

} // namespace latency
