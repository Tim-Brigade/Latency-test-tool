# Changelog

All notable changes to this project will be documented in this file.

## [1.1.0] - 2026-02-16

### Added

- RTSP transport protocol selection — cycle between Auto, TCP-only, and UDP-only with the `P` key
- Automatic UDP/TCP fallback when using Auto transport mode
- Connection diagnostics panel with per-attempt details and troubleshooting suggestions
- Connection history for quick reconnection (keys `1-9`)
- Decode statistics panel showing FPS, decode time, hardware acceleration status, and transport protocol
- Screenshot capture saved to `screenshots/` directory with timestamped filenames

### Changed

- Timestamp clock now starts automatically when a stream connects (removed manual `T` toggle)
- Improved H.264 stream startup time
- Streamlined UI layout for camera capture use case

### Fixed

- URL input no longer captures the `U` key as text when the input field is active

## [1.0.0] - 2025-12-01

### Added

- Initial release
- Real-time timestamp display with 10ms resolution
- RTSP/RTP stream connection via FFmpeg
- Freeze-frame measurement for latency comparison
- Help and About panels
- Distribution packaging script
