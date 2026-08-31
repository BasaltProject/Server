#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <doctest.h>

#include <optional>
#include <set>
#include <string>

using namespace basalt::server::registry;
using basalt::wire::common::Identifier;

TEST_CASE("a state id and its properties convert into each other") {
    // The arithmetic replaces a hand-written table of every state worldgen writes: that table and
    // this agreed on all 4977 of its entries, which is why it is not carried across.
    for (const auto& [stateId, path] : defaultBlockStatesTable()) {
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
    for (const auto& [stateId, path] : defaultBlockStatesTable()) {
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
    const std::string_view index = blockShapeIndexTable();
    REQUIRE(index.size() % 2 == 0);
    CHECK(static_cast<std::int32_t>(index.size() / 2) >= blocks::stateCount());
    for (std::size_t at = 0; at + 1 < index.size(); at += 2) {
        const std::size_t high = kAlphabet.find(index[at]);
        const std::size_t low = kAlphabet.find(index[at + 1]);
        CAPTURE(at / 2);
        REQUIRE(high != std::string_view::npos);
        REQUIRE(low != std::string_view::npos);
        REQUIRE(high * kAlphabet.size() + low < blockShapeSpecsTable().size());
    }
}

TEST_CASE("a table row is one row, however it was written in the source") {
    // The long rows are written as several joined pieces. Reading them as separate rows would turn
    // one bitset into a dozen fragments, which is exactly the bug this catches.
    for (const std::string_view set : supportSetsTable()) {
        CHECK(set.size() > 2);
    }
    for (const auto& [name, states] : blockFamiliesTable()) {
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
    CHECK(unique(itemIdsTable()) == itemIdsTable().size());
    CHECK(unique(entityTypeIdsTable()) == entityTypeIdsTable().size());
    CHECK(unique(particleTypeIdsTable()) == particleTypeIdsTable().size());
    CHECK(unique(dataComponentIdsTable()) == dataComponentIdsTable().size());
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

TEST_CASE("a shape answers the questions a fluid and a torch ask of it") {
    const std::int32_t stone = blocks::stone().stateId;
    CHECK(blockShapes::isFullCube(stone));
    CHECK(blockShapes::blocksMotion(stone));
    CHECK(blockShapes::isFaceSturdy(stone, BlockFace::Up));
    CHECK(blockShapes::isCollisionFaceFull(stone, BlockFace::Up));
    CHECK_FALSE(blockShapes::isReplaceable(stone));

    const std::int32_t air = blocks::air().stateId;
    CHECK(blockShapes::isEmptyShape(air));
    CHECK_FALSE(blockShapes::blocksMotion(air));
    // parity: air is replaceable, which is what lets a block be placed into it at all.
    CHECK(blockShapes::isReplaceable(air));
}

TEST_CASE("two half slabs facing each other close a gap neither closes alone") {
    // parity: Shapes.mergedFaceOccludes. Each shape is measured in its own cube, so what matters is
    // which of them reaches the plane they share: a top slab below reaches it, and so does a bottom
    // slab above. Stacked the other way round neither of them does, and the plane stays open.
    const std::optional<std::int32_t> bottom =
        blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "bottom"}, {"waterlogged", "false"}});
    const std::optional<std::int32_t> top =
        blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "top"}, {"waterlogged", "false"}});
    REQUIRE(bottom.has_value());
    REQUIRE(top.has_value());
    CHECK_FALSE(blockShapes::isFullCube(*bottom));
    CHECK_FALSE(blockShapes::isFullCube(*top));
    // Looking down from the bottom slab above to the top slab below: both reach the shared plane.
    CHECK(blockShapes::mergedFaceOccludes(*bottom, *top, BlockFace::Down));
    // The same two the other way up: neither reaches it, so the plane is open.
    CHECK_FALSE(blockShapes::mergedFaceOccludes(*top, *bottom, BlockFace::Down));
}

TEST_CASE("a face mask is the same however many times it is asked for") {
    // The masks are built up front so the table stays immutable: several region threads ask about a
    // shape at once, and a table that fills itself lazily would need a lock between them.
    const std::int32_t stone = blocks::stone().stateId;
    for (int repeat = 0; repeat < 3; ++repeat) {
        CHECK(blockShapes::faceCovers(stone, BlockFace::North, 0, blockShapes::kUnits, 0, blockShapes::kUnits));
    }
}

TEST_CASE("an item's block is itself unless the table says otherwise") {
    // parity: BlockItem.getBlock and Block.asItem. Without the dozen exceptions, middle-clicking
    // dust or a crop gives nothing, because no item is called `redstone_wire`.
    CHECK(itemBlocks::of(Identifier::vanilla("stone")) == Identifier::vanilla("stone"));
    CHECK(itemBlocks::of(Identifier::vanilla("redstone")) == Identifier::vanilla("redstone_wire"));
    CHECK(itemBlocks::itemOf(Identifier::vanilla("redstone_wire")) == Identifier::vanilla("redstone"));
}

TEST_CASE("an item that never wears is not one that wears slowly") {
    // parity: ItemStack.hurtAndBreak returns immediately for an item with no max_damage component.
    CHECK(itemDurability::isDamageable(Identifier::vanilla("diamond_pickaxe")));
    CHECK(itemDurability::maxDamageOf(Identifier::vanilla("diamond_pickaxe")) > 0);
    CHECK_FALSE(itemDurability::isDamageable(Identifier::vanilla("stone")));
    CHECK(itemDurability::maxDamageOf(Identifier::vanilla("stone")) == 0);
}

TEST_CASE("stack size falls back to the default rather than to zero") {
    CHECK(itemStacking::maxStackSize(Identifier::vanilla("stone")) == itemStacking::kDefaultStackSize);
    CHECK(itemStacking::maxStackSize(Identifier::vanilla("ender_pearl")) == 16);
    CHECK(itemStacking::cooldownTicks(Identifier::vanilla("ender_pearl")) > 0);
    CHECK(itemStacking::cooldownTicks(Identifier::vanilla("stone")) == 0);
}

TEST_CASE("copper weathers along its own chain") {
    // parity: WeatheringCopper.NEXT_BY_BLOCK is a map of blocks, not a naming rule.
    CHECK(weathering::stageOf("copper_block") == 0);
    const std::optional<std::string_view> next = weathering::next("copper_block");
    REQUIRE(next.has_value());
    CHECK(weathering::stageOf(*next) == 1);
    CHECK_FALSE(weathering::weathers("stone"));
}

TEST_CASE("a dye carries a colour and nothing else does") {
    CHECK(dyeColours::of(Identifier::vanilla("red_dye")) == 11546150);
    CHECK_FALSE(dyeColours::of(Identifier::vanilla("red_wool")).has_value());
    CHECK_FALSE(dyeColours::of(Identifier{"basalt", "red_dye"}).has_value());
}

TEST_CASE("what burns in a furnace burns for a while") {
    CHECK(fuels::isFuel(Identifier::vanilla("coal")));
    CHECK(fuels::burnTicks(Identifier::vanilla("coal")) > 0);
    CHECK_FALSE(fuels::isFuel(Identifier::vanilla("stone")));
}
