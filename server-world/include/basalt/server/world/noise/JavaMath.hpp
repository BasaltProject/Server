#pragma once

#include <cmath>
#include <cstdint>

namespace basalt::server::world::noise::javaMath {

[[nodiscard]] inline float fmin(float a, float b) noexcept {
    if (std::isnan(a) || std::isnan(b)) {
        return a + b;
    }
    if (a == 0.0F && b == 0.0F) {
        return std::signbit(a) ? a : b;
    }
    return a <= b ? a : b;
}

[[nodiscard]] inline float fmax(float a, float b) noexcept {
    if (std::isnan(a) || std::isnan(b)) {
        return a + b;
    }
    if (a == 0.0F && b == 0.0F) {
        return std::signbit(a) ? b : a;
    }
    return a >= b ? a : b;
}

[[nodiscard]] inline double pow(double base, double exponent) noexcept { return std::pow(base, exponent); }

[[nodiscard]] inline float signum(float value) noexcept {
    if (value == 0.0F || std::isnan(value)) {
        return value;
    }
    return value > 0.0F ? 1.0F : -1.0F;
}

[[nodiscard]] constexpr std::int32_t floorDiv(std::int32_t x, std::int32_t y) noexcept {
    const std::int32_t q = x / y;
    return (x % y != 0 && ((x < 0) != (y < 0))) ? q - 1 : q;
}

[[nodiscard]] constexpr std::int32_t floorMod(std::int32_t x, std::int32_t y) noexcept {
    return x - floorDiv(x, y) * y;
}

[[nodiscard]] inline std::int32_t toInt(double value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    if (value >= 2147483647.0) {
        return 2147483647;
    }
    if (value <= -2147483648.0) {
        return -2147483647 - 1;
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] inline std::int32_t floor(double value) noexcept { return toInt(std::floor(value)); }

[[nodiscard]] inline std::int32_t floor(float value) noexcept { return toInt(std::floor(static_cast<double>(value))); }

[[nodiscard]] constexpr float lerp(float alpha, float p0, float p1) noexcept { return p0 + alpha * (p1 - p0); }

[[nodiscard]] constexpr float lerp2(float a1, float a2, float x00, float x10, float x01, float x11) noexcept {
    return lerp(a2, lerp(a1, x00, x10), lerp(a1, x01, x11));
}

[[nodiscard]] constexpr float lerp3(float a1, float a2, float a3, float x000, float x100, float x010, float x110,
                                    float x001, float x101, float x011, float x111) noexcept {
    return lerp(a3, lerp2(a1, a2, x000, x100, x010, x110), lerp2(a1, a2, x001, x101, x011, x111));
}

[[nodiscard]] constexpr float smoothstep(float x) noexcept { return x * x * x * (x * (x * 6.0F - 15.0F) + 10.0F); }

[[nodiscard]] inline float clamp(float value, float min, float max) noexcept {
    return value < min ? min : fmin(value, max);
}

} // namespace basalt::server::world::noise::javaMath
