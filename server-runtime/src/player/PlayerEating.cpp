#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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

constexpr std::int32_t kOminousBottleTicks = 120000;

/// parity: `TeleportRandomlyConsumeEffect.apply`'s sixteen tries and `LevelEvent` 2017.
constexpr std::int32_t kTeleportAttempts = 16;
constexpr std::int32_t kTeleportParticlesEvent = 2017;
constexpr std::int32_t kPackRadius = 127;
/// parity: the Nether's `logical_height`.
constexpr std::int32_t kCeilingLogicalHeight = 128;

/// parity: `BlockUtil.clampedPackDifferenceInPosition` with 127 on every axis.
[[nodiscard]] std::int32_t packedDifference(std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ,
                                            std::int32_t toX, std::int32_t toY, std::int32_t toZ) {
    const auto axis = [](std::int32_t from, std::int32_t to) {
        return (std::clamp(to - from, -kPackRadius, kPackRadius) + kPackRadius) & 0xFF;
    };
    return (axis(fromX, toX) << 16) | (axis(fromY, toY) << 8) | axis(fromZ, toZ);
}

/// parity: 26.3 `TeleportRandomlyConsumeEffect.apply` for a player.
void teleportRandomly(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, float diameter) {
    if (!player.connection().teleportTo || (player.vehicleId.has_value() && !context.dismount)) {
        return;
    }
    const auto next = [&context] { return context.nextDouble ? context.nextDouble() : 0.5; };
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const std::int32_t minY = facts.chunkHeight.minY();
    const std::int32_t logicalHeight = facts.hasCeiling ? kCeilingLogicalHeight : facts.chunkHeight.height();
    const entity::ai::BlockTagged tagged = [&context](std::string_view tag, std::int32_t stateId) {
        const Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
        return block != nullptr && context.blockTags != nullptr &&
               world::realBlockTags::contains(*context.blockTags, tag, *block);
    };
    const double height =
        player.sneaking ? world::blockPlacement::kSneakingHeight : world::blockPlacement::kPlayerHeight;
    for (std::int32_t attempt = 0; attempt < kTeleportAttempts; ++attempt) {
        const double x = player.x + ((next() - 0.5) * static_cast<double>(diameter));
        const double y = std::clamp(player.y + ((next() - 0.5) * static_cast<double>(diameter)),
                                    static_cast<double>(minY), static_cast<double>(minY + logicalHeight - 1));
        const double z = player.z + ((next() - 0.5) * static_cast<double>(diameter));
        // parity: `apply` gets off the mount before each try.
        if (player.vehicleId.has_value()) {
            context.dismount(player);
        }
        const std::optional<double> landing =
            entity::ai::randomTeleportLanding(world.state(), tagged, x, y, z, world::blockPlacement::kPlayerWidth,
                                              height, "minecraft:consumable_does_not_teleport_to", false);
        if (!landing.has_value()) {
            continue;
        }
        const double fromX = player.x;
        const double fromY = player.y;
        const double fromZ = player.z;
        player.connection().teleportTo(x, *landing, z, player.yaw, player.pitch);
        // parity: `TeleportRandomlyConsumeEffect.apply` lets go of any impulse.
        impulse::reset(player);
        world::vibrations::emit(world.state(), "teleport", fromX, fromY, fromZ, world.gameTime(),
                                {.entity = player.uuid()});
        const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
        const auto blockOf = [](double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); };
        const std::int32_t packed =
            packedDifference(blockOf(fromX), blockOf(fromY), blockOf(fromZ), blockOf(x), blockOf(*landing), blockOf(z));
        for (const PlayerSession* heard : playersIn(context, world.name())) {
            playSoundFor(*heard, "minecraft:item.chorus_fruit.teleport", wire::protocol::play::SoundSource::Players, x,
                         *landing, z, 1.0F, 1.0F, seed);
            if (heard->connection().sendLevelEvent) {
                heard->connection().sendLevelEvent(kTeleportParticlesEvent, blockOf(fromX), blockOf(fromY),
                                                   blockOf(fromZ), packed);
            }
        }
        return;
    }
}

