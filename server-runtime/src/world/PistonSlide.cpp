#include "PistonSlide.hpp"

#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
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

/// The box a moving block sweeps: where it is now and where it lands.
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

/// parity: `PistonMovingBlockEntity.saveAdditional` writes the state it carries the way a chunk
/// palette does, by name and properties, so a world of ours opens in vanilla with the block mid
/// slide intact. `facing` is the direction's own 3D value, which is what our `BlockFace` counts in.
[[nodiscard]] wire::nbt::NbtCompound carriedTag(std::int32_t stateId, BlockFace direction, bool extending,
                                                bool source) {
    wire::nbt::NbtCompound state;
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    state.putString("Name", block == nullptr ? "minecraft:air" : block->toString());
    if (const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
        !properties.empty()) {
        wire::nbt::NbtCompound tag;
        for (const auto& [name, value] : properties) {
            tag.putString(name, value);
        }
        state.set("Properties", wire::nbt::NbtTag::ofCompound(std::move(tag)));
    }
    wire::nbt::NbtCompound data;
    data.set("blockState", wire::nbt::NbtTag::ofCompound(std::move(state)));
    data.putInt("facing", static_cast<std::int32_t>(direction));
    // parity: what is saved is `progressO`, a turn behind the live `progress`, and it measures zero
    // for the whole of a slide in three of the four benched circuits. The fourth writes a half on
    // the second turn, which depends on when the block entity first ticks against the block event,
    // and this build lands the slide from the piston's own turn instead of ticking each square.
    data.putFloat("progress", 0.0F);
    data.putBoolean("extending", extending);
    data.putBoolean("source", source);
    return data;
}

/// The state a `moving_piston` is carrying, read back out of its block entity.
[[nodiscard]] std::optional<std::int32_t> carriedState(const wire::nbt::NbtCompound& data) {
    const wire::nbt::NbtCompound* state = data.getCompound("blockState");
    if (state == nullptr) {
        return std::nullopt;
    }
    const std::string_view name = state->getString("Name");
    if (name.empty()) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (const wire::nbt::NbtCompound* tag = state->getCompound("Properties"); tag != nullptr) {
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

/// parity: `checkIfExtend`'s test for a head that has not finished going out yet: the square two along
/// is a `moving_piston` facing our way that is still extending. Vanilla also lets it through on
/// progress or on the tick it last moved, which inside a tick is always true.

/// The half of a write a piston does for every square it touches: the shapes around it are redrawn
/// and whatever they no longer hold up falls, and nobody re-reads the signal. parity: `moveBlocks`
/// writes with `UPDATE_MOVE_BY_PISTON`, and what falls out writes its own air with `UPDATE_ALL`, so
/// a lever knocked off the top of a piston does tell what it was powering.
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
        // parity: a piston reads the signal from `neighborChanged` like anything else, and the lever
        // it has just knocked off its own top was the one powering it: without this the piston stays
        // out with nothing holding it there.
        for (const registry::BlockFace face : kSides) {
            trigger(context, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                    at[2] + registry::stepZ(face));
        }
    }
}

void slide(const Context& context, const Push& push, BlockFace moving, BlockFace facing, bool extending,
           std::vector<BlockChange>* changes = nullptr) {
    // parity: `moveBlocks` builds the pushed block's `moving_piston` from the default state with only
    // the facing set, so it is always `normal`. Only the arm carries the piston's own type.
    const std::optional<std::int32_t> block_ = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("moving_piston"),
        {{"facing", std::string{nameOf(facing)}}, {"type", "normal"}}, std::nullopt);
    if (!block_.has_value()) {
        return;
    }
    // parity: `moveBlocks` reads every pushed state before writing anything, because one block's
    // destination is the next one's source and reading as it goes would carry the wrong state.
    std::vector<std::pair<Position, std::int32_t>> carried;
    carried.reserve(push.moved.size());
    for (const Position& position : push.moved) {
        if (const std::optional<std::int32_t> state = context.world.blockState(position[0], position[1], position[2]);
            state.has_value()) {
            carried.emplace_back(position, *state);
        }
    }
    // parity: the `moving_piston` goes at the destination carrying what will land there, not where
    // the block still stands. That is what a client draws sliding, and why the square a block is
    // leaving stops being solid at once.
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
    // parity: `deleteAfterMove`, the squares nobody landed on. A block that moved into where its
    // neighbour stood must not be wiped by that neighbour leaving.
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

/// The push going out: the blocks slide, the arm turns into the head it is about to be, and the body
/// takes `extended` from the moment the slide starts.
/// parity: `PistonBaseBlock.moveBlocks` walks what it breaks **backwards**, drops it and only then
/// puts air there. Doing it before the slide matters: the arm lands on that square next, and reading
/// what it was afterwards would read the arm.
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
        // parity: `PistonBaseBlock.moveBlocks` raises `BLOCK_DESTROY` on every square it clears, so a
        // sculk sensor hears a piston crushing a torch exactly as it hears one being broken by hand.
        vibrations::emit(context.world, "block_destroy", static_cast<double>((*position)[0]) + 0.5,
                         static_cast<double>((*position)[1]) + 0.5, static_cast<double>((*position)[2]) + 0.5,
                         context.gameTime);
        // parity: `moveBlocks` runs `affectNeighborsAfterRemoval` on every square it clears, and with
        // `movedByPiston` **false**. `RedStoneWireBlock`'s does three things and the order is the
        // whole point: first `updateNeighborsAt` on each of its six squares, **while the line still
        // holds the strength it had**, then its own power update, and then the wires again. That is
        // why a lamp two squares from the cut lights for its four ticks before going out.
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

