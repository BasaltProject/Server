#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace basalt::server::runtime::world::pistons {
namespace {

using registry::BlockFace;

/// parity: `PistonBaseBlock.TRIGGER_EXTEND` and `TRIGGER_CONTRACT`.
constexpr std::int32_t kExtend = 0;
constexpr std::int32_t kContract = 1;

constexpr std::string_view kExtendSound = "block.piston.extend";
constexpr std::string_view kContractSound = "block.piston.contract";
constexpr float kSoundVolume = 0.5F;
constexpr float kExtendPitchSpread = 0.25F;
constexpr float kContractPitchSpread = 0.15F;
constexpr float kBasePitch = 0.6F;

constexpr std::array<BlockFace, 6> kAllFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                             BlockFace::South, BlockFace::West, BlockFace::East};

[[nodiscard]] std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
        return "up";
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    default:
        return "east";
    }
}

[[nodiscard]] std::optional<BlockFace> faceNamed(std::string_view name) {
    for (const BlockFace face : kAllFaces) {
        if (nameOf(face) == name) {
            return face;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id == nullptr ? std::string{} : std::string{id->path()};
}

[[nodiscard]] bool isPiston(std::string_view path) { return path == "piston" || path == "sticky_piston"; }

/// parity: the four blocks `isPushable` names one by one before it looks at anything else.
[[nodiscard]] bool isImmovable(std::string_view path) {
    return path == "obsidian" || path == "crying_obsidian" || path == "respawn_anchor" ||
           path == "reinforced_deepslate";
}

/// parity: `Blocks.SLIME_BLOCK` and `Blocks.HONEY_BLOCK`, the two that carry their neighbours.
[[nodiscard]] bool isSticky(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "slime_block" || path == "honey_block";
}

/// parity: `canStickToEachOther`: slime and honey each stick to anything but each other.
[[nodiscard]] bool sticksTo(std::int32_t one, std::int32_t other) {
    const std::string first = pathOf(one);
    const std::string second = pathOf(other);
    if ((first == "honey_block" && second == "slime_block") || (first == "slime_block" && second == "honey_block")) {
        return false;
    }
    return isSticky(one) || isSticky(other);
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] Position step(const Position& from, BlockFace face, std::int32_t times = 1) {
    return Position{from[0] + (registry::stepX(face) * times), from[1] + (registry::stepY(face) * times),
                    from[2] + (registry::stepZ(face) * times)};
}

/// One push being worked out. parity: `PistonStructureResolver`'s own two lists.
class Resolver {
public:
    Resolver(const Context& context, Position piston, BlockFace pushing)
        : context_(context), piston_(piston), pushing_(pushing) {}

    /// parity: `addBlockLine`: what a sticky block drags behind it, then the line ahead of it.
    ///
    /// `connection` is the direction this line was reached from, which only a `PUSH_ONLY` block reads:
    /// glazed terracotta moves when the push comes at it head on and refuses every other approach.
    bool addLine(const Position& start, BlockFace connection) {
        std::optional<std::int32_t> nextState = stateAt(start);
        if (!nextState.has_value() || *nextState == air()) {
            return true;
        }
        if (!isPushable(context_, start[0], start[1], start[2], pushing_, false, connection) || isPistonItself(start) ||
            indexOf(start) > -1) {
            return true;
        }
        std::int32_t blockCount = 1;
        if (blockCount + static_cast<std::int32_t>(toPush.size()) > kPushLimit) {
            return false;
        }
        while (isSticky(*nextState)) {
            const Position behind = step(start, registry::opposite(pushing_), blockCount);
            const std::int32_t previous = *nextState;
            nextState = stateAt(behind);
            if (!nextState.has_value() || *nextState == air() || !sticksTo(previous, *nextState) ||
                !isPushable(context_, behind[0], behind[1], behind[2], pushing_, false, registry::opposite(pushing_)) ||
                isPistonItself(behind)) {
                break;
            }
            ++blockCount;
            if (blockCount + static_cast<std::int32_t>(toPush.size()) > kPushLimit) {
                return false;
            }
        }
        std::int32_t blocksAdded = 0;
        for (std::int32_t index = blockCount - 1; index >= 0; --index) {
            toPush.push_back(step(start, registry::opposite(pushing_), index));
            ++blocksAdded;
        }
        std::int32_t ahead = 1;
        while (true) {
            const Position position = step(start, pushing_, ahead);
            const std::int32_t collision = indexOf(position);
            if (collision > -1) {
                reorderAtCollision(blocksAdded, collision);
                for (std::int32_t index = 0; index <= collision + blocksAdded; ++index) {
                    if (index >= static_cast<std::int32_t>(toPush.size())) {
                        break;
                    }
                    const Position moved = toPush[static_cast<std::size_t>(index)];
                    const std::optional<std::int32_t> movedState = stateAt(moved);
                    if (movedState.has_value() && isSticky(*movedState) && !addBranches(moved)) {
                        return false;
                    }
                }
                return true;
            }
            const std::optional<std::int32_t> state = stateAt(position);
            if (!state.has_value() || *state == air()) {
                return true;
            }
            if (!isPushable(context_, position[0], position[1], position[2], pushing_, true, pushing_) ||
                isPistonItself(position)) {
                return false;
            }
            if (registry::blockSupport::pushReaction(*state) == registry::PushReaction::Destroy) {
                destroyed.push_back(position);
                return true;
            }
            if (static_cast<std::int32_t>(toPush.size()) >= kPushLimit) {
                return false;
            }
            toPush.push_back(position);
            ++ahead;
        }
    }

    /// parity: `addBranchingBlocks`: every side a sticky block reaches, off the push axis.
    bool addBranches(const Position& from) {
        const std::optional<std::int32_t> fromState = stateAt(from);
        if (!fromState.has_value()) {
            return true;
        }
        for (const BlockFace face : kAllFaces) {
            if (registry::axisOf(face) == registry::axisOf(pushing_)) {
                continue;
            }
            const Position neighbour = step(from, face);
            const std::optional<std::int32_t> neighbourState = stateAt(neighbour);
            if (!neighbourState.has_value() || !sticksTo(*neighbourState, *fromState)) {
                continue;
            }
            if (!addLine(neighbour, face)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::optional<std::int32_t> stateAt(const Position& position) const {
        return context_.world.blockState(position[0], position[1], position[2]);
    }

    std::vector<Position> toPush;
    std::vector<Position> destroyed;

private:
    [[nodiscard]] std::int32_t indexOf(const Position& position) const {
        const auto found = std::ranges::find(toPush, position);
        return found == toPush.end() ? -1 : static_cast<std::int32_t>(found - toPush.begin());
    }

    [[nodiscard]] bool isPistonItself(const Position& position) const { return position == piston_; }

    /// parity: `reorderListAtCollision`, which puts the line that just arrived ahead of the rest.
    void reorderAtCollision(std::int32_t blocksAdded, std::int32_t collision) {
        const auto head = static_cast<std::size_t>(collision);
        const std::size_t added = static_cast<std::size_t>(blocksAdded);
        std::vector<Position> reordered;
        reordered.reserve(toPush.size());
        reordered.insert(reordered.end(), toPush.begin(), toPush.begin() + static_cast<std::ptrdiff_t>(head));
        reordered.insert(reordered.end(), toPush.end() - static_cast<std::ptrdiff_t>(added), toPush.end());
        reordered.insert(reordered.end(), toPush.begin() + static_cast<std::ptrdiff_t>(head),
                         toPush.end() - static_cast<std::ptrdiff_t>(added));
        toPush = std::move(reordered);
    }

    const Context& context_;
    Position piston_;
    BlockFace pushing_;
};

[[nodiscard]] float pitch(random::RandomSource& random, float spread) {
    return (random.nextFloat() * spread) + kBasePitch;
}

/// parity: `PistonBaseBlock.getNeighborSignal`, which ignores its own face: a piston cannot be
/// powered by what it is about to push.
[[nodiscard]] bool signalToPiston(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                  BlockFace facing) {
    for (const BlockFace face : kAllFaces) {
        if (face == facing) {
            continue;
        }
        if (redstone::signalFrom(context.world, context.worldName, x + registry::stepX(face), y + registry::stepY(face),
                                 z + registry::stepZ(face), face) > 0) {
            return true;
        }
    }
    // parity: the block above is asked twice, once for itself and once for what is over it, which is
    // the quirk that makes a piston fire from a signal two blocks up.
    if (redstone::signalFrom(context.world, context.worldName, x, y + 1, z, BlockFace::Up) > 0) {
        return true;
    }
    for (const BlockFace face : kAllFaces) {
        if (face == BlockFace::Down) {
            continue;
        }
        if (redstone::signalFrom(context.world, context.worldName, x + registry::stepX(face),
                                 y + 1 + registry::stepY(face), z + registry::stepZ(face), face) > 0) {
            return true;
        }
    }
    return false;
}

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

/// parity: `MovingPistonBlock` and its block entity: while the arm slides, each block on the way is a
/// `moving_piston` carrying the state it will become. That is what a client draws mid-slide, and it
/// is why a block being pushed is not solid for those two ticks.
void beginMove(const Context& context, const Push& push, BlockFace direction, bool sticky) {
    const std::optional<std::int32_t> moving = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla("moving_piston"),
        {{"facing", std::string{nameOf(direction)}}, {"type", sticky ? "sticky" : "normal"}}, std::nullopt);
    if (!moving.has_value()) {
        return;
    }
    for (const Position& position : push.moved) {
        const std::optional<std::int32_t> carried = context.world.blockState(position[0], position[1], position[2]);
        server::world::Chunk* chunk = context.world.chunkIfLoaded(position[0] >> 4, position[2] >> 4);
        if (!carried.has_value() || chunk == nullptr ||
            !context.world.setBlockState(position[0], position[1], position[2], *moving)) {
            continue;
        }
        wire::nbt::NbtCompound data;
        data.putInt("blockState", *carried);
        data.putString("facing", std::string{nameOf(direction)});
        data.putBoolean("extending", true);
        data.putBoolean("source", false);
        chunk->setBlockEntity(
            server::world::ChunkBlockEntity{position[0] & 15, position[1], position[2] & 15, block(), std::move(data)});
    }
}

/// The blocks mid-slide: every `moving_piston` around the piston, with the state it carries, furthest
/// along first so nothing lands on a block that has not moved yet.
[[nodiscard]] std::vector<std::pair<Position, std::int32_t>> carriedBlocks(const Context& context, std::int32_t pistonX,
                                                                           std::int32_t pistonY, std::int32_t pistonZ,
                                                                           BlockFace direction) {
    // A push reaches twelve blocks plus its branches, so the chunk the piston is in and the eight
    // around it hold every block that is on the move.
    std::vector<std::pair<Position, std::int32_t>> carried;
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            const server::world::Chunk* chunk =
                context.world.chunkIfLoaded((pistonX >> 4) + offsetX, (pistonZ >> 4) + offsetZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                if (entity.type() != block()) {
                    continue;
                }
                if (!entity.data().contains("blockState")) {
                    continue;
                }
                const std::int32_t state = entity.data().getInt("blockState");
                const std::int32_t x = (chunk->chunkX() << 4) + entity.localX();
                const std::int32_t z = (chunk->chunkZ() << 4) + entity.localZ();
                if (std::abs(x - pistonX) > kPushLimit + 1 || std::abs(entity.y() - pistonY) > kPushLimit + 1 ||
                    std::abs(z - pistonZ) > kPushLimit + 1) {
                    continue;
                }
                carried.emplace_back(Position{x, entity.y(), z}, state);
            }
        }
    }
    std::ranges::sort(carried, [direction](const auto& one, const auto& other) {
        const auto along = [direction](const Position& at) {
            return (at[0] * registry::stepX(direction)) + (at[1] * registry::stepY(direction)) +
                   (at[2] * registry::stepZ(direction));
        };
        return along(one.first) > along(other.first);
    });
    return carried;
}

/// Puts every carried block down one along, which is what `PistonMovingBlockEntity.finalTick` does
/// when the slide ends.
[[nodiscard]] std::vector<BlockChange> landCarried(const Context& context,
                                                   const std::vector<std::pair<Position, std::int32_t>>& carried,
                                                   BlockFace direction) {
    std::vector<BlockChange> changes;
    for (const auto& [position, stateId] : carried) {
        const Position to = step(position, direction);
        if (context.world.setBlockState(position[0], position[1], position[2], air())) {
            changes.push_back(BlockChange{.x = position[0], .y = position[1], .z = position[2], .stateId = air()});
        }
        if (context.world.setBlockState(to[0], to[1], to[2], stateId)) {
            changes.push_back(BlockChange{.x = to[0], .y = to[1], .z = to[2], .stateId = stateId});
        }
    }
    return changes;
}

[[nodiscard]] std::vector<BlockChange> extend(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, BlockFace direction, const std::string& path,
                                              const std::vector<PlayerPosition>& players,
                                              const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    // The blocks are `moving_piston` right now, each carrying what it is about to become.
    const std::vector<std::pair<Position, std::int32_t>> carried = carriedBlocks(context, x, y, z, direction);
    std::vector<Position> moving;
    moving.reserve(carried.size());
    for (const auto& [position, unused] : carried) {
        moving.push_back(position);
    }
    // Whatever they sweep comes along, which is `moveCollidedEntities`.
    dragAlong(context, moving, direction, players, onPlayerPushed);
    std::vector<BlockChange> changes = landCarried(context, carried, direction);
    // parity: the arm, which is a block of its own and carries whether the piston is sticky.
    const Position head = step(Position{x, y, z}, direction);
    const std::optional<std::int32_t> arm =
        registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("piston_head"),
                                                   {{"facing", std::string{nameOf(direction)}},
                                                    {"type", path == "sticky_piston" ? "sticky" : "normal"},
                                                    {"short", "false"}},
                                                   std::nullopt);
    if (arm.has_value() && context.world.setBlockState(head[0], head[1], head[2], *arm)) {
        changes.push_back(BlockChange{.x = head[0], .y = head[1], .z = head[2], .stateId = *arm});
    }
    const std::optional<std::int32_t> body = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(path), {{"extended", "true"}}, stateId);
    if (body.has_value() && context.world.setBlockState(x, y, z, *body)) {
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *body});
    }
    return changes;
}

