#include "LatencyMeasurer.h"
#include <algorithm>
#include <cmath>

namespace latency {

LatencyMeasurer::LatencyMeasurer() = default;

void LatencyMeasurer::setPatternRegion(const PatternRegion& region) {
    patternRegion_ = region;
}

void LatencyMeasurer::clearPatternRegion() {
    patternRegion_.reset();
}

LatencyMeasurement LatencyMeasurer::measure(const VideoFrame* frame, uint32_t currentTimestamp) {
    LatencyMeasurement result;
    result.actualTimestamp = currentTimestamp;
    result.valid = false;

    if (!frame || !frame->data) {
        return result;
    }

    // Auto-detect pattern region if not set
    if (!patternRegion_) {
        auto detected = detectPatternRegion(frame);
        if (detected) {
            patternRegion_ = detected;
        } else {
            return result;  // Pattern not found
        }
    }

    // Decode timestamp from pattern
    auto timestamp = decodeBinaryPattern(frame, *patternRegion_);
    if (!timestamp) {
        // Pattern detection might have drifted, try re-detecting
        patternRegion_.reset();
        return result;
    }

    // Sanity check: timestamp should be reasonable (less than 24 hours in ms)
    const uint32_t MAX_REASONABLE_TIMESTAMP = 24 * 60 * 60 * 1000;  // 24 hours
    if (*timestamp > MAX_REASONABLE_TIMESTAMP) {
        patternRegion_.reset();
        return result;
    }

    // Sanity check: latency should be reasonable (-10s to +60s)
    int32_t latency = static_cast<int32_t>(currentTimestamp) - static_cast<int32_t>(*timestamp);
    if (latency < -10000 || latency > 60000) {
        // Likely a false detection, re-detect next frame
        patternRegion_.reset();
        return result;
    }

    result.displayedTimestamp = *timestamp;
    result.latencyMs = latency;
    result.valid = true;

    return result;
}

std::optional<PatternRegion> LatencyMeasurer::detectPatternRegion(const VideoFrame* frame) {
    if (!frame || !frame->data) {
        return std::nullopt;
    }

    const int bytesPerPixel = 3;  // RGB24

    // First, look for the bright green border marker
    // Scan for regions with high green content
    for (int y = 10; y < frame->height - 60; y += 2) {
        for (int x = 10; x < frame->width - 200; x += 2) {
            // Check if this pixel is bright green
            int offset = y * frame->pitch + x * bytesPerPixel;
            uint8_t r = frame->data[offset];
            uint8_t g = frame->data[offset + 1];
            uint8_t b = frame->data[offset + 2];

            // Green marker: high green, low red, low blue
            if (g > 180 && r < 100 && b < 100) {
                // Found potential green marker, look for the pattern inside
                auto region = findPatternNearGreen(frame, x, y);
                if (region) {
                    return region;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<PatternRegion> LatencyMeasurer::findPatternNearGreen(const VideoFrame* frame, int greenX, int greenY) {
    const int bytesPerPixel = 3;

    // The green border is outside the white border
    // Search in a small area around this green pixel for the white border, then the pattern

    // Look for white region (the inner border)
    for (int dy = 0; dy < 20; dy++) {
        for (int dx = 0; dx < 20; dx++) {
            int checkY = greenY + dy;
            int checkX = greenX + dx;

            if (checkX >= frame->width || checkY >= frame->height) continue;

            int offset = checkY * frame->pitch + checkX * bytesPerPixel;
            uint8_t r = frame->data[offset];
            uint8_t g = frame->data[offset + 1];
            uint8_t b = frame->data[offset + 2];

            // White pixel (inside the green border)
            if (r > 200 && g > 200 && b > 200) {
                // Found white, now find the extent of the pattern
                // Scan right to find pattern width
                int patternStartX = checkX;
                int patternEndX = checkX;

                for (int sx = checkX; sx < frame->width && sx < checkX + 900; sx++) {
                    int sOffset = checkY * frame->pitch + sx * bytesPerPixel;
                    uint8_t sr = frame->data[sOffset];
                    uint8_t sg = frame->data[sOffset + 1];
                    uint8_t sb = frame->data[sOffset + 2];

                    // Still in pattern area (white, black, or gray)
                    int brightness = (sr + sg + sb) / 3;
                    bool isGreen = (sg > 180 && sr < 100 && sb < 100);

                    if (isGreen) {
                        // Hit the green border on the other side
                        patternEndX = sx;
                        break;
                    }
                }

                int patternWidth = patternEndX - patternStartX;
                if (patternWidth < 100 || patternWidth > 900) continue;

                // Find pattern height
                int patternStartY = checkY;
                int patternEndY = checkY;

                for (int sy = checkY; sy < frame->height && sy < checkY + 100; sy++) {
                    int sOffset = sy * frame->pitch + (checkX + patternWidth/2) * bytesPerPixel;
                    uint8_t sg = frame->data[sOffset + 1];
                    uint8_t sr = frame->data[sOffset];
                    uint8_t sb = frame->data[sOffset + 2];

                    bool isGreen = (sg > 180 && sr < 100 && sb < 100);
                    if (isGreen) {
                        patternEndY = sy;
                        break;
                    }
                }

                int patternHeight = patternEndY - patternStartY;
                if (patternHeight < 20 || patternHeight > 100) continue;

                // Validate: check for sync pattern (alternating bright/dark)
                int midY = patternStartY + patternHeight / 2;
                if (!validateSyncPattern(frame, patternStartX + 5, midY)) {
                    continue;
                }

                PatternRegion region;
                region.x = patternStartX;
                region.y = patternStartY;
                region.width = patternWidth;
                region.height = patternHeight;

                return region;
            }
        }
    }

    return std::nullopt;
}

bool LatencyMeasurer::validateSyncPattern(const VideoFrame* frame, int x, int y) {
    const int bytesPerPixel = 3;

    if (y < 0 || y >= frame->height) return false;

    // Check for alternating pattern: expect at least 3 transitions in first 80 pixels
    int transitions = 0;
    bool lastBright = false;
    bool firstSample = true;

    for (int dx = 0; dx < 80 && (x + dx) < frame->width; dx += 8) {
        int offset = y * frame->pitch + (x + dx) * bytesPerPixel;
        int brightness = (frame->data[offset] + frame->data[offset + 1] + frame->data[offset + 2]) / 3;
        bool isBright = brightness > brightnessThreshold_;

        if (firstSample) {
            lastBright = isBright;
            firstSample = false;
        } else if (isBright != lastBright) {
            transitions++;
            lastBright = isBright;
        }
    }

    return transitions >= 3;
}

std::optional<uint32_t> LatencyMeasurer::decodeBinaryPattern(const VideoFrame* frame,
                                                               const PatternRegion& region) {
    if (!frame || !frame->data) {
        return std::nullopt;
    }

    const int bytesPerPixel = 3;

    // Calculate bit dimensions based on region size
    // Account for the white border (PATTERN_BORDER on each side)
    int innerWidth = region.width - 2 * PATTERN_BORDER;
    int totalBits = SYNC_BITS + PATTERN_BITS + SYNC_BITS;
    float bitWidth = static_cast<float>(innerWidth) / totalBits;

    if (bitWidth < 2.0f) {
        return std::nullopt;  // Pattern too small
    }

    // Sample from middle of pattern height
    int sampleY = region.y + region.height / 2;
    if (sampleY < 0 || sampleY >= frame->height) {
        return std::nullopt;
    }

    const uint8_t* row = frame->data + sampleY * frame->pitch;

    // Skip border and start sync pattern, read data bits
    int dataStartX = region.x + PATTERN_BORDER + static_cast<int>(SYNC_BITS * bitWidth);

    uint32_t timestamp = 0;
    int highBits = 0;
    int lowBits = 0;

    for (int bit = 0; bit < PATTERN_BITS; bit++) {
        int sampleX = dataStartX + static_cast<int>((bit + 0.5f) * bitWidth);

        if (sampleX < 0 || sampleX >= frame->width) {
            return std::nullopt;
        }

        // Sample a small region for robustness
        int totalBrightness = 0;
        int samples = 0;

        for (int dx = -2; dx <= 2; dx++) {
            int xPos = sampleX + dx;
            if (xPos >= 0 && xPos < frame->width) {
                int offset = xPos * bytesPerPixel;
                totalBrightness += (row[offset] + row[offset + 1] + row[offset + 2]) / 3;
                samples++;
            }
        }

        int avgBrightness = samples > 0 ? totalBrightness / samples : 0;
        bool bitValue = avgBrightness > brightnessThreshold_;

        if (bitValue) {
            highBits++;
            timestamp |= (1U << (PATTERN_BITS - 1 - bit));
        } else {
            lowBits++;
        }
    }

    // Sanity check: should have a mix of high and low bits for a valid timestamp
    // (all zeros or all ones is suspicious)
    if (highBits < 2 || lowBits < 2) {
        return std::nullopt;
    }

    return timestamp;
}

uint8_t LatencyMeasurer::getRegionBrightness(const uint8_t* data, int pitch,
                                              int x, int y, int w, int h) const {
    const int bytesPerPixel = 3;
    int total = 0;
    int samples = 0;

    for (int dy = 0; dy < h; dy++) {
        const uint8_t* row = data + (y + dy) * pitch;
        for (int dx = 0; dx < w; dx++) {
            int offset = (x + dx) * bytesPerPixel;
            total += (row[offset] + row[offset + 1] + row[offset + 2]) / 3;
            samples++;
        }
    }

    return samples > 0 ? static_cast<uint8_t>(total / samples) : 0;
}

} // namespace latency
