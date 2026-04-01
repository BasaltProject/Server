#pragma once

#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace basalt::server::runtime::world {

/// What a block asks the clients to do besides changing state.
///
/// parity: `Level.playSound`, `Level.blockEvent` and `Level.levelEvent`. A block reacting on its own
/// has no player to answer to, so without this its note or its ring had nowhere to go.
namespace blockEffect {

/// parity: `Level.playSound`, at the middle of the block.
struct Sound {
    std::string name;
    wire::protocol::play::SoundSource source = wire::protocol::play::SoundSource::Blocks;
    float volume = 1.0F;
    float pitch = 1.0F;

    friend bool operator==(const Sound&, const Sound&) = default;
};

/// parity: `Level.blockEvent`, whose two bytes mean whatever the block says they mean.
struct Event {
    std::int32_t b0 = 0;
    std::int32_t b1 = 0;

    friend bool operator==(const Event&, const Event&) = default;
};

/// parity: `Level.levelEvent`, the numbered one-off effects (`LevelEvent`).
struct Effect {
    /// parity: `LevelEvent.SOUND_DISPENSER_FAIL`, the click of a dispenser with nothing in it.
    static constexpr std::int32_t kDispenserFail = 1001;
    /// parity: `LevelEvent.SOUND_FIREWORK_SHOOT`, the dispense sound a rocket plays instead.
    static constexpr std::int32_t kFireworkShoot = 1004;
    /// parity: `LevelEvent.PARTICLES_SHOOT_SMOKE`, the puff out of the face, whose data is the face.
    static constexpr std::int32_t kDispenserSmoke = 2000;
    /// parity: `LevelEvent.PARTICLES_AND_SOUND_BRUSH_BLOCK_COMPLETE`'s neighbour in the list, the
    /// puff of smoke a redstone torch makes when it burns out.
    static constexpr std::int32_t kRedstoneTorchBurnout = 1502;
    /// parity: the click a crafter makes when what is in it is not a recipe.
    static constexpr std::int32_t kCrafterFail = 1050;
    /// parity: the sparks off a lightning rod, whose data is the rod's axis.
    static constexpr std::int32_t kLightningRodSparks = 3002;
    /// parity: `LevelEvent.PARTICLES_AND_SOUND_WAX_ON`, and the two an axe raises taking it off
    /// again and scraping a step of oxidation away.
    static constexpr std::int32_t kWaxOn = 3003;
    static constexpr std::int32_t kWaxOff = 3004;
    static constexpr std::int32_t kCopperScrape = 3005;
    /// parity: `LevelEvent.PARTICLES_AND_SOUND_PLANT_GROWTH`, the green sparkles bone meal makes,
    /// whose data is how many of them to draw.
    static constexpr std::int32_t kPlantGrowth = 1505;
    /// parity: `LevelEvent.COMPOSTER_FILL`, whose data says whether the layer went up: the client
    /// turns a one into the success sound and the green sparkles, and a zero into the plain fill.
    static constexpr std::int32_t kComposterFill = 1500;
    /// parity: `LevelEvent.SOUND_PLAY_JUKEBOX_SONG`, whose data is the song's `jukebox_song` id.
    static constexpr std::int32_t kPlayJukeboxSong = 1010;
    /// parity: `LevelEvent.SOUND_STOP_JUKEBOX_SONG`.
    static constexpr std::int32_t kStopJukeboxSong = 1011;

    std::int32_t type = 0;
    std::int32_t data = 0;

    friend bool operator==(const Effect&, const Effect&) = default;
};

} // namespace blockEffect

/// One of the three, as a variant rather than a hierarchy: there is no RTTI here to ask what a
/// pointer really is, and three cases that never grow do not need one.
using BlockEffect = std::variant<blockEffect::Sound, blockEffect::Event, blockEffect::Effect>;

/// A block that changed, and whatever it wants the clients to hear or see about it.
struct BlockChange {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    std::vector<BlockEffect> effects;

    friend bool operator==(const BlockChange&, const BlockChange&) = default;
};

} // namespace basalt::server::runtime::world
