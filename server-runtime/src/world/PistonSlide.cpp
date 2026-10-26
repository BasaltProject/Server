#include "PistonSlide.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <set>

namespace basalt::server::runtime::world::pistons {
namespace {

using namespace internal;

[[nodiscard]] bool inSweptArea(double x, double y, double z, const Position& block, BlockFace direction) {
    const Position to = step(block, direction);
    const std::int32_t minX = std::min(block[0], to[0]);
    const std::int32_t minY = std::min(block[1], to[1]);
    const std::int32_t minZ = std::min(block[2], to[2]);
    const std::int32_t maxX = std::max(block[0], to[0]) + 1;
    const std::int32_t maxY = std::max(block[1], to[1]) + 1;
    const std::int32_t maxZ = std::max(block[2], to[2]) + 1;
    return x >= minX && x < maxX && y >= minY && y < maxY && z >= minZ && z < maxZ;
}

/// parity: `PistonMovingBlockEntity.saveAdditional` writes the state it carries with.
[[nodiscard]] wire::nbt::NbtCompound carriedTag(std::int32_t stateId, BlockFace direction, bool extending,
                                                bool source) {
    wire::nbt::NbtCompound state;
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    state.putString("id", block == nullptr ? "minecraft:air" : block->toString());
    if (const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
        !properties.empty()) {
        wire::nbt::NbtCompound tag;
        for (const auto& [name, value] : properties) {
            tag.putString(name, value);
        }
        state.set("properties", wire::nbt::NbtTag::ofCompound(std::move(tag)));
    }
    wire::nbt::NbtCompound data;
    data.set("blockState", wire::nbt::NbtTag::ofCompound(std::move(state)));
    data.putInt("facing", static_cast<std::int32_t>(direction));
    // parity: what is saved is `progressO`.
    data.putFloat("progress", 0.0F);
    data.putBoolean("extending", extending);
    data.putBoolean("source", source);
    return data;
}

[[nodiscard]] std::optional<std::int32_t> carriedState(const wire::nbt::NbtCompound& data) {
    const wire::nbt::NbtCompound* state = data.getCompound("blockState");
    if (state == nullptr) {
        return std::nullopt;
    }
    // 26.2 spelled the two fields `Name` and `Properties`.
    const bool legacy = state->contains("Name");
    const std::string_view name = state->getString(legacy ? "Name" : "id");
    if (name.empty()) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (const wire::nbt::NbtCompound* tag = state->getCompound(legacy ? "Properties" : "properties"); tag != nullptr) {
        for (std::size_t index = 0; index < tag->size(); ++index) {
            if (const auto* value = tag->values()[index].get<std::string>(); value != nullptr) {
                properties.emplace(tag->names()[index], *value);
            }
        }
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> block =
        wire::common::Identifier::parse(name);
    if (!block.has_value()) {
        return std::nullopt;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, properties, std::nullopt);
}

/// and whatever they no longer hold up falls, and nobody re-reads the signal. parity: `moveBlocks`
void settleAround(const Context& context, const std::vector<Position>& touched, std::vector<BlockChange>* changes) {
    if (touched.empty()) {
        return;
    }
    const neighbourUpdates::Context around{.world = context.world,
                                           .worldName = context.worldName,
                                           .minY = context.minY,
                                           .maxY = context.maxY,
                                           .random = context.random,
                                           .gameTime = context.gameTime};
    std::vector<Position> knocked;
    const neighbourUpdates::OnBroken dropped = [&context, &knocked](std::int32_t stateId, std::int32_t bx,
                                                                    std::int32_t by, std::int32_t bz) {
        static_cast<void>(context.world.setBlockState(bx, by, bz, air()));
        if (context.onDestroyed) {
            context.onDestroyed(stateId, bx, by, bz);
        }
        knocked.push_back({bx, by, bz});
    };
    for (const Position& at : touched) {
        neighbourUpdates::reshapeOnly(around, at[0], at[1], at[2], dropped);
    }
    constexpr std::array<registry::BlockFace, 6> kSides{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                        registry::BlockFace::North, registry::BlockFace::South,
                                                        registry::BlockFace::West,  registry::BlockFace::East};
    for (const Position& at : knocked) {
        const std::vector<BlockChange> told = redstoneReactors::update(
            context.world, context.worldName, at[0], at[1], at[2], context.gameTime, context.random, true, false);
        if (changes != nullptr) {
            changes->insert(changes->end(), told.begin(), told.end());
        }
        // parity: a piston reads the signal from `neighborChanged` like anything else.
        for (const registry::BlockFace face : kSides) {
            trigger(context, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                    at[2] + registry::stepZ(face));
        }
    }
}

/// parity: the two loops in `moveBlocks`.
void slide(const Context& context, const Push& push, BlockFace moving, BlockFace facing, bool extending,
           std::vector<BlockChange>* changes = nullptr) {
    // parity: `moveBlocks` builds the pushed block's `moving_piston` from the default state with only.
    const std::optional<std::int32_t> block_ = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("moving_piston"),
        {{"facing", std::string{nameOf(facing)}}, {"type", "normal"}}, std::nullopt);
    if (!block_.has_value()) {
        return;
    }
    // parity: `moveBlocks` reads every pushed state before writing anything.
    std::vector<std::pair<Position, std::int32_t>> carried;
    carried.reserve(push.moved.size());
    for (const Position& position : push.moved) {
        if (const std::optional<std::int32_t> state = context.world.blockState(position[0], position[1], position[2]);
            state.has_value()) {
            carried.emplace_back(position, *state);
        }
    }
    // parity: the `moving_piston` goes at the destination carrying what will land there.
    std::set<Position> destinations;
    for (auto entry = carried.rbegin(); entry != carried.rend(); ++entry) {
        const Position to = step(entry->first, moving);
        server::world::Chunk* chunk = context.world.chunkIfLoaded(to[0] >> 4, to[2] >> 4);
        if (chunk == nullptr || !context.world.setBlockState(to[0], to[1], to[2], *block_)) {
            continue;
        }
        destinations.insert(to);
        chunk->setBlockEntity(server::world::ChunkBlockEntity{to[0] & 15, to[1], to[2] & 15, block(),
                                                              carriedTag(entry->second, facing, extending, false)});
    }
    // parity: `deleteAfterMove`.
    std::vector<Position> emptied;
    for (const auto& [position, unused] : carried) {
        if (destinations.contains(position)) {
            continue;
        }
        static_cast<void>(context.world.setBlockState(position[0], position[1], position[2], air()));
        emptied.push_back(position);
    }
    std::vector<Position> touched{destinations.begin(), destinations.end()};
    touched.insert(touched.end(), emptied.begin(), emptied.end());
    settleAround(context, touched, changes);
}

/// parity: `PistonBaseBlock.moveBlocks` walks what it breaks **backwards**.
void destroyPushed(const Context& context, const Push& push, std::vector<BlockChange>& changes) {
    for (auto position = push.destroyed.rbegin(); position != push.destroyed.rend(); ++position) {
        const std::optional<std::int32_t> broken =
            context.world.blockState((*position)[0], (*position)[1], (*position)[2]);
        if (!broken.has_value()) {
            continue;
        }
        if (context.onDestroyed) {
            context.onDestroyed(*broken, (*position)[0], (*position)[1], (*position)[2]);
        }
        static_cast<void>(context.world.setBlockState((*position)[0], (*position)[1], (*position)[2], air()));
        // parity: `PistonBaseBlock.moveBlocks` raises `BLOCK_DESTROY` on every square it clears.
        vibrations::emit(context.world, "block_destroy", static_cast<double>((*position)[0]) + 0.5,
                         static_cast<double>((*position)[1]) + 0.5, static_cast<double>((*position)[2]) + 0.5,
                         context.gameTime);
        // parity: `moveBlocks` runs `affectNeighborsAfterRemoval` on every cleared square with `movedByPiston`.
        const registry::BlockFace kSix[]{registry::BlockFace::Down,  registry::BlockFace::Up,
                                         registry::BlockFace::North, registry::BlockFace::South,
                                         registry::BlockFace::West,  registry::BlockFace::East};
        for (const registry::BlockFace face : kSix) {
            const std::int32_t nx = (*position)[0] + registry::stepX(face);
            const std::int32_t ny = (*position)[1] + registry::stepY(face);
            const std::int32_t nz = (*position)[2] + registry::stepZ(face);
            redstoneComponents::scheduleAround(context.world, context.worldName, nx, ny, nz, false,
                                               comparatorReading::of(context.world));
            for (const BlockChange& told : redstoneReactors::update(context.world, context.worldName, nx, ny, nz,
                                                                    context.gameTime, context.random, true, false)) {
                changes.push_back(told);
            }
        }
        const std::vector<BlockChange> settled =
            redstone::settle(context.world, context.worldName, (*position)[0], (*position)[1], (*position)[2]);
        for (const BlockChange& change : settled) {
            changes.push_back(change);
        }
        for (const BlockChange& change : settled) {
            redstoneComponents::scheduleAroundWire(context.world, context.worldName, change.x, change.y, change.z,
                                                   comparatorReading::of(context.world));
            for (const BlockChange& told :
                 redstoneReactors::updateAroundWire(context.world, context.worldName, change.x, change.y, change.z,
                                                    context.gameTime, context.random, true)) {
                changes.push_back(told);
            }
        }
        if (tripwires::isWire(*broken)) {
            if (const std::optional<std::int32_t> live =
                    registry::blockStateDefinitions::stateIdOf(tripwires::wireBlock(), {{"powered", "true"}}, *broken);
                live.has_value()) {
                tripwires::updateSource(context.world, (*position)[0], (*position)[1], (*position)[2], *live,
                                        context.gameTime, changes);
            }
        }
    }
}

/// parity: `PistonMovingBlockEntity.tick` dries what it lands.
[[nodiscard]] std::int32_t dried(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto logged = properties.find("waterlogged");
    if (logged == properties.end() || logged->second != "true") {
        return stateId;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, {{"waterlogged", "false"}}, stateId).value_or(stateId);
}

/// Puts every carried block down where it already stands. parity: `PistonMovingBlockEntity.finalTick`.
[[nodiscard]] std::vector<BlockChange> landCarried(const Context& context, const std::vector<Carried>& carried) {
    std::vector<BlockChange> changes;
    std::vector<Position> landed;
    for (const Carried& entry : carried) {
        // parity: `PistonMovingBlockEntity` runs `Block.updateFromNeighbourShapes` on the state it is.
        if (registry::blockFluids::hasWaterFluid(entry.stateId)) {
            context.world.scheduleFluidTick(entry.at[0], entry.at[1], entry.at[2], fluidId(FluidKind::Water, true),
                                            fluidFlow::kWaterTickDelay);
        }
        const std::int32_t stateId = dried(entry.stateId);
        if (context.world.setBlockState(entry.at[0], entry.at[1], entry.at[2], stateId)) {
            changes.push_back(BlockChange{.x = entry.at[0], .y = entry.at[1], .z = entry.at[2], .stateId = stateId});
            landed.push_back(entry.at);
        }
    }
    // parity: `PistonMovingBlockEntity.finalTick` ends in `neighborChanged` on the square the block.
    for (const Position& at : landed) {
        // parity: the flag is sixty-seven, so the write itself tells the six neighbours before the.
        if (context.onLanded) {
            context.onLanded(at[0], at[1], at[2]);
        }
        const std::vector<BlockChange> told = redstoneReactors::update(context.world, context.worldName, at[0], at[1],
                                                                       at[2], context.gameTime, context.random, true);
        changes.insert(changes.end(), told.begin(), told.end());
    }
    return changes;
}

void addBoxes(std::vector<entity::Aabb>& boxes, std::int32_t stateId, double x, double y, double z) {
    constexpr double kUnits = registry::blockShapes::kUnits;
    const std::span<const std::int32_t> shape = registry::blockShapes::collisionBoxes(stateId);
    for (std::size_t at = 0; at + 5 < shape.size(); at += registry::blockShapes::kCorners) {
        boxes.push_back(entity::Aabb{.minX = x + (shape[at] / kUnits),
                                     .minY = y + (shape[at + 1] / kUnits),
                                     .minZ = z + (shape[at + 2] / kUnits),
                                     .maxX = x + (shape[at + 3] / kUnits),
                                     .maxY = y + (shape[at + 4] / kUnits),
                                     .maxZ = z + (shape[at + 5] / kUnits)});
    }
}

[[nodiscard]] std::optional<std::int32_t> withProperties(const wire::common::Identifier& block,
                                                         std::map<std::string, std::string> properties) {
    return registry::blockStateDefinitions::stateIdOf(block, properties, std::nullopt);
}

} // namespace

[[nodiscard]] bool isRetractingOntoSlide(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                         BlockFace direction) {
    const Position pushed = step(Position{x, y, z}, direction, 2);
    const std::optional<std::int32_t> state = context.world.blockState(pushed[0], pushed[1], pushed[2]);
    if (!state.has_value() || pathOf(*state) != "moving_piston" ||
        faceNamed(propertyOf(*state, "facing")) != direction) {
        return false;
    }
    const server::world::Chunk* chunk = context.world.chunkIfLoaded(pushed[0] >> 4, pushed[2] >> 4);
    if (chunk == nullptr) {
        return false;
    }
    for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
        if (entity.type() != block() || entity.localX() != (pushed[0] & 15) || entity.y() != pushed[1] ||
            entity.localZ() != (pushed[2] & 15)) {
            continue;
        }
        return entity.data().getBoolean("extending");
    }
    return false;
}

