#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/CopperGolemGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <string>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace golems = copperGolems;
namespace reg = server::registry;

using golems::GolemState;
using golems::Interaction;
using golems::Position;
using golems::Slots;
using golems::Travel;

constexpr std::int32_t kChestSize = 27;
constexpr double kUnits = 16.0;
/// parity: `RandomStroll.stroll(1.0, 2, 2)` and `DoNothing(30, 60)`.
constexpr std::int32_t kStrollRange = 2;
constexpr std::int32_t kWaitMin = 30;
constexpr std::int32_t kWaitMax = 60;
/// parity: `CLOSE_ENOUGH_TO_START_INTERACTING_WITH_TARGET_DISTANCE` and its path-end twin.
constexpr double kReachWalking = 0.5;
constexpr double kReachArrived = 1.0;

[[nodiscard]] std::optional<std::string> chestPath(world::WorldState& world, const Position& at) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    if (chunk == nullptr || chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15) == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || !golems::isChest(block->path())) {
        return std::nullopt;
    }
    return block->path();
}

[[nodiscard]] bool picking(const ServerEntity& mob) { return !golems::mainHand(mob).has_value(); }

/// parity: `isWantedBlock`, by what the golem holds.
[[nodiscard]] bool wanted(const ServerEntity& mob, std::string_view path) {
    return picking(mob) ? golems::isSourceChest(path) : golems::isDestinationChest(path);
}

/// parity: `getConnectedTargets` and the `CompoundContainer` order both halves are read in.
[[nodiscard]] std::vector<Position> halvesOf(world::WorldState& world, const Position& at) {
    if (const auto pair = world::doubleChests::halves(world, at[0], at[1], at[2]); pair.has_value()) {
        return {pair->first, pair->second};
    }
    return {at};
}

[[nodiscard]] Slots readSlots(world::WorldState& world, const std::vector<Position>& halves) {
    Slots slots;
    for (const Position& half : halves) {
        world.unpackLoot(half[0], half[1], half[2]);
        const server::world::Chunk* chunk = world.chunkIfLoaded(half[0] >> 4, half[2] >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(half[0] & 15, half[1], half[2] & 15);
        Slots part = entity == nullptr ? Slots(kChestSize) : world::blockInventory::read(entity->data(), kChestSize);
        slots.insert(slots.end(), part.begin(), part.end());
    }
    return slots;
}

void writeSlots(MobContext& ctx, const std::vector<Position>& halves, const Slots& slots) {
    for (std::size_t index = 0; index < halves.size(); ++index) {
        const Position& half = halves[index];
        server::world::Chunk* chunk = ctx.world.chunkIfLoaded(half[0] >> 4, half[2] >> 4);
        server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(half[0] & 15, half[1], half[2] & 15);
        if (entity == nullptr) {
            continue;
        }
        const auto from = slots.begin() + static_cast<std::ptrdiff_t>(index * kChestSize);
        world::blockInventory::write(entity->data(), Slots(from, from + kChestSize));
        chunk->markDirty();
        ctx.actions.containerChanged(ctx.mob, half[0], half[1], half[2]);
    }
}

/// parity: `ChestBlock.isChestBlockedAt`, by the block above. A cat sitting there is not modelled.
[[nodiscard]] bool blocked(world::WorldState& world, const Position& at) {
    const std::optional<std::int32_t> above = world.blockState(at[0], at[1] + 1, at[2]);
    return above.has_value() && reg::blockSupport::conductsRedstone(*above);
}

[[nodiscard]] bool locked(world::WorldState& world, const Position& at) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    return entity != nullptr && entity->data().find("lock") != nullptr;
}

[[nodiscard]] std::array<double, 3> centreOf(const ServerEntity& mob) {
    return {mob.x, mob.y + (mob.height() / 2.0), mob.z};
}

