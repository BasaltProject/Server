#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

namespace beacons {

inline constexpr std::int32_t kMaxLevels = 4;

inline constexpr std::int64_t kApplyInterval = 80;

inline constexpr std::int32_t kFullDampening = 15;

[[nodiscard]] std::span<const std::span<const std::string_view>> effectTiers();

using IsBaseBlock = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] std::int32_t levelsUnder(const IsBaseBlock& isBase, std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::int32_t minY);

using PassesBeam = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] bool beamReachesSky(const PassesBeam& passes, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::int32_t maxY);

[[nodiscard]] bool letsBeamThrough(std::int32_t dampening, std::string_view block);

[[nodiscard]] bool allows(std::string_view primary, std::string_view secondary, std::int32_t levels);

struct Beam {
    double range = 0.0;
    std::int32_t amplifier = 0;
    std::int32_t durationTicks = 0;
};

[[nodiscard]] Beam beamOf(std::int32_t levels, bool secondaryIsPrimary);

[[nodiscard]] bool reaches(const Beam& beam, std::int32_t x, std::int32_t y, std::int32_t z, double px, double py,
                           double pz);

struct Stored {
    std::string primary;
    std::string secondary;
    std::int32_t levels = 0;
};

[[nodiscard]] Stored read(const wire::nbt::NbtCompound& data);

void write(const Stored& stored, wire::nbt::NbtCompound& data);

} // namespace beacons

} // namespace basalt::server::runtime::world
