#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::entity;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;

namespace {

[[nodiscard]] ServerEntity spawn(std::string path, std::int32_t id = 1) {
    return ServerEntity{id, Uuid{1, 2}, Identifier{"minecraft", std::move(path)}, 0, 0.0, 64.0, 0.0};
}

[[nodiscard]] Identifier of(std::string path) { return Identifier::vanilla(std::move(path)); }

} // namespace

TEST_CASE("what counts as a mob, which a category alone does not say") {
    CHECK(mobs::isMob(of("zombie")));
    CHECK(mobs::isMob(of("cow")));
    // parity: a villager and an iron golem are `misc` because nothing spawns them in a wave, and
    // reading that as "not a mob" left both standing still with no AI at all.
    CHECK(mobs::categoryOf(of("villager")) == "misc");
    CHECK(mobs::isMob(of("villager")));
    CHECK(mobs::isMob(of("iron_golem")));
    // Furniture is furniture: it has no follow range because it is not a `Mob`.
    CHECK_FALSE(mobs::isMob(of("arrow")));
    CHECK_FALSE(mobs::isMob(of("item")));
    CHECK_FALSE(mobs::isMob(of("boat")));
    CHECK(mobs::categoryOf(of("zombie")) == "monster");
    CHECK(mobs::categoryOf(of("cow")) == "creature");

    // Which of the two halves of the rule each type is answered by, over the whole registry.
    std::int32_t miscWithFollowRange = 0;
    std::int32_t spawnedWithoutFollowRange = 0;
    for (const registry::EntityTypeEntry& entry : registry::entityTypes::all()) {
        const bool misc = registry::entityTypeShapes::categoryOf(entry.id) == "misc";
        const bool follow = registry::entityAttributes::baseValue(entry.id, "follow_range").has_value();
        if (misc && follow) {
            ++miscWithFollowRange;
        }
        if (!misc && !follow) {
            CAPTURE(entry.id.toString());
            ++spawnedWithoutFollowRange;
        }
    }
    // The half that earns its place: the mobs nothing spawns in a wave, which the category alone
    // would have called furniture.
    CHECK(miscWithFollowRange > 0);
    // And the other half: everything in a spawn category is a mob, so nothing needs the attribute
    // to prove it.
    CHECK(spawnedWithoutFollowRange == 0);
}

TEST_CASE("a cube is what its size makes it") {
    CHECK(cubeMobs::isCube(of("slime")));
    CHECK(cubeMobs::isCube(of("magma_cube")));
    CHECK(cubeMobs::isCube(of("sulfur_cube")));
    CHECK_FALSE(cubeMobs::isCube(of("cow")));

    // parity: `AbstractCubeMob.setSize`: health is the square of the size, speed is 0.2 plus a
    // tenth of it, and damage is the size itself.
    ServerEntity slime = spawn("slime");
    cubeMobs::applySize(slime, 4);
    CHECK(slime.cubeSize == 4);
    CHECK(slime.health == 16.0);
    CHECK(slime.attribute(registry::entityAttributes::kMaxHealth) == 16.0);
    CHECK(slime.attribute(registry::entityAttributes::kMovementSpeed) == doctest::Approx(0.6));
    CHECK(slime.attribute(registry::entityAttributes::kAttackDamage) == 4.0);

    // The smallest is what `/summon` gives, and one is the floor.
    ServerEntity small = spawn("slime");
    cubeMobs::applySize(small, 0);
    CHECK(small.cubeSize == cubeMobs::kDefaultSize);
    CHECK(small.health == 1.0);
    ServerEntity huge = spawn("magma_cube");
    cubeMobs::applySize(huge, 1000);
    CHECK(huge.cubeSize == cubeMobs::kMaxSize);

    // Anything else is left alone, attributes and all.
    ServerEntity cow = spawn("cow");
    const double health = cow.health;
    cubeMobs::applySize(cow, 4);
    CHECK(cow.health == health);
    CHECK(cow.attributeValues.empty());
    CHECK(cow.cubeSize == 1);
}