/// parity: `isWithinTargetDistance`: the chest's collision bounds grown by `distance` across and half
/// a block up and down, against the golem's box centred on `from`.
[[nodiscard]] bool within(MobContext& ctx, const Position& at, double distance, const std::array<double, 3>& from) {
    const std::optional<std::int32_t> stateId = ctx.world.blockState(at[0], at[1], at[2]);
    if (!stateId.has_value()) {
        return false;
    }
    const std::span<const std::int32_t> boxes = reg::blockShapes::collisionBoxes(*stateId);
    if (boxes.size() < 6) {
        return false;
    }
    Aabb bounds{.minX = std::numeric_limits<double>::max(),
                .minY = std::numeric_limits<double>::max(),
                .minZ = std::numeric_limits<double>::max(),
                .maxX = std::numeric_limits<double>::lowest(),
                .maxY = std::numeric_limits<double>::lowest(),
                .maxZ = std::numeric_limits<double>::lowest()};
    for (std::size_t box = 0; box + 5 < boxes.size(); box += 6) {
        bounds.minX = std::min(bounds.minX, boxes[box] / kUnits);
        bounds.minY = std::min(bounds.minY, boxes[box + 1] / kUnits);
        bounds.minZ = std::min(bounds.minZ, boxes[box + 2] / kUnits);
        bounds.maxX = std::max(bounds.maxX, boxes[box + 3] / kUnits);
        bounds.maxY = std::max(bounds.maxY, boxes[box + 4] / kUnits);
        bounds.maxZ = std::max(bounds.maxZ, boxes[box + 5] / kUnits);
    }
    const Aabb target{.minX = at[0] + bounds.minX - distance,
                      .minY = at[1] + bounds.minY - 0.5,
                      .minZ = at[2] + bounds.minZ - distance,
                      .maxX = at[0] + bounds.maxX + distance,
                      .maxY = at[1] + bounds.maxY + 0.5,
                      .maxZ = at[2] + bounds.maxZ + distance};
    const double halfWidth = ctx.mob.width() / 2.0;
    const double halfHeight = ctx.mob.height() / 2.0;
    const Aabb body{.minX = from[0] - halfWidth,
                    .minY = from[1] - halfHeight,
                    .minZ = from[2] - halfWidth,
                    .maxX = from[0] + halfWidth,
                    .maxY = from[1] + halfHeight,
                    .maxZ = from[2] + halfWidth};
    return target.intersects(body);
}

/// parity: `getInteractionRange`, wider once the path has been walked to its end.
[[nodiscard]] double reach(const ServerEntity& mob, const GolemState& state) {
    return !mob.path.has_value() && state.approachEnd.has_value() ? kReachArrived : kReachWalking;
}

/// parity: `canSeeAnyTargetSide`, a line to the middle of each face that meets the chest itself.
[[nodiscard]] bool seesAnySide(MobContext& ctx, const Position& at, const std::array<double, 3>& eye) {
    for (const reg::BlockFace face : {reg::BlockFace::Down, reg::BlockFace::Up, reg::BlockFace::North,
                                      reg::BlockFace::South, reg::BlockFace::West, reg::BlockFace::East}) {
        const double toX = at[0] + 0.5 + (0.5 * reg::stepX(face));
        const double toY = at[1] + 0.5 + (0.5 * reg::stepY(face));
        const double toZ = at[2] + 0.5 + (0.5 * reg::stepZ(face));
        const std::optional<BlockHit> hit = blockRay::cast(ctx.world, eye[0], eye[1], eye[2], toX, toY, toZ);
        if (hit.has_value() && hit->blockX == at[0] && hit->blockY == at[1] && hit->blockZ == at[2]) {
            return true;
        }
    }
    return false;
}

/// The squares beside a chest a golem could stand in, the shortest walk first. Our paths only end on
/// standable squares, where the jar's may end beside the target.
[[nodiscard]] std::optional<std::vector<Position>> pathBeside(MobContext& ctx, const Position& at) {
    std::optional<std::vector<Position>> best;
    const auto from =
        Position{static_cast<std::int32_t>(std::floor(ctx.mob.x)), static_cast<std::int32_t>(std::floor(ctx.mob.y)),
                 static_cast<std::int32_t>(std::floor(ctx.mob.z))};
    for (const std::int32_t dy : {0, -1, 1}) {
        for (const auto& [dx, dz] : {std::pair{0, -1}, std::pair{0, 1}, std::pair{-1, 0}, std::pair{1, 0}}) {
            const Position stand{at[0] + dx, at[1] + dy, at[2] + dz};
            if (!pathFinder::canStandAt(ctx.world, stand[0], stand[1], stand[2])) {
                continue;
            }
            if (stand == from) {
                return std::vector<Position>{};
            }
            std::optional<std::vector<Position>> path =
                pathFinder::find(ctx.world, from[0], from[1], from[2], stand[0], stand[1], stand[2]);
            if (path.has_value() && (!best.has_value() || path->size() < best->size())) {
                best = std::move(path);
            }
        }
    }
    return best;
}

