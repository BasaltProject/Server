#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
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

TEST_CASE("the default state of every block belongs to that block") {
    // The invariant that block placement and the plugin API both broke by reaching for `blockIdOf`:
    // a block id used as a state id lands inside some other block's range without any error, and
    // dropped a piece of redstone dust as `oak_wood` and a redstone block as `pale_oak_leaves`.
    for (const auto& [blockId, path] : blockIdsTable()) {
        const Identifier block = Identifier::vanilla(std::string{path});
        const std::optional<std::int32_t> defaultState = blocks::defaultStateIdOf(block);
        REQUIRE(defaultState.has_value());
        const Identifier* owner = blockStateDefinitions::blockOf(*defaultState);
        REQUIRE(owner != nullptr);
        CHECK(owner->path() == path);
        static_cast<void>(blockId);
    }
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

TEST_CASE("a closed trapdoor at the top of its block holds a dust up") {
    // parity: `RedStoneWireBlock.canSurviveOn` asks `isFaceSturdy(UP)`, and a trapdoor at the top of
    // its block is a full slab whose top face is the whole square. Measured in `polvotrampilla`: the
    // jar leaves the dust standing on it and this build swept it away the moment it was placed.
    const std::optional<std::int32_t> top = blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_trapdoor"),
        {{"facing", "east"}, {"half", "top"}, {"open", "false"}, {"powered", "false"}, {"waterlogged", "false"}},
        std::nullopt);
    REQUIRE(top.has_value());
    CHECK(blockShapes::isFaceSturdy(*top, BlockFace::Up));

    // And the same trapdoor at the bottom of its block does not: there is nothing up there.
    const std::optional<std::int32_t> bottom = blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_trapdoor"),
        {{"facing", "east"}, {"half", "bottom"}, {"open", "false"}, {"powered", "false"}, {"waterlogged", "false"}},
        std::nullopt);
    REQUIRE(bottom.has_value());
    CHECK_FALSE(blockShapes::isFaceSturdy(*bottom, BlockFace::Up));

    // Y la regla generica **no** sabe contestarlo, que es el motivo de que el polvo tenga su propio
    // caso en `blockSurvival::isSupported`: la regla del polvo es un conjunto de bloques sacado de la
    // sonda, y la sonda prueba cada bloque en su estado por defecto, asi que una trampilla solo
    // cuenta como suelo cerrada y arriba, y eso un conjunto de ids no lo puede decir.
    const std::int32_t dust =
        blockStateDefinitions::stateIdOf(Identifier::vanilla("redstone_wire"), {}, std::nullopt).value_or(0);
    const std::int32_t air = blocks::air().stateId;
    CHECK(blockSupport::canSurvive(dust, blocks::stone().stateId, air, air, air, air, air, false));
    CHECK_FALSE(blockSupport::canSurvive(dust, *top, air, air, air, air, air, false));
}

TEST_CASE("the generic support rule cannot answer a state question, and these are the ones it gets wrong") {
    const std::optional<std::int32_t> top = blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_trapdoor"),
        {{"facing", "east"}, {"half", "top"}, {"open", "false"}, {"powered", "false"}, {"waterlogged", "false"}},
        std::nullopt);
    REQUIRE(top.has_value());
    const std::int32_t air = blocks::air().stateId;

    // A trapdoor closed at the top of its block is sturdy, so in the jar it holds all of these up.
    // The generated rule is a set of block ids taken from a probe of default states, and a trapdoor
    // in its default state does not support, so it never made any of those sets.
    //
    // These four have their own case in `blockSurvival::isSupported` because of it. The list is the
    // point: it is what a reader needs to know before trusting the table for a fifth block.
    for (const std::string& name : {std::string{"redstone_wire"}, std::string{"stone_pressure_plate"},
                                    std::string{"oak_pressure_plate"}, std::string{"light_weighted_pressure_plate"}}) {
        const std::optional<std::int32_t> state =
            blockStateDefinitions::stateIdOf(Identifier::vanilla(name), {}, std::nullopt);
        REQUIRE(state.has_value());
        CHECK(blockSupport::canSurvive(*state, blocks::stone().stateId, air, air, air, air, air, false));
        CHECK_FALSE(blockSupport::canSurvive(*state, *top, air, air, air, air, air, false));
    }

    // And these ones the table does get right, so they need no case of their own.
    for (const std::string& name :
         {std::string{"repeater"}, std::string{"comparator"}, std::string{"redstone_torch"}, std::string{"rail"},
          std::string{"powered_rail"}, std::string{"detector_rail"}, std::string{"activator_rail"},
          std::string{"tripwire"}, std::string{"sculk_sensor"}, std::string{"lectern"}, std::string{"target"}}) {
        const std::optional<std::int32_t> state =
            blockStateDefinitions::stateIdOf(Identifier::vanilla(name), {}, std::nullopt);
        REQUIRE(state.has_value());
        CHECK(blockSupport::canSurvive(*state, *top, air, air, air, air, air, false));
    }
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

