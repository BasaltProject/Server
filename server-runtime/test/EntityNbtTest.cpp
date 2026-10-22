#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

using basalt::server::runtime::entity::AttributeOperation;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::ChunkGeneratedEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace entityNbt = basalt::server::runtime::entity::entityNbt;
namespace ageableMobs = basalt::server::runtime::entity::ageableMobs;
namespace cubeMobs = basalt::server::runtime::entity::cubeMobs;

namespace {

[[nodiscard]] ServerEntity entityOf(const std::string& type, std::int32_t entityId = 1) {
    return ServerEntity{entityId, Uuid{0x1234, 0x5678}, Identifier::vanilla(type), 7, 1.5, 64.0, -2.5, 90.0F, 10.0F};
}

[[nodiscard]] entityNbt::AllocateEntityId ids(std::shared_ptr<std::int32_t> next) {
    return [next] { return (*next)++; };
}

[[nodiscard]] std::optional<std::int32_t> anyType(const Identifier&) { return 7; }

} // namespace

TEST_CASE("an entity comes back off its own tag as it went in") {
    ServerEntity cow = entityOf("cow");
    cow.velocityX = 0.25;
    cow.velocityY = -0.5;
    cow.velocityZ = 0.75;
    cow.health = 8.0;
    cow.age = -1200;
    cow.airSupply = 120;
    cow.persistent = true;
    cow.customName = "Bessie";
    cow.tags.emplace_back("chosen");

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(cow), 42, 9);

    REQUIRE(back != nullptr);
    CHECK(back->entityId() == 42);
    CHECK(back->protocolType() == 9);
    CHECK(back->uuid() == cow.uuid());
    CHECK(back->type() == cow.type());
    CHECK(back->x == doctest::Approx(1.5));
    CHECK(back->z == doctest::Approx(-2.5));
    CHECK(back->yaw == doctest::Approx(90.0F));
    CHECK(back->pitch == doctest::Approx(10.0F));
    CHECK(back->velocityY == doctest::Approx(-0.5));
    CHECK(back->health == doctest::Approx(8.0));
    CHECK(back->age == -1200);
    CHECK(back->airSupply == 120);
    CHECK(back->persistent);
    REQUIRE(back->customName.has_value());
    CHECK(*back->customName == "Bessie");
    REQUIRE(back->tags.size() == 1);
    CHECK(back->tags[0] == "chosen");
}

TEST_CASE("a tag with no position at all is not an entity") {
    NbtCompound tag;
    tag.putString("id", "minecraft:cow");

    CHECK(entityNbt::decode(tag, 1, 1) == nullptr);

    NbtList shortPos{NbtType::Double};
    shortPos.push(NbtTag::ofDouble(1.0));
    tag.set("Pos", NbtTag::ofList(std::move(shortPos)));
    CHECK(entityNbt::decode(tag, 1, 1) == nullptr);
}

TEST_CASE("a tag naming nothing this build can parse is not an entity") {
    NbtCompound tag;
    tag.putString("id", "one:too:many");

    CHECK(entityNbt::decode(tag, 1, 1) == nullptr);
}

TEST_CASE("a saved uuid is kept, and a broken one is replaced rather than refused") {
    ServerEntity cow = entityOf("cow");
    NbtCompound tag = entityNbt::encode(cow);
    tag.putString("UUID", "not a uuid");

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);

    REQUIRE(back != nullptr);
    CHECK(back->uuid() != cow.uuid());
}

TEST_CASE("the mobs that keep a boolean baby flag are read through it") {
    // parity: `AgeableMob` writes an age.
    REQUIRE(ageableMobs::usesBooleanBabyFlag("zombie"));
    NbtCompound tag;
    tag.putString("id", "minecraft:zombie");
    NbtList pos{NbtType::Double};
    pos.push(NbtTag::ofDouble(0.0));
    pos.push(NbtTag::ofDouble(0.0));
    pos.push(NbtTag::ofDouble(0.0));
    tag.set("Pos", NbtTag::ofList(std::move(pos)));
    tag.putBoolean("IsBaby", true);

    const std::unique_ptr<ServerEntity> baby = entityNbt::decode(tag, 1, 1);

    REQUIRE(baby != nullptr);
    CHECK(baby->age == ageableMobs::kBabyStartAge);

    tag.putBoolean("IsBaby", false);
    tag.putInt("Age", 55);
    const std::unique_ptr<ServerEntity> grown = entityNbt::decode(tag, 1, 1);
    REQUIRE(grown != nullptr);
    CHECK(grown->age == 55);
}

