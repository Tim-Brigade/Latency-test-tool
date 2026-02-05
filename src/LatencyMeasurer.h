#pragma once

#include "VideoDecoder.h"
#include "TimestampDisplay.h"
#include <vector>
#include <cstdint>
#include <optional>

namespace latency {

struct PatternRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct LatencyMeasurement {
    uint32_t displayedTimestamp = 0;  // Timestamp read from video
    uint32_t actualTimestamp = 0;     // Actual current timestamp
    int32_t latencyMs = 0;            // Difference (actual - displayed)
    bool valid = false;               // Whether measurement was successful
};

class LatencyMeasurer {
public:
    LatencyMeasurer();
    ~LatencyMeasurer() = default;

    // Analyze a frame and extract the timestamp
    LatencyMeasurement measure(const VideoFrame* frame, uint32_t currentTimestamp);

    // Auto-detect pattern region in frame
    std::optional<PatternRegion> detectPatternRegion(const VideoFrame* frame);

    // Set manual pattern region
    void setPatternRegion(const PatternRegion& region);
    void clearPatternRegion();

    // Get detected region
    const std::optional<PatternRegion>& getPatternRegion() const { return patternRegion_; }

private:
    // Decode binary pattern from pixel data
    std::optional<uint32_t> decodeBinaryPattern(const VideoFrame* frame, const PatternRegion& region);

    // Find pattern near a detected green pixel
    std::optional<PatternRegion> findPatternNearGreen(const VideoFrame* frame, int greenX, int greenY);

    // Validate sync pattern exists at location
    bool validateSyncPattern(const VideoFrame* frame, int x, int y);

    // Get average brightness of a region
    uint8_t getRegionBrightness(const uint8_t* data, int pitch, int x, int y, int w, int h) const;

    std::optional<PatternRegion> patternRegion_;
    int brightnessThreshold_ = 128;  // Threshold for black/white detection
};

} // namespace latency
