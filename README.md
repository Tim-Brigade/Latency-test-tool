# Video Latency Test Tool

A Windows desktop application for measuring end-to-end video latency in RTSP/RTP camera streams. Point your camera at the application's timestamp display, then measure the delay between the displayed time and what appears in the video feed.

## Features

- **Real-time timestamp display** - High-precision millisecond clock rendered on screen
- **RTSP/RTP stream support** - Connect to IP cameras and video encoders via FFmpeg
- **Automatic latency measurement** - Binary pattern recognition to decode timestamps from video
- **Export results** - Save measurements to JSON for analysis
- **Screenshot capture** - Document your test setup and results

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
| `D` | Disconnect |
| `T` | Start/Stop test |
| `E` | Export results to JSON |
| `S` | Save screenshot |
| `ESC` | Quit |

### Example Workflow

1. Launch the application
2. Press `U` and enter your camera's RTSP URL (e.g., `rtsp://192.168.1.100:554/stream`)
3. Press `C` to connect
4. Position your camera to capture the timestamp display
5. Press `T` to start the test
6. The application will measure and display latency values
7. Press `E` to export results

## Project Structure

```
Latency-test-tool/
├── src/
│   ├── main.cpp              # Entry point
│   ├── App.cpp/h             # Main application class
│   ├── TimestampDisplay.cpp/h # Timestamp rendering
│   ├── VideoDecoder.cpp/h    # FFmpeg video decoding
│   ├── VideoRenderer.cpp/h   # SDL video rendering
│   ├── LatencyMeasurer.cpp/h # Pattern detection & measurement
│   ├── ResultsManager.cpp/h  # Results export
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

### "Failed to connect to stream"
- Verify the RTSP URL is correct and accessible
- Check firewall settings
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
