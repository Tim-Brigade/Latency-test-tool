#include "TimestampDisplay.h"
#include <sstream>
#include <iomanip>

namespace latency {

TimestampDisplay::TimestampDisplay() = default;

TimestampDisplay::~TimestampDisplay() {
    if (font_) {
        TTF_CloseFont(font_);
    }
    if (largeFont_) {
        TTF_CloseFont(largeFont_);
    }
}

bool TimestampDisplay::init(SDL_Renderer* renderer, const std::string& fontPath, int fontSize) {
    renderer_ = renderer;

    // Load regular font
    font_ = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!font_) {
        const char* fallbackPaths[] = {
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/cour.ttf",
            "C:/Windows/Fonts/arial.ttf"
        };
        for (const auto& path : fallbackPaths) {
            font_ = TTF_OpenFont(path, fontSize);
            if (font_) break;
        }
    }

    // Load large font for the main clock display
    largeFont_ = TTF_OpenFont(fontPath.c_str(), fontSize * 2);
    if (!largeFont_) {
        const char* fallbackPaths[] = {
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/cour.ttf",
            "C:/Windows/Fonts/arial.ttf"
        };
        for (const auto& path : fallbackPaths) {
            largeFont_ = TTF_OpenFont(path, fontSize * 2);
            if (largeFont_) break;
        }
    }

    return font_ != nullptr;
}

void TimestampDisplay::startTest() {
    testStartTime_ = std::chrono::steady_clock::now();
    running_ = true;
}

void TimestampDisplay::stopTest() {
    running_ = false;
}

uint32_t TimestampDisplay::getCurrentTimestamp() const {
    if (!running_) return 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - testStartTime_);
    return static_cast<uint32_t>(elapsed.count());
}

void TimestampDisplay::render(int x, int y, int width, int height) {
    // Background
    SDL_SetRenderDrawColor(renderer_, 10, 10, 15, 255);
    SDL_Rect bgRect = {x, y, width, height};
    SDL_RenderFillRect(renderer_, &bgRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 50, 50, 70, 255);
    SDL_RenderDrawRect(renderer_, &bgRect);

    uint32_t timestamp = getCurrentTimestamp();
    int centerX = x + width / 2;

    // Title at top
    if (font_) {
        SDL_Color titleColor = running_ ? SDL_Color{100, 255, 150, 255} : SDL_Color{150, 150, 150, 255};
        const char* title = running_ ? "CLOCK RUNNING" : "PRESS [T] TO START";
        SDL_Surface* surface = TTF_RenderText_Blended(font_, title, titleColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect dstRect = {centerX - surface->w / 2, y + 15, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Clock display - centered vertically
    int clockY = y + height / 2 - 60;
    renderLargeClock(centerX, clockY, timestamp);
    renderMilliseconds(centerX, clockY + 70, timestamp);

    // Instructions at bottom
    if (font_) {
        SDL_Color cyan = {80, 180, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font_, "Point camera here", cyan);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect dstRect = {centerX - surface->w / 2, y + height - 55, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        SDL_Color yellow = {255, 220, 80, 255};
        surface = TTF_RenderText_Blended(font_, "[SPACE] freeze", yellow);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect dstRect = {centerX - surface->w / 2, y + height - 30, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void TimestampDisplay::renderLargeClock(int centerX, int y, uint32_t timestamp) {
    if (!largeFont_) return;

    // Format: MM:SS
    uint32_t totalSec = timestamp / 1000;
    uint32_t sec = totalSec % 60;
    uint32_t min = (totalSec / 60) % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << min << ":" << std::setw(2) << sec;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, oss.str().c_str(), white);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture) {
        SDL_Rect dstRect = {
            centerX - surface->w / 2,
            y,
            surface->w,
            surface->h
        };
        SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void TimestampDisplay::renderMilliseconds(int centerX, int y, uint32_t timestamp) {
    if (!largeFont_) return;

    // Show milliseconds prominently
    uint32_t ms = timestamp % 1000;

    std::ostringstream oss;
    oss << "." << std::setfill('0') << std::setw(3) << ms;

    // Bright green for visibility
    SDL_Color green = {0, 255, 100, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, oss.str().c_str(), green);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture) {
        SDL_Rect dstRect = {
            centerX - surface->w / 2,
            y,
            surface->w,
            surface->h
        };
        SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

} // namespace latency
