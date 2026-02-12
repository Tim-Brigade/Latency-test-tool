#include "App.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace latency {

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init(const AppConfig& config) {
    config_ = config;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow(
        "Video Latency Test Tool",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config_.windowWidth,
        config_.windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Load fonts
    const char* fontPaths[] = {
        config_.fontPath.c_str(),
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };

    for (const auto& path : fontPaths) {
        if (!smallFont_) smallFont_ = TTF_OpenFont(path, 18);
        if (!font_) font_ = TTF_OpenFont(path, 24);
        if (!largeFont_) largeFont_ = TTF_OpenFont(path, 72);
        if (smallFont_ && font_ && largeFont_) break;
    }

    // Initialize components
    timestampDisplay_ = std::make_unique<TimestampDisplay>();
    timestampDisplay_->init(renderer_, config_.fontPath, config_.fontSize);

    videoDecoder_ = std::make_unique<VideoDecoder>();
    videoRenderer_ = std::make_unique<VideoRenderer>();
    videoRenderer_->init(renderer_);

    // Load connection history
    historyFilePath_ = "connection_history.txt";
    loadConnectionHistory();

    // Set default URL or use most recent from history
    if (!connectionHistory_.empty()) {
        urlInput_ = connectionHistory_[0];
    } else {
        urlInput_ = "rtsp://192.168.1.100:554/stream";
    }

    // Disable text input by default - only enable when editing URL
    SDL_StopTextInput();

    return true;
}

void App::run() {
    appRunning_ = true;

    while (appRunning_) {
        handleEvents();

        // Process video frames (unless paused)
        if (videoDecoder_->isConnected() && !paused_) {
            auto frame = videoDecoder_->getFrame();
            if (frame) {
                videoRenderer_->updateFrame(std::move(frame));
            }
        }

        render();
        SDL_Delay(1);
    }
}

void App::shutdown() {
    disconnect();

    timestampDisplay_.reset();
    videoDecoder_.reset();
    videoRenderer_.reset();

    if (largeFont_) { TTF_CloseFont(largeFont_); largeFont_ = nullptr; }
    if (font_) { TTF_CloseFont(font_); font_ = nullptr; }
    if (smallFont_) { TTF_CloseFont(smallFont_); smallFont_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }

    TTF_Quit();
    SDL_Quit();
}

void App::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                appRunning_ = false;
                break;
            case SDL_KEYDOWN:
                handleKeyDown(event.key.keysym.sym);
                break;
            case SDL_TEXTINPUT:
                handleTextInput(event.text.text);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    config_.windowWidth = event.window.data1;
                    config_.windowHeight = event.window.data2;
                }
                break;
        }
    }
}

void App::handleKeyDown(SDL_Keycode key) {
    if (urlInputActive_) {
        switch (key) {
            case SDLK_BACKSPACE:
                if (!urlInput_.empty()) urlInput_.pop_back();
                break;
            case SDLK_RETURN:
                urlInputActive_ = false;
                SDL_StopTextInput();
                if (state_ == AppState::Disconnected) connect();
                break;
            case SDLK_ESCAPE:
                urlInputActive_ = false;
                SDL_StopTextInput();
                break;
            default:
                break;
        }
        return;
    }

    // Diagnostics panel intercepts keys
    if (showingDiagnostics_) {
        switch (key) {
            case SDLK_ESCAPE:
                showingDiagnostics_ = false;
                return;
            case SDLK_c:
                showingDiagnostics_ = false;
                connect();
                return;
            case SDLK_p:
                cycleTransportProtocol();
                return;
            case SDLK_u:
                showingDiagnostics_ = false;
                urlInputActive_ = true;
                SDL_StartTextInput();
                return;
            default:
                return;
        }
    }

    switch (key) {
        case SDLK_c:
            if (state_ == AppState::Disconnected) connect();
            break;

        case SDLK_d:
            if (state_ != AppState::Disconnected) disconnect();
            break;

        case SDLK_t:
            if (state_ == AppState::Connected) {
                startClock();
            } else if (state_ == AppState::Running) {
                stopClock();
            }
            break;

        case SDLK_SPACE:
            if (state_ == AppState::Running || state_ == AppState::Connected) {
                togglePause();
            }
            break;

        case SDLK_s:
            saveScreenshot();
            break;

        case SDLK_u:
            urlInputActive_ = true;
            SDL_StartTextInput();
            break;

        case SDLK_p:
            if (state_ == AppState::Disconnected) {
                cycleTransportProtocol();
            }
            break;

        case SDLK_ESCAPE:
            if (showingHelp_ || showingAbout_) {
                showingHelp_ = false;
                showingAbout_ = false;
            } else if (paused_) {
                togglePause();  // Unpause
            } else if (state_ == AppState::Running) {
                stopClock();
            } else {
                appRunning_ = false;
            }
            break;

        case SDLK_F1:
            showingHelp_ = !showingHelp_;
            showingAbout_ = false;
            break;

        case SDLK_F2:
            showingAbout_ = !showingAbout_;
            showingHelp_ = false;
            break;

        // Number keys 1-9 to select from connection history
        case SDLK_1: case SDLK_2: case SDLK_3:
        case SDLK_4: case SDLK_5: case SDLK_6:
        case SDLK_7: case SDLK_8: case SDLK_9:
            if (state_ == AppState::Disconnected && !urlInputActive_) {
                int index = key - SDLK_1;
                selectFromHistory(index);
            }
            break;

        default:
            break;
    }
}