TEST_CASE("a block that needs holding up says so, and stone does not") {
    const std::int32_t stone = blocks::stone().stateId;
    CHECK_FALSE(blockSupport::needsSupport(stone));

    // A torch needs a face under or beside it, which is the whole point of the support table.
    const std::optional<std::int32_t> torch = blocks::stateIdOf(Identifier::vanilla("torch"), {});
    REQUIRE(torch.has_value());
    CHECK(blockSupport::needsSupport(*torch));
    CHECK(blockSupport::isModelled(*torch));

    const std::int32_t air = blocks::air().stateId;
    // Standing on stone it survives; floating in air it does not.
    CHECK(blockSupport::canSurvive(*torch, stone, air, air, air, air, air));
    CHECK_FALSE(blockSupport::canSurvive(*torch, air, air, air, air, air, air));
}

TEST_CASE("water and everything holding it answer the same question") {
    const std::int32_t water = blocks::water().stateId;
    CHECK(blockSupport::holdsWater(water));
    CHECK_FALSE(blockSupport::holdsWater(blocks::stone().stateId));

    const std::optional<std::int32_t> wetSlab =
        blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "bottom"}, {"waterlogged", "true"}});
    REQUIRE(wetSlab.has_value());
    CHECK(blockSupport::holdsWater(*wetSlab));
}

TEST_CASE("a piston moves what nobody named and refuses what it cannot") {
    // parity: the table lists only the states that are not Normal, which is nearly everything.
    CHECK(blockSupport::pushReaction(blocks::stone().stateId) == PushReaction::Normal);

    const std::optional<std::int32_t> obsidian = blocks::stateIdOf(Identifier::vanilla("barrier"), {});
    REQUIRE(obsidian.has_value());
    CHECK(blockSupport::pushReaction(*obsidian) == PushReaction::Block);

    // A flower comes off rather than riding along.
    const std::optional<std::int32_t> sapling = blocks::stateIdOf(Identifier::vanilla("acacia_sapling"), {});
    REQUIRE(sapling.has_value());
    CHECK(blockSupport::pushReaction(*sapling) == PushReaction::Destroy);

    // Bedrock is Normal here and a piston still will not move it: what stops it is its destroy time
    // of -1, which PistonBaseBlock checks separately. parity: Block.isPushable.
    CHECK(blockSupport::pushReaction(blocks::bedrock().stateId) == PushReaction::Normal);
    CHECK(blocks::hardnessOf(blocks::bedrock().block)->isUnbreakable());
}

TEST_CASE("conducting redstone is not the same as being solid") {
    // A slab conducts, a piece of glass does not.
    CHECK(blockSupport::conductsRedstone(blocks::stone().stateId));
    const std::optional<std::int32_t> glass = blocks::stateIdOf(Identifier::vanilla("glass"), {});
    REQUIRE(glass.has_value());
    CHECK(blockShapes::isFullCube(*glass));
    CHECK_FALSE(blockSupport::conductsRedstone(*glass));
}

TEST_CASE("a mob's attributes are its own") {
    // parity: DefaultAttributes. A cow is not a zombie is not a rabbit.
    const std::optional<double> cow =
        entityAttributes::baseValue(Identifier::vanilla("cow"), entityAttributes::kMaxHealth);
    const std::optional<double> zombie =
        entityAttributes::baseValue(Identifier::vanilla("zombie"), entityAttributes::kMaxHealth);
    REQUIRE(cow.has_value());
    REQUIRE(zombie.has_value());
    CHECK(*cow != *zombie);
    // The name travels with or without its namespace, since a plugin will write both.
    CHECK(entityAttributes::baseValue(Identifier::vanilla("cow"), "minecraft:max_health") == cow);
    // Having attributes at all is what being alive means.
    CHECK(entityAttributes::isLiving(Identifier::vanilla("cow")));
    CHECK_FALSE(entityAttributes::isLiving(Identifier::vanilla("item")));
}

