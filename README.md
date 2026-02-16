# Video Latency Test Tool

A Windows desktop application for measuring end-to-end video latency in RTSP/RTP camera streams. Point your camera at the application's timestamp display, then measure the delay between the displayed time and what appears in the video feed.

## Features

- **Real-time timestamp display** - High-precision clock with 10ms resolution on white background (optimized for camera capture)
- **RTSP/RTP stream support** - Connect to IP cameras and video encoders via FFmpeg
- **Transport protocol selection** - Choose between Auto (UDP with TCP fallback), TCP-only, or UDP-only modes
- **Connection diagnostics** - Detailed failure analysis with per-attempt info and troubleshooting suggestions
- **Freeze-frame measurement** - Pause video to compare displayed time vs captured time
- **Connection history** - Remembers recent connections for quick reconnection (keys 1-9)
- **Decode statistics** - Real-time display of decoder performance, FPS, hardware acceleration, and transport protocol
- **Screenshot capture** - Save timestamped screenshots to `screenshots/` directory

## How It Works

1. Display a precise timestamp on your monitor
2. Point your camera at the screen
3. The application captures the video stream and reads the timestamp visible in the frame
4. Latency = Current Time - Timestamp Shown in Video

## Requirements

- Windows 10/11 (x64)
- [Visual Studio 2019 or 2022](https://visualstudio.microsoft.com/) with C++ desktop development workload
- [CMake 3.20+](https://cmake.org/download/)
- [vcpkg](https://github.com/Microsoft/vcpkg) package manager

## Installation

### 1. Install vcpkg (if not already installed)

```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

Set the environment variable:
```bash
setx VCPKG_ROOT C:\vcpkg
```

### 2. Clone and Build

```bash
git clone https://github.com/YOUR_USERNAME/Latency-test-tool.git
cd Latency-test-tool
build.bat
```

The build script will automatically:
- Install dependencies (SDL2, SDL2_ttf, FFmpeg) via vcpkg
- Configure the project with CMake
- Build the Release executable

### 3. Add a Font

Download a monospace font (e.g., [Roboto Mono](https://fonts.google.com/specimen/Roboto+Mono)) and place `RobotoMono-Bold.ttf` in `resources/fonts/`.

## Usage

### Running the Application

```bash
run.bat
```

Or directly:
```bash
build\Release\LatencyTestTool.exe
```

### Controls

| Key | Action |
|-----|--------|
| `U` | Edit stream URL |
| `C` | Connect to stream |
| `D` | Disconnect from stream |
| `P` | Cycle transport protocol (Auto/TCP/UDP) |
| `SPACE` | Freeze frame to measure latency |
| `S` | Save screenshot |
| `1-9` | Quick connect to recent URLs |
| `F1` | Show help panel |
| `F2` | Show about panel |
| `ESC` | Close panel / Unpause / Quit |

### Example Workflow

1. Launch the application
2. Press `U` and enter your camera's RTSP URL (e.g., `rtsp://192.168.1.100:554/stream`)
3. Press `C` to connect — the timestamp clock starts automatically
4. Position your camera to capture the white timestamp display panel
5. Press `SPACE` to freeze the frame and compare times
6. The frozen time shown in the video vs the clock panel shows the latency
7. Press `S` to save a screenshot for documentation

## Distribution

To share the application with others who don't need to build from source:

### Creating a Distribution Package

```bash
package.bat
```

This creates `dist/LatencyTestTool-v1.1.0-win64.zip` containing the executable and all required DLLs.

### For Recipients

1. Extract the zip file
2. Run `LatencyTestTool.exe`
3. If the app doesn't start, install [Visual C++ Redistributable 2022](https://aka.ms/vs/17/release/vc_redist.x64.exe)

No installation or build tools required - just extract and run.

## Project Structure

```
Latency-test-tool/
├── src/
│   ├── main.cpp              # Entry point
│   ├── App.cpp/h             # Main application class
│   ├── TimestampDisplay.cpp/h # Timestamp rendering
│   ├── VideoDecoder.cpp/h    # FFmpeg video decoding
│   ├── VideoRenderer.cpp/h   # SDL video rendering
│   └── Config.cpp/h          # Configuration
├── resources/
│   └── fonts/                # TTF fonts
├── CMakeLists.txt            # CMake build configuration
├── vcpkg.json                # vcpkg dependencies
├── build.bat                 # Build script
├── run.bat                   # Run script
└── setup.bat                 # First-time setup
```

## Dependencies

Managed via vcpkg:
- **SDL2** - Window management and rendering
- **SDL2_ttf** - TrueType font rendering
- **FFmpeg** - Video decoding (avcodec, avformat, swscale)

## Troubleshooting

### Connection diagnostics

When a connection fails, a diagnostics panel appears automatically showing:

- Details of each connection attempt (TCP and UDP)
- The stage where the connection failed and FFmpeg error messages
- Actionable suggestions to resolve the issue

From the diagnostics panel you can press `P` to change transport protocol, `C` to retry, or `U` to edit the URL.

### "Failed to connect to stream"

- Try cycling the transport protocol with `P` (Auto/TCP/UDP) — some cameras only support one protocol
- Verify the RTSP URL is correct and accessible
- Check firewall settings (UDP streams may need ports 6970-6999 open)
- Ensure the camera supports RTSP

### "Font not found"

- Download a TTF font and place it in `resources/fonts/`
- Update the font path in `main.cpp` if using a different filename

### Build errors

- Ensure Visual Studio C++ workload is installed
- Run `build.bat` from a Developer Command Prompt if issues persist

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is provided as-is for educational and testing purposes.

## Acknowledgments

- [SDL2](https://www.libsdl.org/) - Simple DirectMedia Layer
- [FFmpeg](https://ffmpeg.org/) - Multimedia framework
- [vcpkg](https://github.com/Microsoft/vcpkg) - C++ package manager
