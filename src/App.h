#pragma once

#include "Config.h"
#include "TimestampDisplay.h"
#include "VideoDecoder.h"
#include "VideoRenderer.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <memory>

namespace latency {

enum class AppState {
    Disconnected,
    Connecting,
    Connected,
    Running  // Clock is running, measuring
};

class App {
public:
    App();
    ~App();

    bool init(const AppConfig& config);
    void run();
    void shutdown();

private:
    // Event handling
    void handleEvents();
    void handleKeyDown(SDL_Keycode key);
    void handleTextInput(const char* text);

    // Rendering
    void render();
    void renderUI();
    void renderStatusBar();
    void renderPauseOverlay();
    void renderStatsPanel();
    void renderInputField(int x, int y, int width, int height,
                          const std::string& label, const std::string& value, bool active);
    void renderButton(int x, int y, int width, int height,
                      const std::string& text, bool enabled);

    // Actions
    void connect();
    void disconnect();
    void startClock();
    void stopClock();
    void togglePause();
    void saveScreenshot();

    // Text rendering helper
    void renderText(const std::string& text, int x, int y, SDL_Color color);
    void renderTextCentered(const std::string& text, int centerX, int y, SDL_Color color);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    TTF_Font* smallFont_ = nullptr;
    TTF_Font* largeFont_ = nullptr;

    AppConfig config_;
    StreamConfig streamConfig_;

    std::unique_ptr<TimestampDisplay> timestampDisplay_;
    std::unique_ptr<VideoDecoder> videoDecoder_;
    std::unique_ptr<VideoRenderer> videoRenderer_;

    AppState state_ = AppState::Disconnected;
    bool appRunning_ = false;

    // UI state
    std::string urlInput_;
    bool urlInputActive_ = false;  // Start inactive - press U to edit

    // Pause state
    bool paused_ = false;
    uint32_t pausedTimestamp_ = 0;  // Clock time when paused
};

} // namespace latency
