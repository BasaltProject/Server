#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/EntityRegistry.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/PrimedTntQueue.hpp>
#include <basalt/server/runtime/world/SculkSensorTracker.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <doctest.h>

#include <memory>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::Aabb;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> spawn(std::string path, std::int32_t id, double x = 0.0, double z = 0.0) {
    return std::make_unique<ServerEntity>(id, Uuid{1, static_cast<std::uint64_t>(id)},
                                          Identifier{"minecraft", std::move(path)}, 0, x, 64.0, z);
}

[[nodiscard]] Identifier of(std::string path) { return Identifier::vanilla(std::move(path)); }

} // namespace

TEST_CASE("a box is what an entity fills, and what it fills after it moves") {
    // parity: `AABB` around the feet: half the width each way, the whole height upward.
    const Aabb box = Aabb::of(0.5, 64.0, 0.5, 0.6, 1.8);
    CHECK(box.minX == doctest::Approx(0.2));
    CHECK(box.maxX == doctest::Approx(0.8));
    CHECK(box.minY == 64.0);
    CHECK(box.maxY == doctest::Approx(65.8));

    const Aabb moved = box.move(1.0, -2.0, 0.5);
    CHECK(moved.minX == doctest::Approx(1.2));
    CHECK(moved.minY == 62.0);
    CHECK(moved.maxZ == doctest::Approx(1.3));

    // parity: `expandTowards` grows along the movement only, which is what a swept box is.
    const Aabb swept = box.expandTowards(2.0, 0.0, -3.0);
    CHECK(swept.maxX == doctest::Approx(2.8));
    CHECK(swept.minX == doctest::Approx(0.2));
    CHECK(swept.minZ == doctest::Approx(-2.8));
    CHECK(swept.maxZ == doctest::Approx(0.8));

    const Aabb fat = box.inflate(0.5);
    CHECK(fat.minX == doctest::Approx(-0.3));
    CHECK(fat.maxY == doctest::Approx(66.3));
}

TEST_CASE("two boxes that share a face are beside each other, not inside each other") {
    const Aabb block{.minX = 0.0, .minY = 0.0, .minZ = 0.0, .maxX = 1.0, .maxY = 1.0, .maxZ = 1.0};
    const Aabb touching{.minX = 1.0, .minY = 0.0, .minZ = 0.0, .maxX = 2.0, .maxY = 1.0, .maxZ = 1.0};
    const Aabb overlapping{.minX = 0.9, .minY = 0.0, .minZ = 0.0, .maxX = 2.0, .maxY = 1.0, .maxZ = 1.0};
    // Which is what lets an entity stand exactly on a block without being inside it.
    CHECK_FALSE(block.intersects(touching));
    CHECK(block.intersects(overlapping));
    CHECK(block.intersects(block));

    CHECK(block.contains(0.5, 0.5, 0.5));
    // The edges belong to it, unlike the intersection test above.
    CHECK(block.contains(1.0, 1.0, 1.0));
    CHECK_FALSE(block.contains(1.1, 0.5, 0.5));
}

TEST_CASE("which mobs are refused by liquid and which live in it") {
    CHECK(naturalSpawnObstruction::allowsLiquids(of("cod")));
    CHECK(naturalSpawnObstruction::allowsLiquids(of("squid")));
    CHECK(naturalSpawnObstruction::allowsLiquids(of("drowned")));
    // parity: the one land mob that spawns in lava.
    CHECK(naturalSpawnObstruction::allowsLiquids(of("strider")));
    CHECK_FALSE(naturalSpawnObstruction::allowsLiquids(of("zombie")));
    CHECK_FALSE(naturalSpawnObstruction::allowsLiquids(of("cow")));
}