/// parity: a sticky piston brings one block back with it; a plain one leaves it where it is.
[[nodiscard]] std::vector<BlockChange> retract(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, BlockFace direction, const std::string& path) {
    std::vector<BlockChange> changes;
    const Position head = step(Position{x, y, z}, direction);
    if (context.world.setBlockState(head[0], head[1], head[2], air())) {
        changes.push_back(BlockChange{.x = head[0], .y = head[1], .z = head[2], .stateId = air()});
    }
    if (path == "sticky_piston") {
        const Position pulled = step(head, direction);
        const std::optional<std::int32_t> state = context.world.blockState(pulled[0], pulled[1], pulled[2]);
        if (state.has_value() && *state != air() &&
            isPushable(context, pulled[0], pulled[1], pulled[2], registry::opposite(direction), false, direction)) {
            if (context.world.setBlockState(head[0], head[1], head[2], *state)) {
                changes.push_back(BlockChange{.x = head[0], .y = head[1], .z = head[2], .stateId = *state});
            }
            if (context.world.setBlockState(pulled[0], pulled[1], pulled[2], air())) {
                changes.push_back(BlockChange{.x = pulled[0], .y = pulled[1], .z = pulled[2], .stateId = air()});
            }
        }
    }
    const std::optional<std::int32_t> body = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(path), {{"extended", "false"}}, stateId);
    if (body.has_value() && context.world.setBlockState(x, y, z, *body)) {
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *body});
    }
    return changes;
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("piston");
    return instance;
}

