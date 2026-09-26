#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/entity/villager/VillagerNbt.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::entity::villager::BlockPos;
using basalt::server::runtime::entity::villager::GossipType;
using basalt::server::runtime::entity::villager::RaidView;
using basalt::server::runtime::entity::villager::VillagerWorld;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace ai = basalt::server::runtime::entity::ai;
namespace blocks = basalt::server::registry::blocks;
namespace villager = basalt::server::runtime::entity::villager;
namespace brain = basalt::server::runtime::entity::villager::villagerBrain;

namespace {

class NoActions final : public ai::MobActions {
public:
    void swing(ServerEntity&) override {}

    void hurtPlayer(ServerEntity&, std::int32_t, float) override {}

    void hurtMob(ServerEntity&, ServerEntity&, float) override {}

    void breed(ServerEntity&, ServerEntity&) override {}

    void eatGrass(ServerEntity&) override {}

    void primeCreeper(ServerEntity&) override {}

    void explodeCreeper(ServerEntity&) override {}

    void setCharging(ServerEntity&, bool) override {}

    void shootFireball(ServerEntity&, double, double, double, bool) override {}

    void setBeamTarget(ServerEntity&, std::optional<std::int32_t>) override {}

    void setCasting(ServerEntity&, bool) override {}

    void throwPotion(ServerEntity&, double, double, double, std::string_view) override {}

    void castFangs(ServerEntity&, double, double, double) override {}

    void summonVexes(ServerEntity&, std::int32_t) override {}

    void applyEffect(std::int32_t, std::string_view, std::int32_t, std::int32_t, std::optional<std::int32_t>) override {
    }

    void setSleeping(ServerEntity&, bool) override {}

    void teleported(ServerEntity&) override {}

    void shootArrow(ServerEntity&, double, double, double, float) override {}
};

/// A world that only records what the villager asked of it.
class QuietWorld final : public VillagerWorld {
public:
    std::optional<RaidView> raidAt(const ServerEntity&) override { return std::nullopt; }

    void entityEvent(ServerEntity&, std::int8_t event) override { events.push_back(event); }

    void sound(ServerEntity&, std::string_view, float, float) override {}

    void blockSound(ServerEntity&, const BlockPos&, std::string_view) override {}

    void levelEvent(ServerEntity&, std::int32_t, const BlockPos&, std::int32_t) override {}

    bool setBlock(ServerEntity&, const BlockPos&, std::int32_t) override { return true; }

    void destroyBlock(ServerEntity&, const BlockPos&) override {}

    void setDoorOpen(ServerEntity&, const BlockPos&, bool) override {}

    void ringBell(ServerEntity&, const BlockPos&) override {}

    std::vector<basalt::server::runtime::world::trades::Offer>* offers(ServerEntity& mob) override {
        return mob.offers == nullptr ? nullptr : &mob.offers->list;
    }

    std::optional<std::int32_t> composterAdd(ServerEntity&, const BlockPos&, const Identifier&) override {
        return std::nullopt;
    }

    void composterExtract(ServerEntity&, const BlockPos&) override {}

    bool growCrop(ServerEntity&, const BlockPos&) override { return false; }

    void throwItem(ServerEntity&, const basalt::server::world::loot::ItemStack&, double, double, double) override {}

    void dropItem(ServerEntity&, const basalt::server::world::loot::ItemStack&, float) override {}

    void throwGift(ServerEntity&, std::string_view, std::int32_t) override {}

    void setSleeping(ServerEntity& mob, std::optional<BlockPos> bed) override {
        mob.sleeping = bed.has_value();
        if (bed.has_value()) {
            mob.x = bed->x + 0.5;
            mob.y = bed->y + 0.6875;
            mob.z = bed->z + 0.5;
        }
    }

    void villagerDataChanged(ServerEntity&) override { dataChanges++; }

    void mainHandChanged(ServerEntity&) override {}

    void unhappyChanged(ServerEntity&, std::int32_t) override {}

    void addEffect(ServerEntity&, std::string_view effect, std::int32_t, std::int32_t) override {
        effects.emplace_back(effect);
    }

    bool summonGolem(ServerEntity&) override {
        golems++;
        return true;
    }

    ServerEntity* spawnChild(ServerEntity&, ServerEntity&) override { return nullptr; }

