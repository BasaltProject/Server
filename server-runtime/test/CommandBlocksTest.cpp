#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::server::world::ChunkBlockEntity;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] std::int32_t blockOf(const std::string& path, const std::string& facing = "east",
                                   const std::string& conditional = "false") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla(path), {{"facing", facing}, {"conditional", conditional}}, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();
    /// Every command the run callback was handed, so a test can pin what really ran.
    std::vector<std::string> ran;

    Loaded() { state.getOrGenerateChunk(0, 0); }

    /// Puts a command block down with its block entity, as `/setblock` would.
    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, const std::string& command,
             bool automatic = false) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
        Chunk* chunk = state.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        nbt::NbtCompound data;
        data.putString("Command", command);
        data.putBoolean("auto", automatic);
        chunk->setBlockEntity(ChunkBlockEntity{x & 15, y, z & 15, Identifier::vanilla("command_block"), data});
    }

    [[nodiscard]] nbt::NbtCompound& dataAt(std::int32_t x, std::int32_t y, std::int32_t z) {
        Chunk* chunk = state.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
        REQUIRE(entity != nullptr);
        return entity->data();
    }

    /// `successes` is what the command answered: one for an ordinary command, more for a fork.
    [[nodiscard]] commandBlocks::RunCommand runner(std::int32_t successes = 1) {
        return [this, successes](std::int32_t, std::int32_t, std::int32_t, const std::string& command) {
            ran.push_back(command);
            return successes;
        };
    }
};

} // namespace

TEST_CASE("a signal books a turn for an impulse block, and only on the way up") {
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "say hello");

    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(loaded.dataAt(8, 64, 8).getBoolean("powered"));
    REQUIRE(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));
    // parity: `setPoweredAndUpdate` does nothing at all when the signal has not moved.
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);
    static_cast<void>(loaded.state.dueBlockTicks());
    CHECK_FALSE(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));

    // And going away books nothing.
    commandBlocks::onSignal(loaded.state, 8, 64, 8, false);
    CHECK_FALSE(loaded.dataAt(8, 64, 8).getBoolean("powered"));
    CHECK_FALSE(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));
}

TEST_CASE("neither a chain nor a repeating block starts itself from a signal") {
    // parity: `setPoweredAndUpdate` returns early for both: the chain waits for whoever points at it
    // and the repeating one runs off its own booked turn.
    for (const std::string& path : {std::string{"chain_command_block"}, std::string{"repeating_command_block"}}) {
        Loaded loaded;
        loaded.put(8, 64, 8, blockOf(path), "say hello", path == "repeating_command_block");

        commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

        CHECK(loaded.dataAt(8, 64, 8).getBoolean("powered"));
        CHECK_FALSE(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));
    }
}

TEST_CASE("the turn runs the command and counts the success") {
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "say hello");
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner()));

    REQUIRE(loaded.ran.size() == 1);
    CHECK(loaded.ran.front() == "say hello");
    CHECK(loaded.dataAt(8, 64, 8).getInt("SuccessCount") == 1);
    CHECK(commandBlocks::comparatorSignal(loaded.dataAt(8, 64, 8)) == 1);
}

TEST_CASE("the same tick runs a block once, which is what stops a chain looping") {
    // parity: `performCommand` answers false when `gameTime == lastExecution`, and that guard is the
    // only thing holding a ring of chain blocks back.
    // parity: `UpdateLastExecution` reads true when it is absent, so a block put down by command and
    // never told otherwise already marks its turn. Nothing sets it here on purpose.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "say hello");
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);
    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 7, true, 8, loaded.runner()));
    CHECK(loaded.ran.size() == 1);

    commandBlocks::onSignal(loaded.state, 8, 64, 8, false);
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);
    static_cast<void>(commandBlocks::tick(loaded.state, 8, 64, 8, 7, true, 8, loaded.runner()));

    CHECK(loaded.ran.size() == 1);
}

TEST_CASE("a chain runs on from the block that ran, and turns the corner it points at") {
    // parity: `executeChain` takes the facing of the block it is standing on for the next step, not
    // the one it started with.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block", "east"), "first");
    loaded.put(9, 64, 8, blockOf("chain_command_block", "south"), "second");
    loaded.put(9, 64, 9, blockOf("chain_command_block", "south"), "third");
    for (const std::array<std::int32_t, 3>& at : {std::array{9, 64, 8}, std::array{9, 64, 9}}) {
        loaded.dataAt(at[0], at[1], at[2]).putBoolean("auto", true);
    }
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    static_cast<void>(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner()));

    REQUIRE(loaded.ran.size() == 3);
    CHECK(loaded.ran[0] == "first");
    CHECK(loaded.ran[1] == "second");
    CHECK(loaded.ran[2] == "third");
}

