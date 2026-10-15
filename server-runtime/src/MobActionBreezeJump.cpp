#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/Breezes.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/LongJump.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::protocol::play::SoundSource;
namespace breezes = entity::breezes;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using breezes::BreezeState;
using breezes::Pose;

constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
/// parity: `LongJump.MAX_JUMP_VELOCITY_MULTIPLIER`, times the follow range.
constexpr float kJumpVelocityPerRange = 0.058333334F;

[[nodiscard]] float wrapDegrees(float angle) {
    float wrapped = std::fmod(angle, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

[[nodiscard]] const wire::common::Identifier* blockAt(const world::WorldState& world, const kit::Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
}

/// parity: `EntityType.isBlockDangerous` for a mob that is not fire immune.
[[nodiscard]] bool dangerous(const world::WorldState& world, const kit::Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr) {
        return false;
    }
    static constexpr std::array<std::string_view, 9> kDangerous{
        "fire",        "soul_fire",        "lava",   "magma_block", "lava_cauldron",
        "wither_rose", "sweet_berry_bush", "cactus", "powder_snow"};
    const std::string_view path = block->path();
    if (std::ranges::find(kDangerous, path) != kDangerous.end()) {
        return true;
    }
    if (path != "campfire" && path != "soul_campfire") {
        return false;
    }
    const std::map<std::string, std::string> properties = server::registry::blockStateDefinitions::propertiesOf(*state);
    const auto lit = properties.find("lit");
    return lit != properties.end() && lit->second == "true";
}

/// parity: `LongJump.snapToSurface`.
[[nodiscard]] std::optional<kit::Block> snapToSurface(world::WorldState& world, const kit::Vec& target) {
    for (const double direction : {-10.0, 10.0}) {
        if (const std::optional<entity::BlockHit> hit = entity::blockRay::cast(
                world, target[0], target[1], target[2], target[0], target[1] + direction, target[2]);
            hit.has_value()) {
            return kit::Block{mth::floor(hit->x), mth::floor(hit->y) + 1, mth::floor(hit->z)};
        }
    }
    return std::nullopt;
}

} // namespace

namespace entity::breezes {

BreezeState& stateOf(ServerEntity& breeze) {
    if (breeze.breeze == nullptr) {
        breeze.breeze = std::make_shared<BreezeState>();
    }
    return *breeze.breeze;
}

ai::brainKit::Vec pointBehind(const ai::brainKit::Vec& enemy, float headYaw, random::RandomSource& random) {
    const float viewAngle = headYaw + 180.0F + (static_cast<float>(random.nextGaussian()) * 90.0F / 2.0F);
    const float distance = 4.0F + (random.nextFloat() * (8.0F - 4.0F));
    const float yCos = mth::cos(static_cast<double>((-viewAngle * kRadians) - std::numbers::pi_v<float>));
    const float ySin = mth::sin(static_cast<double>((-viewAngle * kRadians) - std::numbers::pi_v<float>));
    const float xCos = -mth::cos(0.0);
    return {enemy[0] + (static_cast<double>(ySin * xCos) * static_cast<double>(distance)), enemy[1] + 0.0,
            enemy[2] + (static_cast<double>(yCos * xCos) * static_cast<double>(distance))};
}

bool lineOfSight(world::WorldState& world, const ServerEntity& breeze, const ai::brainKit::Vec& target) {
    const double dx = target[0] - breeze.x;
    const double dy = target[1] - breeze.y;
    const double dz = target[2] - breeze.z;
    const double range = std::max(50.0, breeze.attributeWithModifiers("minecraft:follow_range"));
    if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) > range) {
        return false;
    }
    return !blockRay::cast(world, breeze.x, breeze.y, breeze.z, target[0], target[1], target[2]).has_value();
}