TEST_CASE("an entity is saved with the fields its own class has and no others") {
    // parity: `Health` is LivingEntity.
    const NbtCompound arrow = entityNbt::encode(entityOf("arrow"));
    CHECK(arrow.contains("Air"));
    CHECK_FALSE(arrow.contains("Health"));
    CHECK_FALSE(arrow.contains("Age"));
    CHECK_FALSE(arrow.contains("NoAI"));
    CHECK_FALSE(arrow.contains("Sitting"));
    CHECK_FALSE(arrow.contains("InLove"));

    const NbtCompound cow = entityNbt::encode(entityOf("cow"));
    CHECK(cow.contains("Health"));
    CHECK(cow.contains("Age"));
    CHECK(cow.contains("NoAI"));
    CHECK(cow.contains("InLove"));
    CHECK_FALSE(cow.contains("Sitting"));

    const NbtCompound stand = entityNbt::encode(entityOf("armor_stand"));
    CHECK(stand.contains("Health"));
    CHECK_FALSE(stand.contains("NoAI"));
    CHECK(entityNbt::encode(entityOf("wolf")).contains("Sitting"));
}

TEST_CASE("primed tnt keeps its fuse under the name vanilla gives it") {
    ServerEntity tnt = entityOf("tnt");
    tnt.fuse = 40;

    const NbtCompound tag = entityNbt::encode(tnt);
    // parity: `PrimedTnt.TAG_FUSE`.
    CHECK(tag.getShort("fuse") == 40);
    CHECK_FALSE(tag.contains("Fuse"));
    CHECK(entityNbt::decode(tag, 1, 1)->fuse == 40);

    CHECK_FALSE(entityNbt::encode(entityOf("cow")).contains("fuse"));
    CHECK(entityNbt::decode(entityNbt::encode(entityOf("cow")), 1, 1)->fuse == ServerEntity::kNoFuse);
}

TEST_CASE("a cube's size is written one lower than it is kept") {
    ServerEntity slime = entityOf("slime");
    slime.cubeSize = 4;

    const NbtCompound tag = entityNbt::encode(slime);

    // parity: `Slime` stores size zero for the smallest one.
    CHECK(tag.getInt("Size") == 3);
    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);
    REQUIRE(back != nullptr);
    CHECK(back->cubeSize == 4);

    const NbtCompound cow = entityNbt::encode(entityOf("cow"));
    CHECK_FALSE(cow.contains("Size"));
    CHECK(entityNbt::decode(cow, 1, 1)->cubeSize == cubeMobs::kDefaultSize);
}

TEST_CASE("a villager keeps its profession without the namespace and a level of at least one") {
    ServerEntity villager = entityOf("villager");
    villager.profession = "librarian";
    villager.villagerLevel = 3;
    villager.villagerExperience = 250;

    const NbtCompound tag = entityNbt::encode(villager);

    REQUIRE(tag.getCompound("VillagerData") != nullptr);
    CHECK(tag.getCompound("VillagerData")->getString("profession") == "minecraft:librarian");
    CHECK(tag.getCompound("VillagerData")->getString("type") == "minecraft:plains");
    CHECK(tag.getInt("Xp") == 250);

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);
    REQUIRE(back != nullptr);
    CHECK(back->profession == "librarian");
    CHECK(back->villagerLevel == 3);
    CHECK(back->villagerExperience == 250);
}

TEST_CASE("a villager level of nothing at all is read as one") {
    NbtCompound tag = entityNbt::encode(entityOf("villager"));
    NbtCompound data;
    data.putString("profession", "minecraft:nitwit");
    data.putInt("level", 0);
    tag.set("VillagerData", NbtTag::ofCompound(std::move(data)));

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);

    REQUIRE(back != nullptr);
    CHECK(back->villagerLevel == 1);
}

TEST_CASE("a patrolling mob keeps where it was going, and everything else does not") {
    ServerEntity pillager = entityOf("pillager");
    pillager.patrolTarget = std::array<std::int32_t, 3>{10, 64, -20};
    pillager.patrolLeader = true;
    pillager.patrolling = true;

    const NbtCompound tag = entityNbt::encode(pillager);
    CHECK(tag.getIntArray("patrol_target").size() == 3);

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);
    REQUIRE(back != nullptr);
    REQUIRE(back->patrolTarget.has_value());
    CHECK((*back->patrolTarget)[0] == 10);
    CHECK(back->patrolLeader);

    ServerEntity cow = entityOf("cow");
    cow.patrolLeader = true;
    CHECK_FALSE(entityNbt::encode(cow).contains("PatrolLeader"));
}