TEST_CASE("a conditional block reads the one behind it") {
    // parity: `markConditionMet` looks at the opposite of its own facing, and asks for a command block
    // there whose `SuccessCount` is not zero.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block", "east"), "first");
    loaded.put(9, 64, 8, blockOf("chain_command_block", "east", "true"), "second");
    loaded.dataAt(9, 64, 8).putBoolean("auto", true);
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    static_cast<void>(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner(0)));

    // The first one ran and failed, so the conditional one behind it stays put.
    REQUIRE(loaded.ran.size() == 1);
    CHECK(loaded.ran.front() == "first");
    CHECK(loaded.dataAt(9, 64, 8).getInt("SuccessCount") == 0);
}

TEST_CASE("a repeating block books its own next turn while it has a signal") {
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("repeating_command_block"), "say hello");
    loaded.dataAt(8, 64, 8).putBoolean("powered", true);
    loaded.dataAt(8, 64, 8).putBoolean("conditionMet", true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner()));

    CHECK(loaded.ran.size() == 1);
    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));
}

TEST_CASE("a command block is put down with every field its save writes") {
    // parity: `BaseCommandBlock.save` and `CommandBlockEntity.saveAdditional` write seven fields
    // whether or not anybody set them, so an empty block entity is a different block entity: a world
    // of ours would open in vanilla with no `UpdateLastExecution`, which is what decides whether the
    // block can run twice in one tick. `LastExecution` is the one left out, and only while it is -1.
    const nbt::NbtCompound data = commandBlocks::defaults();

    CHECK(data.getString("Command").empty());
    CHECK(data.getInt("SuccessCount") == 0);
    CHECK(data.getBoolean("TrackOutput"));
    CHECK(data.getBoolean("UpdateLastExecution"));
    CHECK_FALSE(data.getBoolean("powered"));
    CHECK_FALSE(data.getBoolean("conditionMet"));
    CHECK_FALSE(data.getBoolean("auto"));
    CHECK(data.find("LastExecution") == nullptr);
}

TEST_CASE("turning auto on is what starts a repeating block nobody has powered") {
    // parity: `CommandBlockEntity.setAutomatic` books a turn on the way from off to on, and only for a
    // block that is not powered and is not a chain. `loadAdditional` runs it, so writing the data is
    // the moment it happens.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("repeating_command_block"), "say hello");
    loaded.dataAt(8, 64, 8).putBoolean("auto", true);

    commandBlocks::onDataLoaded(loaded.state, 8, 64, 8, false);

    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));

    // Already on before the write books nothing, and neither does a chain block.
    Loaded again;
    again.put(8, 64, 8, blockOf("repeating_command_block"), "say hello", true);
    commandBlocks::onDataLoaded(again.state, 8, 64, 8, true);
    CHECK_FALSE(again.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));

    Loaded chained;
    chained.put(8, 64, 8, blockOf("chain_command_block"), "say hello", true);
    commandBlocks::onDataLoaded(chained.state, 8, 64, 8, false);
    CHECK_FALSE(chained.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("command_block")));
}

TEST_CASE("the game rule stops the command without stopping the cycle") {
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "say hello");
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 1, false, 8, loaded.runner()));

    CHECK(loaded.ran.empty());
    CHECK(loaded.dataAt(8, 64, 8).getInt("SuccessCount") == 0);
}

TEST_CASE("a turn that ran nothing still counts as a turn, so the comparator hears it") {
    // parity: `CommandBlock.tick` ends in `updateNeighbourForOutputSignal` whatever it did. A
    // conditional block whose condition failed puts `SuccessCount` back to zero, and that has to reach
    // the comparator as much as a one would.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block", "east", "true"), "say hello");
    loaded.dataAt(8, 64, 8).putInt("SuccessCount", 1);
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner()));

    CHECK(loaded.ran.empty());
    CHECK(loaded.dataAt(8, 64, 8).getInt("SuccessCount") == 0);
}

TEST_CASE("the one command that runs without running") {
    // parity: `BaseCommandBlock.performCommand` answers `Searge` before it looks at the rule or at
    // the command itself: one success, nothing run. A comparator beside such a block reads one.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "sEaRgE");
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 7, false, 8, loaded.runner()));
    CHECK(loaded.ran.empty());
    CHECK(loaded.dataAt(8, 64, 8).getInt("SuccessCount") == 1);
    CHECK(loaded.dataAt(8, 64, 8).getLong("LastExecution") == 7);
}

TEST_CASE("a command that forks counts every branch that worked") {
    // parity: `BaseCommandBlock.performCommand` counts brigadier's callbacks, so an `/execute as @e`
    // over three entities leaves three and the comparator beside the block reads three. The count is
    // not the command's own result: it is how many times it succeeded.
    Loaded loaded;
    loaded.put(8, 64, 8, blockOf("command_block"), "execute as @e run say hi");
    commandBlocks::onSignal(loaded.state, 8, 64, 8, true);

    CHECK(commandBlocks::tick(loaded.state, 8, 64, 8, 1, true, 8, loaded.runner(3)));
    CHECK(loaded.dataAt(8, 64, 8).getInt("SuccessCount") == 3);
    CHECK(commandBlocks::comparatorSignal(loaded.dataAt(8, 64, 8)) == 3);
}