TEST_CASE("the two questions a spawn asks about what is already standing there") {
    const Aabb spawnBox = naturalSpawnObstruction::spawnBox(of("zombie"), 0.5, 64.0, 0.5);
    const Aabb entityBox = naturalSpawnObstruction::entityBox(of("zombie"), 0.5, 64.0, 0.5);

    // A cow standing in the same place blocks building, so it is in the way of the entity box.
    const std::unique_ptr<ServerEntity> cow = spawn("cow", 1, 0.5, 0.5);
    CHECK(naturalSpawnObstruction::blocksBuilding(cow->type()));
    CHECK_FALSE(naturalSpawnObstruction::canBeCollidedWith(cow->type()));
    CHECK(naturalSpawnObstruction::entityBlocksSpawn(*cow, spawnBox, entityBox));

    // An arrow is neither, so it is in nobody's way.
    const std::unique_ptr<ServerEntity> arrow = spawn("arrow", 2, 0.5, 0.5);
    CHECK_FALSE(naturalSpawnObstruction::blocksBuilding(arrow->type()));
    CHECK_FALSE(naturalSpawnObstruction::entityBlocksSpawn(*arrow, spawnBox, entityBox));

    // A boat is both, which is why it is the family the spawn box asks about.
    const std::unique_ptr<ServerEntity> boat = spawn("oak_boat", 3, 0.5, 0.5);
    CHECK(naturalSpawnObstruction::canBeCollidedWith(boat->type()));
    CHECK(naturalSpawnObstruction::blocksBuilding(boat->type()));
    CHECK(naturalSpawnObstruction::entityBlocksSpawn(*boat, spawnBox, entityBox));

    // Something already removed is not standing anywhere.
    cow->removed = true;
    CHECK_FALSE(naturalSpawnObstruction::entityBlocksSpawn(*cow, spawnBox, entityBox));

    // And one far away is not in the way either.
    const std::unique_ptr<ServerEntity> far = spawn("cow", 4, 40.5, 40.5);
    CHECK_FALSE(naturalSpawnObstruction::entityBlocksSpawn(*far, spawnBox, entityBox));
}

TEST_CASE("a cube asks about a box four times its own, which is what makes the two questions two") {
    // parity: `EntityType.getSpawnAABB`, whose builder scales the cube mobs and nothing else.
    const Aabb cubeSpawn = naturalSpawnObstruction::spawnBox(of("slime"), 0.5, 64.0, 0.5);
    const Aabb cubeBox = naturalSpawnObstruction::entityBox(of("slime"), 0.5, 64.0, 0.5);
    CHECK(cubeSpawn.maxY - cubeSpawn.minY == doctest::Approx(4.0 * (cubeBox.maxY - cubeBox.minY)));
    CHECK(cubeSpawn.maxX - cubeSpawn.minX == doctest::Approx(4.0 * (cubeBox.maxX - cubeBox.minX)));
    // Everything else asks about its own box.
    CHECK(naturalSpawnObstruction::spawnBox(of("zombie"), 0.5, 64.0, 0.5) ==
          naturalSpawnObstruction::entityBox(of("zombie"), 0.5, 64.0, 0.5));

    // A boat between the two boxes: outside the cube itself, inside the room it needs. Only the
    // half that asks about the spawn box refuses it, which is why there are two halves.
    const std::unique_ptr<ServerEntity> boat = spawn("oak_boat", 1, 2.0, 0.5);
    CHECK_FALSE(Aabb::of(boat->x, boat->y, boat->z, boat->width(), boat->height()).intersects(cubeBox));
    CHECK(Aabb::of(boat->x, boat->y, boat->z, boat->width(), boat->height()).intersects(cubeSpawn));
    CHECK(naturalSpawnObstruction::entityBlocksSpawn(*boat, cubeSpawn, cubeBox));
}

