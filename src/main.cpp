#include "App.h"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "Video Latency Test Tool v1.0" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  U - Edit stream URL" << std::endl;
    std::cout << "  C - Connect to stream" << std::endl;
    std::cout << "  D - Disconnect" << std::endl;
    std::cout << "  T - Start/Stop test" << std::endl;
    std::cout << "  E - Export results to JSON" << std::endl;
    std::cout << "  S - Save screenshot" << std::endl;
    std::cout << "  ESC - Quit" << std::endl;
    std::cout << std::endl;

    latency::AppConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.timestampPanelWidth = 450;
    config.fontPath = "resources/fonts/RobotoMono-Bold.ttf";
    config.fontSize = 36;

    latency::App app;

    if (!app.init(config)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    app.run();

    return 0;
}