void lookAt(ServerEntity& breeze, const ai::brainKit::Vec& target) {
    const double dx = target[0] - breeze.x;
    const double dy = target[1] - (breeze.y + mobBreathing::eyeHeightOf(breeze));
    const double dz = target[2] - breeze.z;
    const double horizontal = std::sqrt((dx * dx) + (dz * dz));
    breeze.pitch = wrapDegrees(static_cast<float>(-(std::atan2(dy, horizontal) * 180.0 / std::numbers::pi)));
    breeze.yaw = wrapDegrees(static_cast<float>(std::atan2(dz, dx) * 180.0 / std::numbers::pi) - 90.0F);
    breeze.headYaw = breeze.yaw;
}

} // namespace entity::breezes

bool MobActionSystem::breezeCanJump(entity::ai::MobContext& context, BreezeState& state,
                                    const std::optional<kit::Vec>& target) {
    ServerEntity& breeze = context.mob;
    const bool inWater = entity::ai::inWater(context.world, breeze);
    if (!breeze.onGround && !inWater) {
        return false;
    }
    // parity: not over its head in water, nor in lava.
    const double eyes = breeze.y + entity::mobBreathing::eyeHeightOf(breeze);
    const std::optional<std::int32_t> atEyes =
        context.world.blockState(mth::floor(breeze.x), mth::floor(eyes), mth::floor(breeze.z));
    const std::optional<std::int32_t> atFeet =
        context.world.blockState(mth::floor(breeze.x), mth::floor(breeze.y), mth::floor(breeze.z));
    if ((atEyes.has_value() && world::fluids::of(*atEyes) != world::Submersion::None) ||
        (atFeet.has_value() && world::fluids::of(*atFeet) == world::Submersion::Lava)) {
        return false;
    }
    if (state.jumpTarget.has_value()) {
        return true;
    }
    if (!target.has_value()) {
        return false;
    }
    const double dx = (*target)[0] - breeze.x;
    const double dy = (*target)[1] - breeze.y;
    const double dz = (*target)[2] - breeze.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    // parity: `outOfAggroRange` forgets the target; `tooCloseForJump` is inside four.
    const double range = breeze.attributeWithModifiers("minecraft:follow_range");
    if ((dx * dx) + (dy * dy) + (dz * dz) >= range * range) {
        breeze.targetId.reset();
        return false;
    }
    if (static_cast<float>(distance) - 4.0F <= 0.0F) {
        return false;
    }
    // parity: `canJumpFromCurrentPosition`.
    const kit::Block own{mth::floor(breeze.x), mth::floor(breeze.y), mth::floor(breeze.z)};
    if (const wire::common::Identifier* block = blockAt(context.world, own);
        block != nullptr && block->path() == "honey_block") {
        return false;
    }
    for (std::int32_t up = 1; up <= 4; ++up) {
        const std::optional<std::int32_t> above = context.world.blockState(own[0], own[1] + up, own[2]);
        if (!above.has_value() ||
            (!server::registry::blocks::isAir(*above) && world::fluids::of(*above) != world::Submersion::Water)) {
            return false;
        }
    }
    const float headYaw = breeze.targetId.has_value() ? breezeTargetHeadYaw(context, *breeze.targetId) : 0.0F;
    const std::optional<kit::Block> landing =
        snapToSurface(context.world, breezes::pointBehind(*target, headYaw, context.random));
    if (!landing.has_value() || dangerous(context.world, {(*landing)[0], (*landing)[1] - 1, (*landing)[2]})) {
        return false;
    }
    const kit::Vec middle{(*landing)[0] + 0.5, (*landing)[1] + 0.5, (*landing)[2] + 0.5};
    if (!breezes::lineOfSight(context.world, breeze, middle) &&
        !breezes::lineOfSight(context.world, breeze, {middle[0], middle[1] + 4.0, middle[2]})) {
        return false;
    }
    state.jumpTarget = landing;
    return true;
}