TEST_CASE("which mobs have a baby, and how the client is told") {
    CHECK(ageableMobs::isAgeable("cow"));
    CHECK(ageableMobs::isAgeable(of("villager")));
    // Ageable is not the same question as breedable: a squid and a slime age too.
    CHECK(ageableMobs::isAgeable("squid"));
    CHECK(ageableMobs::isAgeable("slime"));
    CHECK_FALSE(ageableMobs::isAgeable("zombie"));
    CHECK_FALSE(ageableMobs::isAgeable("creeper"));

    // The zombie family carries a flag rather than an age, and both kinds tell the client.
    CHECK(ageableMobs::usesBooleanBabyFlag("zombie"));
    CHECK(ageableMobs::usesBooleanBabyFlag("zombified_piglin"));
    CHECK_FALSE(ageableMobs::usesBooleanBabyFlag("cow"));
    CHECK(ageableMobs::hasBabyMetadata("cow"));
    CHECK(ageableMobs::hasBabyMetadata("zombie"));
    CHECK_FALSE(ageableMobs::hasBabyMetadata("creeper"));

    // parity: the index the baby flag rides at, and the age a baby starts from.
    CHECK(ageableMobs::kBabyDataIndex == 16);
    CHECK(ageableMobs::kBabyStartAge == -24000);
}

TEST_CASE("what tames an animal, and what the client draws once it is tamed") {
    CHECK(taming::isTamable(of("wolf")));
    CHECK(taming::isTamable(of("cat")));
    CHECK(taming::isTamable(of("ocelot")));
    CHECK(taming::isTamable(of("parrot")));
    CHECK_FALSE(taming::isTamable(of("cow")));

    CHECK(taming::tamedBy(of("wolf"), of("bone")));
    CHECK_FALSE(taming::tamedBy(of("wolf"), of("cod")));
    CHECK(taming::tamedBy(of("cat"), of("cod")));
    CHECK(taming::tamedBy(of("cat"), of("salmon")));
    CHECK(taming::tamedBy(of("ocelot"), of("salmon")));
    CHECK(taming::tamedBy(of("parrot"), of("torchflower_seeds")));
    CHECK_FALSE(taming::tamedBy(of("parrot"), of("bone")));
    CHECK_FALSE(taming::tamedBy(of("cow"), of("wheat")));

    ServerEntity wolf = spawn("wolf");
    CHECK(taming::flagsOf(wolf) == 0);
    wolf.sitting = true;
    CHECK(taming::flagsOf(wolf) == taming::kFlagSitting);
    wolf.owner = "an-owner-uuid";
    CHECK(taming::flagsOf(wolf) == (taming::kFlagSitting | taming::kFlagTamed));
    wolf.sitting = false;
    CHECK(taming::flagsOf(wolf) == taming::kFlagTamed);
}

TEST_CASE("what keeps a mob in the world without anybody asking for it") {
    ServerEntity cow = spawn("cow");
    CHECK_FALSE(mobPersistence::keeps(cow));

    // Named or commanded, which is the ordinary flag.
    cow.persistent = true;
    CHECK(mobPersistence::keeps(cow));
    cow.persistent = false;

    // A rider and a leashed mob cannot vanish out from under their relationship.
    cow.vehicleId = 7;
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.vehicleId.reset();
    cow.leashHolderId = 7;
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.leashHolderId.reset();
    cow.leashHolderUuid = "an-uuid";
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.leashHolderUuid.reset();
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(cow));

    // A leash that only exists in the saved data counts too, under either spelling.
    NbtCompound leashed;
    leashed.putBoolean("leash", true);
    cow.sourceNbt = leashed;
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    NbtCompound leashedOldSpelling;
    leashedOldSpelling.putBoolean("Leash", true);
    cow.sourceNbt = leashedOldSpelling;
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.sourceNbt.reset();

    // Taming and raiding both call setPersistenceRequired in vanilla.
    cow.owner = "an-owner";
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.owner.reset();
    cow.raidCenter = std::array<std::int32_t, 3>{1, 2, 3};
    CHECK(mobPersistence::requiresCustomPersistence(cow));
    cow.raidCenter.reset();
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(cow));
}

