#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace basalt::server::runtime::entity::shulkers {
namespace {

namespace mth = wire::common::mth;
using registry::BlockFace;
using Block = std::array<std::int32_t, 3>;

constexpr std::array kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                            BlockFace::South, BlockFace::West, BlockFace::East};

/// parity: `ShulkerBullet.SPEED`.
constexpr double kBulletSpeed = 0.15;

[[nodiscard]] Block relative(const Block& at, BlockFace face) {
    return {at[0] + registry::stepX(face), at[1] + registry::stepY(face), at[2] + registry::stepZ(face)};
}

[[nodiscard]] Block blockOf(double x, double y, double z) { return {mth::floor(x), mth::floor(y), mth::floor(z)}; }

[[nodiscard]] bool isAir(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && (block->path() == "air" || block->path() == "cave_air" || block->path() == "void_air");
}

/// parity: `Level.noCollision(entity, box)`: no block and no other shulker in the way.
[[nodiscard]] bool noCollision(const world::WorldState& world, const ServerEntity& self, Obstacles others,
                               const Aabb& box) {
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    if (entityCollision::collides(blockAt, box)) {
        return false;
    }
    for (const ServerEntity* other : others) {
        if (other == &self || other->removed || other->dying || !isShulker(other->type())) {
            continue;
        }
        if (box.intersects(Aabb::of(other->x, other->y, other->z, 1.0, 1.0))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] BlockFace randomFace(random::RandomSource& random) {
    return kFaces[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kFaces.size())))];
}

} // namespace

bool isShulker(const wire::common::Identifier& type) { return type.path() == "shulker"; }

bool isBullet(const wire::common::Identifier& type) { return type.path() == "shulker_bullet"; }

ShulkerState& stateOf(ServerEntity& shulker) {
    if (shulker.shulker == nullptr) {
        shulker.shulker = std::make_shared<ShulkerState>();
        if (shulker.sourceNbt.has_value()) {
            const wire::nbt::NbtCompound& data = *shulker.sourceNbt;
            const std::int32_t face = data.getByte("AttachFace", 0);
            shulker.shulker->attachFace =
                face >= 0 && face < 6 ? kFaces[static_cast<std::size_t>(face)] : BlockFace::Down;
            shulker.shulker->peek = data.getByte("Peek", 0);
            shulker.shulker->color = data.getByte("Color", kNoColor);
        }
        shulker.shulker->healthSeen = shulker.health;
    }
    return *shulker.shulker;
}

void save(ServerEntity& shulker) {
    const ShulkerState& state = stateOf(shulker);
    if (!shulker.sourceNbt.has_value()) {
        shulker.sourceNbt.emplace();
    }
    shulker.sourceNbt->putByte("AttachFace", static_cast<std::int8_t>(state.attachFace));
    shulker.sourceNbt->putByte("Peek", static_cast<std::int8_t>(state.peek));
    shulker.sourceNbt->putByte("Color", state.color);
}

float physicalPeek(float amount) {
    return 0.5F - (mth::sin(static_cast<double>((0.5F + amount) * std::numbers::pi_v<float>)) * 0.5F);
}

Aabb progressAabb(float size, BlockFace towards, float progress, double x, double y, double z) {
    const auto half = static_cast<double>(size) * 0.5;
    Aabb box{-half, 0.0, -half, half, static_cast<double>(size), half};
    // parity: `getProgressDeltaAabb` from -1.
    const double grow = static_cast<double>(std::max(-1.0F, progress)) * static_cast<double>(size);
    box = box.expandTowards(registry::stepX(towards) * grow, registry::stepY(towards) * grow,
                            registry::stepZ(towards) * grow);
    return box.move(x, y, z);
}

