#include "App.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    latency::AppConfig config;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.timestampPanelWidth = 450;
    config.fontPath = "resources/fonts/RobotoMono-Bold.ttf";
    config.fontSize = 36;

    latency::App app;

    if (!app.init(config)) {
        return 1;
    }

    app.run();

    return 0;
}