void MobActionSystem::breezeJumps(world::LoadedWorld& world, entity::ai::MobContext& context, BreezeState& state) {
    ServerEntity& breeze = context.mob;
    const std::int64_t now = context.gameTime;
    const auto stop = [&] {
        state.jumping = false;
        if (state.pose == Pose::LongJumping || state.pose == Pose::Inhaling) {
            state.pose = Pose::Standing;
        }
        state.jumpTarget.reset();
        state.jumpInhaling.erase();
        state.leavingWater = false;
    };
    if (now > state.jumpEnd || state.pose == Pose::Standing || state.jumpCooldown.has(now)) {
        stop();
        return;
    }
    const bool inWater = entity::ai::inWater(context.world, breeze);
    if (!inWater) {
        state.leavingWater = false;
    }
    if (!state.jumpInhaling.has(now) && state.pose == Pose::Inhaling) {
        // parity: `calculateOptimalJumpVector`: the five angles shuffled, the first within reach.
        std::optional<kit::Vec> velocity;
        if (state.jumpTarget.has_value()) {
            std::array<std::int32_t, 5> angles{40, 55, 60, 75, 80};
            for (std::size_t index = angles.size(); index > 1; --index) {
                std::swap(angles[index - 1],
                          angles[static_cast<std::size_t>(context.random.nextInt(static_cast<std::int32_t>(index)))]);
            }
            const float maxVelocity =
                kJumpVelocityPerRange * static_cast<float>(breeze.attributeWithModifiers("minecraft:follow_range"));
            const kit::Vec landing{(*state.jumpTarget)[0] + 0.5, static_cast<double>((*state.jumpTarget)[1]),
                                   (*state.jumpTarget)[2] + 0.5};
            for (const std::int32_t angle : angles) {
                velocity = entity::ai::longJump::vectorForAngle(context, landing, maxVelocity, angle, breeze.width(),
                                                                breeze.height(), false);
                if (velocity.has_value()) {
                    // parity: jump boost adds its power along the launch's own upward share.
                    if (const auto boost = breeze.effects.find(wire::common::Identifier::vanilla("jump_boost"));
                        boost != breeze.effects.end()) {
                        const double length =
                            std::sqrt(((*velocity)[0] * (*velocity)[0]) + ((*velocity)[1] * (*velocity)[1]) +
                                      ((*velocity)[2] * (*velocity)[2]));
                        const double power =
                            static_cast<double>(0.1F * static_cast<float>(boost->second.amplifier + 1));
                        (*velocity)[1] += length < 1.0E-4 ? 0.0 : (*velocity)[1] / length * power;
                    }
                    break;
                }
            }
        }
        if (!velocity.has_value()) {
            state.pose = Pose::Standing;
            return;
        }
        state.leavingWater = inWater;
        playSound(world, breeze, "entity.breeze.jump", 1.0F, 1.0F);
        state.pose = Pose::LongJumping;
        breeze.discardFriction = true;
        breeze.velocityX = (*velocity)[0];
        breeze.velocityY = (*velocity)[1];
        breeze.velocityZ = (*velocity)[2];
    } else if (state.pose == Pose::LongJumping && (breeze.onGround || (inWater && !state.leavingWater))) {
        // parity: `isFinishedJumping`, and a shot soon after it lands.
        playSound(world, breeze, "entity.breeze.land", 1.0F, 1.0F);
        state.pose = Pose::Standing;
        breeze.discardFriction = false;
        state.jumpCooldown.setFor(true, now, state.hurtBy.has(now) ? 2 : 10);
        state.shoot.setFor(true, now, 100);
    }
}

void MobActionSystem::breezeStartsJump(world::LoadedWorld& world, entity::ai::MobContext& context, BreezeState& state) {
    // parity: `LongJump.start`: the breath in, the charge sound, a look where it will land.
    const std::int64_t now = context.gameTime;
    state.jumping = true;
    state.jumpEnd = now + breezes::kLongJumpDuration;
    if (!state.jumpInhaling.has(now)) {
        state.jumpInhaling.setFor(true, now, breezes::kInhaling);
    }
    state.pose = Pose::Inhaling;
    playSound(world, context.mob, "entity.breeze.charge", 1.0F, 1.0F, SoundSource::Hostile);
    if (state.jumpTarget.has_value()) {
        breezes::lookAt(context.mob,
                        {(*state.jumpTarget)[0] + 0.5, (*state.jumpTarget)[1] + 0.5, (*state.jumpTarget)[2] + 0.5});
    }
}

} // namespace basalt::server::runtime