TEST_CASE("whether the blocks leave room for a mob") {
    const std::int32_t stone = registry::blocks::stone().stateId;
    const std::int32_t air = registry::blocks::air().stateId;
    const std::int32_t water = registry::blocks::water().stateId;

    // Nothing loaded is not a refusal: the spawner asks about places the world has not caught up
    // with, and refusing there would be refusing at random.
    const naturalSpawnObstruction::BlockAt nothing = [](std::int32_t, std::int32_t, std::int32_t) {
        return std::optional<std::int32_t>{};
    };
    CHECK(naturalSpawnObstruction::blocksAllow(of("zombie"), 0.5, 64.0, 0.5, nothing));

    const naturalSpawnObstruction::BlockAt open = [air](std::int32_t, std::int32_t, std::int32_t) {
        return std::optional<std::int32_t>{air};
    };
    CHECK(naturalSpawnObstruction::blocksAllow(of("zombie"), 0.5, 64.0, 0.5, open));

    // Solid stone where the mob would stand is a refusal.
    const naturalSpawnObstruction::BlockAt solid = [stone](std::int32_t, std::int32_t, std::int32_t) {
        return std::optional<std::int32_t>{stone};
    };
    CHECK_FALSE(naturalSpawnObstruction::blocksAllow(of("zombie"), 0.5, 64.0, 0.5, solid));

    // Water refuses a zombie and welcomes a cod.
    const naturalSpawnObstruction::BlockAt flooded = [water](std::int32_t, std::int32_t, std::int32_t) {
        return std::optional<std::int32_t>{water};
    };
    CHECK_FALSE(naturalSpawnObstruction::blocksAllow(of("zombie"), 0.5, 64.0, 0.5, flooded));
    CHECK(naturalSpawnObstruction::blocksAllow(of("cod"), 0.5, 64.0, 0.5, flooded));
}

TEST_CASE("the tnt that was lit waits one tick for an entity id") {
    WorldMutex mutex;
    PrimedTntQueue queue{mutex};
    CHECK(queue.takePrimedTnt().empty());

    queue.primeTnt(1, 64, 2);
    queue.primeTnt(3, 65, 4);
    const std::vector<std::array<std::int32_t, 3>> lit = queue.takePrimedTnt();
    REQUIRE(lit.size() == 2);
    CHECK(lit[0] == std::array<std::int32_t, 3>{1, 64, 2});
    CHECK(lit[1] == std::array<std::int32_t, 3>{3, 65, 4});
    // Taking empties it, so the same block does not go off twice.
    CHECK(queue.takePrimedTnt().empty());
}

TEST_CASE("the sensors a footstep can reach") {
    WorldMutex mutex;
    SculkSensorTracker sensors{mutex};
    const std::int32_t sensor = registry::blocks::named("sculk_sensor")->stateId;
    const std::int32_t stone = registry::blocks::stone().stateId;

    CHECK_FALSE(sensors.hasNear(0.0, 64.0, 0.0, 8));

    sensors.remember(10, 64, 10, sensor);
    CHECK(sensors.size() == 1);
    CHECK(sensors.hasNear(10.0, 64.0, 10.0, 8));
    CHECK(sensors.hasNear(3.0, 64.0, 3.0, 8));
    // The range is a cube, as vanilla's own listener range is.
    CHECK_FALSE(sensors.hasNear(0.0, 64.0, 0.0, 8));
    CHECK(sensors.hasNear(0.0, 64.0, 0.0, 10));

    // Height counts as much as the two other axes: a sensor in the ceiling is not underfoot.
    CHECK_FALSE(sensors.hasNear(10.0, 40.0, 10.0, 8));
    CHECK(sensors.hasNear(10.0, 58.0, 10.0, 8));

    // Remembering the same place twice is remembering it once.
    sensors.remember(10, 64, 10, sensor);
    CHECK(sensors.size() == 1);

    // Breaking it takes it out again, which is what a set keyed by position is for.
    sensors.remember(10, 64, 10, stone);
    CHECK(sensors.size() == 0);
    CHECK_FALSE(sensors.hasNear(10.0, 64.0, 10.0, 8));

    // parity: the calibrated one listens too, and is a different block.
    sensors.remember(20, 64, 20, registry::blocks::named("calibrated_sculk_sensor")->stateId);
    CHECK(sensors.size() == 1);
    CHECK(sensors.hasNear(20.0, 64.0, 20.0, 1));
    sensors.remember(20, 64, 20, stone);
    CHECK(sensors.size() == 0);

    // Negative coordinates survive the packing.
    sensors.remember(-40, -12, -1000, sensor);
    CHECK(sensors.hasNear(-40.0, -12.0, -1000.0, 1));
    CHECK_FALSE(sensors.hasNear(-40.0, -12.0, -900.0, 1));
}