TEST_CASE("an attribute travels as the number its registry gives it") {
    // This table exists because these were written by hand once and attack_speed went out as
    // attack_knockback, which is a mistake nobody spots by reading the code.
    CHECK(attributeIds::protocolId("attack_speed") == 5);
    CHECK(attributeIds::protocolId("minecraft:attack_knockback") == 4);
    CHECK(attributeIds::protocolId("max_health") == 23);
    CHECK_FALSE(attributeIds::exists("not_an_attribute"));
}

TEST_CASE("brewing walks the chain vanilla builds in code") {
    // parity: PotionBrewing.bootstrap. Water plus nether wart is the awkward potion every other
    // brew starts from.
    const Identifier bottle = Identifier::vanilla("potion");
    const std::optional<brewing::Brew> awkward =
        brewing::mix(bottle, std::string{"water"}, Identifier::vanilla("nether_wart"));
    REQUIRE(awkward.has_value());
    CHECK(awkward->potion == "awkward");
    CHECK(awkward->bottle == bottle);

    // Gunpowder changes the bottle and leaves what is in it alone.
    const std::optional<brewing::Brew> splash =
        brewing::mix(bottle, std::string{"awkward"}, Identifier::vanilla("gunpowder"));
    REQUIRE(splash.has_value());
    CHECK(splash->bottle == Identifier::vanilla("splash_potion"));
    CHECK(splash->potion == "awkward");

    CHECK(brewing::isIngredient(Identifier::vanilla("nether_wart")));
    // Stone really is one of the twenty one ingredients: water and stone give a mundane potion, and
    // awkward and stone give an infested one. Twenty one, not the handful it used to be.
    CHECK(brewing::isIngredient(Identifier::vanilla("stone")));
    CHECK_FALSE(brewing::isIngredient(Identifier::vanilla("dirt")));
    CHECK_FALSE(brewing::mix(bottle, std::string{"water"}, Identifier::vanilla("dirt")).has_value());
}

TEST_CASE("a slime's spawn box is bigger than the slime") {
    // parity: the cube mobs carry a builder-only safety scale on the box a spawn is checked against.
    const Identifier slime = Identifier::vanilla("slime");
    CHECK(entityTypeShapes::spawnWidthOf(slime) == entityTypeShapes::widthOf(slime) * 4.0);
    const Identifier cow = Identifier::vanilla("cow");
    CHECK(entityTypeShapes::spawnWidthOf(cow) == entityTypeShapes::widthOf(cow));
    CHECK(entityTypeShapes::categoryOf(cow) == "creature");
    CHECK(entityTypeShapes::isFireImmune(Identifier::vanilla("blaze")));
    CHECK(entityTypeShapes::isImmuneTo(Identifier::vanilla("fox"), "sweet_berry_bush"));
    CHECK_FALSE(entityTypeShapes::isImmuneTo(Identifier::vanilla("cow"), "sweet_berry_bush"));
}

TEST_CASE("the first matching tool rule wins, which is what an exclusion is for") {
    // parity: ToolComponent. `incorrect_for_diamond_tool` is listed first, so it beats the
    // `mineable/pickaxe` rule that follows it.
    const Identifier pickaxe = Identifier::vanilla("diamond_pickaxe");
    const std::span<const itemTools::Rule> rules = itemTools::rulesFor(pickaxe);
    REQUIRE(rules.size() >= 2);
    CHECK(rules.front().blockTag.find("incorrect_for") != std::string_view::npos);

    const Identifier obsidian = Identifier::vanilla("obsidian");
    // A lookup that says the exclusion holds it: the tool is then not correct, whatever follows.
    const auto excluded = [](std::string_view tag, const Identifier&) {
        return tag.find("incorrect_for") != std::string_view::npos;
    };
    CHECK_FALSE(itemTools::isCorrectForDrops(&pickaxe, obsidian, excluded));

    const auto mineable = [](std::string_view tag, const Identifier&) { return tag == "minecraft:mineable/pickaxe"; };
    CHECK(itemTools::isCorrectForDrops(&pickaxe, obsidian, mineable));
    CHECK(itemTools::miningSpeed(&pickaxe, obsidian, mineable) > 1.0F);
    // A bare hand is never correct, which is why stone gives nothing when punched.
    CHECK_FALSE(itemTools::isCorrectForDrops(nullptr, obsidian, mineable));
    CHECK(itemTools::miningSpeed(nullptr, obsidian, mineable) == 1.0F);
}

