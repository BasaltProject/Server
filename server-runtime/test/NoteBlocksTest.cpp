#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// The sound in a note block's effects, or empty when it played none.
[[nodiscard]] std::string soundOf(const std::vector<BlockEffect>& effects) {
    for (const BlockEffect& effect : effects) {
        if (const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effect); sound != nullptr) {
            return sound->name;
        }
    }
    return {};
}

[[nodiscard]] float pitchOf(const std::vector<BlockEffect>& effects) {
    for (const BlockEffect& effect : effects) {
        if (const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effect); sound != nullptr) {
            return sound->pitch;
        }
    }
    return 0.0F;
}

} // namespace

TEST_CASE("the instrument comes from what is under the note block") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));

    // Nothing under it: the harp.
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "harp");

    // parity: the block underneath decides, and each of these is its own instrument.
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("sand")));
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "snare");
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("gold_block")));
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "bell");
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "basedrum");
}

TEST_CASE("a head on top wins over whatever is underneath") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("gold_block")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("zombie_head")));

    // parity: `setInstrument` asks the block above first, and only a head answers from up there.
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "zombie");

    // A head *under* it is not its instrument: that falls back to the harp.
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::air().stateId));
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("zombie_head")));
    CHECK(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "harp");
}

TEST_CASE("a note block retunes when what is under it changes") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));
    CHECK(propertyOf(*world.blockState(8, 64, 8), "instrument") == "harp");

    REQUIRE(world.setBlockState(8, 63, 8, stateOf("gold_block")));
    const std::optional<std::int32_t> tuned = noteBlocks::retune(world, 8, 64, 8, *world.blockState(8, 64, 8));
    REQUIRE(tuned.has_value());
    CHECK(propertyOf(*tuned, "instrument") == "bell");
    CHECK(world.blockState(8, 64, 8) == *tuned);

    // Asked again with nothing changed it says nothing, and a block that is not a note block is
    // left alone.
    CHECK_FALSE(noteBlocks::retune(world, 8, 64, 8, *world.blockState(8, 64, 8)).has_value());
    CHECK_FALSE(noteBlocks::retune(world, 8, 63, 8, *world.blockState(8, 63, 8)).has_value());
}

TEST_CASE("a note block with something on top stays quiet") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));
    CHECK_FALSE(noteBlocks::strike(world, 8, 64, 8).empty());

    // parity: `playNote` needs air above it, so a block on top silences it entirely.
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    CHECK(noteBlocks::strike(world, 8, 64, 8).empty());

    // Unless the instrument is one that lives up there: a head is the block on top.
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("zombie_head")));
    const std::optional<std::int32_t> tuned = noteBlocks::retune(world, 8, 64, 8, *world.blockState(8, 64, 8));
    REQUIRE(tuned.has_value());
    CHECK_FALSE(noteBlocks::strike(world, 8, 64, 8).empty());
    CHECK(soundOf(noteBlocks::strike(world, 8, 64, 8)) == "block.note_block.imitate.zombie");
}

TEST_CASE("the note decides the pitch, and only for a tunable instrument") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));

    const float low = pitchOf(noteBlocks::strike(world, 8, 64, 8));
    // parity: note zero is the bottom of the two octaves, an octave below the middle.
    CHECK(low == doctest::Approx(0.5F));

    const std::optional<std::int32_t> higher = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("note_block"), {{"note", "12"}}, *world.blockState(8, 64, 8));
    REQUIRE(higher.has_value());
    REQUIRE(world.setBlockState(8, 64, 8, *higher));
    CHECK(pitchOf(noteBlocks::strike(world, 8, 64, 8)) == doctest::Approx(1.0F));

    // A head is not tunable, so its note counts for nothing: back at note zero, where a harp
    // would play at half pitch, a head still plays at one.
    const std::optional<std::int32_t> lowAgain = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("note_block"), {{"note", "0"}}, *world.blockState(8, 64, 8));
    REQUIRE(lowAgain.has_value());
    REQUIRE(world.setBlockState(8, 64, 8, *lowAgain));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("zombie_head")));
    static_cast<void>(noteBlocks::retune(world, 8, 64, 8, *world.blockState(8, 64, 8)));
    CHECK(pitchOf(noteBlocks::strike(world, 8, 64, 8)) == doctest::Approx(1.0F));
}

TEST_CASE("every note carries the swing, and a player head carries nothing else") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));

    // parity: `triggerEvent` swings the block for the client whatever it is playing.
    const std::vector<BlockEffect> played = noteBlocks::strike(world, 8, 64, 8);
    REQUIRE_FALSE(played.empty());
    const blockEffect::Event* swing = std::get_if<blockEffect::Event>(&played[0]);
    REQUIRE(swing != nullptr);
    CHECK(swing->b0 == 0);
    CHECK(swing->b1 == 0);

    // parity: a player head has no sound of its own that this build can look up, so the swing goes
    // out and the sound does not.
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("player_head")));
    static_cast<void>(noteBlocks::retune(world, 8, 64, 8, *world.blockState(8, 64, 8)));
    REQUIRE(noteBlocks::instrumentAt(world, 8, 64, 8).serializedName() == "custom_head");
    const std::vector<BlockEffect> head = noteBlocks::strike(world, 8, 64, 8);
    CHECK(head.size() == 1);
    CHECK(std::get_if<blockEffect::Event>(&head[0]) != nullptr);
}

TEST_CASE("tuning a note block moves the note on one and wraps at the top") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));

    const std::optional<BlockChange> first = noteBlocks::tune(world, 8, 64, 8);
    REQUIRE(first.has_value());
    CHECK(propertyOf(first->stateId, "note") == "1");
    // It plays as it is tuned, which is what a player hears while setting one.
    CHECK_FALSE(first->effects.empty());

    for (std::int32_t at = 1; at < noteBlocks::kNotes; ++at) {
        static_cast<void>(noteBlocks::tune(world, 8, 64, 8));
    }
    // Twenty-five notes, so it comes back round to zero.
    CHECK(propertyOf(*world.blockState(8, 64, 8), "note") == "0");

    // What is not a note block is not tuned.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    CHECK_FALSE(noteBlocks::tune(world, 9, 64, 8).has_value());
    CHECK(noteBlocks::strike(world, 9, 64, 8).empty());
}