TEST_CASE("a fish out of a bucket, an enderman holding something, and a cube that ate") {
    ServerEntity salmon = spawn("salmon");
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(salmon));
    NbtCompound bucketed;
    bucketed.putBoolean("FromBucket", true);
    salmon.sourceNbt = bucketed;
    CHECK(mobPersistence::requiresCustomPersistence(salmon));
    // The key is exact: the fish family spells it one way and only that way.
    NbtCompound wrongSpelling;
    wrongSpelling.putBoolean("from_bucket", true);
    salmon.sourceNbt = wrongSpelling;
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(salmon));

    ServerEntity enderman = spawn("enderman");
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(enderman));
    NbtCompound holding;
    NbtCompound state;
    state.putString("Name", "minecraft:grass_block");
    holding.set("carriedBlockState", basalt::wire::nbt::NbtTag::ofCompound(std::move(state)));
    enderman.sourceNbt = holding;
    CHECK(mobPersistence::requiresCustomPersistence(enderman));
    // An empty block state is an enderman holding nothing, which is every other enderman.
    NbtCompound holdingNothing;
    holdingNothing.set("carriedBlockState", basalt::wire::nbt::NbtTag::ofCompound(NbtCompound{}));
    enderman.sourceNbt = holdingNothing;
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(enderman));

    // And a sulfur cube, which spells its own bucket key the other way and also counts what it
    // is wearing.
    ServerEntity cube = spawn("sulfur_cube");
    NbtCompound cubeBucket;
    cubeBucket.putBoolean("from_bucket", true);
    cube.sourceNbt = cubeBucket;
    CHECK(mobPersistence::requiresCustomPersistence(cube));

    NbtCompound body;
    body.putString("id", "minecraft:saddle");
    body.putInt("count", 1);
    NbtCompound equipment;
    equipment.set("body", basalt::wire::nbt::NbtTag::ofCompound(std::move(body)));
    NbtCompound wearing;
    wearing.set("equipment", basalt::wire::nbt::NbtTag::ofCompound(std::move(equipment)));
    cube.sourceNbt = wearing;
    CHECK(mobPersistence::requiresCustomPersistence(cube));

    // A slot that names an item nobody has is not an item either: both halves have to hold.
    NbtCompound noneOfThem;
    noneOfThem.putString("id", "minecraft:saddle");
    noneOfThem.putInt("count", 0);
    NbtCompound noneEquipment;
    noneEquipment.set("body", basalt::wire::nbt::NbtTag::ofCompound(std::move(noneOfThem)));
    NbtCompound noneWearing;
    noneWearing.set("equipment", basalt::wire::nbt::NbtTag::ofCompound(std::move(noneEquipment)));
    cube.sourceNbt = noneWearing;
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(cube));

    // An empty slot is not an item.
    NbtCompound emptyBody;
    emptyBody.putString("id", "");
    emptyBody.putInt("count", 0);
    NbtCompound emptyEquipment;
    emptyEquipment.set("body", basalt::wire::nbt::NbtTag::ofCompound(std::move(emptyBody)));
    NbtCompound emptyWearing;
    emptyWearing.set("equipment", basalt::wire::nbt::NbtTag::ofCompound(std::move(emptyEquipment)));
    cube.sourceNbt = emptyWearing;
    CHECK_FALSE(mobPersistence::requiresCustomPersistence(cube));
}

TEST_CASE("the undead burning in daylight, and everything that stops it") {
    rng::RandomSource rolls = rng::RandomSource::legacy(0);
    CHECK(sunBurn::burns(of("zombie")));
    CHECK(sunBurn::burns(of("skeleton")));
    CHECK(sunBurn::burns(of("phantom")));
    CHECK_FALSE(sunBurn::burns(of("creeper")));
    CHECK_FALSE(sunBurn::burns(of("cow")));

    // Night, a roof, water and the wrong mob each answer no on their own, and answer it every
    // time: catching fire is a roll, and not catching fire for these reasons is not.
    for (std::int32_t attempt = 0; attempt < 500; ++attempt) {
        CHECK_FALSE(sunBurn::shouldBurn(of("zombie"), false, 15, true, false, rolls));
        CHECK_FALSE(sunBurn::shouldBurn(of("zombie"), true, 15, false, false, rolls));
        CHECK_FALSE(sunBurn::shouldBurn(of("zombie"), true, 15, true, true, rolls));
        CHECK_FALSE(sunBurn::shouldBurn(of("creeper"), true, 15, true, false, rolls));
        // parity: the light where it stands has to be over half of fifteen, so seven never burns
        // however long it stands there.
        CHECK_FALSE(sunBurn::shouldBurn(of("zombie"), true, 7, true, false, rolls));
    }

    // parity: even in full daylight it is a roll, `nextFloat() * 30 < (1 - 0.4) * 2`, which is one
    // tick in twenty-five: a zombie in the open catches within a second or two rather than at once.
    std::int32_t caught = 0;
    for (std::int32_t attempt = 0; attempt < 3000; ++attempt) {
        if (sunBurn::shouldBurn(of("zombie"), true, 15, true, false, rolls)) {
            ++caught;
        }
    }
    // A twenty-fifth of three thousand is a hundred and twenty; the window is wide enough for the
    // roll and narrow enough to tell it apart from the odds without the margin, which are two
    // thirtieths and would land near two hundred.
    CHECK(caught > 85);
    CHECK(caught < 165);

    // Dimmer light is a longer wait, not a different rule: at nine the odds are a tenth of that.
    std::int32_t dim = 0;
    for (std::int32_t attempt = 0; attempt < 3000; ++attempt) {
        if (sunBurn::shouldBurn(of("zombie"), true, 9, true, false, rolls)) {
            ++dim;
        }
    }
    CHECK(dim > 0);
    CHECK(dim < caught);

    CHECK(sunBurn::kFireTicks == 160);
    CHECK(sunBurn::kDamageInterval == 20);
}