/// takes `extended` at once. parity: `MovingPistonBlock`: each square on the way is a `moving_piston`
void beginMove(const Context& context, const Push& push, BlockFace direction, bool sticky, std::int32_t pistonX,
               std::int32_t pistonY, std::int32_t pistonZ, const std::string& path, std::vector<BlockChange>& changes) {
    destroyPushed(context, push, changes);
    slide(context, push, direction, direction, true, &changes);
    const std::optional<std::int32_t> movingArm = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("moving_piston"),
        {{"facing", std::string{nameOf(direction)}}, {"type", sticky ? "sticky" : "normal"}}, std::nullopt);
    // parity: the arm slides too, so the square in front carries the head it is about to be.
    const Position head = step(Position{pistonX, pistonY, pistonZ}, direction);
    const std::optional<std::int32_t> arm = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("piston_head"),
        {{"facing", std::string{nameOf(direction)}}, {"type", sticky ? "sticky" : "normal"}, {"short", "false"}},
        std::nullopt);
    server::world::Chunk* armChunk = context.world.chunkIfLoaded(head[0] >> 4, head[2] >> 4);
    if (arm.has_value() && movingArm.has_value() && armChunk != nullptr &&
        context.world.setBlockState(head[0], head[1], head[2], *movingArm)) {
        armChunk->setBlockEntity(server::world::ChunkBlockEntity{head[0] & 15, head[1], head[2] & 15, block(),
                                                                 carriedTag(*arm, direction, true, true)});
    }
    // parity: the body reads `extended` from the moment the slide starts.
    const std::optional<std::int32_t> standing = context.world.blockState(pistonX, pistonY, pistonZ);
    if (!standing.has_value()) {
        return;
    }
    if (const std::optional<std::int32_t> body = registry::blockStateDefinitions::stateIdOf(
            wire::common::Identifier::vanilla(path), {{"extended", "true"}}, *standing);
        body.has_value()) {
        static_cast<void>(context.world.setBlockState(pistonX, pistonY, pistonZ, *body));
    }
    // parity: the body and the arm are written like everything else the piston moves, so what was.
    settleAround(context, {Position{pistonX, pistonY, pistonZ}, head}, &changes);
}

