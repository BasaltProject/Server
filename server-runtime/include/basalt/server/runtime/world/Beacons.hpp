#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// A beacon: how tall its pyramid is, what a pyramid that tall may offer, and what it hands out.
///
/// parity: `BeaconBlockEntity`. The beam itself is drawn by the client off the blocks above, so
/// nothing here draws anything: what a server owes is the pyramid, the two effects and the handing
/// out every four seconds.
namespace beacons {

/// parity: `MAX_LEVELS`, and the same number of levels a second effect needs.
inline constexpr std::int32_t kMaxLevels = 4;

/// parity: the eighty ticks between one handing out and the next.
inline constexpr std::int64_t kApplyInterval = 80;

/// parity: the light dampening of a square that lets nothing through.
inline constexpr std::int32_t kFullDampening = 15;

/// parity: `BEACON_EFFECTS`, one list per level of pyramid. The fourth is the one a beacon will
/// only ever offer as its second effect.
[[nodiscard]] std::span<const std::span<const std::string_view>> effectTiers();

/// Whether that square holds a block a pyramid may be built of.
using IsBaseBlock = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `updateBase`: how many full squares of base blocks stand under the beacon, counting
/// outwards, stopping at the first ring that is not whole.
[[nodiscard]] std::int32_t levelsUnder(const IsBaseBlock& isBase, std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::int32_t minY);

/// Whether the beam gets out: every square from just above the beacon up has to let it past.
///
/// parity: `BeaconBlockEntity.tick` walks to the world surface, which is the highest square that is
/// not air, so walking to the top of the world answers the same and needs no heightmap.
using PassesBeam = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] bool beamReachesSky(const PassesBeam& passes, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::int32_t maxY);

/// parity: `state.getLightDampening() >= 15` stops the beam, and bedrock is the one square that
/// stops all the light and lets the beam through anyway.
[[nodiscard]] bool letsBeamThrough(std::int32_t dampening, std::string_view block);

/// parity: `validateEffects`. An empty name is "none", which both may be.
[[nodiscard]] bool allows(std::string_view primary, std::string_view secondary, std::int32_t levels);

/// What a working beacon hands out. parity: `applyEffects`.
struct Beam {
    /// How far it reaches sideways. Its own column reaches as high as the world does.
    double range = 0.0;
    std::int32_t amplifier = 0;
    std::int32_t durationTicks = 0;
};

[[nodiscard]] Beam beamOf(std::int32_t levels, bool secondaryIsPrimary);

/// Whether somebody standing there is handed what the beacon at that block gives out.
///
/// parity: `applyEffects` inflates the beacon's own square by the reach and then opens the top of
/// it to the roof of the world, so the floor is the reach below the block and the ceiling is not
/// one.
[[nodiscard]] bool reaches(const Beam& beam, std::int32_t x, std::int32_t y, std::int32_t z, double px, double py,
                           double pz);

/// What a beacon's own block entity keeps. parity: `primary_effect` and `secondary_effect`, which
/// 26.2 writes as names rather than as the numbers older worlds used.
struct Stored {
    std::string primary;
    std::string secondary;
    /// How tall the pyramid was the last time it was counted. Not vanilla's to save, but the screen
    /// reads it and counting it again for every click would walk the same eighty squares.
    std::int32_t levels = 0;
};

[[nodiscard]] Stored read(const wire::nbt::NbtCompound& data);

void write(const Stored& stored, wire::nbt::NbtCompound& data);

} // namespace beacons

} // namespace basalt::server::runtime::world