TEST_CASE("a block with no hardness comes apart on the spot and bedrock never does") {
    const auto never = [](std::string_view, const Identifier&) { return false; };
    const Identifier flower = Identifier::vanilla("dandelion");
    CHECK(itemTools::destroyProgressPerTick(nullptr, flower, 0.0F, false, {}, never) > 1.0F);
    CHECK(itemTools::destroyProgressPerTick(nullptr, Identifier::vanilla("bedrock"), -1.0F, false, {}, never) == 0.0F);
}

TEST_CASE("ice is slippery and honey kills a jump") {
    // parity: only the blocks that differ from the defaults are in the table.
    const std::int32_t stone = blocks::stone().stateId;
    CHECK(blockMovement::frictionOf(stone) == blockMovement::kDefaultFriction);

    const std::optional<std::int32_t> ice = blocks::stateIdOf(Identifier::vanilla("ice"), {});
    REQUIRE(ice.has_value());
    CHECK(blockMovement::frictionOf(*ice) > blockMovement::kDefaultFriction);

    const std::optional<std::int32_t> honey = blocks::stateIdOf(Identifier::vanilla("honey_block"), {});
    REQUIRE(honey.has_value());
    CHECK(blockMovement::jumpFactorOf(*honey) < blockMovement::kDefaultJumpFactor);

    const std::optional<std::int32_t> slime = blocks::stateIdOf(Identifier::vanilla("slime_block"), {});
    REQUIRE(slime.has_value());
    CHECK(blockMovement::bouncinessOf(*slime) > 0.0F);
}

TEST_CASE("a note block plays what is under it, and a head is not tunable") {
    // parity: NoteBlockInstrument. Anything the table does not name is the harp.
    CHECK(noteBlockInstruments::of(Identifier::vanilla("stone")).serializedName() == "basedrum");
    CHECK(noteBlockInstruments::of(Identifier::vanilla("dirt")).serializedName() == "harp");

    const NoteBlockInstrument* skeleton = noteBlockInstruments::byName("skeleton");
    REQUIRE(skeleton != nullptr);
    CHECK_FALSE(skeleton->isTunable());
    CHECK(skeleton->worksAbove());

    const NoteBlockInstrument* harp = noteBlockInstruments::byName("harp");
    REQUIRE(harp != nullptr);
    CHECK(harp->isTunable());
    CHECK_FALSE(harp->worksAbove());

    // parity: NoteBlock.getPitchFromNote, an octave either side of the middle note.
    CHECK(noteBlockInstruments::pitchOf(12) == doctest::Approx(1.0F));
    CHECK(noteBlockInstruments::pitchOf(24) == doctest::Approx(2.0F));
    CHECK(noteBlockInstruments::pitchOf(0) == doctest::Approx(0.5F));
}

TEST_CASE("a statistic's id is its position, which is protocol") {
    // The statistics screen reads each counter by id, so the order is not a detail.
    CHECK(customStats::idOf("leave_game") == 0);
    CHECK(customStats::nameOf(0) == "leave_game");
    CHECK_FALSE(customStats::idOf("not_a_statistic").has_value());
    CHECK_FALSE(customStats::nameOf(static_cast<std::int32_t>(customStats::count())).has_value());
}

TEST_CASE("a point of interest is made of blocks, and a bed is sixteen of them") {
    const std::vector<Identifier> home = pointsOfInterest::blocksOf(Identifier::vanilla("home"));
    CHECK(home.size() == 16);
    CHECK(std::ranges::find(home, Identifier::vanilla("red_bed")) != home.end());
    CHECK(pointsOfInterest::blocksOf(Identifier::vanilla("armorer")).size() == 1);
    CHECK(pointsOfInterest::blocksOf(Identifier::vanilla("nothing")).empty());
    CHECK(pointsOfInterest::ids().size() == 21);
}

TEST_CASE("a waterlogged block does not burn, whatever it is made of") {
    const std::optional<std::int32_t> dryFence = blocks::stateIdOf(
        Identifier::vanilla("oak_fence"),
        {{"north", "false"}, {"east", "false"}, {"south", "false"}, {"west", "false"}, {"waterlogged", "false"}});
    const std::optional<std::int32_t> wetFence = blocks::stateIdOf(
        Identifier::vanilla("oak_fence"),
        {{"north", "false"}, {"east", "false"}, {"south", "false"}, {"west", "false"}, {"waterlogged", "true"}});
    REQUIRE(dryFence.has_value());
    REQUIRE(wetFence.has_value());
    CHECK(fireOdds::igniteOdds(*dryFence) > 0);
    CHECK(fireOdds::igniteOdds(*wetFence) == 0);
    CHECK(fireOdds::burnOdds(*wetFence) == 0);
}