class Transport {
public:
    Transport(MobContext& ctx, GolemState& state) : ctx_(ctx), state_(state) {}

    void tick() {
        expireMemories();
        const bool updated = updateInvalidTarget();
        if (!state_.target.has_value()) {
            onStartTravelling();
            return;
        }
        if (updated) {
            return;
        }
        if (state_.travel == Travel::Queuing) {
            onQueuing();
        }
        if (state_.travel == Travel::Travelling) {
            onTravel();
        }
        if (state_.travel == Travel::Interacting) {
            onReached();
        }
    }

    void onStartTravelling() {
        // parity: `CopperGolemAi.onTravelling`, which lets go of the chest and stands idle.
        closeChest();
        setPose(golems::Pose::Idle);
        state_.travel = Travel::Travelling;
        state_.interaction.reset();
        state_.ticksSinceReaching = 0;
    }

private:
    void expireMemories() {
        if (!state_.visited.empty() && ctx_.gameTime >= state_.visitedUntil) {
            state_.visited.clear();
        }
        if (!state_.unreachable.empty() && ctx_.gameTime >= state_.unreachableUntil) {
            state_.unreachable.clear();
        }
    }

    [[nodiscard]] bool updateInvalidTarget() {
        if (hasValidTarget()) {
            return false;
        }
        stopTargeting();
        if (const std::optional<Position> found = findTarget(); found.has_value()) {
            state_.target = found;
            onStartTravelling();
            setVisited(*found);
            return true;
        }
        enterCooldown();
        return true;
    }

    [[nodiscard]] bool hasValidTarget() {
        if (!state_.target.has_value()) {
            return false;
        }
        const Position at = *state_.target;
        const std::optional<std::string> path = chestPath(ctx_.world, at);
        if (!path.has_value() || !wanted(ctx_.mob, *path) || blocked(ctx_.world, at)) {
            return false;
        }
        if (state_.travel != Travel::Travelling || hasValidTravellingPath(at)) {
            return true;
        }
        markUnreachable(at);
        return false;
    }

    /// parity: `hasValidTravellingPath`, judged from where the path ends.
    [[nodiscard]] bool hasValidTravellingPath(const Position& at) {
        if (!state_.approachEnd.has_value()) {
            if (within(ctx_, at, reach(ctx_.mob, state_), centreOf(ctx_.mob))) {
                return seesAnySide(ctx_, at, centreOf(ctx_.mob));
            }
            std::optional<std::vector<Position>> path = pathBeside(ctx_, at);
            if (!path.has_value()) {
                return false;
            }
            walk(std::move(*path));
        }
        const Position end = state_.approachEnd.value_or(Position{static_cast<std::int32_t>(std::floor(ctx_.mob.x)),
                                                                  static_cast<std::int32_t>(std::floor(ctx_.mob.y)),
                                                                  static_cast<std::int32_t>(std::floor(ctx_.mob.z))});
        const std::array<double, 3> from{end[0] + 0.5, end[1] + (ctx_.mob.height() / 2.0), end[2] + 0.5};
        return within(ctx_, at, reach(ctx_.mob, state_), from) && seesAnySide(ctx_, at, from);
    }

    void walk(std::vector<Position> path) {
        if (path.empty()) {
            state_.approachEnd = Position{static_cast<std::int32_t>(std::floor(ctx_.mob.x)),
                                          static_cast<std::int32_t>(std::floor(ctx_.mob.y)),
                                          static_cast<std::int32_t>(std::floor(ctx_.mob.z))};
            return;
        }
        state_.approachEnd = path.back();
        const Position end = path.back();
        static_cast<void>(navigation::moveTo(ctx_.world, ctx_.mob, end[0] + 0.5, end[1], end[2] + 0.5, 1.0));
    }