void App::handleTextInput(const char* text) {
    if (urlInputActive_) {
        urlInput_ += text;
    }
}

void App::render() {
    SDL_SetRenderDrawColor(renderer_, 20, 20, 25, 255);
    SDL_RenderClear(renderer_);

    const int padding = 8;
    const int topBarHeight = 75;
    const int bottomBarHeight = 35;
    const int contentHeight = config_.windowHeight - topBarHeight - bottomBarHeight;

    renderUI();

    // Timestamp display (left panel) - pass paused state to freeze the clock
    int timestampWidth = config_.timestampPanelWidth;
    timestampDisplay_->render(
        padding,
        topBarHeight,
        timestampWidth - padding * 2,
        contentHeight,
        paused_,
        pausedTimestamp_
    );

    // Video (right panel)
    int videoX = timestampWidth;
    int videoWidth = config_.windowWidth - videoX - padding;
    videoRenderer_->render(
        videoX,
        topBarHeight,
        videoWidth,
        contentHeight
    );

    // Stats panel (before pause overlay so it's visible when not paused)
    renderStatsPanel();

    // Connection history (when disconnected)
    if (state_ == AppState::Disconnected && !connectionHistory_.empty()) {
        renderConnectionHistory();
    }

    // Pause overlay
    if (paused_) {
        renderPauseOverlay();
    }

    // Help/About panels
    if (showingHelp_) {
        renderHelpPanel();
    }
    if (showingAbout_) {
        renderAboutPanel();
    }
    if (showingDiagnostics_) {
        renderDiagnosticsPanel();
    }

    renderStatusBar();

    SDL_RenderPresent(renderer_);
}

void App::renderUI() {
    const int padding = 10;

    // Top bar background
    SDL_SetRenderDrawColor(renderer_, 30, 30, 35, 255);
    SDL_Rect topBar = {0, 0, config_.windowWidth, 70};
    SDL_RenderFillRect(renderer_, &topBar);

    // URL input - shorter label
    std::string label = urlInputActive_ ? "Type URL, ENTER to connect, ESC to cancel:" : "URL [U to edit]:";
    renderInputField(padding, 8, config_.windowWidth - 300, 28, label, urlInput_, urlInputActive_);

    // Buttons on the right
    int buttonX = config_.windowWidth - 280;
    int buttonWidth = 85;
    int buttonY = 30;

    bool canConnect = state_ == AppState::Disconnected && !urlInputActive_;
    bool canRun = (state_ == AppState::Connected || state_ == AppState::Running) && !urlInputActive_;

    renderButton(buttonX, buttonY, buttonWidth, 28, "[C]onnect", canConnect);
    renderButton(buttonX + buttonWidth + 5, buttonY, buttonWidth, 28,
                 state_ == AppState::Running ? "[T] Stop" : "[T] Start", canRun);
    renderButton(buttonX + (buttonWidth + 5) * 2, buttonY, buttonWidth, 28, "[S]ave", true);

    // Separator line
    SDL_SetRenderDrawColor(renderer_, 60, 60, 70, 255);
    SDL_RenderDrawLine(renderer_, 0, 70, config_.windowWidth, 70);
}