TEST_CASE("a biome keeps the id saved chunks were written with") {
    // The original overworld entries keep their ids so saved chunks go on decoding after the nether,
    // the end and the void were added.
    CHECK(biomes::registry().idOf(Identifier::vanilla("plains")) == 0);
    CHECK(biomes::plains().id == Identifier::vanilla("plains"));
    CHECK(biomes::registry().size() == 66);
}

TEST_CASE("a fence joins its own kind and grabs a solid wall") {
    // parity: FenceBlock.isSameFence is not "another fence": a wooden fence joins wooden fences and
    // a nether brick fence joins nether brick ones, and the two ignore each other.
    const std::map<std::string, std::string> open = {
        {"north", "false"}, {"east", "false"}, {"south", "false"}, {"west", "false"}, {"waterlogged", "false"}};
    const std::optional<std::int32_t> oak = blocks::stateIdOf(Identifier::vanilla("oak_fence"), open);
    const std::optional<std::int32_t> nether = blocks::stateIdOf(Identifier::vanilla("nether_brick_fence"), open);
    REQUIRE(oak.has_value());
    REQUIRE(nether.has_value());
    CHECK(blockConnections::isFence(*oak));
    CHECK(blockConnections::connects(*oak));

    CHECK(blockConnections::connectsTo(*oak, *oak, BlockFace::North));
    CHECK_FALSE(blockConnections::connectsTo(*oak, *nether, BlockFace::North));
    // A solid block is grabbed by both, which is the sturdy-face half of the rule.
    CHECK(blockConnections::connectsTo(*oak, blocks::stone().stateId, BlockFace::North));
    CHECK_FALSE(blockConnections::connectsTo(*oak, blocks::air().stateId, BlockFace::North));
}

TEST_CASE("stairs turn a corner when the stairs behind them face across") {
    // parity: StairBlock.getStairsShape.
    const auto stairs = [](std::string_view facing) {
        return blocks::stateIdOf(
            Identifier::vanilla("oak_stairs"),
            {{"facing", std::string{facing}}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}});
    };
    const std::optional<std::int32_t> north = stairs("north");
    const std::optional<std::int32_t> east = stairs("east");
    REQUIRE(north.has_value());
    REQUIRE(east.has_value());
    const std::int32_t air = blocks::air().stateId;

    CHECK(blockConnections::stairShape(*north, air, air, air, air) == "straight");
    // Stairs behind it facing across turn it into an outer corner.
    CHECK(blockConnections::stairShape(*north, *east, air, air, air) == "outer_right");
    CHECK_FALSE(blockConnections::stairShape(blocks::stone().stateId, air, air, air, air).has_value());
}

TEST_CASE("a tag that refers to another tag is flattened through it") {
    // parity: TagLoader.build, resolving what a tag refers to before the tag itself.
    using namespace basalt::server::registry::tags;
    const Identifier planks = Identifier::vanilla("planks");
    const Identifier wood = Identifier::vanilla("wood");
    const Identifier oak = Identifier::vanilla("oak_planks");
    const Identifier log = Identifier::vanilla("oak_log");

    const std::map<Identifier, TagFile> files = {
        {planks, TagFile{{TagEntry{oak, false, true}}, false}},
        {wood, TagFile{{TagEntry{planks, true, true}, TagEntry{log, false, true}}, false}},
    };
    const auto resolved = resolve(files, [&](const Identifier& id) { return id == oak || id == log; });
    REQUIRE(resolved.has_value());
    CHECK(resolved->at(wood) == std::set<Identifier>{oak, log});
    CHECK(resolved->at(planks) == std::set<Identifier>{oak});
}

TEST_CASE("a missing element is an error when required and dropped when not") {
    using namespace basalt::server::registry::tags;
    const Identifier tag = Identifier::vanilla("planks");
    const Identifier missing = Identifier::vanilla("nothing");
    const auto nothingExists = [](const Identifier&) { return false; };

    const std::map<Identifier, TagFile> required = {{tag, TagFile{{TagEntry{missing, false, true}}, false}}};
    const auto refused = resolve(required, nothingExists);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().kind == TagError::Kind::MissingElement);

    const std::map<Identifier, TagFile> optional = {{tag, TagFile{{TagEntry{missing, false, false}}, false}}};
    const auto accepted = resolve(optional, nothingExists);
    REQUIRE(accepted.has_value());
    CHECK(accepted->at(tag).empty());
}

