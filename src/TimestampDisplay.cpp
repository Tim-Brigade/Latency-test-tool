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

void TimestampDisplay::render(int x, int y, int width, int height, bool paused, uint32_t frozenTimestamp) {
    // White background for faster camera shutter
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_Rect bgRect = {x, y, width, height};
    SDL_RenderFillRect(renderer_, &bgRect);

    // Dark border
    SDL_SetRenderDrawColor(renderer_, 60, 60, 60, 255);
    SDL_RenderDrawRect(renderer_, &bgRect);

    // Use frozen timestamp when paused, otherwise current timestamp
    uint32_t timestamp = paused ? frozenTimestamp : getCurrentTimestamp();
    int centerX = x + width / 2;

    // Title at top - dark text on white background
    if (font_) {
        SDL_Color titleColor = running_ ? SDL_Color{0, 120, 60, 255} : SDL_Color{100, 100, 100, 255};
        const char* title = paused ? "PAUSED" : (running_ ? "CLOCK RUNNING" : "PRESS [T] TO START");
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

    // Instructions at bottom - dark text
    if (font_) {
        SDL_Color darkBlue = {0, 80, 150, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font_, "Point camera here", darkBlue);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect dstRect = {centerX - surface->w / 2, y + height - 55, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        SDL_Color darkOrange = {180, 100, 0, 255};
        surface = TTF_RenderText_Blended(font_, "[SPACE] freeze", darkOrange);
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

    // Dark text on white background
    SDL_Color black = {0, 0, 0, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, oss.str().c_str(), black);
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

    // Show centiseconds (10ms resolution) - more readable than full milliseconds
    uint32_t cs = (timestamp % 1000) / 10;

    std::ostringstream oss;
    oss << "." << std::setfill('0') << std::setw(2) << cs;

    // Dark green for visibility on white background
    SDL_Color darkGreen = {0, 100, 50, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, oss.str().c_str(), darkGreen);
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
