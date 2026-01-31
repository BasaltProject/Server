#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::commandBlocks {
namespace {

using registry::BlockFace;

constexpr std::string_view kCommandKey = "Command";
constexpr std::string_view kSuccessKey = "SuccessCount";
constexpr std::string_view kPoweredKey = "powered";
constexpr std::string_view kConditionKey = "conditionMet";
constexpr std::string_view kAutoKey = "auto";
constexpr std::string_view kUpdateLastKey = "UpdateLastExecution";
constexpr std::string_view kLastExecutionKey = "LastExecution";

/// parity: `CommandBlock.tick` books its turn one tick out, both for the first one and for the
/// repeating block's own next.
constexpr std::int32_t kDelay = 1;

[[nodiscard]] const wire::common::Identifier& impulseBlock() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("command_block");
    return instance;
}

[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::string valueOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::optional<BlockFace> faceNamed(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, BlockFace>, 6> kNames{{{"down", BlockFace::Down},
                                                                                   {"up", BlockFace::Up},
                                                                                   {"north", BlockFace::North},
                                                                                   {"south", BlockFace::South},
                                                                                   {"west", BlockFace::West},
                                                                                   {"east", BlockFace::East}}};
    const auto found = std::ranges::find_if(kNames, [name](const auto& one) { return one.first == name; });
    return found == kNames.end() ? std::nullopt : std::optional{found->second};
}

/// parity: `CommandBlockEntity.markConditionMet`. It is always true unless the block is conditional,
/// and then it is "the square **behind** me holds a command block whose `SuccessCount` is not zero".
/// Behind is the opposite of the block's own facing.
[[nodiscard]] bool markConditionMet(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stateId) {
    bool met = true;
    if (valueOf(stateId, "conditional") == "true") {
        met = false;
        const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing"));
        if (facing.has_value()) {
            const BlockFace behind = registry::opposite(*facing);
            const std::int32_t bx = x + registry::stepX(behind);
            const std::int32_t by = y + registry::stepY(behind);
            const std::int32_t bz = z + registry::stepZ(behind);
            const std::optional<std::int32_t> neighbour = world.blockState(bx, by, bz);
            if (neighbour.has_value() && modeOf(pathOf(*neighbour)).has_value()) {
                const server::world::ChunkBlockEntity* held = entityAt(world, bx, by, bz);
                met = held != nullptr && held->data().getInt(std::string{kSuccessKey}, 0) > 0;
            }
        }
    }
    if (server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z); entity != nullptr) {
        entity->data().putBoolean(std::string{kConditionKey}, met);
        world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    }
    return met;
}

/// parity: `BaseCommandBlock.performCommand`. The guard at the top is what stops a chain running
/// twice on one tick, and it is the only thing that does.
[[nodiscard]] bool performCommand(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::int64_t gameTime, bool commandBlocksWork, const RunCommand& run) {
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return false;
    }
    if (entity->data().getLong(std::string{kLastExecutionKey}, -1) == gameTime) {
        return false;
    }
    entity->data().putInt(std::string{kSuccessKey}, 0);
    const std::string command{entity->data().getString(std::string{kCommandKey})};
    if (commandBlocksWork && !command.empty() && run && run(x, y, z, command)) {
        entity->data().putInt(std::string{kSuccessKey}, 1);
    }
    // parity: `updateLastExecution` off leaves the mark at -1, which is what lets the same block run
    // again inside one tick. It reads **true** when absent: `BaseCommandBlock.load` is
    // `getBooleanOr("UpdateLastExecution", true)` and the field starts true, so a block placed by
    // command and never told otherwise marks its turn. A -1 is not written at all, which is what
    // `saveAdditional` does with it.
    if (entity->data().getBoolean(std::string{kUpdateLastKey}, true)) {
        entity->data().putLong(std::string{kLastExecutionKey}, gameTime);
    } else {
        static_cast<void>(entity->data().remove(std::string{kLastExecutionKey}));
    }
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    return true;
}

/// parity: `CommandBlock.executeChain`, which walks on from the block that just ran. The direction of
/// each step is the **facing of the block it is standing on**, not the one it started with, so a chain
/// turns corners.
void executeChain(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace direction,
                  std::int64_t gameTime, bool commandBlocksWork, std::int32_t maxChainLength, const RunCommand& run) {
    std::int32_t left = maxChainLength;
    std::int32_t cx = x;
    std::int32_t cy = y;
    std::int32_t cz = z;
    while (left-- > 0) {
        cx += registry::stepX(direction);
        cy += registry::stepY(direction);
        cz += registry::stepZ(direction);
        const std::optional<std::int32_t> stateId = world.blockState(cx, cy, cz);
        if (!stateId.has_value() || modeOf(pathOf(*stateId)) != Mode::Chain) {
            return;
        }
        const server::world::ChunkBlockEntity* entity = entityAt(world, cx, cy, cz);
        if (entity == nullptr) {
            return;
        }
        const bool awake =
            entity->data().getBoolean(std::string{kPoweredKey}) || entity->data().getBoolean(std::string{kAutoKey});
        if (awake) {
            if (markConditionMet(world, cx, cy, cz, *stateId)) {
                if (!performCommand(world, cx, cy, cz, gameTime, commandBlocksWork, run)) {
                    return;
                }
            } else if (valueOf(*stateId, "conditional") == "true") {
                if (server::world::ChunkBlockEntity* held = entityAt(world, cx, cy, cz); held != nullptr) {
                    held->data().putInt(std::string{kSuccessKey}, 0);
                    world.chunkIfLoaded(cx >> 4, cz >> 4)->markDirty();
                }
            }
        }
        const std::optional<BlockFace> next = faceNamed(valueOf(*stateId, "facing"));
        if (!next.has_value()) {
            return;
        }
        direction = *next;
    }
}