TEST_CASE("a cycle is refused rather than followed forever") {
    using namespace basalt::server::registry::tags;
    const Identifier first = Identifier::vanilla("first");
    const Identifier second = Identifier::vanilla("second");
    const std::map<Identifier, TagFile> files = {
        {first, TagFile{{TagEntry{second, true, true}}, false}},
        {second, TagFile{{TagEntry{first, true, true}}, false}},
    };
    const auto resolved = resolve(files, [](const Identifier&) { return true; });
    REQUIRE_FALSE(resolved.has_value());
    CHECK(resolved.error().kind == TagError::Kind::Cycle);
}

TEST_CASE("what is in water says so through its property, or is one of the four that cannot") {
    // parity: BlockState.getFluidState. Checked against the state arithmetic this replaces: of
    // 32,366 states the two agreed on all but 30, and those 30 are these four blocks.
    CHECK(blockFluids::hasWaterFluid(blocks::water().stateId));
    // Every level of it, not just the source, which is one state of sixteen.
    CHECK(blockFluids::hasWaterFluid(blocks::water().stateId + 15));
    CHECK_FALSE(blockFluids::hasWaterFluid(blocks::stone().stateId));

    for (const std::string_view path : {"seagrass", "kelp", "kelp_plant"}) {
        const std::optional<std::int32_t> state = blocks::stateIdOf(Identifier::vanilla(std::string{path}), {});
        CAPTURE(path);
        REQUIRE(state.has_value());
        CHECK(blockFluids::hasWaterFluid(*state));
    }

    const std::optional<std::int32_t> wetSlab =
        blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "bottom"}, {"waterlogged", "true"}});
    REQUIRE(wetSlab.has_value());
    CHECK(blockFluids::hasWaterFluid(*wetSlab));

    CHECK(blockFluids::hasFluid(blocks::lava().stateId));
    CHECK_FALSE(blockFluids::hasWaterFluid(blocks::lava().stateId));
    CHECK_FALSE(blockFluids::hasFluid(blocks::stone().stateId));
}

TEST_CASE("food carries what eating it is worth, and what it leaves in the hand") {
    const FoodValue* bread = itemFood::of(Identifier::vanilla("bread"));
    REQUIRE(bread != nullptr);
    CHECK(bread->nutrition > 0);
    CHECK(bread->consumeTicks() > 0);
    CHECK_FALSE(bread->remainder.has_value());

    // parity: minecraft:use_remainder, the bowl left behind by a stew.
    const FoodValue* stew = itemFood::of(Identifier::vanilla("mushroom_stew"));
    REQUIRE(stew != nullptr);
    REQUIRE(stew->remainder.has_value());
    CHECK(*stew->remainder == Identifier::vanilla("bowl"));

    // A golden apple may be eaten on a full stomach, and it applies effects.
    const FoodValue* golden = itemFood::of(Identifier::vanilla("golden_apple"));
    REQUIRE(golden != nullptr);
    CHECK(golden->canAlwaysEat);
    CHECK_FALSE(golden->effects.empty());
    CHECK(golden->effects.front().as<ConsumeEffect::Apply>() != nullptr);

    CHECK(itemFood::of(Identifier::vanilla("stone")) == nullptr);
}

TEST_CASE("a tag file is read the way vanilla writes one") {
    using namespace basalt::server::registry::tags;
    const auto file = tagFileJson::parse(R"({"values": ["minecraft:oak_planks", "#minecraft:logs",
                                            {"id": "minecraft:nothing", "required": false}]})");
    REQUIRE(file.has_value());
    REQUIRE(file->entries.size() == 3);
    CHECK(file->entries[0].id == Identifier::vanilla("oak_planks"));
    CHECK_FALSE(file->entries[0].isTag);
    CHECK(file->entries[0].required);
    // A leading hash is what makes an entry a reference to another tag.
    CHECK(file->entries[1].isTag);
    CHECK(file->entries[1].id == Identifier::vanilla("logs"));
    CHECK_FALSE(file->entries[2].required);
    CHECK_FALSE(file->replace);

    CHECK(tagFileJson::parse(R"({"replace": true, "values": []})")->replace);
    // A shape this build does not understand is refused rather than half-read.
    CHECK(tagFileJson::parse(R"({"values": [42]})").error() == tagFileJson::ParseError::UnsupportedEntry);
    CHECK(tagFileJson::parse(R"({})").error() == tagFileJson::ParseError::MissingValues);
}
