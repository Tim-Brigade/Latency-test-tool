#include "App.h"
#include <iostream>
#include <sstream>
#include <iomanip>

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

    urlInput_ = "rtsp://192.168.1.100:554/stream";

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
                if (state_ == AppState::Disconnected) connect();
                break;
            case SDLK_ESCAPE:
                urlInputActive_ = false;
                break;
            default:
                break;
        }
        return;
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
            break;

        case SDLK_ESCAPE:
            if (paused_) {
                togglePause();  // Unpause
            } else if (state_ == AppState::Running) {
                stopClock();
            } else {
                appRunning_ = false;
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

    // Timestamp display (left panel)
    int timestampWidth = config_.timestampPanelWidth;
    timestampDisplay_->render(
        padding,
        topBarHeight,
        timestampWidth - padding * 2,
        contentHeight
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

    // Pause overlay
    if (paused_) {
        renderPauseOverlay();
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
    renderTextCentered("PAUSED", videoX + videoWidth / 2, videoY + 50, red);

    // Show current live time vs frozen time
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color green = {100, 255, 100, 255};
    SDL_Color yellow = {255, 255, 100, 255};

    int infoY = videoY + videoHeight / 2 - 60;

    renderTextCentered("FROZEN FRAME TIME:", videoX + videoWidth / 2, infoY, white);

    // Format frozen timestamp
    uint32_t ms = pausedTimestamp_ % 1000;
    uint32_t totalSec = pausedTimestamp_ / 1000;
    uint32_t sec = totalSec % 60;
    uint32_t min = (totalSec / 60) % 60;

    std::ostringstream frozenTime;
    frozenTime << std::setfill('0') << std::setw(2) << min << ":"
               << std::setw(2) << sec << "." << std::setw(3) << ms;

    if (largeFont_) {
        SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, frozenTime.str().c_str(), green);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect rect = {videoX + (videoWidth - surface->w) / 2, infoY + 30, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Show current live time for comparison
    uint32_t currentTs = timestampDisplay_->getCurrentTimestamp();
    uint32_t curMs = currentTs % 1000;
    uint32_t curTotalSec = currentTs / 1000;
    uint32_t curSec = curTotalSec % 60;
    uint32_t curMin = (curTotalSec / 60) % 60;

    std::ostringstream currentTime;
    currentTime << std::setfill('0') << std::setw(2) << curMin << ":"
                << std::setw(2) << curSec << "." << std::setw(3) << curMs;

    renderTextCentered("CURRENT LIVE TIME:", videoX + videoWidth / 2, infoY + 130, white);

    if (largeFont_) {
        SDL_Surface* surface = TTF_RenderText_Blended(largeFont_, currentTime.str().c_str(), yellow);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            if (texture) {
                SDL_Rect rect = {videoX + (videoWidth - surface->w) / 2, infoY + 160, surface->w, surface->h};
                SDL_RenderCopy(renderer_, texture, nullptr, &rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Calculate and show difference
    int32_t diff = static_cast<int32_t>(currentTs) - static_cast<int32_t>(pausedTimestamp_);

    std::ostringstream diffText;
    diffText << "LATENCY: " << diff << " ms";

    SDL_Color cyan = {100, 200, 255, 255};
    renderTextCentered(diffText.str(), videoX + videoWidth / 2, infoY + 260, cyan);

    renderTextCentered("[SPACE] to unpause  |  [S] screenshot", videoX + videoWidth / 2, videoY + videoHeight - 40, white);
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
        case AppState::Disconnected:
            statusText = "Disconnected - Press C to connect, or U to edit URL";
            statusColor = {150, 150, 150, 255};
            break;
        case AppState::Connecting:
            statusText = "Connecting...";
            statusColor = {255, 200, 100, 255};
            break;
        case AppState::Connected:
            statusText = "Connected: " + videoDecoder_->getStreamInfo().codecName +
                        " " + std::to_string(videoDecoder_->getStreamInfo().width) +
                        "x" + std::to_string(videoDecoder_->getStreamInfo().height) +
                        " - Press T to start clock";
            statusColor = {100, 200, 100, 255};
            break;
        case AppState::Running:
            if (paused_) {
                statusText = "PAUSED - Press SPACE to unpause";
                statusColor = {255, 100, 100, 255};
            } else {
                statusText = "Running - Press SPACE to freeze frame, T to stop";
                statusColor = {100, 150, 255, 255};
            }
            break;
    }

    if (!videoDecoder_->getLastError().empty() && state_ == AppState::Disconnected) {
        statusText = "Error: " + videoDecoder_->getLastError();
        statusColor = {255, 100, 100, 255};
    }

    renderText(statusText, 10, y, statusColor);
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

    if (videoDecoder_->connect(streamConfig_)) {
        state_ = AppState::Connected;
    } else {
        state_ = AppState::Disconnected;
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

} // namespace latency
