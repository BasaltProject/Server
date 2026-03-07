#include <basalt/server/runtime/world/Beacons.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace basalt::server::runtime::world::beacons {
namespace {

constexpr std::array<std::string_view, 2> kFirst{"minecraft:speed", "minecraft:haste"};
constexpr std::array<std::string_view, 2> kSecond{"minecraft:resistance", "minecraft:jump_boost"};
constexpr std::array<std::string_view, 1> kThird{"minecraft:strength"};
constexpr std::array<std::string_view, 1> kFourth{"minecraft:regeneration"};

constexpr std::array<std::span<const std::string_view>, 4> kTiers{kFirst, kSecond, kThird, kFourth};

/// What the block entity calls its two effects, and what this server calls the height it counted.
constexpr std::string_view kPrimaryKey = "primary_effect";
constexpr std::string_view kSecondaryKey = "secondary_effect";
constexpr std::string_view kLevelsKey = "Levels";

/// parity: `getRequiredLevelsFor`: nothing needs none, and something nobody offers needs more than
/// any pyramid can be.
[[nodiscard]] std::int32_t levelsFor(const std::string_view effect) {
    if (effect.empty()) {
        return 0;
    }
    for (std::size_t tier = 0; tier < kTiers.size(); ++tier) {
        if (std::ranges::find(kTiers[tier], effect) != kTiers[tier].end()) {
            return static_cast<std::int32_t>(tier) + 1;
        }
    }
    return std::numeric_limits<std::int32_t>::max();
}

} // namespace

std::span<const std::span<const std::string_view>> effectTiers() { return kTiers; }

std::int32_t levelsUnder(const IsBaseBlock& isBase, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                         const std::int32_t minY) {
    if (!isBase) {
        return 0;
    }
    std::int32_t levels = 0;
    for (std::int32_t step = 1; step <= kMaxLevels; ++step) {
        const std::int32_t below = y - step;
        if (below < minY) {
            break;
        }
        bool whole = true;
        for (std::int32_t px = x - step; px <= x + step && whole; ++px) {
            for (std::int32_t pz = z - step; pz <= z + step; ++pz) {
                if (!isBase(px, below, pz)) {
                    whole = false;
                    break;
                }
            }
        }
        if (!whole) {
            break;
        }
        levels = step;
    }
    return levels;
}

bool letsBeamThrough(const std::int32_t dampening, std::string_view block) {
    if (block.starts_with("minecraft:")) {
        block.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    return dampening < kFullDampening || block == "bedrock";
}

bool beamReachesSky(const PassesBeam& passes, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                    const std::int32_t maxY) {
    if (!passes) {
        return false;
    }
    for (std::int32_t above = y + 1; above <= maxY; ++above) {
        if (!passes(x, above, z)) {
            return false;
        }
    }
    return true;
}

bool allows(const std::string_view primary, const std::string_view secondary, const std::int32_t levels) {
    if (!secondary.empty() && levels < kMaxLevels) {
        return false;
    }
    const std::int32_t primaryLevel = levelsFor(primary);
    const std::int32_t secondaryLevel = levelsFor(secondary);
    if (primaryLevel > levels || secondaryLevel > levels) {
        return false;
    }
    // parity: the fourth tier is offered as a second effect and never as the first.
    if (primaryLevel >= kMaxLevels) {
        return false;
    }
    return secondaryLevel == 0 || secondaryLevel >= kMaxLevels || primary == secondary;
}

Beam beamOf(const std::int32_t levels, const bool secondaryIsPrimary) {
    /// parity: ten blocks per level and ten more, and nine seconds plus two per level.
    constexpr double kRangePerLevel = 10.0;
    constexpr std::int32_t kBaseSeconds = 9;
    constexpr std::int32_t kSecondsPerLevel = 2;
    constexpr std::int32_t kTicksPerSecond = 20;

    return Beam{.range = (static_cast<double>(levels) * kRangePerLevel) + kRangePerLevel,
                .amplifier = levels >= kMaxLevels && secondaryIsPrimary ? 1 : 0,
                .durationTicks = (kBaseSeconds + (levels * kSecondsPerLevel)) * kTicksPerSecond};
}

bool reaches(const Beam& beam, const std::int32_t x, const std::int32_t y, const std::int32_t z, const double px,
             const double py, const double pz) {
    return px >= x - beam.range && px <= x + 1 + beam.range && pz >= z - beam.range && pz <= z + 1 + beam.range &&
           py >= y - beam.range;
}

Stored read(const wire::nbt::NbtCompound& data) {
    const auto text = [&data](const std::string_view name) {
        const wire::nbt::NbtTag* value = data.find(name);
        const std::string* held = value == nullptr ? nullptr : value->get<std::string>();
        return held == nullptr ? std::string{} : *held;
    };
    return Stored{.primary = text(kPrimaryKey),
                  .secondary = text(kSecondaryKey),
                  .levels = data.contains(kLevelsKey) ? data.getInt(kLevelsKey) : 0};
}

void write(const Stored& stored, wire::nbt::NbtCompound& data) {
    const auto put = [&data](const std::string_view name, const std::string& value) {
        if (value.empty()) {
            static_cast<void>(data.remove(name));
            return;
        }
        data.putString(name, value);
    };
    put(kPrimaryKey, stored.primary);
    put(kSecondaryKey, stored.secondary);
    data.putInt(kLevelsKey, stored.levels);
}

} // namespace basalt::server::runtime::world::beacons
