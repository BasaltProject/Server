#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerFaces.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;

/// The noise a block makes under a hand, to everyone in that world at once.
void playBlockSound(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                    const std::int32_t z, const std::int32_t stateId, const std::string& sound, const float volume,
                    const float pitch) {
    if (context.blocks == nullptr) {
        return;
    }
    const std::vector<world::BlockEffect> effects{world::blockEffect::Sound{
        .name = sound, .source = wire::protocol::play::SoundSource::Blocks, .volume = volume, .pitch = pitch}};
    context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, stateId, effects);
}

/// parity: `BellBlock.isProperHit`, which will not ring from the very top of the block.
constexpr float kBellHitTop = 0.8124F;

std::string axisOf(const std::string_view direction) {
    return direction == "north" || direction == "south" ? "z" : "x";
}

/// parity: `ItemFrame`'s own direction numbering, which a block event reuses for the bell.
std::int32_t frameDirectionData(const std::string_view facing) {
    if (facing == "south") {
        return 0;
    }
    if (facing == "west") {
        return 1;
    }
    return facing == "north" ? 2 : 3;
}

} // namespace

bool tuneNoteBlockFor(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x,
                      const std::int32_t y, const std::int32_t z) {
    // parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
    const std::optional<world::BlockChange> change = world::noteBlocks::tune(world.state(), x, y, z);
    if (!change.has_value()) {
        return false;
    }
    announceBlock(context, world.name(), x, y, z, change->stateId);
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, change->stateId, change->effects);
    }
    return true;
}

bool ringBellFor(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                 const std::int32_t z, const wire::protocol::play::Direction face, const float clickY) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value() || face == wire::protocol::play::Direction::Up ||
        face == wire::protocol::play::Direction::Down || clickY > kBellHitTop) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    const auto facing = properties.find("facing");
    const bool sameAxis = axisOf(facing == properties.end() ? "north" : facing->second) == axisOf(faceName(face));
    const auto attachment = properties.find("attachment");
    const std::string mounting = attachment == properties.end() ? std::string{} : attachment->second;
    // parity: `BellBlock.isProperHit`, which is what a bell can swing towards.
    if (mounting != "ceiling" && (mounting == "floor" ? !sameAxis : sameAxis)) {
        return false;
    }
    if (context.blocks == nullptr) {
        return false;
    }
    const std::vector<world::BlockEffect> effects{
        world::blockEffect::Event{.b0 = 1, .b1 = frameDirectionData(faceName(face))},
        world::blockEffect::Sound{.name = "block.bell.use",
                                  .source = wire::protocol::play::SoundSource::Blocks,
                                  .volume = 2.0F,
                                  .pitch = 1.0F}};
    context.blocks->sendEffects(playersIn(context, world.name()), x, y, z, *stateId, effects);
    return true;
}

bool toggleBlockFor(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                    const std::int32_t z, const float yaw) {
    const std::optional<std::int32_t> before = world.state().blockState(x, y, z);
    const std::optional<world::blockInteractions::BlockToggle> toggled =
        world::blockInteractions::toggle(world.state(), x, y, z, yaw);
    if (!toggled.has_value()) {
        return false;
    }
    // parity: `ButtonBlock.press` schedules its own release. Everything else stays put until
    // something touches it again.
    if (const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(before.value_or(0));
        pressed.has_value()) {
        if (const wire::common::Identifier* button = reg::blockStateDefinitions::blockOf(before.value_or(0));
            button != nullptr) {
            world.state().scheduleBlockTick(x, y, z, *button, *pressed);
        }
    }
    for (const world::BlockChange& change : toggled->changes) {
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
    }
    // parity: `LeverBlock.updateNeighbours`, twice over: the block itself and whatever it is stuck
    // to, or a torch on the far side of the wall never hears the flip.
    for (const world::BlockChange& change : toggled->changes) {
        notifyAround(context, world, change.x, change.y, change.z, std::nullopt);
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(change.stateId, change.x, change.y, change.z);
            wall.has_value()) {
            notifyAround(context, world, (*wall)[0], (*wall)[1], (*wall)[2], std::nullopt);
        }
    }
    playBlockSound(context, world, x, y, z, before.value_or(0), toggled->sound, 1.0F, 1.0F);
    // parity: `LeverBlock.pull` and `ButtonBlock.press` raise `BLOCK_ACTIVATE` or `BLOCK_DEACTIVATE`
    // by the new `powered`, and a door, a trapdoor or a gate raise `BLOCK_OPEN` or `BLOCK_CLOSE` by
    // the new `open`. A sculk sensor beside one hears the flip.
    if (const std::optional<std::int32_t> after = world.state().blockState(x, y, z); after.has_value()) {
        const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*after);
        if (const auto open = properties.find("open"); open != properties.end()) {
            heardBySculk(context, world, open->second == "true" ? "block_open" : "block_close", x, y, z);
        } else if (const auto powered = properties.find("powered"); powered != properties.end()) {
            heardBySculk(context, world, powered->second == "true" ? "block_activate" : "block_deactivate", x, y, z);
        }
    }
    return true;
}

bool cycleBlockFor(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                   const std::int32_t z) {
    const std::optional<world::blockInteractions::BlockCycle> cycled =
        world::blockInteractions::cycle(world.state(), x, y, z);
    if (!cycled.has_value()) {
        return false;
    }
    announceBlock(context, world.name(), x, y, z, cycled->stateId);
    // parity: `DaylightDetectorBlock.useWithoutItem` writes `inverted` telling nobody and then lets
    // `updateSignalStrength` write the reading, which is the write that carries.
    if (const std::optional<world::BlockChange> reread =
            world::daylightDetectors::update(world.state(), world.clock(), world.skyDarken(), x, y, z,
                                             config::dimensionFacts(world.declaration().dimension).hasSkylight,
                                             config::dimensionFacts(world.declaration().dimension).hasDayTimeline);
        reread.has_value()) {
        announceBlock(context, world.name(), x, y, z, reread->stateId);
    }
    // parity: every one of these is a component, so the change has to reach the dust and the diodes
    // around them.
    notifyAround(context, world, x, y, z, std::nullopt);
    if (cycled->sound.has_value()) {
        playBlockSound(context, world, x, y, z, cycled->stateId, *cycled->sound, cycled->volume, cycled->pitch);
    }
    return true;
}

} // namespace basalt::server::runtime::player