/// parity: `CommandBlock.execute`, which runs the command when there is one, zeroes the count when
/// there is not, and carries on down the chain either way.
void execute(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             std::int64_t gameTime, bool commandBlocksWork, std::int32_t maxChainLength, const RunCommand& run) {
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return;
    }
    if (entity->data().getString(std::string{kCommandKey}).empty()) {
        entity->data().putInt(std::string{kSuccessKey}, 0);
        world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    } else {
        static_cast<void>(performCommand(world, x, y, z, gameTime, commandBlocksWork, run));
    }
    if (const std::optional<BlockFace> facing = faceNamed(valueOf(stateId, "facing")); facing.has_value()) {
        executeChain(world, x, y, z, *facing, gameTime, commandBlocksWork, maxChainLength, run);
    }
}

} // namespace

std::optional<Mode> modeOf(const std::string_view path) {
    if (path == "command_block") {
        return Mode::Impulse;
    }
    if (path == "chain_command_block") {
        return Mode::Chain;
    }
    if (path == "repeating_command_block") {
        return Mode::Repeating;
    }
    return std::nullopt;
}

void onSignal(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z, const bool powered) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<Mode> mode = modeOf(pathOf(*stateId));
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (!mode.has_value() || entity == nullptr) {
        return;
    }
    if (entity->data().getBoolean(std::string{kPoweredKey}) == powered) {
        return;
    }
    entity->data().putBoolean(std::string{kPoweredKey}, powered);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    if (!powered) {
        return;
    }
    // parity: a repeating block runs off its own booked turn and a chain block off whoever points at
    // it, so neither starts from the signal itself.
    if (entity->data().getBoolean(std::string{kAutoKey}) || *mode == Mode::Chain) {
        return;
    }
    static_cast<void>(markConditionMet(world, x, y, z, *stateId));
    world.scheduleBlockTick(x, y, z, impulseBlock(), kDelay);
}

bool tick(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
          const std::int64_t gameTime, const bool commandBlocksWork, const std::int32_t maxChainLength,
          const RunCommand& run) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const std::optional<Mode> mode = modeOf(pathOf(*stateId));
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (!mode.has_value() || entity == nullptr) {
        return false;
    }
    const bool conditional = valueOf(*stateId, "conditional") == "true";
    // parity: the turn acts on the condition the **last** pass left behind, and a repeating block
    // marks the new one before it looks.
    const bool wasMet = entity->data().getBoolean(std::string{kConditionKey});
    if (*mode == Mode::Repeating) {
        static_cast<void>(markConditionMet(world, x, y, z, *stateId));
        if (wasMet) {
            execute(world, x, y, z, *stateId, gameTime, commandBlocksWork, maxChainLength, run);
        } else if (conditional) {
            entity->data().putInt(std::string{kSuccessKey}, 0);
            world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
        }
        const bool awake =
            entity->data().getBoolean(std::string{kPoweredKey}) || entity->data().getBoolean(std::string{kAutoKey});
        if (awake) {
            world.scheduleBlockTick(x, y, z, impulseBlock(), kDelay);
        }
    } else if (*mode == Mode::Impulse) {
        if (wasMet) {
            execute(world, x, y, z, *stateId, gameTime, commandBlocksWork, maxChainLength, run);
        } else if (conditional) {
            entity->data().putInt(std::string{kSuccessKey}, 0);
            world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
        }
    }
    // parity: `CommandBlock.tick` ends in `updateNeighbourForOutputSignal` **whatever** it did, so a
    // turn that ran nothing still tells the comparators. That is what carries a conditional block's
    // `SuccessCount` back down to zero when its condition fails.
    return true;
}

void onDataLoaded(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const bool wasAutomatic) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::optional<Mode> mode = modeOf(pathOf(*stateId));
    const server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (!mode.has_value() || entity == nullptr || *mode == Mode::Chain) {
        return;
    }
    if (wasAutomatic || !entity->data().getBoolean(std::string{kAutoKey}) ||
        entity->data().getBoolean(std::string{kPoweredKey})) {
        return;
    }
    world.scheduleBlockTick(x, y, z, impulseBlock(), kDelay);
}

wire::nbt::NbtCompound defaults() {
    wire::nbt::NbtCompound data;
    data.putString(std::string{kCommandKey}, "");
    data.putInt(std::string{kSuccessKey}, 0);
    data.putBoolean("TrackOutput", true);
    data.putBoolean(std::string{kUpdateLastKey}, true);
    data.putBoolean(std::string{kPoweredKey}, false);
    data.putBoolean(std::string{kConditionKey}, false);
    data.putBoolean(std::string{kAutoKey}, false);
    return data;
}

std::optional<std::int32_t> comparatorSignal(const wire::nbt::NbtCompound& data) {
    return data.getInt(std::string{kSuccessKey}, 0);
}

} // namespace basalt::server::runtime::world::commandBlocks