bool isPushable(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace pushing,
                bool allowDestroyable, registry::BlockFace connection) {
    if (y < context.minY || y > context.maxY) {
        return false;
    }
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    if (*stateId == air()) {
        return true;
    }
    const std::string path = pathOf(*stateId);
    if (path.empty() || isImmovable(path)) {
        return false;
    }
    if (pushing == BlockFace::Down && y == context.minY) {
        return false;
    }
    if (pushing == BlockFace::Up && y == context.maxY) {
        return false;
    }
    if (!isPiston(path)) {
        const registry::BlockHardness* hardness = registry::blocks::hardnessOf(wire::common::Identifier::vanilla(path));
        if (hardness != nullptr && hardness->isUnbreakable()) {
            return false;
        }
        switch (registry::blockSupport::pushReaction(*stateId)) {
        case registry::PushReaction::Block:
            return false;
        case registry::PushReaction::Destroy:
            return allowDestroyable;
        case registry::PushReaction::PushOnly:
            return pushing == connection;
        default:
            break;
        }
    } else if (propertyOf(*stateId, "extended") == "true") {
        // An extended piston is holding its own head: it is not going anywhere.
        return false;
    }
    // parity: the last line of `isPushable`. A chest is `NORMAL` and still will not move, because
    // what it holds lives in a block entity and a piston does not carry those.
    return !registry::blockConnections::hasBlockEntity(*stateId);
}

