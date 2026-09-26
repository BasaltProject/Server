#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/LongJump.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::ai::longJump {
namespace {

namespace mth = wire::common::mth;
using brainKit::Block;
using brainKit::Vec;

[[nodiscard]] std::int32_t sample(random::RandomSource& random, const std::array<std::int32_t, 2>& range) {
    return range[0] + random.nextInt(range[1] - range[0] + 1);
}

[[nodiscard]] Block blockOf(const ServerEntity& mob) {
    return {mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)};
}

/// parity: `WeightedRandom.getRandomItem` over the candidates, which it takes out of the list.
[[nodiscard]] std::optional<std::pair<Block, std::int32_t>>
takeWeighted(random::RandomSource& random, std::vector<std::pair<Block, std::int32_t>>& candidates) {
    std::int64_t total = 0;
    for (const auto& candidate : candidates) {
        total += candidate.second;
    }
    if (total <= 0) {
        return std::nullopt;
    }
    std::int32_t pick = random.nextInt(static_cast<std::int32_t>(total));
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        pick -= candidates[index].second;
        if (pick < 0) {
            const auto chosen = candidates[index];
            candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(index));
            return chosen;
        }
    }
    return std::nullopt;
}

std::optional<Vec> jumpVector(MobContext& context, const Spec& spec, const Vec& target) {
    ServerEntity& body = context.mob;
    // parity: `calculateOptimalJumpVector`: the four angles in a shuffled order, the first that clears.
    std::array<std::int32_t, 4> angles{65, 70, 75, 80};
    random::RandomSource shuffle =
        random::RandomSource::legacy(static_cast<std::int64_t>(context.gameTime * 31 + body.entityId()));
    for (std::size_t index = angles.size() - 1; index > 0; --index) {
        std::swap(angles[index],
                  angles[static_cast<std::size_t>(shuffle.nextInt(static_cast<std::int32_t>(index + 1)))]);
    }
    const auto maxVelocity = static_cast<float>(body.attributeWithModifiers("minecraft:jump_strength") *
                                                static_cast<double>(spec.velocityMultiplier));
    for (const std::int32_t angle : angles) {
        if (std::optional<Vec> vector = vectorForAngle(context, target, maxVelocity, angle,
                                                       body.width() * static_cast<double>(spec.jumpingScale),
                                                       body.height() * static_cast<double>(spec.jumpingScale), true);
            vector.has_value()) {
            return vector;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<Vec> vectorForAngle(MobContext& context, const Vec& target, float maxVelocity, std::int32_t angle,
                                  double width, double height, bool checkCollision) {
    ServerEntity& body = context.mob;
    constexpr double kGravity = 0.08;
    const entityCollision::BlockAt blockAt = [&context](std::int32_t x, std::int32_t y, std::int32_t z) {
        return context.world.blockState(x, y, z);
    };
    // parity: `LongJumpUtil.calculateJumpVectorForAngle`, half a block short of the middle.
    double planeX = target[0] - body.x;
    double planeZ = target[2] - body.z;
    const double planeLength = std::sqrt((planeX * planeX) + (planeZ * planeZ));
    if (planeLength < 1.0E-4) {
        return std::nullopt;
    }
    planeX = planeX / planeLength * 0.5;
    planeZ = planeZ / planeLength * 0.5;
    const double dx = target[0] - planeX - body.x;
    const double dy = target[1] - body.y;
    const double dz = target[2] - planeZ - body.z;
    const float radians = static_cast<float>(angle) * std::numbers::pi_v<float> / 180.0F;
    const double xzAngle = std::atan2(dz, dx);
    const double r2 = (dx * dx) + (dz * dz);
    const double r = std::sqrt(r2);
    const double sin2 = std::sin(static_cast<double>(2.0F * radians));
    const double cos2 = std::pow(std::cos(static_cast<double>(radians)), 2.0);
    const double sinA = std::sin(static_cast<double>(radians));
    const double cosA = std::cos(static_cast<double>(radians));
    const double v0sqr = r2 * kGravity / ((r * sin2) - (2.0 * dy * cos2));
    if (v0sqr < 0.0) {
        return std::nullopt;
    }
    const double v0 = std::sqrt(v0sqr);
    if (v0 > static_cast<double>(maxVelocity)) {
        return std::nullopt;
    }
    const double v0r = v0 * cosA;
    const double v0y = v0 * sinA;
    if (checkCollision) {
        const std::int32_t samples = static_cast<std::int32_t>(std::ceil(r / v0r)) * 2;
        double ri = 0.0;
        std::optional<Vec> previous;
        for (std::int32_t index = 0; index < samples - 1; ++index) {
            ri += r / samples;
            const double yi = (sinA / cosA * ri) - (ri * ri * kGravity / (2.0 * v0sqr * cosA * cosA));
            const Vec sample{body.x + (ri * std::cos(xzAngle)), body.y + yi, body.z + (ri * std::sin(xzAngle))};
            if (previous.has_value()) {
                // parity: `isClearTransition`, the box stepped along the arc.
                const double sx = sample[0] - (*previous)[0];
                const double sy = sample[1] - (*previous)[1];
                const double sz = sample[2] - (*previous)[2];
                const double length = std::sqrt((sx * sx) + (sy * sy) + (sz * sz));
                const double step = std::min(width, height);
                const std::int32_t checks = static_cast<std::int32_t>(std::ceil(length / step));
                Vec point = *previous;
                for (std::int32_t check = 0; check < checks; ++check) {
                    if (check == checks - 1) {
                        point = sample;
                    } else {
                        point = {point[0] + (sx / length * step * static_cast<double>(0.9F)),
                                 point[1] + (sy / length * step * static_cast<double>(0.9F)),
                                 point[2] + (sz / length * step * static_cast<double>(0.9F))};
                    }
                    if (!entityCollision::collidingBoxes(blockAt, Aabb::of(point[0], point[1], point[2], width, height))
                             .empty()) {
                        return std::nullopt;
                    }
                }
            }
            previous = sample;
        }
    }
    return Vec{v0r * std::cos(xzAngle) * static_cast<double>(0.95F), v0y * static_cast<double>(0.95F),
               v0r * std::sin(xzAngle) * static_cast<double>(0.95F)};
}

bool defaultAcceptable(const MobContext& mob, const Block& target) {
    const std::optional<std::int32_t> below = mob.world.blockState(target[0], target[1] - 1, target[2]);
    const std::optional<std::int32_t> at = mob.world.blockState(target[0], target[1], target[2]);
    return below.has_value() && at.has_value() && registry::blockShapes::isSolidRender(*below) &&
           registry::blockShapes::isEmptyShape(*at);
}

void countDown(State& state) {
    if (state.cooldown.has_value() && --*state.cooldown <= 0) {
        state.cooldown.reset();
    }
}

Event tick(MobContext& mob, State& state, const Spec& spec, brainKit::Look& look) {
    ServerEntity& body = mob.mob;
    const auto cooldown = [&](bool half) {
        const std::int32_t ticks = sample(mob.random, spec.cooldown);
        state.cooldown = half ? ticks / 2 : ticks;
    };
    // parity: `LongJumpMidJump`: until it lands, then the slowdown.
    if (state.midJump) {
        if (!body.onGround) {
            return Event::None;
        }
        body.velocityX *= 0.1;
        body.velocityZ *= 0.1;
        body.discardFriction = false;
        state.midJump = false;
        cooldown(false);
        return Event::Landed;
    }
    const bool submerged = inWater(mob.world, body);
    if (!state.jumping) {
        // parity: `checkExtraStartConditions`: on the ground and out of water, lava and honey.
        const std::optional<std::int32_t> inside =
            mob.world.blockState(mth::floor(body.x), mth::floor(body.y), mth::floor(body.z));
        const wire::common::Identifier* block =
            inside.has_value() ? registry::blockStateDefinitions::blockOf(*inside) : nullptr;
        if (!body.onGround || submerged ||
            (block != nullptr && (block->path() == "lava" || block->path() == "honey_block"))) {
            cooldown(true);
            return Event::None;
        }
        state.jumping = true;
        state.run = 0;
        state.chosen.reset();
        state.tries = 20;
        state.from = Vec{body.x, body.y, body.z};
        state.candidates.clear();
        state.notPreferred.clear();
        const Block own = blockOf(body);
        // parity: `BlockPos.betweenClosedStream`, x fastest, then y, then z.
        for (std::int32_t z = own[2] - spec.width; z <= own[2] + spec.width; ++z) {
            for (std::int32_t y = own[1] - spec.height; y <= own[1] + spec.height; ++y) {
                for (std::int32_t x = own[0] - spec.width; x <= own[0] + spec.width; ++x) {
                    if (x == own[0] && y == own[1] && z == own[2]) {
                        continue;
                    }
                    const std::int32_t distance =
                        ((x - own[0]) * (x - own[0])) + ((y - own[1]) * (y - own[1])) + ((z - own[2]) * (z - own[2]));
                    state.candidates.emplace_back(Block{x, y, z}, distance);
                }
            }
        }
        // parity: `LongJumpToPreferredBlock.start`, which decides once whether to look for them.
        state.wantsPreferred = spec.preferred && mob.random.nextFloat() < spec.preferredChance;
    }
    // parity: `canStillUse`: still where it started, tries left and something to try.
    const bool valid = state.from.has_value() && *state.from == Vec{body.x, body.y, body.z} && state.tries > 0 &&
                       !submerged && (state.chosen.has_value() || !state.candidates.empty()) && ++state.run <= 200;
    if (!valid) {
        state.jumping = false;
        cooldown(true);
        look = {};
        return Event::None;
    }
    if (state.chosen.has_value()) {
        if (mob.gameTime - state.prepareStart >= 40) {
            // parity: the jump itself, with no friction until it lands.
            body.discardFriction = true;
            body.velocityX = (*state.chosen)[0];
            body.velocityY = (*state.chosen)[1];
            body.velocityZ = (*state.chosen)[2];
            state.midJump = true;
            state.jumping = false;
            return Event::Jumped;
        }
        return Event::None;
    }
    --state.tries;
    // parity: `pickCandidate`, weighted by distance, skipping what a short walk reaches anyway.
    while (!state.candidates.empty() || !state.notPreferred.empty()) {
        std::optional<std::pair<Block, std::int32_t>> candidate;
        if (state.wantsPreferred) {
            while (!state.candidates.empty() && !candidate.has_value()) {
                const std::optional<std::pair<Block, std::int32_t>> drawn = takeWeighted(mob.random, state.candidates);
                if (!drawn.has_value()) {
                    break;
                }
                const Block below{drawn->first[0], drawn->first[1] - 1, drawn->first[2]};
                if (spec.preferred(below)) {
                    candidate = drawn;
                } else {
                    state.notPreferred.push_back(*drawn);
                }
            }
            if (!candidate.has_value() && !state.notPreferred.empty()) {
                candidate = state.notPreferred.front();
                state.notPreferred.erase(state.notPreferred.begin());
            }
        } else {
            candidate = takeWeighted(mob.random, state.candidates);
        }
        if (!candidate.has_value()) {
            return Event::None;
        }
        const Block target = candidate->first;
        const Block own = blockOf(body);
        if ((target[0] == own[0] && target[2] == own[2]) ||
            !(spec.acceptable ? spec.acceptable(target) : defaultAcceptable(mob, target))) {
            continue;
        }
        const std::optional<Vec> jump = jumpVector(mob, spec, {target[0] + 0.5, target[1] + 0.5, target[2] + 0.5});
        if (!jump.has_value()) {
            continue;
        }
        look = brainKit::Look{.at = Vec{target[0] + 0.5, target[1] + 0.5, target[2] + 0.5}};
        const std::int32_t manhattan =
            std::abs(target[0] - own[0]) + std::abs(target[1] - own[1]) + std::abs(target[2] - own[2]);
        if (manhattan <= 8 && navigation::canReach(mob.world, body, target[0] + 0.5, target[1], target[2] + 0.5)) {
            continue;
        }
        state.chosen = jump;
        state.prepareStart = mob.gameTime;
        return Event::None;
    }
    return Event::None;
}

} // namespace basalt::server::runtime::entity::ai::longJump
