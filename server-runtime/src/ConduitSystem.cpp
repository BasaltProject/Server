#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ConduitSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerTrident.hpp>
#include <basalt/server/runtime/world/Conduits.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::common::Uuid;
using wire::protocol::play::SoundSource;
namespace conduits = world::conduits;

constexpr std::string_view kTarget = "Target";
constexpr std::int32_t kAmbientDelay = 60;
constexpr std::int32_t kAmbientSpread = 40;

[[nodiscard]] std::optional<Uuid> targetOf(const wire::nbt::NbtCompound& data) {
    const std::span<const std::int32_t> parts = data.getIntArray(kTarget);
    if (parts.size() != 4) {
        return std::nullopt;
    }
    const auto word = [parts](std::size_t at) {
        return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
    };
    return Uuid{(word(0) << 32U) | word(1), (word(2) << 32U) | word(3)};
}

void writeTarget(wire::nbt::NbtCompound& data, const std::optional<Uuid>& target) {
    if (!target.has_value()) {
        static_cast<void>(data.remove(kTarget));
        return;
    }
    const auto half = [](std::uint64_t bits, bool high) {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(high ? bits >> 32U : bits));
    };
    data.putIntArray(kTarget, {half(target->high(), true), half(target->high(), false), half(target->low(), true),
                               half(target->low(), false)});
}

[[nodiscard]] conduits::Position blockOf(double x, double y, double z) {
    return {wire::common::mth::floor(x), wire::common::mth::floor(y), wire::common::mth::floor(z)};
}

/// parity: `Entity.isInWaterOrRain` for a mob.
[[nodiscard]] bool mobInWaterOrRain(world::LoadedWorld& world, const entity::ServerEntity& mob) {
    const std::optional<std::int32_t> feet = world.state().blockState(
        wire::common::mth::floor(mob.x), wire::common::mth::floor(mob.y), wire::common::mth::floor(mob.z));
    return (feet.has_value() && server::registry::blockFluids::hasWaterFluid(*feet)) ||
           world::precipitation::entityInRain(world, mob.x, mob.y, mob.z, mob.height());
}

[[nodiscard]] bool alive(const entity::ServerEntity& mob) { return !mob.removed && !mob.dying && mob.health > 0.0; }

/// parity: `ConduitBlockEntity.updateDestroyTarget`.
[[nodiscard]] entity::ServerEntity* nextTarget(world::LoadedWorld& world, const conduits::Position& at,
                                               const std::optional<Uuid>& current, bool hunting,
                                               random::RandomSource& random) {
    if (!hunting) {
        return nullptr;
    }
    const std::vector<entity::ServerEntity*> everyone = world.state().entities();
    if (current.has_value()) {
        for (entity::ServerEntity* mob : everyone) {
            if (mob != nullptr && mob->uuid() == *current) {
                return alive(*mob) && conduits::closerThan(at, blockOf(mob->x, mob->y, mob->z), conduits::kKillRange)
                           ? mob
                           : nullptr;
            }
        }
        return nullptr;
    }
    // parity: `selectNewTarget`, every `Enemy` in water or rain whose box meets the cube eight out.
    std::vector<entity::ServerEntity*> candidates;
    const double minX = at[0] - conduits::kKillRange;
    const double minY = at[1] - conduits::kKillRange;
    const double minZ = at[2] - conduits::kKillRange;
    const double maxX = at[0] + 1 + conduits::kKillRange;
    const double maxY = at[1] + 1 + conduits::kKillRange;
    const double maxZ = at[2] + 1 + conduits::kKillRange;
    for (entity::ServerEntity* mob : everyone) {
        if (mob == nullptr || !alive(*mob) || entity::mobs::categoryOf(mob->type()) != "monster") {
            continue;
        }
        const double half = mob->width() / 2.0;
        if (mob->x + half <= minX || mob->x - half >= maxX || mob->y + mob->height() <= minY || mob->y >= maxY ||
            mob->z + half <= minZ || mob->z - half >= maxZ || !mobInWaterOrRain(world, *mob)) {
            continue;
        }
        candidates.push_back(mob);
    }
    if (candidates.empty()) {
        return nullptr;
    }
    return candidates[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(candidates.size())))];
}

} // namespace