/// Puts every carried block down where it already stands, which is what
/// `PistonMovingBlockEntity.finalTick` does when the slide ends.
[[nodiscard]] std::vector<BlockChange> landCarried(const Context& context, const std::vector<Carried>& carried) {
    std::vector<BlockChange> changes;
    std::vector<Position> landed;
    for (const Carried& entry : carried) {
        if (context.world.setBlockState(entry.at[0], entry.at[1], entry.at[2], entry.stateId)) {
            changes.push_back(
                BlockChange{.x = entry.at[0], .y = entry.at[1], .z = entry.at[2], .stateId = entry.stateId});
            landed.push_back(entry.at);
        }
    }
    // parity: `PistonMovingBlockEntity.finalTick` ends in `neighborChanged` on the square the block
    // landed on, so a lamp that arrives with nothing powering it any more goes dark. Without it a
    // block kept whatever state it was carrying for ever.
    for (const Position& at : landed) {
        const std::vector<BlockChange> told = redstoneReactors::update(context.world, context.worldName, at[0], at[1],
                                                                       at[2], context.gameTime, context.random, true);
        changes.insert(changes.end(), told.begin(), told.end());
    }
    return changes;
}

/// The slide ending, whichever way it was going.
///
/// parity: `PistonMovingBlockEntity.finalTick`. The body, the arm and every block on the move took
/// their places when the slide began; what is left is turning each `moving_piston` into the state it
/// has been carrying, where it already stands.

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

/// parity: `MovingPistonBlock` and its block entity: while the arm slides, each block on the way is a
/// `moving_piston` carrying the state it will become. That is what a client draws mid-slide, and it
/// is why a block being pushed is not solid for those two ticks.
///
/// parity: the two loops in `moveBlocks`. `moving` is where the blocks go, which is the piston's own

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
    // parity: the body reads `extended` from the moment the slide starts, not when it ends. It is
    // built from the piston that is standing there, or it would come back facing whichever way the
    // default state points.
    const std::optional<std::int32_t> standing = context.world.blockState(pistonX, pistonY, pistonZ);
    if (!standing.has_value()) {
        return;
    }
    if (const std::optional<std::int32_t> body = registry::blockStateDefinitions::stateIdOf(
            wire::common::Identifier::vanilla(path), {{"extended", "true"}}, *standing);
        body.has_value()) {
        static_cast<void>(context.world.setBlockState(pistonX, pistonY, pistonZ, *body));
    }
    // parity: the body and the arm are written like everything else the piston moves, so what was
    // standing on them falls: an extended piston is not a full block any more and a lever on its top
    // cannot survive it.
    settleAround(context, {Position{pistonX, pistonY, pistonZ}, head}, &changes);
}

/// The pull back starting, which vanilla also does the moment the event runs rather than when the
/// blocks land: the body itself becomes a `moving_piston` carrying the piston at rest, the head goes,
/// and a sticky piston drags one block back with it.
///
/// parity: `PistonBaseBlock.triggerEvent` for `b0` 1 and 2, plus `moveBlocks` with `extending` false.
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
    // parity: the extra test `triggerEvent` makes before pulling: a `PUSH_ONLY` block is pushable and
    // still is not dragged back, because only a `NORMAL` one or another piston comes along.
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

/// The blocks mid-slide: this piston's own `moving_piston` squares, furthest along first so nothing
/// lands on a block that has not moved yet. Found by walking out from the piston, not by sweeping
/// the chunks around it: what one piston moves is face-connected, and sweeping also picked up a
/// second piston's slide four squares away, landing its blocks a tick early.
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
            if (std::abs(next[0] - pistonX) > kPushLimit + 1 || std::abs(next[1] - pistonY) > kPushLimit + 1 ||
                std::abs(next[2] - pistonZ) > kPushLimit + 1) {
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
    // Whatever they sweep comes along, which is `moveCollidedEntities`.
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

bool advanceSlide(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    // The block entity a sliding square carries is vanilla's `piston`, whatever the block is called.
    if (entity == nullptr || entity->type() != block()) {
        return false;
    }
    // A slide books one turn of its own, one tick after it starts, and the landing books another the
    // tick after that. Both fall on the same square when the piston is pulling back, and the value
    // already written is what tells them apart: the landing's turn finds the half already there.
    if (entity->data().getFloat("progress") > 0.0F) {
        return false;
    }
    // parity: `progressO = progress` and then `progress += 0.5F`. What is written down is the value
    // from before, so the square that has just been created still says zero and this turn is the one
    // that makes it a half.
    entity->data().putFloat("progress", kSlideStep);
    return true;
}

std::vector<BlockChange> finish(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::vector<PlayerPosition>& players,
                                const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    // parity: a slide already in flight is finished, not reconsidered: `finalTick` asks nothing. The
    // body itself is a `moving_piston` while it pulls back, so what stands here is not always a
    // piston, and its facing is the only thing worth reading.
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

} // namespace basalt::server::runtime::world::pistons