/// parity: `PistonBaseBlock.triggerEvent` for `b0` 1 and 2.
void beginRetract(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  BlockFace direction, bool sticky, const std::string& path, bool sliding) {
    const std::optional<std::int32_t> bodyMoving = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("moving_piston"),
        {{"facing", std::string{nameOf(direction)}}, {"type", sticky ? "sticky" : "normal"}}, std::nullopt);
    const std::optional<std::int32_t> resting = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(path), {{"extended", "false"}}, stateId);
    server::world::Chunk* chunk = context.world.chunkIfLoaded(x >> 4, z >> 4);
    if (!bodyMoving.has_value() || !resting.has_value() || chunk == nullptr) {
        return;
    }
    if (!context.world.setBlockState(x, y, z, *bodyMoving)) {
        return;
    }
    chunk->setBlockEntity(
        server::world::ChunkBlockEntity{x & 15, y, z & 15, block(), carriedTag(*resting, direction, false, true)});

    const Position head = step(Position{x, y, z}, direction);
    static_cast<void>(context.world.setBlockState(head[0], head[1], head[2], air()));
    if (!sticky || sliding) {
        return;
    }
    // parity: the extra test `triggerEvent` makes before pulling.
    const Position pulled = step(head, direction);
    const std::optional<std::int32_t> state = context.world.blockState(pulled[0], pulled[1], pulled[2]);
    if (!state.has_value() || *state == air() ||
        !isPushable(context, pulled[0], pulled[1], pulled[2], registry::opposite(direction), false, direction)) {
        return;
    }
    if (registry::blockSupport::pushReaction(*state) != registry::PushReaction::Normal && !isPiston(pathOf(*state))) {
        return;
    }
    const std::optional<Push> push = resolve(context, x, y, z, direction, false);
    if (!push.has_value()) {
        return;
    }
    slide(context, *push, registry::opposite(direction), direction, false);
}