    /// parity: `getTransportTarget`, the nearest wanted chest in the loaded chunks around.
    [[nodiscard]] std::optional<Position> findTarget() {
        const auto blockX = static_cast<std::int32_t>(std::floor(ctx_.mob.x));
        const auto blockY = static_cast<std::int32_t>(std::floor(ctx_.mob.y));
        const auto blockZ = static_cast<std::int32_t>(std::floor(ctx_.mob.z));
        const bool riding = ctx_.mob.vehicleId.has_value();
        const std::int32_t across = riding ? 1 : golems::kHorizontalSearch;
        const std::int32_t up = riding ? 1 : golems::kVerticalSearch;
        const std::int32_t chunks = (across / 16) + 1;
        std::optional<Position> best;
        double closest = std::numeric_limits<float>::max();
        for (std::int32_t chunkX = (blockX >> 4) - chunks; chunkX <= (blockX >> 4) + chunks; ++chunkX) {
            for (std::int32_t chunkZ = (blockZ >> 4) - chunks; chunkZ <= (blockZ >> 4) + chunks; ++chunkZ) {
                const server::world::Chunk* chunk = ctx_.world.chunkIfLoaded(chunkX, chunkZ);
                if (chunk == nullptr) {
                    continue;
                }
                for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                    const Position at{(chunkX << 4) + entity.localX(), entity.y(), (chunkZ << 4) + entity.localZ()};
                    const double dx = at[0] + 0.5 - ctx_.mob.x;
                    const double dy = at[1] + 0.5 - ctx_.mob.y;
                    const double dz = at[2] + 0.5 - ctx_.mob.z;
                    const double distance = (dx * dx) + (dy * dy) + (dz * dz);
                    if (distance >= closest) {
                        continue;
                    }
                    // parity: `new AABB(blockPosition()).inflate(...)`, which `contains` checks with
                    // the lower edge closed and the upper one open.
                    const bool inside = at[0] >= blockX - across && at[0] < blockX + 1 + across &&
                                        at[1] >= blockY - up && at[1] < blockY + 1 + up && at[2] >= blockZ - across &&
                                        at[2] < blockZ + 1 + across;
                    if (!inside || !validToPick(at)) {
                        continue;
                    }
                    best = at;
                    closest = distance;
                }
            }
        }
        return best;
    }

    [[nodiscard]] bool validToPick(const Position& at) {
        const std::optional<std::string> path = chestPath(ctx_.world, at);
        if (!path.has_value() || !wanted(ctx_.mob, *path) || locked(ctx_.world, at)) {
            return false;
        }
        for (const Position& half : halvesOf(ctx_.world, at)) {
            if (std::ranges::find(state_.visited, half) != state_.visited.end() ||
                std::ranges::find(state_.unreachable, half) != state_.unreachable.end()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool anotherInteracting(const Position& at) {
        for (const Position& half : halvesOf(ctx_.world, at)) {
            if (ctx_.actions.chestInUse(ctx_.mob, half[0], half[1], half[2])) {
                return true;
            }
        }
        return false;
    }

    void onQueuing() {
        if (!anotherInteracting(*state_.target)) {
            state_.travel = Travel::Travelling;
            walkTowards();
        }
    }

    void onTravel() {
        const Position at = *state_.target;
        if (within(ctx_, at, golems::kQueueDistance, centreOf(ctx_.mob)) && anotherInteracting(at)) {
            stopInPlace();
            state_.travel = Travel::Queuing;
        } else if (within(ctx_, at, reach(ctx_.mob, state_), centreOf(ctx_.mob))) {
            state_.interaction = interactionFor(at);
            state_.travel = Travel::Interacting;
        } else {
            walkTowards();
        }
    }

    void walkTowards() {
        if (ctx_.mob.path.has_value() || !state_.target.has_value()) {
            return;
        }
        // A walk that ended short of the chest looks for another way from here.
        state_.approachEnd.reset();
        if (std::optional<std::vector<Position>> path = pathBeside(ctx_, *state_.target); path.has_value()) {
            walk(std::move(*path));
        }
    }

    void onReached() {
        const Position at = *state_.target;
        if (!within(ctx_, at, golems::kKeepInteractingDistance, centreOf(ctx_.mob))) {
            onStartTravelling();
            return;
        }
        state_.ticksSinceReaching++;
        interact(at);
        if (state_.ticksSinceReaching >= golems::kInteractionTicks) {
            finishInteraction(at);
            onStartTravelling();
        }
    }

    /// parity: `doReachedTargetInteraction`'s four outcomes, decided the moment the golem arrives.
    [[nodiscard]] Interaction interactionFor(const Position& at) {
        const Slots slots = readSlots(ctx_.world, halvesOf(ctx_.world, at));
        const std::optional<server::world::loot::ItemStack> held = golems::mainHand(ctx_.mob);
        if (!held.has_value()) {
            return golems::isEmpty(slots) ? Interaction::PickupNoItem : Interaction::PickupItem;
        }
        return golems::isEmpty(slots) || golems::holdsSameItem(slots, held->item) ? Interaction::PlaceItem
                                                                                  : Interaction::PlaceNoItem;
    }

    /// parity: `onTargetInteraction` over `CopperGolemAi.onReachedTargetInteraction`.
    void interact(const Position& at) {
        lookControl::lookAt(ctx_.mob, at[0] + 0.5, at[1] + 0.5, at[2] + 0.5);
        stopInPlace();
        if (!state_.interaction.has_value()) {
            return;
        }
        if (state_.ticksSinceReaching == golems::kOpenTick) {
            ctx_.actions.golemOpensChest(ctx_.mob, at[0], at[1], at[2], true);
            state_.openedChest = at;
            setPose(poseFor(*state_.interaction));
        }
        if (state_.ticksSinceReaching == golems::kSoundTick) {
            ctx_.actions.mobSound(ctx_.mob, soundFor(*state_.interaction));
        }
        if (state_.ticksSinceReaching == golems::kInteractionTicks) {
            closeChest();
        }
    }

    void finishInteraction(const Position& at) {
        const std::vector<Position> halves = halvesOf(ctx_.world, at);
        Slots slots = readSlots(ctx_.world, halves);
        std::optional<server::world::loot::ItemStack> held = golems::mainHand(ctx_.mob);
        if (!held.has_value()) {
            if (golems::isEmpty(slots)) {
                stopTargeting();
                return;
            }
            // parity: `pickUpItems`, a guaranteed drop from then on.
            const std::optional<server::world::loot::ItemStack> taken = golems::pickUp(slots);
            golems::setMainHand(ctx_.mob, taken, true);
            writeSlots(ctx_, halves, slots);
            ctx_.actions.heldItemChanged(ctx_.mob);
            clearAfterMatch();
            return;
        }
        if (!golems::isEmpty(slots) && !golems::holdsSameItem(slots, held->item)) {
            stopTargeting();
            return;
        }
        // parity: `putDownItem`.
        const std::optional<server::world::loot::ItemStack> left = golems::putDown(slots, *held);
        writeSlots(ctx_, halves, slots);
        golems::setMainHand(ctx_.mob, left, false);
        ctx_.actions.heldItemChanged(ctx_.mob);
        if (left.has_value()) {
            stopTargeting();
        } else {
            clearAfterMatch();
        }
    }

    [[nodiscard]] static golems::Pose poseFor(Interaction interaction) {
        switch (interaction) {
        case Interaction::PickupItem:
            return golems::Pose::GettingItem;
        case Interaction::PickupNoItem:
            return golems::Pose::GettingNoItem;
        case Interaction::PlaceItem:
            return golems::Pose::DroppingItem;
        case Interaction::PlaceNoItem:
            return golems::Pose::DroppingNoItem;
        }
        return golems::Pose::Idle;
    }

    /// parity: the sounds `CopperGolemAi` names, whose registered ids read the other way round for
    /// the two pickups.
    [[nodiscard]] static std::string_view soundFor(Interaction interaction) {
        switch (interaction) {
        case Interaction::PickupItem:
            return "entity.copper_golem.no_item_get";
        case Interaction::PickupNoItem:
            return "entity.copper_golem.no_item_no_get";
        case Interaction::PlaceItem:
            return "entity.copper_golem.item_drop";
        case Interaction::PlaceNoItem:
            return "entity.copper_golem.item_no_drop";
        }
        return {};
    }

    void setPose(golems::Pose pose) { state_.pose = pose; }

    void closeChest() {
        if (state_.openedChest.has_value()) {
            const Position at = *state_.openedChest;
            state_.openedChest.reset();
            ctx_.actions.golemOpensChest(ctx_.mob, at[0], at[1], at[2], false);
        }
    }

    void stopInPlace() {
        navigation::stop(ctx_.mob);
        moveControl::stop(ctx_.mob);
        ctx_.mob.velocityX = 0.0;
        ctx_.mob.velocityZ = 0.0;
    }

    void stopTargeting() {
        state_.ticksSinceReaching = 0;
        state_.target.reset();
        navigation::stop(ctx_.mob);
        state_.approachEnd.reset();
    }

    void clearAfterMatch() {
        stopTargeting();
        state_.visited.clear();
        state_.unreachable.clear();
    }

    void enterCooldown() {
        stopTargeting();
        state_.transportCooldown = golems::kIdleCooldown;
        state_.visited.clear();
        state_.unreachable.clear();
    }

    void setVisited(const Position& at) {
        if (std::ranges::find(state_.visited, at) == state_.visited.end()) {
            state_.visited.push_back(at);
        }
        if (state_.visited.size() > golems::kMaxVisited) {
            enterCooldown();
        } else {
            state_.visitedUntil = ctx_.gameTime + golems::kVisitedMemoryTicks;
        }
    }

    void markUnreachable(const Position& at) {
        std::erase(state_.visited, at);
        if (std::ranges::find(state_.unreachable, at) == state_.unreachable.end()) {
            state_.unreachable.push_back(at);
        }
        if (state_.unreachable.size() > golems::kMaxUnreachable) {
            enterCooldown();
        } else {
            state_.visitedUntil = ctx_.gameTime + golems::kVisitedMemoryTicks;
            state_.unreachableUntil = ctx_.gameTime + golems::kVisitedMemoryTicks;
        }
    }

    MobContext& ctx_;
    GolemState& state_;
};

} // namespace

bool CopperGolemTransportGoal::canUse(MobContext& mob) {
    const GolemState& state = golems::stateOf(mob.mob, mob.random);
    return state.transportCooldown <= 0 && mob.mob.panicTicks <= 0;
}

void CopperGolemTransportGoal::stop(MobContext& mob) {
    Transport{mob, golems::stateOf(mob.mob, mob.random)}.onStartTravelling();
}

void CopperGolemTransportGoal::tick(MobContext& mob) { Transport{mob, golems::stateOf(mob.mob, mob.random)}.tick(); }

bool CopperGolemIdleGoal::canUse(MobContext& mob) {
    const GolemState& state = golems::stateOf(mob.mob, mob.random);
    return state.transportCooldown > 0 && navigation::isDone(mob.mob);
}

bool CopperGolemIdleGoal::canContinueToUse(MobContext& mob) {
    const GolemState& state = golems::stateOf(mob.mob, mob.random);
    return state.waitTicks > 0 || !navigation::isDone(mob.mob);
}

void CopperGolemIdleGoal::start(MobContext& mob) {
    GolemState& state = golems::stateOf(mob.mob, mob.random);
    // parity: `RunOne` shuffles its two choices evenly, and a stroll with nowhere to go gives way
    // to the wait.
    if (mob.random.nextBoolean()) {
        const std::optional<std::array<double, 3>> spot =
            mobBrain::strollTarget(mob.world, mob.mob, mob.random, kStrollRange, kStrollRange);
        if (spot.has_value() && navigation::moveTo(mob.world, mob.mob, (*spot)[0], (*spot)[1], (*spot)[2], 1.0)) {
            return;
        }
    }
    state.waitTicks = kWaitMin + mob.random.nextInt(kWaitMax - kWaitMin + 1);
}

void CopperGolemIdleGoal::tick(MobContext& mob) {
    GolemState& state = golems::stateOf(mob.mob, mob.random);
    if (state.waitTicks > 0) {
        state.waitTicks--;
    }
}

void CopperGolemIdleGoal::stop(MobContext& mob) { golems::stateOf(mob.mob, mob.random).waitTicks = 0; }

} // namespace basalt::server::runtime::entity::ai