TEST_CASE("when a mob is forgotten for being too far from anybody") {
    rng::RandomSource rolls = rng::RandomSource::legacy(0);
    ServerEntity zombie = spawn("zombie");
    zombie.idleTicks = 1200;

    // Past the category's own distance it goes at once.
    CHECK(mobDespawn::shouldDespawn(zombie, MobCategory::Monster, 200.0, rolls));
    // Inside 32 blocks it never goes, and the clock is reset so it is not forgotten under your
    // nose the moment you step back.
    CHECK_FALSE(mobDespawn::shouldDespawn(zombie, MobCategory::Monster, 10.0, rolls));
    CHECK(zombie.idleTicks == 0);

    // Between the two it takes the roll, but only after ten minutes of nobody near it.
    // Ten minutes of nobody near it, and not a tick less: twenty thousand rolls with the clock
    // still running answer no every time, where without the clock it would have gone long since.
    for (std::int32_t attempt = 0; attempt < 20000; ++attempt) {
        zombie.idleTicks = 10;
        CHECK_FALSE(mobDespawn::shouldDespawn(zombie, MobCategory::Monster, 100.0, rolls));
    }
    zombie.idleTicks = mobDespawn::kIdleBeforeRandom + 1;
    std::int32_t despawned = 0;
    for (std::int32_t attempt = 0; attempt < 4000; ++attempt) {
        if (mobDespawn::shouldDespawn(zombie, MobCategory::Monster, 100.0, rolls)) {
            ++despawned;
        }
    }
    // One in eight hundred a tick, so a few of four thousand and nowhere near all of them.
    CHECK(despawned > 0);
    CHECK(despawned < 40);

    // Nobody in the world at all is nobody to be far from.
    CHECK_FALSE(mobDespawn::shouldDespawn(zombie, MobCategory::Monster, std::nullopt, rolls));
}

TEST_CASE("the mobs that are never forgotten, whatever the distance") {
    rng::RandomSource rolls = rng::RandomSource::legacy(0);
    for (const std::string& path : {"cow", "sheep", "wolf", "turtle", "trader_llama", "wandering_trader"}) {
        CAPTURE(path);
        ServerEntity animal = spawn(path);
        animal.idleTicks = 100000;
        CHECK_FALSE(mobDespawn::shouldDespawn(animal, MobCategory::Creature, 5000.0, rolls));
    }
    // parity: nothing in the misc category is forgotten, so a village does not empty when you
    // walk away.
    ServerEntity villager = spawn("villager");
    villager.idleTicks = 100000;
    CHECK_FALSE(mobDespawn::shouldDespawn(villager, MobCategory::Misc, 5000.0, rolls));

    // A chicken is only forgotten while it is carrying a jockey.
    ServerEntity chicken = spawn("chicken");
    chicken.idleTicks = 100000;
    CHECK_FALSE(mobDespawn::shouldDespawn(chicken, MobCategory::Creature, 5000.0, rolls));
    NbtCompound jockey;
    jockey.putBoolean("IsChickenJockey", true);
    chicken.sourceNbt = jockey;
    CHECK(mobDespawn::shouldDespawn(chicken, MobCategory::Creature, 5000.0, rolls));

    // A cat has two minutes before it can be forgotten at all.
    ServerEntity cat = spawn("cat");
    cat.idleTicks = 100000;
    cat.tickCount = 100;
    CHECK_FALSE(mobDespawn::shouldDespawn(cat, MobCategory::Creature, 5000.0, rolls));
    cat.tickCount = 5000;
    CHECK(mobDespawn::shouldDespawn(cat, MobCategory::Creature, 5000.0, rolls));

    // An ocelot has the same grace, and trusting one settles it for good.
    ServerEntity young = spawn("ocelot");
    young.idleTicks = 100000;
    young.tickCount = 100;
    CHECK_FALSE(mobDespawn::shouldDespawn(young, MobCategory::Creature, 5000.0, rolls));

    ServerEntity ocelot = spawn("ocelot");
    ocelot.idleTicks = 100000;
    ocelot.tickCount = 5000;
    CHECK(mobDespawn::shouldDespawn(ocelot, MobCategory::Creature, 5000.0, rolls));
    NbtCompound trusting;
    trusting.putBoolean("Trusting", true);
    ocelot.sourceNbt = trusting;
    CHECK_FALSE(mobDespawn::shouldDespawn(ocelot, MobCategory::Creature, 5000.0, rolls));

    // A patrol may leave the player, but not past the hard boundary.
    ServerEntity pillager = spawn("pillager");
    pillager.patrolling = true;
    pillager.idleTicks = 100000;
    // Not once in twenty thousand ticks, where an ordinary monster taking a roll of one in eight
    // hundred would have gone twenty-five times over.
    for (std::int32_t attempt = 0; attempt < 20000; ++attempt) {
        CHECK_FALSE(mobDespawn::shouldDespawn(pillager, MobCategory::Monster, 100.0, rolls));
    }
    CHECK(mobDespawn::shouldDespawn(pillager, MobCategory::Monster, 200.0, rolls));

    // And anything kept for another reason is kept, with its clock reset.
    ServerEntity named = spawn("zombie");
    named.persistent = true;
    named.idleTicks = 100000;
    CHECK_FALSE(mobDespawn::shouldDespawn(named, MobCategory::Monster, 5000.0, rolls));
    CHECK(named.idleTicks == 0);
}