bool canStayAt(const world::WorldState& world, const ServerEntity& shulker, Obstacles others, const Block& at,
               BlockFace face) {
    // parity: `isPositionBlocked`, anything but air, bar a moving piston where it already is.
    if (!isAir(world, at)) {
        const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
        const wire::common::Identifier* block =
            state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
        const bool ownPiston =
            block != nullptr && block->path() == "moving_piston" && at == blockOf(shulker.x, shulker.y, shulker.z);
        if (!ownPiston) {
            return false;
        }
    }
    const Block holder = relative(at, face);
    const std::optional<std::int32_t> held = world.blockState(holder[0], holder[1], holder[2]);
    if (!held.has_value() || !registry::blockShapes::isCollisionFaceFull(*held, registry::opposite(face))) {
        return false;
    }
    const Aabb open =
        progressAabb(1.0F, registry::opposite(face), 1.0F, at[0] + 0.5, at[1], at[2] + 0.5).inflate(-1.0E-6);
    return noCollision(world, shulker, others, open);
}

std::optional<BlockFace> attachableSurface(const world::WorldState& world, const ServerEntity& shulker,
                                           Obstacles others, const Block& at) {
    for (const BlockFace face : kFaces) {
        if (canStayAt(world, shulker, others, at, face)) {
            return face;
        }
    }
    return std::nullopt;
}

bool updatePeek(ShulkerState& state) {
    state.currentPeekOld = state.currentPeek;
    const float target = static_cast<float>(state.peek) * 0.01F;
    if (state.currentPeek == target) {
        return false;
    }
    if (state.currentPeek > target) {
        state.currentPeek = std::clamp(state.currentPeek - 0.05F, target, 1.0F);
    } else {
        state.currentPeek = std::clamp(state.currentPeek + 0.05F, 0.0F, target);
    }
    return true;
}

std::optional<Landing> findTeleport(const world::WorldState& world, const ServerEntity& shulker, Obstacles others,
                                    random::RandomSource& random, std::int32_t minY, const RefusesBlock& refuses) {
    const Block from = blockOf(shulker.x, shulker.y, shulker.z);
    const auto between = [&random] { return random.nextInt(17) - 8; };
    for (std::int32_t attempt = 0; attempt < 5; ++attempt) {
        const std::int32_t dx = between();
        const std::int32_t dy = between();
        const std::int32_t dz = between();
        const Block target{from[0] + dx, from[1] + dy, from[2] + dz};
        const Aabb cell = Aabb{static_cast<double>(target[0]),
                               static_cast<double>(target[1]),
                               static_cast<double>(target[2]),
                               target[0] + 1.0,
                               target[1] + 1.0,
                               target[2] + 1.0}
                              .inflate(-1.0E-6);
        if (target[1] <= minY || !isAir(world, target) || !noCollision(world, shulker, others, cell)) {
            continue;
        }
        const std::optional<BlockFace> face = attachableSurface(world, shulker, others, target);
        if (!face.has_value()) {
            continue;
        }
        const Block holder = relative(target, *face);
        const std::optional<std::int32_t> held = world.blockState(holder[0], holder[1], holder[2]);
        if (held.has_value() && refuses && refuses(*held)) {
            continue;
        }
        return Landing{.at = target, .face = *face};
    }
    return std::nullopt;
}