    void launchFirework(ServerEntity&, std::int32_t, std::int32_t) override {}

    std::optional<std::int32_t> heroLevel(std::int32_t) override { return hero; }

    std::optional<std::string> playerMainHand(std::int32_t) override { return std::nullopt; }

    bool canSeeSky(ServerEntity&, const BlockPos&) override { return true; }

    std::int32_t motionBlockingHeight(ServerEntity&, std::int32_t, std::int32_t) override { return 64; }

    bool mobGriefing(ServerEntity&) override { return true; }

    std::int64_t currentDay(ServerEntity&) override { return 0; }

    void itemsNear(ServerEntity&, double, double, std::vector<ItemView>& out) override { out.clear(); }

    std::int32_t takeItem(ServerEntity&, std::int32_t, std::int32_t) override { return 0; }

    std::vector<std::int8_t> events;
    std::int32_t dataChanges = 0;
    std::vector<std::string> effects;
    std::optional<std::int32_t> hero;
    std::int32_t golems = 0;
};

struct Fixture {
    WorldState world;
    std::vector<std::unique_ptr<ServerEntity>> owned;
    std::vector<ServerEntity*> entities;
    std::vector<ai::PlayerView> players;
    RandomSource random = RandomSource::legacy(7);
    NoActions actions;
    QuietWorld villagers;
    std::int64_t gameTime = 100;

    Fixture() {
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                world.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
        const std::int32_t stone = blocks::named("stone")->stateId;
        for (std::int32_t x = -16; x < 32; x++) {
            for (std::int32_t z = -16; z < 32; z++) {
                REQUIRE(world.setBlockState(x, 63, z, stone));
            }
        }
    }

    ServerEntity& villagerAt(double x, double z) {
        const basalt::server::registry::EntityTypeEntry* entry =
            basalt::server::registry::entityTypes::of(Identifier::vanilla("villager"));
        REQUIRE(entry != nullptr);
        const auto id = static_cast<std::int32_t>(owned.size() + 1);
        owned.push_back(std::make_unique<ServerEntity>(id, Uuid{static_cast<std::uint64_t>(id), 1}, entry->id,
                                                       entry->protocolId, x, 64.0, z));
        ServerEntity& made = *owned.back();
        made.profession = "none";
        made.onGround = true;
        brain::stateOf(made).dataFinalized = true;
        entities.push_back(&made);
        return made;
    }

    /// One turn of every villager: the brain, then the walk the brain asked for.
    void tick(std::int64_t dayTime) {
        for (ServerEntity* mob : entities) {
            ai::MobContext context{.world = world,
                                   .mob = *mob,
                                   .random = random,
                                   .entities = entities,
                                   .players = players,
                                   .actions = actions,
                                   .dayTime = dayTime,
                                   .gameTime = gameTime,
                                   .speed = 0.5};
            brain::tick(context, villagers);
            ai::navigation::tick(world, *mob);
            ai::moveControl::tick(world, *mob, 0.5);
            ai::lookControl::tick(*mob);
        }
        ++gameTime;
    }
};

} // namespace

TEST_CASE("an unemployed villager claims a free workstation and takes its profession") {
    Fixture fixture;
    const BlockPos composter{10, 64, 6};
    REQUIRE(fixture.world.setBlockState(composter.x, composter.y, composter.z, blocks::named("composter")->stateId));
    ServerEntity& villager = fixture.villagerAt(4.5, 4.5);

    for (std::int32_t turn = 0; turn < 1200 && villager.profession == "none"; ++turn) {
        fixture.tick(3000);
    }

    CHECK(villager.profession == "farmer");
    const villager::Memories& memories = brain::stateOf(villager).memories;
    REQUIRE(memories.jobSite.present());
    CHECK(*memories.jobSite.value == composter);
    // parity: the one ticket a workstation has is taken, so nobody else can claim it.
    const std::optional<basalt::server::runtime::world::poi::PoiRecord> record = fixture.world.poi().record(composter);
    REQUIRE(record.has_value());
    CHECK(record->freeTickets == 0);
    CHECK(fixture.villagers.dataChanges > 0);
}

