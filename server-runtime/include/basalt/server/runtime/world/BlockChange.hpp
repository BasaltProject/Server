#pragma once

#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace basalt::server::runtime::world {

namespace blockEffect {

struct Sound {
    std::string name;
    wire::protocol::play::SoundSource source = wire::protocol::play::SoundSource::Blocks;
    float volume = 1.0F;
    float pitch = 1.0F;

    friend bool operator==(const Sound&, const Sound&) = default;
};

struct Event {
    std::int32_t b0 = 0;
    std::int32_t b1 = 0;

    friend bool operator==(const Event&, const Event&) = default;
};

struct Effect {
    static constexpr std::int32_t kDispenserDispense = 1000;
    static constexpr std::int32_t kDispenserFail = 1001;
    /// parity: `LevelEvent.SOUND_DISPENSER_PROJECTILE_LAUNCH`.
    static constexpr std::int32_t kDispenserShoot = 1002;
    static constexpr std::int32_t kFireworkShoot = 1004;
    static constexpr std::int32_t kFireChargeShoot = 1018;
    static constexpr std::int32_t kWindChargeShoot = 1051;
    static constexpr std::int32_t kDispenserSmoke = 2000;
    static constexpr std::int32_t kRedstoneTorchBurnout = 1502;
    /// parity: the click a crafter makes when what is in it is not a recipe.
    static constexpr std::int32_t kCrafterFail = 1050;
    static constexpr std::int32_t kLightningRodSparks = 3002;
    static constexpr std::int32_t kWaxOn = 3003;
    static constexpr std::int32_t kWaxOff = 3004;
    static constexpr std::int32_t kCopperScrape = 3005;
    static constexpr std::int32_t kPlantGrowth = 1505;
    static constexpr std::int32_t kComposterFill = 1500;
    static constexpr std::int32_t kPlayJukeboxSong = 1010;
    static constexpr std::int32_t kStopJukeboxSong = 1011;

    std::int32_t type = 0;
    std::int32_t data = 0;

    friend bool operator==(const Effect&, const Effect&) = default;
};

} // namespace blockEffect

using BlockEffect = std::variant<blockEffect::Sound, blockEffect::Event, blockEffect::Effect>;

struct BlockChange {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    std::vector<BlockEffect> effects;

    friend bool operator==(const BlockChange&, const BlockChange&) = default;
};

} // namespace basalt::server::runtime::world
