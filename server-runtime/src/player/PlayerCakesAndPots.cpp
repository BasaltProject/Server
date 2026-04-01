#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerCakesAndPots.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: `CakeBlock.MAX_BITES`.
constexpr std::int32_t kMaxBites = 6;
/// parity: `CakeBlock.eat`, two food at a tenth saturation.
constexpr std::int32_t kSliceFood = 2;
constexpr float kSliceSaturation = 0.1F;
/// parity: `CandleCakeBlock.candleHit`, the top half of the block.
constexpr float kCandleHitY = 0.5F;

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

void spendOne(const BreakContext& context, PlayerSession& player, std::int32_t slot,
              const storage::StoredItemStack& held) {
    if (player.gamemode == Gamemode::Creative) {
        return;
    }
    storage::StoredItemStack left = held;
    left.count = held.count - 1;
    player.inventory.set(slot, held.count <= 1 ? std::nullopt : std::optional{left});
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

void write(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
           std::int32_t y, std::int32_t z, std::int32_t stateId, std::string_view event) {
    if (!world.state().setBlockState(x, y, z, stateId)) {
        return;
    }
    announceBlock(context, world.name(), x, y, z, stateId);
    notifyAround(context, world, x, y, z, std::nullopt);
    heardBySculk(context, world, event, x, y, z, {.entity = player.uuid()});
}

void playBlockSound(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                    std::int32_t z, std::int32_t stateId, std::string sound) {
    if (context.blocks == nullptr) {
        return;
    }
    context.blocks->sendEffects(
        playersIn(context, world.name()), x, y, z, stateId,
        {world::blockEffect::Sound{.name = std::move(sound), .source = wire::protocol::play::SoundSource::Blocks}});
}

/// parity: `Player.canEat(false)`, which an invulnerable player always may.
[[nodiscard]] bool canEat(const PlayerSession& player) {
    return player.gamemode == Gamemode::Creative || player.gamemode == Gamemode::Spectator ||
           player.food.foodLevel() < 20;
}

/// parity: `CakeBlock.eat` over whatever cake state is being eaten.
[[nodiscard]] bool eat(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                       std::int32_t y, std::int32_t z, std::int32_t bites) {
    if (!canEat(player)) {
        return false;
    }
    statAwards::award(player, "eat_cake_slice");
    player.food.eat(kSliceFood, kSliceSaturation);
    if (player.connection().syncHealth) {
        player.connection().syncHealth();
    }
    heardBySculk(context, world, "eat", x, y, z, {.entity = player.uuid()});
    if (bites < kMaxBites) {
        const std::optional<std::int32_t> next =
            reg::blockStateDefinitions::stateIdOf(Identifier::vanilla("cake"), {{"bites", std::to_string(bites + 1)}});
        if (next.has_value()) {
            write(context, player, world, x, y, z, *next, "block_change");
        }
        return true;
    }
    const std::int32_t air = reg::blocks::air().stateId;
    if (world.state().setBlockState(x, y, z, air)) {
        announceBlock(context, world.name(), x, y, z, air);
        notifyAround(context, world, x, y, z, std::nullopt);
        heardBySculk(context, world, "block_destroy", x, y, z, {.entity = player.uuid()});
    }
    return true;
}

/// parity: `POTTED_BY_CONTENT`, which names each pot after its plant except the two azaleas.
[[nodiscard]] std::optional<Identifier> pottedFor(const Identifier& item) {
    if (item.namespaceName() != "minecraft") {
        return std::nullopt;
    }
    const std::string path = item.path() == "azalea"             ? "potted_azalea_bush"
                             : item.path() == "flowering_azalea" ? "potted_flowering_azalea_bush"
                                                                 : "potted_" + item.path();
    const Identifier potted = Identifier::vanilla(path);
    return reg::blocks::defaultStateIdOf(potted).has_value() ? std::optional{potted} : std::nullopt;
}

[[nodiscard]] Identifier plantOf(const Identifier& pot) {
    const std::string& path = pot.path();
    if (path == "potted_azalea_bush") {
        return Identifier::vanilla("azalea");
    }
    if (path == "potted_flowering_azalea_bush") {
        return Identifier::vanilla("flowering_azalea");
    }
    return Identifier::vanilla(path.substr(std::string_view{"potted_"}.size()));
}

} // namespace

