#include "PistonSlide.hpp"

#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
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
            // parity: `addBlockLine` counts the blocks it adds **ahead** in `blocksAdded` too, not
            // only the ones a sticky block dragged behind it. `reorderListAtCollision` and the loop
            // that looks for branches after a collision both measure with that number, so leaving it
            // short reorders the wrong slice.
            ++blocksAdded;
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
} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("piston");
    return instance;
}

bool isPushable(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace pushing,
                bool allowDestroyable, registry::BlockFace connection) {
    // parity: `isPushable` starts with the world and not with the block: out of height or out of the
    // border and nothing moves.
    if (y < context.minY || y > context.maxY || (context.withinBorder && !context.withinBorder(x, z))) {
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

void trigger(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z) {
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
    // parity: `PistonBaseBlock.checkIfExtend`: 0 to push out, 1 to pull back, and the face it moves
    // along as the second byte. Nothing moves here; the event is what moves it.
    std::int32_t event = kContract;
    if (powered) {
        if (!resolve(context, x, y, z, *direction, true).has_value()) {
            return;
        }
        event = kExtend;
    } else if (isRetractingOntoSlide(context, x, y, z, *direction)) {
        // parity: a head that is still sliding out is taken back whole, without asking what is in
        // front of it.
        event = kContractSliding;
    }
    context.world.queueBlockEvent(BlockEvent{
        .x = x, .y = y, .z = z, .block = path, .first = event, .second = static_cast<std::int32_t>(*direction)});
}

void runEvent(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t b0, std::int32_t b1,
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
    // parity: `triggerEvent` asks the signal again, because a pulse can be over by the time the
    // event runs. A piston told to pull back that is powered again only takes `extended` and stays.
    const bool powered = signalToPiston(context, x, y, z, *direction);
    if (powered && b0 != kExtend) {
        if (const std::optional<std::int32_t> body = registry::blockStateDefinitions::stateIdOf(
                wire::common::Identifier::vanilla(path), {{"extended", "true"}}, *stateId);
            body.has_value()) {
            static_cast<void>(context.world.setBlockState(x, y, z, *body));
        }
        return;
    }
    if (!powered && b0 == kExtend) {
        return;
    }
    if (b0 == kExtend) {
        const std::optional<Push> push = resolve(context, x, y, z, *direction, true);
        if (!push.has_value()) {
            return;
        }
        beginMove(context, *push, *direction, path == "sticky_piston", x, y, z, path, effects);
    } else {
        beginRetract(context, x, y, z, *stateId, *direction, path == "sticky_piston", path, b0 == kContractSliding);
    }
    // parity: `PistonBaseBlock.triggerEvent`, whose sound goes with the event, at half volume and a
    // pitch rolled between 0.6 and 0.85 going out, 0.6 and 0.75 coming back.
    const bool extending = b0 == kExtend;
    const blockEffect::Sound sound{.name = std::string{extending ? kExtendSound : kContractSound},
                                   .source = wire::protocol::play::SoundSource::Blocks,
                                   .volume = kSoundVolume,
                                   .pitch =
                                       pitch(context.random, extending ? kExtendPitchSpread : kContractPitchSpread)};
    // parity: `triggerEvent` raises `BLOCK_ACTIVATE` going out and `BLOCK_DEACTIVATE` coming back,
    // both from the piston's own square and both after whatever it broke on the way.
    vibrations::emit(context.world, extending ? "block_activate" : "block_deactivate", static_cast<double>(x) + 0.5,
                     static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, context.gameTime);
    // parity: every square that has just started sliding sits out this turn's ticker, which is the
    // one that only moves `progress` and leaves `progressO` where it was.
    for (const Carried& sliding : carriedBlocks(context, x, y, z, *direction)) {
        context.world.noteSlideStarted(sliding.at[0], sliding.at[1], sliding.at[2]);
    }
    const std::optional<std::int32_t> now = context.world.blockState(x, y, z);
    effects.push_back(BlockChange{.x = x,
                                  .y = y,
                                  .z = z,
                                  .stateId = now.value_or(*stateId),
                                  .effects = {blockEffect::Event{.b0 = b0, .b1 = b1}, sound}});
}

} // namespace basalt::server::runtime::world::pistons
