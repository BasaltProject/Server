#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: what a player throws from, which is where a leftover bowl lands when the bag is full.
constexpr double kThrowEyeHeight = 1.62;

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

void applyConsumeEffectsFor(PlayerSession& player, const std::vector<reg::ConsumeEffect>& effects,
                            const std::function<float()>& nextFloat) {
    for (const reg::ConsumeEffect& effect : effects) {
        if (effect.as<reg::ConsumeEffect::ClearAll>() != nullptr) {
            std::vector<Identifier> held;
            held.reserve(player.effects.size());
            for (const auto& [id, active] : player.effects) {
                held.push_back(id);
            }
            for (const Identifier& id : held) {
                player.effects.erase(id);
                if (player.connection().sendEffectRemoved) {
                    player.connection().sendEffectRemoved(id);
                }
            }
            continue;
        }
        if (const auto* removed = effect.as<reg::ConsumeEffect::Remove>(); removed != nullptr) {
            if (player.effects.erase(removed->effect) > 0 && player.connection().sendEffectRemoved) {
                player.connection().sendEffectRemoved(removed->effect);
            }
            continue;
        }
        const auto* applied = effect.as<reg::ConsumeEffect::Apply>();
        if (applied == nullptr) {
            continue;
        }
        if (applied->probability < 1.0F && (!nextFloat || nextFloat() >= applied->probability)) {
            continue;
        }
        const world::ActiveEffect active{
            .effect = applied->effect, .amplifier = applied->amplifier, .remainingTicks = applied->durationTicks};
        player.effects.insert_or_assign(applied->effect, active);
        if (player.connection().sendEffect) {
            player.connection().sendEffect(active);
        }
    }
}

bool eatHeldItem(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t slot,
                 const std::function<float()>& nextFloat) {
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    const std::optional<Identifier> id = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const reg::FoodValue* food = id.has_value() ? reg::itemFood::of(*id) : nullptr;
    if (food == nullptr) {
        return false;
    }
    const storage::StoredItemStack eaten = *held;

    // parity: `Consumable.onConsume` raises `DRINK` for the four the jar builds with the drinking
    // animation and `EAT` for everything else.
    static constexpr std::array<std::string_view, 4> kDrunk{"milk_bucket", "potion", "honey_bottle", "ominous_bottle"};
    const bool drunk = std::ranges::find(kDrunk, id->path()) != kDrunk.end();
    world::vibrations::emit(world.state(), drunk ? "drink" : "eat", player.x, player.y, player.z, world.gameTime());

    player.food.eat(food->nutrition, food->saturation);
    applyConsumeEffectsFor(player, food->effects, nextFloat);
    if (player.gamemode != Gamemode::Creative) {
        const std::int32_t remaining = eaten.count - 1;
        // parity: `use_remainder` replaces what was eaten: the bowl, the bottle, the bucket.
        std::optional<storage::StoredItemStack> leftover;
        if (food->remainder.has_value()) {
            leftover = eaten;
            leftover->id = food->remainder->toString();
            leftover->count = 1;
        }
        if (remaining > 0) {
            storage::StoredItemStack kept = eaten;
            kept.count = remaining;
            player.inventory.set(slot, kept);
        } else {
            player.inventory.set(slot, leftover);
        }
        if (context.slotChanged) {
            context.slotChanged(player, slot);
        }
        // A stack of two bowls of stew leaves the second bowl looking for a slot of its own, and on
        // the floor when there is none.
        if (remaining > 0 && leftover.has_value()) {
            if (player.inventory.add(*leftover) > 0) {
                if (const std::optional<Identifier> spare = parsedOrNothing(leftover->id); spare.has_value()) {
                    spawnDroppedItem(context, world, *spare, 1, player.x, player.y + kThrowEyeHeight, player.z, 0.0,
                                     0.0, 0.0, 0, wire::nbt::NbtCompound{});
                }
            }
            if (player.connection().sendInventoryAll) {
                player.connection().sendInventoryAll();
            }
        }
    }
    if (player.connection().syncHealth) {
        player.connection().syncHealth();
    }
    return true;
}

} // namespace basalt::server::runtime::player