/// parity: the `MobEffectCategory.BENEFICIAL` ones.
[[nodiscard]] bool isBeneficial(std::string_view path) {
    static constexpr std::array<std::string_view, 19> kBeneficial{
        "speed",        "haste",         "strength",        "instant_health",     "jump_boost",
        "regeneration", "resistance",    "fire_resistance", "water_breathing",    "invisibility",
        "night_vision", "health_boost",  "absorption",      "saturation",         "luck",
        "slow_falling", "conduit_power", "dolphins_grace",  "hero_of_the_village"};
    return std::ranges::find(kBeneficial, path) != kBeneficial.end();
}

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

void applyConsumeEffectsFor(PlayerSession& player, const std::vector<reg::ConsumeEffect>& effects,
                            const std::function<float()>& nextFloat, const ClearScope scope) {
    for (const reg::ConsumeEffect& effect : effects) {
        if (effect.as<reg::ConsumeEffect::ClearAll>() != nullptr) {
            std::vector<Identifier> held;
            held.reserve(player.effects.size());
            for (const auto& [id, active] : player.effects) {
                const bool omen = id.path() == "bad_omen" || id.path() == "raid_omen" || id.path() == "trial_omen";
                if ((scope.keepOmens && omen) || (scope.keepBeneficial && isBeneficial(id.path()))) {
                    continue;
                }
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

    // parity: `Consumable.onConsume` raises `DRINK` for the four drinking-animation items and `EAT` otherwise.
    static constexpr std::array<std::string_view, 4> kDrunk{"milk_bucket", "potion", "honey_bottle", "ominous_bottle"};
    const bool drunk = std::ranges::find(kDrunk, id->path()) != kDrunk.end();
    world::vibrations::emit(world.state(), drunk ? "drink" : "eat", player.x, player.y, player.z, world.gameTime());
    statAwards::used(player, eaten.id);

    std::int32_t nutrition = food->nutrition;
    bool nourished = true;
    if (context.foodEaten) {
        const std::int32_t before = player.food.foodLevel();
        const std::optional<std::int32_t> wanted = context.foodEaten(player, eaten.id, before + nutrition, before);
        nourished = wanted.has_value();
        nutrition = wanted.value_or(before) - before;
    }
    if (nourished) {
        player.food.eat(nutrition, food->saturation);
    }
    // parity: `OminousBottleAmplifier.onConsume`.
    if (id->path() == "ominous_bottle") {
        const world::ActiveEffect omen{.effect = world::mobEffects::badOmen(),
                                       .amplifier = eaten.components.getInt("minecraft:ominous_bottle_amplifier"),
                                       .remainingTicks = kOminousBottleTicks,
                                       .visible = false};
        player.effects.insert_or_assign(omen.effect, omen);
        if (player.connection().sendEffect) {
            player.connection().sendEffect(omen);
        }
    }
    const config::PurpurRules::Mechanics& rules = world.state().gameplay().mechanics;
    const bool milk = id->path() == "milk_bucket";
    applyConsumeEffectsFor(player, food->effects, nextFloat,
                           ClearScope{.keepBeneficial = milk && !rules.milkClearsBeneficialEffects,
                                      .keepOmens = milk && !rules.milkCuresBadOmen});
    for (const reg::ConsumeEffect& effect : food->effects) {
        if (const auto* teleport = effect.as<reg::ConsumeEffect::TeleportRandomly>(); teleport != nullptr) {
            teleportRandomly(context, player, world, teleport->diameter);
        }
    }
    // parity: `ItemStack.applyAfterUseComponentSideEffects`.
    startCooldownFor(player, *id, world.gameTime());
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
        // A stack of two bowls leaves the second bowl looking for a slot, else on the floor.
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
