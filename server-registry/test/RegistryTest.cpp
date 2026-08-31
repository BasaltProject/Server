#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>

#include <doctest.h>

using namespace basalt::server::registry;
using basalt::wire::common::Identifier;

TEST_CASE("a state id and its properties convert into each other") {
    // The arithmetic replaces a hand-written table of every state worldgen writes: that table and
    // this agreed on all 4977 of its entries, which is why it is not carried across.
    for (const auto& [stateId, path] : defaultBlockStates()) {
        const Identifier block = Identifier::vanilla(std::string{path});
        const blockStateDefinitions::Definition* definition = blockStateDefinitions::of(block);
        if (definition == nullptr) {
            continue;
        }
        for (std::int32_t offset = 0; offset < definition->stateCount(); ++offset) {
            const std::int32_t id = definition->firstStateId() + offset;
            const std::map<std::string, std::string> properties = blockStateDefinitions::propertiesOf(id);
            REQUIRE_FALSE(properties.empty());
            const std::optional<std::int32_t> back = blockStateDefinitions::stateIdOf(block, properties);
            REQUIRE(back.has_value());
            CHECK(*back == id);
        }
    }
}

TEST_CASE("every state id belongs to exactly the block whose range covers it") {
    for (const auto& [stateId, path] : defaultBlockStates()) {
        const Identifier* owner = blockStateDefinitions::blockOf(stateId);
        REQUIRE(owner != nullptr);
        CHECK(owner->path() == path);
    }
}

TEST_CASE("a block id is not a block state id") {
    // Two different numbers for every block past the first, and sending one where the other belongs
    // is silently wrong rather than an error.
    const Identifier stone = Identifier::vanilla("stone");
    CHECK(blocks::blockIdOf(stone) == 1);
    CHECK(blocks::stone().stateId == 1);

    const Identifier bedrock = Identifier::vanilla("bedrock");
    const std::optional<std::int32_t> blockId = blocks::blockIdOf(bedrock);
    REQUIRE(blockId.has_value());
    CHECK(*blockId != blocks::bedrock().stateId);
}

TEST_CASE("an unknown property or value is refused rather than ignored") {
    const Identifier grass = Identifier::vanilla("grass_block");
    CHECK(blockStateDefinitions::stateIdOf(grass, {{"snowy", "true"}}).has_value());
    // Silently placing a block the wrong way round is worse than not placing it.
    CHECK_FALSE(blockStateDefinitions::stateIdOf(grass, {{"snowy", "perhaps"}}).has_value());
    CHECK_FALSE(blockStateDefinitions::stateIdOf(grass, {{"facing", "north"}}).has_value());
}

TEST_CASE("bedrock is unbreakable and a flower comes apart on the spot") {
    const BlockHardness* rock = blocks::hardnessOf(Identifier::vanilla("bedrock"));
    REQUIRE(rock != nullptr);
    CHECK(rock->isUnbreakable());

    const BlockHardness* flower = blocks::hardnessOf(Identifier::vanilla("dandelion"));
    REQUIRE(flower != nullptr);
    CHECK(flower->isInstant());
}