TEST_CASE("a warden is angry at somebody, not at the world") {
    ServerEntity warden = spawn("warden");
    CHECK(wardens::isWarden(warden.type()));
    CHECK_FALSE(wardens::isWarden(of("zombie")));
    // Absent rather than empty for everything else: a map per entity would cost every arrow in the
    // world one.
    CHECK_FALSE(warden.anger.has_value());
    CHECK_FALSE(wardens::target(warden).has_value());
    CHECK(wardens::angerAt(warden, 7) == 0);

    CHECK(wardens::anger(warden, 7, wardens::kHeard) == wardens::kHeard);
    CHECK(wardens::anger(warden, 7, wardens::kHeard) == 2 * wardens::kHeard);
    CHECK(wardens::angerAt(warden, 7) == 70);
    // Seventy is not yet enough to come after anybody.
    CHECK_FALSE(wardens::target(warden).has_value());

    CHECK(wardens::anger(warden, 7, wardens::kHeard) == wardens::kAngry + 25);
    CHECK(wardens::target(warden) == 7);

    // It tops out at 150 however much it hears.
    CHECK(wardens::anger(warden, 7, 1000) == wardens::kMaxAnger);

    // Angriest wins, and only above the threshold.
    wardens::anger(warden, 9, wardens::kHurt);
    CHECK(wardens::target(warden) == 7);
    wardens::anger(warden, 9, 1000);
    CHECK(wardens::angerAt(warden, 9) == wardens::kMaxAnger);

    // One off every suspect every tick, and forgotten at one.
    for (std::int32_t tick = 0; tick < 149; ++tick) {
        wardens::tick(warden);
    }
    CHECK(wardens::angerAt(warden, 7) == 1);
    wardens::tick(warden);
    CHECK(wardens::angerAt(warden, 7) == 0);
    CHECK(warden.anger->empty());
    CHECK_FALSE(wardens::target(warden).has_value());
}

TEST_CASE("what middle-clicking an entity gives you") {
    // A mob answers with its own spawn egg.
    CHECK(entityPickResults::itemFor(of("cow")) == of("cow_spawn_egg"));
    CHECK(entityPickResults::itemFor(of("zombie")) == of("zombie_spawn_egg"));

    // The things that were placed from an item answer with that item.
    CHECK(entityPickResults::itemFor(of("minecart")) == of("minecart"));
    CHECK(entityPickResults::itemFor(of("armor_stand")) == of("armor_stand"));
    CHECK(entityPickResults::itemFor(of("item_frame")) == of("item_frame"));
    CHECK(entityPickResults::itemFor(of("oak_boat")) == of("oak_boat"));
    CHECK(entityPickResults::itemFor(of("bamboo_raft")) == of("bamboo_raft"));
    // parity: the one whose item is not called after it.
    CHECK(entityPickResults::itemFor(of("leash_knot")) == of("lead"));

    // And everything else answers with nothing, including the ones that share a name with an item.
    CHECK_FALSE(entityPickResults::itemFor(of("arrow")).has_value());
    CHECK_FALSE(entityPickResults::itemFor(of("snowball")).has_value());
    CHECK_FALSE(entityPickResults::itemFor(of("tnt")).has_value());
    CHECK_FALSE(entityPickResults::itemFor(of("player")).has_value());
    // A plugin's own entity is not vanilla's business.
    CHECK_FALSE(entityPickResults::itemFor(Identifier{"basalt", "cow"}).has_value());
}