[[nodiscard]] std::vector<Carried> carriedBlocks(const Context& context, std::int32_t pistonX, std::int32_t pistonY,
                                                 std::int32_t pistonZ, BlockFace direction) {
    std::vector<Carried> carried;
    std::set<Position> seen;
    std::deque<Position> pending;
    pending.push_back(Position{pistonX, pistonY, pistonZ});
    seen.insert(Position{pistonX, pistonY, pistonZ});
    while (!pending.empty()) {
        const Position at = pending.front();
        pending.pop_front();
        const server::world::Chunk* chunk = context.world.chunkIfLoaded(at[0] >> 4, at[2] >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
        const bool moving = entity != nullptr && entity->type() == block();
        const std::optional<std::int32_t> carriedStateId =
            moving ? carriedState(entity->data()) : std::optional<std::int32_t>{};
        if (moving && carriedStateId.has_value()) {
            carried.push_back(
                Carried{.at = at, .stateId = *carriedStateId, .extending = entity->data().getBoolean("extending")});
        } else if (at != Position{pistonX, pistonY, pistonZ}) {
            continue;
        }
        for (const BlockFace face : kAllFaces) {
            const Position next = step(at, face);
            const std::int32_t reach = context.world.gameplay().blocks.pistonBlockPushLimit + 1;
            if (std::abs(next[0] - pistonX) > reach || std::abs(next[1] - pistonY) > reach ||
                std::abs(next[2] - pistonZ) > reach) {
                continue;
            }
            if (seen.insert(next).second) {
                pending.push_back(next);
            }
        }
    }
    std::ranges::sort(carried, [direction](const Carried& one, const Carried& other) {
        const auto along = [direction](const Position& at) {
            return (at[0] * registry::stepX(direction)) + (at[1] * registry::stepY(direction)) +
                   (at[2] * registry::stepZ(direction));
        };
        return along(one.at) > along(other.at);
    });
    return carried;
}

namespace {

[[nodiscard]] std::vector<BlockChange> land(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                            BlockFace direction, const std::vector<PlayerPosition>& players,
                                            const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    const std::vector<Carried> carried = carriedBlocks(context, x, y, z, direction);
    if (carried.empty()) {
        return {};
    }
    std::vector<Position> moving;
    moving.reserve(carried.size());
    for (const Carried& entry : carried) {
        moving.push_back(entry.at);
    }
    dragAlong(context, moving, carried.front().extending ? direction : registry::opposite(direction), players,
              onPlayerPushed);
    return landCarried(context, carried);
}

} // namespace

/// parity: `PistonMovingBlockEntity.tick`'s own half.
constexpr float kSlideStep = 0.5F;

const wire::common::Identifier& slideTickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("moving_piston");
    return instance;
}