void selectNextMoveDirection(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                             std::optional<std::uint8_t> avoidAxis, const std::optional<std::array<double, 4>>& target,
                             random::RandomSource& random) {
    double yOffset = 0.5;
    Block targetPos{};
    if (!target.has_value()) {
        targetPos = blockOf(bullet.x, bullet.y, bullet.z);
        --targetPos[1];
    } else {
        yOffset = (*target)[3] * 0.5;
        targetPos = blockOf((*target)[0], (*target)[1] + yOffset, (*target)[2]);
    }
    double targetX = targetPos[0] + 0.5;
    double targetY = targetPos[1] + yOffset;
    double targetZ = targetPos[2] + 0.5;
    std::optional<BlockFace> selection;
    const double cx = targetPos[0] + 0.5 - bullet.x;
    const double cy = targetPos[1] + 0.5 - bullet.y;
    const double cz = targetPos[2] + 0.5 - bullet.z;
    if ((cx * cx) + (cy * cy) + (cz * cz) >= 4.0) {
        const Block current = blockOf(bullet.x, bullet.y, bullet.z);
        std::vector<BlockFace> options;
        const auto consider = [&](std::uint8_t axis, BlockFace positive, BlockFace negative) {
            if (avoidAxis == axis) {
                return;
            }
            if (current[axis] < targetPos[axis] && isAir(world, relative(current, positive))) {
                options.push_back(positive);
            } else if (current[axis] > targetPos[axis] && isAir(world, relative(current, negative))) {
                options.push_back(negative);
            }
        };
        consider(0, BlockFace::East, BlockFace::West);
        consider(1, BlockFace::Up, BlockFace::Down);
        consider(2, BlockFace::South, BlockFace::North);
        selection = randomFace(random);
        if (options.empty()) {
            for (std::int32_t attempts = 5; !isAir(world, relative(current, *selection)) && attempts > 0; --attempts) {
                selection = randomFace(random);
            }
        } else {
            selection = options[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(options.size())))];
        }
        targetX = bullet.x + registry::stepX(*selection);
        targetY = bullet.y + registry::stepY(*selection);
        targetZ = bullet.z + registry::stepZ(*selection);
    }
    state.direction = selection;
    const double xa = targetX - bullet.x;
    const double ya = targetY - bullet.y;
    const double za = targetZ - bullet.z;
    const double distance = std::sqrt((xa * xa) + (ya * ya) + (za * za));
    if (distance == 0.0) {
        state.targetDeltaX = 0.0;
        state.targetDeltaY = 0.0;
        state.targetDeltaZ = 0.0;
    } else {
        state.targetDeltaX = xa / distance * kBulletSpeed;
        state.targetDeltaY = ya / distance * kBulletSpeed;
        state.targetDeltaZ = za / distance * kBulletSpeed;
    }
    state.flightSteps = 10 + (random.nextInt(5) * 10);
}

void steer(ServerEntity& bullet, BulletState& state, bool targetAlive) {
    if (!targetAlive) {
        // parity: `applyGravity`, the bullet's own 0.04.
        bullet.velocityY -= 0.04;
        return;
    }
    state.targetDeltaX = std::clamp(state.targetDeltaX * 1.025, -1.0, 1.0);
    state.targetDeltaY = std::clamp(state.targetDeltaY * 1.025, -1.0, 1.0);
    state.targetDeltaZ = std::clamp(state.targetDeltaZ * 1.025, -1.0, 1.0);
    bullet.velocityX += (state.targetDeltaX - bullet.velocityX) * 0.2;
    bullet.velocityY += (state.targetDeltaY - bullet.velocityY) * 0.2;
    bullet.velocityZ += (state.targetDeltaZ - bullet.velocityZ) * 0.2;
}

void turnAfterMove(const world::WorldState& world, ServerEntity& bullet, BulletState& state,
                   const std::array<double, 4>& target, random::RandomSource& random) {
    const auto axisOf = [&state]() -> std::optional<std::uint8_t> {
        if (!state.direction.has_value()) {
            return std::nullopt;
        }
        return static_cast<std::uint8_t>(registry::axisOf(*state.direction));
    };
    if (state.flightSteps > 0) {
        --state.flightSteps;
        if (state.flightSteps == 0) {
            selectNextMoveDirection(world, bullet, state, axisOf(), target, random);
        }
    }
    if (!state.direction.has_value()) {
        return;
    }
    const Block current = blockOf(bullet.x, bullet.y, bullet.z);
    const std::uint8_t axis = *axisOf();
    const Block ahead = relative(current, *state.direction);
    const std::optional<std::int32_t> held = world.blockState(ahead[0], ahead[1], ahead[2]);
    if (held.has_value() && registry::blockShapes::isCollisionFaceFull(*held, BlockFace::Up)) {
        selectNextMoveDirection(world, bullet, state, axis, target, random);
        return;
    }
    const Block targetPos = blockOf(target[0], target[1], target[2]);
    if (current[axis] == targetPos[axis]) {
        selectNextMoveDirection(world, bullet, state, axis, target, random);
    }
}

} // namespace basalt::server::runtime::entity::shulkers