bool useCakeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                const std::optional<storage::StoredItemStack>& held, float clickY) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr) {
        return false;
    }
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*stateId);
    if (block->path() == "cake") {
        const std::int32_t bites = std::stoi(properties.contains("bites") ? properties.at("bites") : "0");
        // parity: a candle, on a cake nobody has bitten, becomes that candle's candle cake.
        if (item.has_value() && bites == 0 && (item->path() == "candle" || item->path().ends_with("_candle"))) {
            const std::optional<std::int32_t> candleCake =
                reg::blocks::defaultStateIdOf(Identifier::vanilla(item->path() + "_cake"));
            if (candleCake.has_value()) {
                spendOne(context, player, heldSlot, *held);
                playBlockSound(context, world, x, y, z, *stateId, "block.cake.add_candle");
                write(context, player, world, x, y, z, *candleCake, "block_change");
                statAwards::used(player, held->id);
                return true;
            }
        }
        return eat(context, player, world, x, y, z, bites);
    }
    if (!block->path().ends_with("candle_cake")) {
        return false;
    }
    // parity: `CandleCakeBlock.useItemOn`, which leaves a flint and steel or a fire charge alone.
    if (item.has_value() && (item->path() == "flint_and_steel" || item->path() == "fire_charge")) {
        return false;
    }
    if (!held.has_value() && clickY > kCandleHitY && properties.contains("lit") && properties.at("lit") == "true") {
        const std::optional<std::int32_t> out =
            reg::blockStateDefinitions::stateIdOf(*block, {{"lit", "false"}}, *stateId);
        if (out.has_value()) {
            write(context, player, world, x, y, z, *out, "block_change");
            playBlockSound(context, world, x, y, z, *out, "block.candle.extinguish");
        }
        return true;
    }
    // parity: `CandleCakeBlock.useWithoutItem` eats a plain cake, and only then drops the candle.
    if (!canEat(player)) {
        return false;
    }
    const std::int32_t candleCake = *stateId;
    static_cast<void>(eat(context, player, world, x, y, z, 0));
    dropLootFor(context, world, *block, candleCake, x, y, z, nullptr);
    return true;
}

bool useFlowerPotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || (block->path() != "flower_pot" && !block->path().starts_with("potted_"))) {
        return false;
    }
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const std::optional<Identifier> potted = item.has_value() ? pottedFor(*item) : std::nullopt;
    const bool empty = block->path() == "flower_pot";
    if (potted.has_value()) {
        // parity: a pot that already holds something answers `CONSUME` and changes nothing.
        if (!empty) {
            return true;
        }
        write(context, player, world, x, y, z, *reg::blocks::defaultStateIdOf(*potted), "block_change");
        statAwards::award(player, "pot_flower");
        spendOne(context, player, heldSlot, *held);
        return true;
    }
    // parity: `useWithoutItem`, which spends the click on an empty pot too.
    if (empty) {
        return true;
    }
    const storage::StoredItemStack plant{.id = plantOf(*block).toString(), .count = 1, .components = {}};
    if (player.inventory.add(plant) > 0) {
        spawnDroppedItem(context, world, plantOf(*block), 1, player.x, player.y + 1.32, player.z, 0.0, 0.0, 0.0, 40,
                         wire::nbt::NbtCompound{});
    }
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
    write(context, player, world, x, y, z, *reg::blocks::defaultStateIdOf(Identifier::vanilla("flower_pot")),
          "block_change");
    return true;
}

} // namespace basalt::server::runtime::player