std::vector<BlockChange> slideTick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const std::vector<PlayerPosition>& players,
                                   const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    server::world::Chunk* chunk = context.world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr || entity->type() != block()) {
        return {};
    }
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "moving_piston") {
        return {};
    }
    // parity: a slide that has just been created spends its first ticker turn moving `progress` from.
    if (context.world.slideStartedThisTick(x, y, z)) {
        return {};
    }
    // parity: `progressO = progress` and then `progress += 0.5F`.
    if (entity->data().getFloat("progress") <= 0.0F) {
        entity->data().putFloat("progress", kSlideStep);
        return {};
    }
    const std::optional<std::int32_t> carried = carriedState(entity->data());
    const std::optional<BlockFace> facing = faceNamed(propertyOf(*stateId, "facing"));
    if (!carried.has_value() || !facing.has_value()) {
        return {};
    }
    const bool extending = entity->data().getBoolean("extending");
    const Carried landing{.at = Position{x, y, z}, .stateId = *carried, .extending = extending};
    dragAlong(context, {landing.at}, extending ? *facing : registry::opposite(*facing), players, onPlayerPushed);
    return landCarried(context, {landing});
}

std::vector<BlockChange> finish(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::vector<PlayerPosition>& players,
                                const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    // parity: a slide already in flight is finished, not reconsidered: `finalTick` asks nothing.
    const std::string path = pathOf(*stateId);
    if (!isPiston(path) && path != "moving_piston") {
        return {};
    }
    const std::optional<BlockFace> direction = faceNamed(propertyOf(*stateId, "facing"));
    if (!direction.has_value()) {
        return {};
    }
    return land(context, x, y, z, *direction, players, onPlayerPushed);
}

