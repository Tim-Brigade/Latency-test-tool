#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdint>
#include <chrono>
#include <string>

namespace latency {

class TimestampDisplay {
public:
    TimestampDisplay();
    ~TimestampDisplay();

    bool init(SDL_Renderer* renderer, const std::string& fontPath, int fontSize);
    void render(int x, int y, int width, int height);

    // Get current timestamp (milliseconds since test start)
    uint32_t getCurrentTimestamp() const;

    // Start/reset the timestamp counter
    void startTest();
    void stopTest();
    bool isRunning() const { return running_; }

private:
    void renderLargeClock(int centerX, int y, uint32_t timestamp);
    void renderMilliseconds(int centerX, int y, uint32_t timestamp);

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    TTF_Font* largeFont_ = nullptr;

    std::chrono::steady_clock::time_point testStartTime_;
    bool running_ = false;
};

} // namespace latency