void App::renderInputField(int x, int y, int width, int height,
                            const std::string& label, const std::string& value, bool active) {
    // Label above the input
    SDL_Color labelColor = active ? SDL_Color{255, 255, 100, 255} : SDL_Color{150, 150, 150, 255};
    if (smallFont_) {
        SDL_Surface* surface = TTF_RenderText_Blended(smallFont_, label.c_str(), labelColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect rect = {x, y, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Input box with highlight when active
    SDL_Rect boxRect = {x, y + 20, width, height};

    if (active) {
        // Yellow border when editing
        SDL_SetRenderDrawColor(renderer_, 80, 80, 40, 255);
        SDL_RenderFillRect(renderer_, &boxRect);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 100, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 35, 35, 40, 255);
        SDL_RenderFillRect(renderer_, &boxRect);
        SDL_SetRenderDrawColor(renderer_, 70, 70, 80, 255);
    }
    SDL_RenderDrawRect(renderer_, &boxRect);

    // URL text
    if (smallFont_ && !value.empty()) {
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(smallFont_, value.c_str(), textColor);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                // Clip to box width
                SDL_Rect srcRect = {0, 0, std::min(surface->w, width - 10), surface->h};
                SDL_Rect dstRect = {x + 5, y + 23, srcRect.w, srcRect.h};
                SDL_RenderCopy(renderer_, texture, &srcRect, &dstRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Blinking cursor when active
    if (active) {
        uint32_t ticks = SDL_GetTicks();
        if ((ticks / 500) % 2 == 0) {
            int cursorX = x + 5;
            if (smallFont_ && !value.empty()) {
                int textWidth;
                TTF_SizeText(smallFont_, value.c_str(), &textWidth, nullptr);
                cursorX = x + 5 + std::min(textWidth, width - 15);
            }
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer_, cursorX, y + 24, cursorX, y + 20 + height - 4);
        }
    }
}

void App::renderButton(int x, int y, int width, int height,
                        const std::string& text, bool enabled) {
    SDL_Rect rect = {x, y, width, height};
    SDL_SetRenderDrawColor(renderer_, enabled ? 50 : 40, enabled ? 80 : 40, enabled ? 120 : 45, 255);
    SDL_RenderFillRect(renderer_, &rect);
    SDL_SetRenderDrawColor(renderer_, enabled ? 80 : 50, enabled ? 120 : 50, enabled ? 180 : 60, 255);
    SDL_RenderDrawRect(renderer_, &rect);

    if (smallFont_) {
        SDL_Color color = enabled ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 100, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(smallFont_, text.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect textRect = {x + (width - surface->w) / 2, y + (height - surface->h) / 2, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &textRect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void App::renderPauseOverlay() {
    // Semi-transparent overlay on video area
    int videoX = config_.timestampPanelWidth;
    int videoY = 75;
    int videoWidth = config_.windowWidth - videoX - 8;
    int videoHeight = config_.windowHeight - 110;

    // Dark overlay
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
    SDL_Rect overlay = {videoX, videoY, videoWidth, videoHeight};
    SDL_RenderFillRect(renderer_, &overlay);

    // PAUSED text
    SDL_Color red = {255, 80, 80, 255};
    renderTextCentered("PAUSED", videoX + videoWidth / 2, videoY + videoHeight / 2 - 20, red);

    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("[SPACE] to unpause  |  [S] screenshot", videoX + videoWidth / 2, videoY + videoHeight - 40, white);
}

void App::renderStatsPanel() {
    if (state_ == AppState::Disconnected || state_ == AppState::Connecting) {
        return;  // No stats to show
    }

    auto stats = videoDecoder_->getDecodeStats();
    const auto& streamInfo = videoDecoder_->getStreamInfo();

    // Stats panel position - bottom right corner, above status bar
    const int panelWidth = 280;
    const int lineHeight = 18;
    const int padding = 8;
    int numLines = 11;
    const int panelHeight = lineHeight * numLines + padding * 2;
    const int panelX = config_.windowWidth - panelWidth - padding;
    const int panelY = config_.windowHeight - 30 - panelHeight - padding;

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 200);
    SDL_Rect panelRect = {panelX, panelY, panelWidth, panelHeight};
    SDL_RenderFillRect(renderer_, &panelRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 80, 80, 100, 255);
    SDL_RenderDrawRect(renderer_, &panelRect);

    // Render stats text
    SDL_Color headerColor = {100, 200, 255, 255};
    SDL_Color labelColor = {180, 180, 180, 255};
    SDL_Color valueColor = {255, 255, 255, 255};
    SDL_Color greenColor = {100, 255, 100, 255};
    SDL_Color yellowColor = {255, 255, 100, 255};

    int y = panelY + padding;
    int labelX = panelX + padding;
    int valueX = panelX + 140;

    // Header
    renderText("DECODE STATS", labelX, y, headerColor);
    y += lineHeight + 4;

    // Decoder info
    renderText("Decoder:", labelX, y, labelColor);
    renderText(stats.decoderName, valueX, y, valueColor);
    y += lineHeight;

    // Hardware acceleration
    renderText("Accel:", labelX, y, labelColor);
    SDL_Color accelColor = stats.isHardwareAccelerated ? greenColor : yellowColor;
    renderText(stats.hwAccelType, valueX, y, accelColor);
    y += lineHeight;

    // Resolution
    renderText("Resolution:", labelX, y, labelColor);
    std::string resStr = std::to_string(streamInfo.width) + "x" + std::to_string(streamInfo.height);
    renderText(resStr, valueX, y, valueColor);
    y += lineHeight;

    // FPS
    renderText("FPS (actual):", labelX, y, labelColor);
    std::ostringstream fpsStr;
    fpsStr << std::fixed << std::setprecision(1) << stats.actualFps;
    renderText(fpsStr.str(), valueX, y, valueColor);
    y += lineHeight;

    // Decode time
    renderText("Decode time:", labelX, y, labelColor);
    std::ostringstream decodeStr;
    decodeStr << std::fixed << std::setprecision(1) << (stats.avgDecodeTimeUs / 1000.0) << " ms";
    renderText(decodeStr.str(), valueX, y, valueColor);
    y += lineHeight;

    // Convert time (RGB conversion)
    renderText("RGB convert:", labelX, y, labelColor);
    std::ostringstream convertStr;
    convertStr << std::fixed << std::setprecision(2) << (stats.avgConvertTimeUs / 1000.0) << " ms";
    renderText(convertStr.str(), valueX, y, valueColor);
    y += lineHeight;

    // Total processing time
    double totalMs = (stats.avgDecodeTimeUs + stats.avgConvertTimeUs) / 1000.0;
    renderText("Total process:", labelX, y, labelColor);
    std::ostringstream totalStr;
    totalStr << std::fixed << std::setprecision(1) << totalMs << " ms";
    renderText(totalStr.str(), valueX, y, valueColor);
    y += lineHeight;

    // Frames decoded/dropped
    renderText("Frames:", labelX, y, labelColor);
    std::ostringstream frameStr;
    frameStr << stats.framesDecoded;
    if (stats.framesDropped > 0) {
        frameStr << " (" << stats.framesDropped << " dropped)";
    }
    SDL_Color frameColor = stats.framesDropped > 0 ? yellowColor : valueColor;
    renderText(frameStr.str(), valueX, y, frameColor);
    y += lineHeight;

    // Queue depth
    renderText("Queue:", labelX, y, labelColor);
    std::string queueStr = std::to_string(stats.queueDepth) + "/" + std::to_string(stats.maxQueueSize);
    renderText(queueStr, valueX, y, valueColor);
}

void App::renderHelpPanel() {
    const int panelWidth = 500;
    const int panelHeight = 450;
    const int panelX = (config_.windowWidth - panelWidth) / 2;
    const int panelY = (config_.windowHeight - panelHeight) / 2;
    const int padding = 20;
    const int lineHeight = 26;

    // Darken background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
    SDL_Rect fullscreen = {0, 0, config_.windowWidth, config_.windowHeight};
    SDL_RenderFillRect(renderer_, &fullscreen);

    // Panel background
    SDL_SetRenderDrawColor(renderer_, 30, 30, 40, 255);
    SDL_Rect panelRect = {panelX, panelY, panelWidth, panelHeight};
    SDL_RenderFillRect(renderer_, &panelRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 80, 120, 180, 255);
    SDL_RenderDrawRect(renderer_, &panelRect);

    SDL_Color titleColor = {100, 200, 255, 255};
    SDL_Color keyColor = {255, 255, 100, 255};
    SDL_Color descColor = {200, 200, 200, 255};
    SDL_Color headerColor = {150, 180, 255, 255};

    int y = panelY + padding;
    int centerX = panelX + panelWidth / 2;

    // Title
    renderTextCentered("HELP - KEYBOARD SHORTCUTS", centerX, y, titleColor);
    y += lineHeight + 10;

    // Connection section
    renderText("CONNECTION:", panelX + padding, y, headerColor);
    y += lineHeight;
    renderText("U", panelX + padding + 20, y, keyColor);
    renderText("Edit stream URL", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("C", panelX + padding + 20, y, keyColor);
    renderText("Connect to stream", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("D", panelX + padding + 20, y, keyColor);
    renderText("Disconnect from stream", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("P", panelX + padding + 20, y, keyColor);
    renderText("Cycle transport (Auto/TCP/UDP)", panelX + padding + 80, y, descColor);
    y += lineHeight + 8;

    // Test section
    renderText("LATENCY TEST:", panelX + padding, y, headerColor);
    y += lineHeight;
    renderText("T", panelX + padding + 20, y, keyColor);
    renderText("Start/Stop timestamp clock", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("SPACE", panelX + padding + 20, y, keyColor);
    renderText("Freeze frame (measure latency)", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("S", panelX + padding + 20, y, keyColor);
    renderText("Save screenshot", panelX + padding + 80, y, descColor);
    y += lineHeight + 8;

    // General section
    renderText("GENERAL:", panelX + padding, y, headerColor);
    y += lineHeight;
    renderText("F1", panelX + padding + 20, y, keyColor);
    renderText("Show this help panel", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("F2", panelX + padding + 20, y, keyColor);
    renderText("Show about panel", panelX + padding + 80, y, descColor);
    y += lineHeight;
    renderText("ESC", panelX + padding + 20, y, keyColor);
    renderText("Close panel / Stop test / Quit", panelX + padding + 80, y, descColor);

    // Footer
    y = panelY + panelHeight - padding - lineHeight;
    renderTextCentered("Press ESC or F1 to close", centerX, y, descColor);
}

void App::renderAboutPanel() {
    const int panelWidth = 520;
    const int panelHeight = 320;
    const int panelX = (config_.windowWidth - panelWidth) / 2;
    const int panelY = (config_.windowHeight - panelHeight) / 2;
    const int padding = 20;
    const int lineHeight = 26;

    // Darken background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
    SDL_Rect fullscreen = {0, 0, config_.windowWidth, config_.windowHeight};
    SDL_RenderFillRect(renderer_, &fullscreen);

    // Panel background
    SDL_SetRenderDrawColor(renderer_, 30, 30, 40, 255);
    SDL_Rect panelRect = {panelX, panelY, panelWidth, panelHeight};
    SDL_RenderFillRect(renderer_, &panelRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 80, 120, 180, 255);
    SDL_RenderDrawRect(renderer_, &panelRect);

    SDL_Color titleColor = {100, 200, 255, 255};
    SDL_Color labelColor = {150, 150, 150, 255};
    SDL_Color valueColor = {255, 255, 255, 255};
    SDL_Color linkColor = {100, 180, 255, 255};

    int y = panelY + padding;
    int centerX = panelX + panelWidth / 2;

    // Title
    renderTextCentered("ABOUT", centerX, y, titleColor);
    y += lineHeight + 15;

    // App name
    renderTextCentered("Video Latency Test Tool", centerX, y, valueColor);
    y += lineHeight;
    renderTextCentered("Version 1.0", centerX, y, labelColor);
    y += lineHeight + 20;

    // Author
    renderText("Author:", panelX + padding, y, labelColor);
    y += lineHeight;
    renderTextCentered("tim.biddulph@brigade-electroincs.com", centerX, y, linkColor);
    y += lineHeight + 20;

    // Description
    renderTextCentered("A tool for measuring end-to-end", centerX, y, labelColor);
    y += lineHeight;
    renderTextCentered("video streaming latency via RTSP/RTP", centerX, y, labelColor);

    // Footer
    y = panelY + panelHeight - padding - lineHeight;
    renderTextCentered("Press ESC or F2 to close", centerX, y, labelColor);
}

void App::renderConnectionHistory() {
    if (connectionHistory_.empty()) return;

    // Position in the video area when disconnected
    const int panelX = config_.timestampPanelWidth + 20;
    const int panelY = 90;
    const int lineHeight = 24;
    const int padding = 15;
    const int panelWidth = 500;
    int numItems = std::min(static_cast<int>(connectionHistory_.size()), MAX_HISTORY_SIZE);
    const int panelHeight = lineHeight * (numItems + 1) + padding * 2;

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 30, 30, 40, 230);
    SDL_Rect panelRect = {panelX, panelY, panelWidth, panelHeight};
    SDL_RenderFillRect(renderer_, &panelRect);

    // Border
    SDL_SetRenderDrawColor(renderer_, 80, 100, 140, 255);
    SDL_RenderDrawRect(renderer_, &panelRect);

    SDL_Color headerColor = {100, 200, 255, 255};
    SDL_Color keyColor = {255, 255, 100, 255};
    SDL_Color urlColor = {200, 200, 200, 255};

    int y = panelY + padding;

    // Header
    renderText("RECENT CONNECTIONS (press 1-9 to connect):", panelX + padding, y, headerColor);
    y += lineHeight + 4;

    // List entries
    for (int i = 0; i < numItems; i++) {
        // Key number
        std::string keyStr = "[" + std::to_string(i + 1) + "]";
        renderText(keyStr, panelX + padding, y, keyColor);

        // URL (truncate if too long)
        std::string url = connectionHistory_[i];
        if (url.length() > 55) {
            url = url.substr(0, 52) + "...";
        }
        renderText(url, panelX + padding + 40, y, urlColor);
        y += lineHeight;
    }
}

void App::renderStatusBar() {
    // Background for status bar
    SDL_SetRenderDrawColor(renderer_, 25, 25, 30, 255);
    SDL_Rect statusBg = {0, config_.windowHeight - 30, config_.windowWidth, 30};
    SDL_RenderFillRect(renderer_, &statusBg);

    SDL_SetRenderDrawColor(renderer_, 50, 50, 60, 255);
    SDL_RenderDrawLine(renderer_, 0, config_.windowHeight - 30, config_.windowWidth, config_.windowHeight - 30);

    int y = config_.windowHeight - 24;

    std::string statusText;
    SDL_Color statusColor;

    switch (state_) {
        case AppState::Disconnected: {
            std::string transportLabel;
            switch (streamConfig_.transport) {
                case TransportProtocol::AUTO: transportLabel = "Auto"; break;
                case TransportProtocol::TCP:  transportLabel = "TCP"; break;
                case TransportProtocol::UDP:  transportLabel = "UDP"; break;
            }
            if (!connectionHistory_.empty()) {
                statusText = "Disconnected [" + transportLabel + "] - C: connect, U: edit URL, P: transport, 1-9: recent";
            } else {
                statusText = "Disconnected [" + transportLabel + "] - C: connect, U: edit URL, P: transport";
            }
            statusColor = {150, 150, 150, 255};
            break;
        }
        case AppState::Connecting:
            statusText = "Connecting...";
            statusColor = {255, 200, 100, 255};
            break;
        case AppState::Connected: {
            std::string protoStr = (videoDecoder_->getDetectedProtocol() == StreamProtocol::RTP) ? "RTP" : "RTSP";
            const auto& diag = videoDecoder_->getConnectionDiagnostics();
            std::string transportStr = "TCP";
            if (!diag.attempts.empty()) {
                transportStr = (diag.attempts.back().transport == TransportProtocol::UDP) ? "UDP" : "TCP";
            }
            statusText = "Connected [" + protoStr + "/" + transportStr + "] - T: start clock, D: disconnect";
            statusColor = {100, 200, 100, 255};
            break;
        }
        case AppState::Running:
            if (paused_) {
                statusText = "PAUSED - SPACE: unpause, D: disconnect";
                statusColor = {255, 100, 100, 255};
            } else {
                statusText = "Running - SPACE: freeze, T: stop clock, D: disconnect";
                statusColor = {100, 150, 255, 255};
            }
            break;
    }

    if (!videoDecoder_->getLastError().empty() && state_ == AppState::Disconnected) {
        statusText = "Error: " + videoDecoder_->getLastError();
        statusColor = {255, 100, 100, 255};
    }

    renderText(statusText, 10, y, statusColor);

    // Help/About shortcuts on right side
    SDL_Color helpColor = {120, 120, 140, 255};
    std::string helpText = "F1: Help  |  F2: About";
    if (smallFont_) {
        int textWidth;
        TTF_SizeText(smallFont_, helpText.c_str(), &textWidth, nullptr);
        renderText(helpText, config_.windowWidth - textWidth - 10, y, helpColor);
    }
}

void App::renderText(const std::string& text, int x, int y, SDL_Color color) {
    if (!smallFont_ || text.empty()) return;

    SDL_Surface* surface = TTF_RenderText_Blended(smallFont_, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture) {
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer_, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void App::renderTextCentered(const std::string& text, int centerX, int y, SDL_Color color) {
    if (!font_ || text.empty()) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font_, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (texture) {
        SDL_Rect rect = {centerX - surface->w / 2, y, surface->w, surface->h};
        SDL_RenderCopy(renderer_, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void App::connect() {
    state_ = AppState::Connecting;
    streamConfig_.url = urlInput_;
    showingDiagnostics_ = false;

    if (videoDecoder_->connect(streamConfig_)) {
        state_ = AppState::Connected;
        addToConnectionHistory(urlInput_);
    } else {
        state_ = AppState::Disconnected;
        showingDiagnostics_ = true;
    }
}

void App::disconnect() {
    if (state_ == AppState::Running) {
        stopClock();
    }
    videoDecoder_->disconnect();
    paused_ = false;
    state_ = AppState::Disconnected;
}

void App::startClock() {
    if (state_ != AppState::Connected) return;
    timestampDisplay_->startTest();
    paused_ = false;
    state_ = AppState::Running;
}

void App::stopClock() {
    if (state_ != AppState::Running) return;
    timestampDisplay_->stopTest();
    paused_ = false;
    state_ = AppState::Connected;
}

void App::togglePause() {
    paused_ = !paused_;
    if (paused_) {
        // Capture current timestamp when pausing
        pausedTimestamp_ = timestampDisplay_->getCurrentTimestamp();
    }
    // Pause/resume decoder to stop frame processing
    videoDecoder_->setPaused(paused_);
}

void App::cycleTransportProtocol() {
    switch (streamConfig_.transport) {
        case TransportProtocol::AUTO:
            streamConfig_.transport = TransportProtocol::TCP;
            break;
        case TransportProtocol::TCP:
            streamConfig_.transport = TransportProtocol::UDP;
            break;
        case TransportProtocol::UDP:
            streamConfig_.transport = TransportProtocol::AUTO;
            break;
    }
}

void App::renderDiagnosticsPanel() {
    const auto& diag = videoDecoder_->getConnectionDiagnostics();

    const int panelWidth = 600;
    const int lineHeight = 22;
    const int padding = 20;

    // Calculate height based on content
    int numLines = 7; // title + summary + url + transport + blank + attempts header + blank
    numLines += static_cast<int>(diag.attempts.size()) * 3;
    numLines += 1 + static_cast<int>(diag.suggestions.size());
    numLines += 2; // footer

    const int panelHeight = std::min(lineHeight * numLines + padding * 2,
                                      config_.windowHeight - 40);
    const int panelX = (config_.windowWidth - panelWidth) / 2;
    const int panelY = (config_.windowHeight - panelHeight) / 2;

    // Darken background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
    SDL_Rect fullscreen = {0, 0, config_.windowWidth, config_.windowHeight};
    SDL_RenderFillRect(renderer_, &fullscreen);

    // Panel background
    SDL_SetRenderDrawColor(renderer_, 30, 30, 40, 255);
    SDL_Rect panelRect = {panelX, panelY, panelWidth, panelHeight};
    SDL_RenderFillRect(renderer_, &panelRect);

    // Red-tinted border
    SDL_SetRenderDrawColor(renderer_, 180, 80, 80, 255);
    SDL_RenderDrawRect(renderer_, &panelRect);

    SDL_Color titleColor = {255, 100, 100, 255};
    SDL_Color labelColor = {180, 180, 180, 255};
    SDL_Color valueColor = {255, 255, 255, 255};
    SDL_Color headerColor = {255, 200, 100, 255};
    SDL_Color suggestionColor = {100, 255, 150, 255};
    SDL_Color stageColor = {255, 180, 100, 255};

    int y = panelY + padding;
    int leftX = panelX + padding;
    int maxY = panelY + panelHeight - padding - lineHeight * 2;

    // Title
    renderTextCentered("CONNECTION FAILED", panelX + panelWidth / 2, y, titleColor);
    y += lineHeight + 8;

    // Summary
    if (y < maxY) {
        renderText(diag.summary, leftX, y, valueColor);
        y += lineHeight + 4;
    }

    // URL
    if (y < maxY) {
        std::string urlDisplay = diag.url;
        if (urlDisplay.length() > 65) urlDisplay = urlDisplay.substr(0, 62) + "...";
        renderText("URL: " + urlDisplay, leftX, y, labelColor);
        y += lineHeight;
    }

    // Transport setting
    if (y < maxY) {
        std::string transportStr;
        switch (streamConfig_.transport) {
            case TransportProtocol::AUTO: transportStr = "Auto (TCP then UDP)"; break;
            case TransportProtocol::TCP: transportStr = "TCP only"; break;
            case TransportProtocol::UDP: transportStr = "UDP only"; break;
        }
        renderText("Transport: " + transportStr, leftX, y, labelColor);
        y += lineHeight + 8;
    }

    // Attempt details
    if (y < maxY) {
        renderText("ATTEMPTS:", leftX, y, headerColor);
        y += lineHeight;
    }

    auto stageName = [](ConnectionStage stage) -> std::string {
        switch (stage) {
            case ConnectionStage::OpeningInput: return "Opening stream";
            case ConnectionStage::FindingStreamInfo: return "Detecting stream format";
            case ConnectionStage::FindingVideoStream: return "Finding video track";
            case ConnectionStage::OpeningCodec: return "Opening video codec";
            case ConnectionStage::Connected: return "Connected";
            default: return "Unknown";
        }
    };

    for (size_t i = 0; i < diag.attempts.size() && y < maxY; i++) {
        const auto& att = diag.attempts[i];
        std::string transportLabel = (att.transport == TransportProtocol::TCP) ? "TCP" : "UDP";

        renderText("  " + std::to_string(i + 1) + ". " + transportLabel + ":", leftX, y, stageColor);
        y += lineHeight;

        if (y < maxY) {
            renderText("     Failed at: " + stageName(att.failedAt), leftX, y, labelColor);
            y += lineHeight;
        }

        if (y < maxY && !att.ffmpegErrorString.empty()) {
            std::string errDisplay = att.ffmpegErrorString;
            if (errDisplay.length() > 55) errDisplay = errDisplay.substr(0, 52) + "...";
            renderText("     Error: " + errDisplay, leftX, y, valueColor);
            y += lineHeight;
        }
    }

    y += 4;

    // Suggestions
    if (!diag.suggestions.empty() && y < maxY) {
        renderText("SUGGESTIONS:", leftX, y, headerColor);
        y += lineHeight;
        for (const auto& suggestion : diag.suggestions) {
            if (y >= maxY) break;
            std::string sugDisplay = suggestion;
            if (sugDisplay.length() > 70) sugDisplay = sugDisplay.substr(0, 67) + "...";
            renderText("  > " + sugDisplay, leftX, y, suggestionColor);
            y += lineHeight;
        }
    }

    // Footer
    y = panelY + panelHeight - padding - lineHeight;
    SDL_Color footerColor = {150, 150, 150, 255};
    renderTextCentered("[ESC] close  |  [P] change transport  |  [C] retry",
                       panelX + panelWidth / 2, y, footerColor);
}

void App::saveScreenshot() {
    int w, h;
    SDL_GetRendererOutputSize(renderer_, &w, &h);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return;

    SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream filename;
    filename << "latency_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".bmp";

    SDL_SaveBMP(surface, filename.str().c_str());
    SDL_FreeSurface(surface);

    std::cout << "Screenshot saved: " << filename.str() << std::endl;
}

void App::loadConnectionHistory() {
    connectionHistory_.clear();
    std::ifstream file(historyFilePath_);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line) && connectionHistory_.size() < MAX_HISTORY_SIZE) {
        if (!line.empty()) {
            connectionHistory_.push_back(line);
        }
    }
}

void App::saveConnectionHistory() {
    std::ofstream file(historyFilePath_);
    if (!file.is_open()) return;

    for (const auto& url : connectionHistory_) {
        file << url << "\n";
    }
}

void App::addToConnectionHistory(const std::string& url) {
    // Remove if already exists (to move to front)
    auto it = std::find(connectionHistory_.begin(), connectionHistory_.end(), url);
    if (it != connectionHistory_.end()) {
        connectionHistory_.erase(it);
    }

    // Add to front
    connectionHistory_.insert(connectionHistory_.begin(), url);

    // Limit size
    if (connectionHistory_.size() > MAX_HISTORY_SIZE) {
        connectionHistory_.resize(MAX_HISTORY_SIZE);
    }

    saveConnectionHistory();
}

void App::selectFromHistory(int index) {
    if (index >= 0 && index < static_cast<int>(connectionHistory_.size())) {
        urlInput_ = connectionHistory_[index];
        connect();
    }
}

} // namespace latency