TEST_CASE("a wandering trader keeps its delay and where it was headed") {
    ServerEntity trader = entityOf("wandering_trader");
    trader.despawnDelay = 4000;
    trader.wanderTarget = std::array<std::int32_t, 3>{1, 2, 3};

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(trader), 1, 1);

    REQUIRE(back != nullptr);
    CHECK(back->despawnDelay == 4000);
    REQUIRE(back->wanderTarget.has_value());
    CHECK((*back->wanderTarget)[2] == 3);

    ServerEntity llama = entityOf("trader_llama");
    llama.despawnDelay = 100;
    const NbtCompound tag = entityNbt::encode(llama);
    CHECK(tag.getInt("DespawnDelay") == 100);
    CHECK_FALSE(tag.contains("wander_target"));
}

TEST_CASE("a trapped skeleton horse remembers that it is one") {
    ServerEntity horse = entityOf("skeleton_horse");
    horse.skeletonTrap = true;
    horse.skeletonTrapTime = 300;
    horse.tamedHorse = true;

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(horse), 1, 1);

    REQUIRE(back != nullptr);
    CHECK(back->skeletonTrap);
    CHECK(back->skeletonTrapTime == 300);
    CHECK(back->tamedHorse);
}

TEST_CASE("a leashed mob remembers who is holding it") {
    ServerEntity cow = entityOf("cow");
    const std::string holder = basalt::wire::common::Uuid{0x0123456789abcdefULL, 0xfedcba9876543210ULL}.toString();
    cow.leashHolderUuid = holder;

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(cow), 1, 1);

    REQUIRE(back != nullptr);
    REQUIRE(back->leashHolderUuid.has_value());
    CHECK(*back->leashHolderUuid == holder);
}

TEST_CASE("what is on fire comes back on fire") {
    ServerEntity arrow = entityOf("arrow");
    arrow.fireTicks = 200;

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(arrow), 1, 1);

    REQUIRE(back != nullptr);
    // parity: `Entity` `Fire` short: a burning arrow lights the tnt it lands in.
    CHECK(back->fireTicks == 200);
}

TEST_CASE("what hangs in the air comes back hanging") {
    ServerEntity arrow = entityOf("arrow");
    arrow.noGravity = true;

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(arrow), 1, 1);

    REQUIRE(back != nullptr);
    // parity: `NoGravity` decides where a projectile goes.
    CHECK(back->noGravity);
}

TEST_CASE("an entity's uuid goes out as the four ints vanilla reads") {
    ServerEntity cow = entityOf("cow");
    const basalt::wire::nbt::NbtCompound tag = entityNbt::encode(cow);
    const std::span<const std::int32_t> parts = tag.getIntArray("UUID");

    REQUIRE(parts.size() == 4);
    const auto word = [parts](std::size_t at) {
        return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
    };
    CHECK(((word(0) << 32) | word(1)) == cow.uuid().high());
    CHECK(((word(2) << 32) | word(3)) == cow.uuid().low());
}

TEST_CASE("a villager's own memories go out and come back") {
    ServerEntity villager = entityOf("villager");
    namespace brain = basalt::server::runtime::entity::villager::villagerBrain;
    brain::stateOf(villager).memories.lastSlept.set(12345);
    brain::stateOf(villager).memories.golemDetectedRecently.set(true, 600);

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(villager), 1, 1);

    REQUIRE(back != nullptr);
    CHECK(brain::stateOf(*back).memories.lastSlept.value == std::optional<std::int64_t>{12345});
    CHECK(brain::stateOf(*back).memories.golemDetectedRecently.ttl == 600);

    ServerEntity quiet = entityOf("villager");
    static_cast<void>(brain::stateOf(quiet));
    const NbtCompound tag = entityNbt::encode(quiet);
    REQUIRE(tag.getCompound("Brain") != nullptr);
    CHECK_FALSE(tag.getCompound("Brain")->getCompound("memories")->contains("minecraft:golem_detected_recently"));
}