std::optional<Push> resolve(const Context& context, std::int32_t pistonX, std::int32_t pistonY, std::int32_t pistonZ,
                            registry::BlockFace facing, bool extending) {
    // parity: pulling back pushes the other way, and starts two blocks out, past the arm.
    const BlockFace pushing = extending ? facing : registry::opposite(facing);
    const Position piston{pistonX, pistonY, pistonZ};
    const Position start = extending ? step(piston, facing) : step(piston, facing, 2);
    Resolver resolver{context, piston, pushing};
    const std::optional<std::int32_t> startState = context.world.blockState(start[0], start[1], start[2]);
    if (!startState.has_value()) {
        return std::nullopt;
    }
    if (!isPushable(context, start[0], start[1], start[2], pushing, false, facing)) {
        if (extending && registry::blockSupport::pushReaction(*startState) == registry::PushReaction::Destroy) {
            return Push{.moved = {}, .destroyed = {start}};
        }
        return std::nullopt;
    }
    if (!resolver.addLine(start, pushing)) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < resolver.toPush.size(); ++index) {
        const Position position = resolver.toPush[index];
        const std::optional<std::int32_t> stateId = resolver.stateAt(position);
        if (stateId.has_value() && isSticky(*stateId) && !resolver.addBranches(position)) {
            return std::nullopt;
        }
    }
    return Push{.moved = resolver.toPush, .destroyed = resolver.destroyed};
}