TEST_CASE("a villager with a bed claims it, walks to it at night and sleeps there") {
    Fixture fixture;
    const std::optional<std::int32_t> head = basalt::server::registry::blocks::stateIdOf(
        Identifier::vanilla("red_bed"), {{"part", "head"}, {"facing", "south"}});
    const std::optional<std::int32_t> foot = basalt::server::registry::blocks::stateIdOf(
        Identifier::vanilla("red_bed"), {{"part", "foot"}, {"facing", "south"}});
    REQUIRE(head.has_value());
    REQUIRE(foot.has_value());
    const BlockPos bed{12, 64, 9};
    REQUIRE(fixture.world.setBlockState(bed.x, bed.y, bed.z, *head));
    REQUIRE(fixture.world.setBlockState(bed.x, bed.y, bed.z - 1, *foot));
    ServerEntity& villager = fixture.villagerAt(3.5, 3.5);

    for (std::int32_t turn = 0; turn < 1200 && !villager.sleeping; ++turn) {
        fixture.tick(13000);
    }

    const villager::Memories& memories = brain::stateOf(villager).memories;
    REQUIRE(memories.home.present());
    CHECK(*memories.home.value == bed);
    CHECK(villager.sleeping);
    CHECK(memories.lastSlept.present());
    // parity: `AcquirePoi`'s event fourteen, the happy particles when a home is claimed.
    CHECK(std::ranges::count(fixture.villagers.events, std::int8_t{14}) >= 1);
}

TEST_CASE("gossip and the brain's saved memories survive a save") {
    Fixture fixture;
    ServerEntity& villager = fixture.villagerAt(4.5, 4.5);
    villager::VillagerState& state = brain::stateOf(villager);
    state.memories.home.set(BlockPos{1, 64, 2});
    state.memories.lastSlept.set(55);
    state.gossips.add(Uuid{9, 9}.toString(), GossipType::kMinorNegative, 25);
    state.gossips.add(Uuid{9, 9}.toString(), GossipType::kTrading, 2);
    state.foodLevel = 3;

    basalt::wire::nbt::NbtCompound tag;
    villager::villagerNbt::write(villager, tag);
    ServerEntity& reloaded = fixture.villagerAt(0.5, 0.5);
    villager::villagerNbt::read(tag, reloaded);

    const villager::VillagerState& read = brain::stateOf(reloaded);
    REQUIRE(read.memories.home.present());
    CHECK(*read.memories.home.value == BlockPos{1, 64, 2});
    CHECK(read.memories.lastSlept.value == std::optional<std::int64_t>{55});
    // parity: minor negative weighs minus one, trading plus one.
    CHECK(read.gossips.reputation(Uuid{9, 9}.toString()) == -23);
    CHECK(read.foodLevel == 3);
}

TEST_CASE("a trade prices by reputation and hero, pays experience and promotes the villager") {
    Fixture fixture;
    ServerEntity& villager = fixture.villagerAt(4.5, 4.5);
    villager.profession = "farmer";
    villager.offers = std::make_shared<basalt::server::runtime::entity::villagers::Offers>();
    villager.offers->list.push_back(
        basalt::server::runtime::world::trades::Offer{.costA = {.item = Identifier::vanilla("wheat"), .count = 10},
                                                      .result = {.item = Identifier::vanilla("emerald"), .count = 1},
                                                      .experience = 10,
                                                      .priceMultiplier = 0.2F});
    const std::string buyer = Uuid{9, 9}.toString();
    brain::stateOf(villager).gossips.add(buyer, GossipType::kTrading, 10);
    fixture.villagers.hero = 1;

    REQUIRE(brain::startTrading(villager, fixture.villagers, 77, buyer, true));
    // parity: floor(10 * 0.2F) off for reputation, floor(0.3625 * 10) off for hero level two.
    CHECK(villager.offers->list[0].specialPrice == -5);
    CHECK_FALSE(brain::startTrading(villager, fixture.villagers, 78, buyer, true));

    const std::int32_t orb = brain::rewardTradeXp(villager, fixture.villagers, 10, fixture.random);
    CHECK(villager.villagerLevel == 2);
    CHECK(orb >= 8);
    CHECK(orb <= 11);
    CHECK(fixture.villagers.effects == std::vector<std::string>{"regeneration"});
    CHECK(brain::stateOf(villager).lastTradedPlayer == std::optional<std::int32_t>{77});

    brain::stopTrading(villager);
    CHECK(villager.offers->list[0].specialPrice == 0);
    CHECK_FALSE(brain::stateOf(villager).tradingPlayer.has_value());
}
