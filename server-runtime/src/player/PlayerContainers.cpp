#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <string>
#include <string_view>

namespace basalt::server::runtime::player {
namespace {

using wire::common::Identifier;

/// parity: `CopperChestBlock` and `ChestBlockEntity` sound families for lidded and barrel containers.
[[nodiscard]] std::string_view soundFamilyOf(const std::string_view path) {
    if (path == "barrel") {
        return "barrel";
    }
    if (path == "ender_chest") {
        return "ender_chest";
    }
    if (world::shulkers::isShulkerBox(path)) {
        return "shulker_box";
    }
    if (path == "weathered_copper_chest" || path == "waxed_weathered_copper_chest") {
        return "copper_chest_weathered";
    }
    if (path == "oxidized_copper_chest" || path == "waxed_oxidized_copper_chest") {
        return "copper_chest_oxidized";
    }
    if (path == "copper_chest" || path == "exposed_copper_chest" || path == "waxed_copper_chest" ||
        path == "waxed_exposed_copper_chest") {
        return "copper_chest";
    }
    if (path == "chest" || path == "trapped_chest") {
        return "chest";
    }
    return {};
}

} // namespace

void announceOpeners(const OpenersContext& context, world::LoadedWorld& world, const std::int32_t x,
                     const std::int32_t y, const std::int32_t z, const std::string_view block,
                     const std::int32_t openers, const std::optional<wire::common::Uuid> opener) {
    const std::string_view family = soundFamilyOf(block);
    if (family.empty()) {
        return;
    }
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path{block};
    if (path == "barrel") {
        // parity: a barrel has no lid to raise, it opens its own block state instead.
        const std::optional<std::int32_t> opened = registry::blockStateDefinitions::stateIdOf(
            Identifier::vanilla("barrel"), {{"open", openers > 0 ? "true" : "false"}}, *stateId);
        if (opened.has_value() && *opened != *stateId && world.state().setBlockState(x, y, z, *opened)) {
            announceBlock(context.blocks, world.name(), x, y, z, *opened);
        }
    } else {
        for (const PlayerSession* player : playersIn(context.blocks, world.name())) {
            if (player->connection().sendBlockEvent) {
                player->connection().sendBlockEvent(x, y, z, *stateId, 1, openers);
            }
        }
    }

    // Only the first in and the last out make a sound, which is why a busy chest is not a rattle.
    std::string sound;
    if (openers == 1) {
        sound = "block." + std::string{family} + ".open";
    } else if (openers == 0) {
        sound = "block." + std::string{family} + ".close";
    } else {
        return;
    }
    // parity: `ChestBlockEntity.playSound` skips the `LEFT` half and plays from the midpoint on the `RIGHT` half.
    double soundX = x + 0.5;
    double soundZ = z + 0.5;
    if (const auto halves = world::doubleChests::halves(world.state(), x, y, z); halves.has_value()) {
        if (x == halves->second[0] && y == halves->second[1] && z == halves->second[2]) {
            return;
        }
        soundX = (static_cast<double>(halves->first[0] + halves->second[0]) * 0.5) + 0.5;
        soundZ = (static_cast<double>(halves->first[2] + halves->second[2]) * 0.5) + 0.5;
    }
    // parity: `CONTAINER_OPEN` on the first in and `CONTAINER_CLOSE` on the last out, with the player.
    if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
        world::vibrations::emit(world.state(), openers == 1 ? "container_open" : "container_close", x + 0.5, y + 0.5,
                                z + 0.5, world.gameTime(), {.entity = opener});
    }
    const float roll = context.nextFloat ? context.nextFloat() : 0.5F;
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* player : playersIn(context.blocks, world.name())) {
        // parity: `ChestBlockEntity.playSound`, half volume and a tenth of a tone of randomness.
        playSoundFor(*player, sound, wire::protocol::play::SoundSource::Blocks, soundX, y + 0.5, soundZ, 0.5F,
                     (roll * 0.1F) + 0.9F, seed);
    }
}

} // namespace basalt::server::runtime::player