TEST_CASE("attributes and their modifiers are read back off the packed list") {
    NbtCompound modifier;
    modifier.putString("id", "minecraft:leader_zombie_bonus");
    modifier.putDouble("amount", 3.0);
    modifier.putString("operation", "add_value");
    NbtList modifiers{NbtType::Compound};
    modifiers.push(NbtTag::ofCompound(modifier));

    NbtCompound attribute;
    attribute.putString("id", "minecraft:max_health");
    attribute.putDouble("base", 20.0);
    attribute.set("modifiers", NbtTag::ofList(std::move(modifiers)));
    NbtList attributes{NbtType::Compound};
    attributes.push(NbtTag::ofCompound(attribute));

    NbtCompound tag = entityNbt::encode(entityOf("zombie"));
    tag.set("attributes", NbtTag::ofList(std::move(attributes)));
    tag.putFloat("Health", 100.0F);

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(tag, 1, 1);

    REQUIRE(back != nullptr);
    CHECK(back->attributeValues.at("minecraft:max_health") == doctest::Approx(20.0));
    REQUIRE(back->attributeModifiers.contains("minecraft:max_health"));
    CHECK(back->attributeModifiers.at("minecraft:max_health").at("minecraft:leader_zombie_bonus").operation ==
          AttributeOperation::AddValue);
    // parity: health saved above what the attributes allow is clamped rather than kept.
    CHECK(back->health == doctest::Approx(23.0));
}

TEST_CASE("a field vanilla writes as a float is read whatever number it was written as") {
    NbtCompound tag = entityNbt::encode(entityOf("cow"));
    tag.set("Health", NbtTag::ofDouble(6.0));
    CHECK(entityNbt::decode(tag, 1, 1)->health == doctest::Approx(6.0));

    tag.set("Health", NbtTag::ofByte(4));
    CHECK(entityNbt::decode(tag, 1, 1)->health == doctest::Approx(4.0));

    tag.set("Age", NbtTag::ofShort(30));
    CHECK(entityNbt::decode(tag, 1, 1)->age == 30);
}

TEST_CASE("a vehicle carries its riders in its own tag") {
    ServerEntity boat = entityOf("boat", 1);
    ServerEntity rider = entityOf("cow", 2);
    ServerEntity pillion = entityOf("chicken", 3);
    boat.passengerIds = {2};
    rider.passengerIds = {3};
    std::map<std::int32_t, ServerEntity*> byId{{1, &boat}, {2, &rider}, {3, &pillion}};

    const NbtCompound tag = entityNbt::encodeTree(boat, [&byId](std::int32_t id) {
        const auto found = byId.find(id);
        return found == byId.end() ? nullptr : found->second;
    });

    REQUIRE(tag.getList("Passengers") != nullptr);
    REQUIRE(tag.getList("Passengers")->size() == 1);
    const NbtCompound* first = (*tag.getList("Passengers"))[0].get<NbtCompound>();
    REQUIRE(first != nullptr);
    CHECK(first->getString("id") == "minecraft:cow");
    REQUIRE(first->getList("Passengers") != nullptr);
    CHECK(first->getList("Passengers")->size() == 1);

    const auto next = std::make_shared<std::int32_t>(10);
    const std::vector<std::unique_ptr<ServerEntity>> back = entityNbt::decodeTree(tag, ids(next), anyType);
    REQUIRE(back.size() == 3);
    CHECK(back[0]->entityId() == 10);
    CHECK(back[1]->vehicleId == 10);
    CHECK(back[2]->vehicleId == back[1]->entityId());
    CHECK(back[0]->passengerIds.size() == 1);
}

TEST_CASE("a rider that was removed is left behind rather than saved with its vehicle") {
    ServerEntity boat = entityOf("boat", 1);
    ServerEntity rider = entityOf("cow", 2);
    rider.removed = true;
    boat.passengerIds = {2, 3};
    std::map<std::int32_t, ServerEntity*> byId{{1, &boat}, {2, &rider}};

    const NbtCompound tag = entityNbt::encodeTree(boat, [&byId](std::int32_t id) {
        const auto found = byId.find(id);
        return found == byId.end() ? nullptr : found->second;
    });

    CHECK_FALSE(tag.contains("Passengers"));
}

TEST_CASE("a type nobody can number is not restored") {
    ServerEntity cow = entityOf("cow");
    const auto next = std::make_shared<std::int32_t>(1);

    CHECK(entityNbt::decodeTree(entityNbt::encode(cow), ids(next), [](const Identifier&) {
              return std::optional<std::int32_t>{};
          }).empty());
}

TEST_CASE("a generated entity is decoded through the same reader") {
    ChunkGeneratedEntity descriptor;
    descriptor.type = Identifier::vanilla("villager");
    descriptor.x = 8.5;
    descriptor.y = 70.0;
    descriptor.z = -3.5;
    descriptor.yaw = 45.0F;
    NbtCompound villagerData;
    villagerData.putString("profession", "minecraft:toolsmith");
    villagerData.putInt("level", 2);
    descriptor.data.set("VillagerData", NbtTag::ofCompound(std::move(villagerData)));

    const std::unique_ptr<ServerEntity> entity = entityNbt::decodeGenerated(descriptor, 5, 6);

    REQUIRE(entity != nullptr);
    CHECK(entity->type() == Identifier::vanilla("villager"));
    CHECK(entity->x == doctest::Approx(8.5));
    CHECK(entity->yaw == doctest::Approx(45.0F));
    CHECK(entity->villagerLevel == 2);
    CHECK(entity->profession == "toolsmith");
}