void dragAlong(const Context& context, const std::vector<Position>& moving, registry::BlockFace direction,
               const std::vector<PlayerPosition>& players,
               const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    if (moving.empty()) {
        return;
    }
    const std::vector<DroppedItem> items = context.world.items();
    for (const Position& position : moving) {
        for (const DroppedItem& item : items) {
            if (!inSweptArea(item.x, item.y, item.z, position, direction)) {
                continue;
            }
            static_cast<void>(context.world.shiftItem(item.entityId, registry::stepX(direction),
                                                      registry::stepY(direction), registry::stepZ(direction)));
        }
        if (!onPlayerPushed) {
            continue;
        }
        for (const PlayerPosition& player : players) {
            if (!inSweptArea(player.x, player.y, player.z, position, direction)) {
                continue;
            }
            onPlayerPushed(PlayerPosition{.uuid = player.uuid,
                                          .x = player.x + registry::stepX(direction),
                                          .y = player.y + registry::stepY(direction),
                                          .z = player.z + registry::stepZ(direction)});
        }
    }
}

std::vector<entity::Aabb> movingCollision(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::vector<entity::Aabb> boxes;
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr || entity->type() != block()) {
        return boxes;
    }
    const wire::nbt::NbtCompound& data = entity->data();
    const std::optional<std::int32_t> carried = carriedState(data);
    const std::int32_t facingIndex = data.getInt("facing");
    if (!carried.has_value() || facingIndex < 0 || facingIndex >= registry::kBlockFaceCount) {
        return boxes;
    }
    const auto direction = static_cast<BlockFace>(facingIndex);
    const bool extending = data.getBoolean("extending");
    const bool source = data.getBoolean("source");
    const float progress = data.getFloat("progress");
    static constexpr std::array<std::string_view, registry::kBlockFaceCount> kNames{"down",  "up",   "north",
                                                                                    "south", "west", "east"};
    const std::string carriedPath = pathOf(*carried);
    const bool pistonBase = carriedPath == "piston" || carriedPath == "sticky_piston";
    // parity: a retracting base keeps the arm it had out, where it stands.
    if (!extending && source && pistonBase) {
        std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*carried);
        properties["extended"] = "true";
        if (const std::optional<std::int32_t> extended =
                withProperties(*registry::blockStateDefinitions::blockOf(*carried), properties);
            extended.has_value()) {
            addBoxes(boxes, *extended, x, y, z);
        }
    }
    std::optional<std::int32_t> moved = carried;
    if (source) {
        // parity: the source square is the head, short while it is nearly home.
        moved = withProperties(wire::common::Identifier::vanilla("piston_head"),
                               {{"facing", std::string{kNames[static_cast<std::size_t>(facingIndex)]}},
                                {"short", extending != (1.0F - progress < 0.25F) ? "true" : "false"},
                                {"type", "normal"}});
    }
    if (!moved.has_value()) {
        return boxes;
    }
    const float along = extending ? progress - 1.0F : 1.0F - progress;
    addBoxes(boxes, *moved, x + static_cast<double>(static_cast<float>(registry::stepX(direction)) * along),
             y + static_cast<double>(static_cast<float>(registry::stepY(direction)) * along),
             z + static_cast<double>(static_cast<float>(registry::stepZ(direction)) * along));
    return boxes;
}

} // namespace basalt::server::runtime::world::pistons