TEST_CASE("what the world knows about the entities in it") {
    WorldMutex mutex;
    EntityRegistry registry{mutex};
    CHECK(registry.entities().empty());
    CHECK(registry.entity(1) == nullptr);

    registry.spawnEntity(spawn("cow", 1, 0.5, 0.5));
    registry.spawnEntity(spawn("zombie", 2, 40.5, 0.5));
    registry.spawnEntity(spawn("cow", 3, 0.5, 40.5));
    CHECK(registry.entities().size() == 3);
    REQUIRE(registry.entity(2) != nullptr);
    CHECK(registry.entity(2)->type().path() == "zombie");

    // Which chunk each one stands in, which is what a save asks.
    CHECK(registry.entitiesIn(0, 0).size() == 1);
    CHECK(registry.entitiesIn(2, 0).size() == 1);
    CHECK(registry.entitiesIn(9, 9).empty());
    CHECK(registry.chunksWithEntities().size() == 3);

    // A cube's size decides its attributes the moment it is spawned.
    std::unique_ptr<ServerEntity> slime = spawn("slime", 4, 0.5, 0.5);
    slime->cubeSize = 4;
    const ServerEntity& spawned = registry.spawnEntity(std::move(slime));
    CHECK(spawned.health == 16.0);

    // Removing is two steps: marked now, forgotten when the sweep says so, and reported once.
    registry.entity(1)->removed = true;
    CHECK(registry.entities().size() == 4);
    CHECK(registry.chunksWithEntities().size() == 3);
    const std::vector<std::int32_t> gone = registry.collectRemovedEntities();
    CHECK(gone == std::vector<std::int32_t>{1});
    CHECK(registry.entities().size() == 3);
    CHECK(registry.collectRemovedEntities().empty());

    // Something removed is not standing in a chunk any more, even before the sweep drops it.
    registry.entity(2)->removed = true;
    CHECK(registry.entitiesIn(2, 0).empty());
    CHECK(registry.chunksWithEntities().size() == 2);

    // Two entities in one chunk are one chunk.
    registry.spawnEntity(spawn("cow", 5, 1.5, 1.5));
    CHECK(registry.entitiesIn(0, 0).size() == 2);
    CHECK(registry.chunksWithEntities().size() == 2);

    // A chunk going out takes its entities with it, removed or not.
    registry.forgetEntitiesIn(0, 0);
    CHECK(registry.entity(4) == nullptr);
    CHECK(registry.entity(5) == nullptr);
    CHECK(registry.entities().size() == 2);
}

TEST_CASE("what a loaded chunk brings back waits for the tick loop") {
    WorldMutex mutex;
    EntityRegistry registry{mutex};
    CHECK(registry.takePendingEntities().empty());

    basalt::wire::nbt::NbtCompound saved;
    saved.putString("id", "minecraft:cow");
    registry.addPendingEntities({saved});
    const std::vector<basalt::wire::nbt::NbtCompound> taken = registry.takePendingEntities();
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].getString("id") == "minecraft:cow");
    // Taken once: an entity id belongs to the server, and it is only handed out once.
    CHECK(registry.takePendingEntities().empty());
}

TEST_CASE("an entity already standing there is in the way of a spawn") {
    WorldMutex mutex;
    EntityRegistry registry{mutex};
    const Aabb spawnBox = naturalSpawnObstruction::spawnBox(of("zombie"), 0.5, 64.0, 0.5);
    const Aabb entityBox = naturalSpawnObstruction::entityBox(of("zombie"), 0.5, 64.0, 0.5);
    CHECK_FALSE(registry.hasNaturalSpawnObstruction(spawnBox, entityBox));

    registry.spawnEntity(spawn("cow", 1, 40.5, 40.5));
    CHECK_FALSE(registry.hasNaturalSpawnObstruction(spawnBox, entityBox));

    registry.spawnEntity(spawn("cow", 2, 0.5, 0.5));
    CHECK(registry.hasNaturalSpawnObstruction(spawnBox, entityBox));
}