TEST_CASE("axolotl playing-dead updates never overwrite the integer variant") {
    namespace entity = basalt::server::runtime::entity;
    using Packet = basalt::wire::protocol::play::SetEntityDataPacket;
    for (const std::int32_t variant : {1, 2}) {
        CAPTURE(variant);
        NbtCompound data;
        data.putInt("Variant", variant);
        const auto initial = entity::generatedEntityMetadata("axolotl", data);
        bool foundVariant = false;
        for (const auto& entry : initial) {
            if (entry.index == 18) {
                const auto* value = std::get_if<Packet::IntValue>(&entry.value);
                REQUIRE(value != nullptr);
                CHECK(value->value == variant);
                foundVariant = true;
            }
        }
        REQUIRE(foundVariant);

        ServerEntity axolotl = entityOf("axolotl");
        auto previous = entity::runtimeEntityMetadata(axolotl);
        auto& state = entity::axolotls::stateOf(axolotl);
        for (const bool playingDead : {false, true, false}) {
            state.playDeadTicks = playingDead ? 200 : 0;
            const auto current = entity::runtimeEntityMetadata(axolotl);
            const Packet packet{.entityId = axolotl.entityId(), .entries = current.changesFrom(&previous)};
            bool foundPlayingDead = false;
            for (const auto& entry : packet.entries) {
                CHECK(entry.index != 18);
                if (entry.index == 19) {
                    const auto* value = std::get_if<Packet::BoolValue>(&entry.value);
                    REQUIRE(value != nullptr);
                    CHECK(value->value == playingDead);
                    foundPlayingDead = true;
                }
            }
            REQUIRE(foundPlayingDead);
            basalt::wire::io::ByteBuffer buffer;
            packet.write(buffer);
            const auto decoded = Packet::read(buffer);
            REQUIRE(decoded.has_value());
            CHECK(decoded->entityId == axolotl.entityId());
            CHECK(decoded->entries == packet.entries);
            CHECK(buffer.readableBytes() == 0);
            CHECK(current.changesFrom(&current).empty());
            previous = current;
        }
    }
}

TEST_CASE("fox variants never overwrite the byte-valued flags metadata") {
    namespace entity = basalt::server::runtime::entity;
    using Packet = basalt::wire::protocol::play::SetEntityDataPacket;
    for (const std::string type : {"red", "snow"}) {
        for (const std::int32_t age : {0, -24000}) {
            CAPTURE(type);
            CAPTURE(age);
            NbtCompound data;
            data.putString("Type", type);
            data.putInt("Age", age);
            const Packet packet{.entityId = 86, .entries = entity::generatedEntityMetadata("fox", data)};
            bool foundVariant = false;
            for (const auto& entry : packet.entries) {
                CHECK(entry.index != 19);
                if (entry.index == 18) {
                    REQUIRE(type == "snow");
                    const auto* value = std::get_if<Packet::IntValue>(&entry.value);
                    REQUIRE(value != nullptr);
                    CHECK(value->value == 1);
                    foundVariant = true;
                }
            }
            CHECK(foundVariant == (type == "snow"));
            basalt::wire::io::ByteBuffer buffer;
            packet.write(buffer);
            const auto decoded = Packet::read(buffer);
            REQUIRE(decoded.has_value());
            CHECK(decoded->entityId == 86);
            CHECK(decoded->entries == packet.entries);
            CHECK(buffer.readableBytes() == 0);
            CHECK_FALSE(buffer.failed());
        }
    }
}

TEST_CASE("a generated vehicle brings its riders with it") {
    ChunkGeneratedEntity rider;
    rider.type = Identifier::vanilla("pillager");
    ChunkGeneratedEntity ravager;
    ravager.type = Identifier::vanilla("ravager");
    ravager.passengers.push_back(rider);
    const auto next = std::make_shared<std::int32_t>(1);

    const std::vector<std::unique_ptr<ServerEntity>> entities =
        entityNbt::decodeGeneratedTree(ravager, ids(next), anyType);

    REQUIRE(entities.size() == 2);
    CHECK(entities[0]->type() == Identifier::vanilla("ravager"));
    CHECK(entities[1]->vehicleId == entities[0]->entityId());
    CHECK(entities[0]->passengerIds.size() == 1);
}