ConduitSystem::ConduitSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, Options options)
    : players_(&players), broadcaster_(&broadcaster), options_(std::move(options)) {}

std::vector<PlayerSession*> ConduitSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

bool ConduitSystem::tick(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                         wire::nbt::NbtCompound& data) {
    const std::int64_t gameTime = world.gameTime();
    // parity: a block entity that was not ticked last turn is a new one, loaded or placed, and
    // starts inactive.
    if (gameTime % conduits::kShapeInterval == 0) {
        std::erase_if(memory_, [gameTime](const auto& entry) { return entry.second.lastTick < gameTime - 1; });
    }
    Memory& memory = memory_[Key{world.name(), x, y, z}];
    if (memory.lastTick != gameTime - 1) {
        memory = Memory{};
    }
    memory.lastTick = gameTime;

    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const std::int32_t stateId = world.state().blockState(x, y, z).value_or(0);
    const auto sound = [&](std::string_view name) {
        broadcaster_->sendEffects(watchersOf(world), x, y, z, stateId,
                                  {world::blockEffect::Sound{.name = std::string{name}}});
    };
    bool changed = false;
    if (gameTime % conduits::kShapeInterval == 0) {
        const auto isWater = [&world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
            const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz);
            return state.has_value() && server::registry::blockFluids::hasWaterFluid(*state);
        };
        const world::realBlockTags::TagMembers* tags = options_.blockTags;
        const auto isFrame = [&world, tags](std::int32_t bx, std::int32_t by, std::int32_t bz) {
            const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz);
            const Identifier* block =
                state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
            return tags != nullptr && block != nullptr &&
                   world::realBlockTags::contains(*tags, "minecraft:conduit_effect_block", *block);
        };
        const std::vector<conduits::Position> frame = conduits::frame(isWater, isFrame, x, y, z);
        const bool active = frame.size() >= conduits::kMinActiveSize;
        if (active != memory.active) {
            sound(active ? "block.conduit.activate" : "block.conduit.deactivate");
        }
        memory.active = active;
        if (active) {
            // parity: `applyEffects`, everybody whose block is inside the range and in water or rain.
            const std::int32_t range = conduits::effectRange(frame.size());
            for (PlayerSession* player : watchersOf(world)) {
                if (conduits::closerThan({x, y, z}, blockOf(player->x, player->y, player->z), range) &&
                    player::trident::inWaterOrRain(world, *player) && player->connection().addEffect) {
                    player->connection().addEffect(Identifier::vanilla("conduit_power"), conduits::kEffectTicks, 0);
                }
            }
            // parity: `updateAndAttackTarget`.
            const std::optional<Uuid> was = targetOf(data);
            entity::ServerEntity* target =
                nextTarget(world, {x, y, z}, was, frame.size() >= conduits::kMinKillSize, random);
            if (target != nullptr) {
                const std::int64_t seed = random.nextLong();
                for (const PlayerSession* heard : watchersOf(world)) {
                    player::playSoundFor(*heard, "block.conduit.attack.target", SoundSource::Blocks, target->x,
                                         target->y, target->z, 1.0F, 1.0F, seed);
                }
                static_cast<void>(entity::entityCombat::hurt(*target, conduits::kAttackDamage, std::nullopt,
                                                             std::nullopt, {}, "magic"));
            }
            const std::optional<Uuid> now = target != nullptr ? std::optional{target->uuid()} : std::nullopt;
            if (now != was) {
                writeTarget(data, now);
                changed = true;
            }
        }
    }
    if (memory.active) {
        if (gameTime % conduits::kAmbientInterval == 0) {
            sound("block.conduit.ambient");
        }
        if (gameTime > memory.nextAmbient) {
            memory.nextAmbient = gameTime + kAmbientDelay + random.nextInt(kAmbientSpread);
            sound("block.conduit.ambient.short");
        }
    }
    return changed;
}

} // namespace basalt::server::runtime
