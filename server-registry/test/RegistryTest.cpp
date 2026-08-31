#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <doctest.h>

#include <set>
#include <string>

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

TEST_CASE("the shape index names a real shape for every block state") {
    // Two characters per state id, in id order: a table of one entry per state would be thirty
    // thousand lines of source for what fits in a few hundred here.
    // Two base64 digits per state, most significant first.
    constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const std::string_view index = blockShapeIndex();
    REQUIRE(index.size() % 2 == 0);
    CHECK(static_cast<std::int32_t>(index.size() / 2) >= blocks::stateCount());
    for (std::size_t at = 0; at + 1 < index.size(); at += 2) {
        const std::size_t high = kAlphabet.find(index[at]);
        const std::size_t low = kAlphabet.find(index[at + 1]);
        CAPTURE(at / 2);
        REQUIRE(high != std::string_view::npos);
        REQUIRE(low != std::string_view::npos);
        REQUIRE(high * kAlphabet.size() + low < blockShapes().size());
    }
}

TEST_CASE("a table row is one row, however it was written in the source") {
    // The long rows are written as several joined pieces. Reading them as separate rows would turn
    // one bitset into a dozen fragments, which is exactly the bug this catches.
    for (const std::string_view set : supportSets()) {
        CHECK(set.size() > 2);
    }
    for (const auto& [name, states] : blockFamilies()) {
        CHECK_FALSE(name.empty());
        CHECK(states.size() > 2);
    }
}

TEST_CASE("an id table names each thing once and numbers it once") {
    const auto unique = [](auto table) {
        std::set<std::int32_t> ids;
        std::set<std::string_view> names;
        for (const auto& [id, name] : table) {
            CHECK(ids.insert(id).second);
            CHECK(names.insert(name).second);
        }
        return ids.size();
    };
    CHECK(unique(itemIds()) == itemIds().size());
    CHECK(unique(entityTypeIds()) == entityTypeIds().size());
    CHECK(unique(particleTypeIds()) == particleTypeIds().size());
    CHECK(unique(dataComponentIds()) == dataComponentIds().size());
}

TEST_CASE("a particle's option shape decides how the packet after it is read") {
    // The options are written straight after the type id with no length, so a wrong shape
    // desynchronises the client on the very next packet rather than looking merely odd.
    using basalt::wire::protocol::play::ParticleOptionShape;

    const ParticleType* dust = particleTypes::of(Identifier::vanilla("dust"));
    REQUIRE(dust != nullptr);
    CHECK(dust->optionShape == ParticleOptionShape::Dust);

    const ParticleType* flame = particleTypes::of(Identifier::vanilla("flame"));
    REQUIRE(flame != nullptr);
    CHECK_FALSE(flame->needsOptions());
}

TEST_CASE("a block entity is named after its type, not its block") {
    // All eight copper chests are `minecraft:chest`: naming one after its block leaves it without a
    // protocol id, so it drops out of the chunk and the block turns invisible.
    const Identifier* chest = blockEntityTypes::ofBlock(Identifier::vanilla("copper_chest"));
    REQUIRE(chest != nullptr);
    CHECK(blockEntityTypes::idOf(*chest).has_value());
}

TEST_CASE("an entity type resolves both ways") {
    const EntityTypeEntry* creeper = entityTypes::of(Identifier::vanilla("creeper"));
    REQUIRE(creeper != nullptr);
    const EntityTypeEntry* back = entityTypes::byProtocolId(creeper->protocolId);
    REQUIRE(back != nullptr);
    CHECK(back->id == creeper->id);
    CHECK(entityTypes::of(Identifier::vanilla("not_a_mob")) == nullptr);
}