std::vector<BlockChange> apply(const Context& context, const Push& push, registry::BlockFace pushing) {
    std::vector<BlockChange> changes;
    for (const Position& position : push.destroyed) {
        if (!context.world.setBlockState(position[0], position[1], position[2], air())) {
            continue;
        }
        changes.push_back(BlockChange{.x = position[0], .y = position[1], .z = position[2], .stateId = air()});
    }
    for (auto position = push.moved.rbegin(); position != push.moved.rend(); ++position) {
        const std::optional<std::int32_t> stateId =
            context.world.blockState((*position)[0], (*position)[1], (*position)[2]);
        if (!stateId.has_value()) {
            continue;
        }
        const Position to = step(*position, pushing);
        if (context.world.setBlockState(to[0], to[1], to[2], *stateId)) {
            changes.push_back(BlockChange{.x = to[0], .y = to[1], .z = to[2], .stateId = *stateId});
        }
        if (context.world.setBlockState((*position)[0], (*position)[1], (*position)[2], air())) {
            changes.push_back(
                BlockChange{.x = (*position)[0], .y = (*position)[1], .z = (*position)[2], .stateId = air()});
        }
    }
    return changes;
}

void trigger(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
             std::vector<BlockChange>& effects) {
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (!isPiston(path)) {
        return;
    }
    const std::optional<BlockFace> direction = faceNamed(propertyOf(*stateId, "facing"));
    if (!direction.has_value()) {
        return;
    }
    const bool extended = propertyOf(*stateId, "extended") == "true";
    const bool powered = signalToPiston(context, x, y, z, *direction);
    if (powered == extended) {
        return;
    }
    if (powered) {
        const std::optional<Push> push = resolve(context, x, y, z, *direction, true);
        if (!push.has_value()) {
            return;
        }
        beginMove(context, *push, *direction, path == "sticky_piston");
    }
    // parity: `PistonBaseBlock.checkIfExtend`: 0 to push out, 1 to pull back, and the face it moves
    // along as the second byte.
    // parity: `PistonBaseBlock.triggerEvent`, whose sound goes with the event, at half volume and a
    // pitch rolled between 0.6 and 0.85 going out, 0.6 and 0.75 coming back.
    const blockEffect::Sound sound{.name = std::string{powered ? kExtendSound : kContractSound},
                                   .source = wire::protocol::play::SoundSource::Blocks,
                                   .volume = kSoundVolume,
                                   .pitch = pitch(context.random, powered ? kExtendPitchSpread : kContractPitchSpread)};
    effects.push_back(BlockChange{.x = x,
                                  .y = y,
                                  .z = z,
                                  .stateId = *stateId,
                                  .effects = {blockEffect::Event{.b0 = powered ? kExtend : kContract,
                                                                 .b1 = static_cast<std::int32_t>(*direction)},
                                              sound}});
    context.world.scheduleBlockTick(x, y, z, block(), kMoveTicks);
}

std::vector<BlockChange> finish(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::vector<PlayerPosition>& players,
                                const std::function<void(const PlayerPosition&)>& onPlayerPushed) {
    const std::optional<std::int32_t> stateId = context.world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::string path = pathOf(*stateId);
    if (!isPiston(path)) {
        return {};
    }
    const std::optional<BlockFace> direction = faceNamed(propertyOf(*stateId, "facing"));
    if (!direction.has_value()) {
        return {};
    }
    const bool extended = propertyOf(*stateId, "extended") == "true";
    const bool powered = signalToPiston(context, x, y, z, *direction);
    if (powered == extended) {
        return {};
    }
    if (powered) {
        return extend(context, x, y, z, *stateId, *direction, path, players, onPlayerPushed);
    }
    return retract(context, x, y, z, *stateId, *direction, path);
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
