#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DroppedItemPhysics.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

/// A floor at y = 63 made of whatever a test names, and air above it.
[[nodiscard]] basalt::server::runtime::entity::entityCollision::BlockAt floorOf(std::int32_t stateId) {
    return [stateId](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
        return y <= 63 ? std::optional<std::int32_t>{stateId}
                       : std::optional<std::int32_t>{registry::blocks::air().stateId};
    };
}

[[nodiscard]] DroppedItem drop(std::int32_t id, std::string item, std::int32_t count, double x, double y, double z) {
    return DroppedItem{.entityId = id,
                       .uuid = Uuid{1, static_cast<std::uint64_t>(id)},
                       .item = Identifier::vanilla(std::move(item)),
                       .count = count,
                       .x = x,
                       .y = y,
                       .z = z};
}

} // namespace

TEST_CASE("an item falls, lands and settles") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "stone", 1, 0.5, 68.0, 0.5));

    // parity: gravity before the move, and the air's own drag after it. One tick is a twenty-fifth
    // of a block, and the second is that plus what the first one kept.
    static_cast<void>(items.tickItems());
    CHECK(items.items()[0].y == doctest::Approx(67.96).epsilon(1e-9));
    CHECK(items.items()[0].velocityY == doctest::Approx(-0.0392).epsilon(1e-9));
    static_cast<void>(items.tickItems());
    CHECK(items.items()[0].y == doctest::Approx(67.8808).epsilon(1e-9));

    // Within a couple of seconds it is on the floor and stopped.
    for (std::int32_t tick = 0; tick < 60; ++tick) {
        CHECK(items.tickItems().empty());
    }
    const DroppedItem settled = items.items()[0];
    CHECK(settled.y == doctest::Approx(64.0));
    CHECK(settled.resting);
    CHECK(settled.velocityY == 0.0);
}

TEST_CASE("an item lands on slime and keeps moving for a while") {
    WorldMutex mutex;
    DroppedItemPhysics stone{mutex, floorOf(stateOf("stone"))};
    WorldMutex slimeMutex;
    DroppedItemPhysics slime{slimeMutex, floorOf(stateOf("slime_block"))};
    stone.spawnItem(drop(1, "stone", 1, 0.5, 68.0, 0.5));
    slime.spawnItem(drop(1, "stone", 1, 0.5, 68.0, 0.5));

    // parity: `Entity.getBlockBounciness`, four fifths of it for something that is not alive.
    std::int32_t stoneTicks = 0;
    while (!stone.items()[0].resting && stoneTicks < 2000) {
        static_cast<void>(stone.tickItems());
        ++stoneTicks;
    }
    CHECK(stone.items()[0].resting);
    CHECK(stoneTicks < 100);

    // parity: the half damping applies only while the item is still moving down, and after a
    // bounce it is moving up, so nothing takes the bounce away: an item on slime keeps bouncing,
    // and never earns the settled flag that would stop the server sending its movement.
    for (std::int32_t tick = 0; tick < 2000; ++tick) {
        static_cast<void>(slime.tickItems());
    }
    CHECK_FALSE(slime.items()[0].resting);
    CHECK(slime.items()[0].velocityY != 0.0);
}

TEST_CASE("ice is slippery and stone is not") {
    WorldMutex stoneMutex;
    DroppedItemPhysics stone{stoneMutex, floorOf(stateOf("stone"))};
    WorldMutex iceMutex;
    DroppedItemPhysics ice{iceMutex, floorOf(stateOf("ice"))};
    DroppedItem sliding = drop(1, "stone", 1, 0.5, 64.0, 0.5);
    sliding.velocityX = 0.5;
    stone.spawnItem(sliding);
    ice.spawnItem(sliding);

    for (std::int32_t tick = 0; tick < 10; ++tick) {
        static_cast<void>(stone.tickItems());
        static_cast<void>(ice.tickItems());
    }
    // parity: the friction of the block underneath, on top of the air's own drag.
    CHECK(ice.items()[0].x > stone.items()[0].x);
}

TEST_CASE("an item is forgotten after five minutes") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "stone", 1, 0.5, 64.0, 0.5));

    std::vector<std::int32_t> despawned;
    for (std::int32_t tick = 0; tick < kItemLifetimeTicks; ++tick) {
        const std::vector<std::int32_t> gone = items.tickItems();
        if (!gone.empty()) {
            despawned = gone;
        }
    }
    CHECK(despawned == std::vector<std::int32_t>{1});
    CHECK(items.items().empty());
}

TEST_CASE("a toss cannot be picked up straight away") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "stone", 1, 0.5, 64.0, 0.5));
    CHECK_FALSE(items.items()[0].canBePickedUp());
    CHECK(items.items()[0].pickupDelay == kDefaultPickupDelay);

    for (std::int32_t tick = 0; tick < kDefaultPickupDelay; ++tick) {
        static_cast<void>(items.tickItems());
    }
    CHECK(items.items()[0].canBePickedUp());
    CHECK(items.items()[0].pickupDelay == 0);
}

TEST_CASE("two stacks of the same thing become one") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "oak_log", 3, 0.5, 64.0, 0.5));
    items.spawnItem(drop(2, "oak_log", 4, 0.6, 64.0, 0.5));

    // Both settle on the first tick, and a settled item is only asked to merge every two seconds.
    std::vector<std::int32_t> gone;
    for (std::int32_t tick = 0; tick < 45 && gone.empty(); ++tick) {
        gone = items.tickItems();
    }
    CHECK(gone == std::vector<std::int32_t>{2});
    REQUIRE(items.items().size() == 1);
    CHECK(items.items()[0].entityId == 1);
    CHECK(items.items()[0].count == 7);
    CHECK(items.items()[0].countChanged);
}

TEST_CASE("what does not merge") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    // A different item, one too far away sideways, one too far above, and two that would not fit
    // in one stack.
    items.spawnItem(drop(1, "oak_log", 3, 0.5, 64.0, 0.5));
    items.spawnItem(drop(2, "stone", 3, 0.5, 64.0, 0.5));
    items.spawnItem(drop(3, "oak_log", 3, 1.5, 64.0, 0.5));
    DroppedItem above = drop(4, "oak_log", 3, 0.5, 64.4, 0.5);
    above.resting = true;
    items.spawnItem(above);
    items.spawnItem(drop(5, "oak_log", 62, 0.55, 64.0, 0.5));

    for (std::int32_t tick = 0; tick < 90; ++tick) {
        static_cast<void>(items.tickItems());
    }
    // Five went in and five are still there, however many ticks they spend beside each other.
    CHECK(items.items().size() == 5);
    for (const DroppedItem& item : items.items()) {
        CAPTURE(item.entityId);
        CHECK_FALSE(item.countChanged);
    }
}

TEST_CASE("a merged stack keeps the younger age and the longer wait") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    DroppedItem old = drop(1, "oak_log", 1, 0.5, 64.0, 0.5);
    old.age = 100;
    old.pickupDelay = 0;
    old.resting = true;
    DroppedItem fresh = drop(2, "oak_log", 1, 0.6, 64.0, 0.5);
    fresh.age = 39;
    fresh.pickupDelay = 20;
    fresh.resting = true;
    items.spawnItem(old);
    items.spawnItem(fresh);

    // Both are settled, so the merge is asked for every two seconds rather than every other tick:
    // the younger one reaches a multiple of forty first and is the one that absorbs the other.
    std::vector<std::int32_t> gone;
    for (std::int32_t tick = 0; tick < 45 && gone.empty(); ++tick) {
        gone = items.tickItems();
    }
    CHECK(gone == std::vector<std::int32_t>{1});
    REQUIRE(items.items().size() == 1);
    const DroppedItem merged = items.items()[0];
    CHECK(merged.entityId == 2);
    CHECK(merged.count == 2);
    // parity: the younger age, so a merge does not shorten what is left of either lifetime, and
    // the longer delay, so a merge is not a way to pick something up early.
    CHECK(merged.age == 40);
    CHECK(merged.pickupDelay == 19);
}

TEST_CASE("a stack that carries something of its own does not merge") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    DroppedItem plain = drop(1, "shulker_box", 1, 0.5, 64.0, 0.5);
    DroppedItem full = drop(2, "shulker_box", 1, 0.6, 64.0, 0.5);
    full.components.putString("id", "minecraft:shulker_box");
    items.spawnItem(plain);
    items.spawnItem(full);

    for (std::int32_t tick = 0; tick < 90; ++tick) {
        CHECK(items.tickItems().empty());
    }
    // A box with something in it is not the same stack as an empty one, whatever the name says.
    CHECK(items.items().size() == 2);
}

TEST_CASE("a settled pair waits two seconds to merge, not two ticks") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "oak_log", 3, 0.5, 64.0, 0.5));
    items.spawnItem(drop(2, "oak_log", 4, 0.6, 64.0, 0.5));

    // parity: `mergeWithNeighbours` is asked every other tick while an item moves and every forty
    // once it has settled, which is what keeps a floor of drops off the tick budget.
    for (std::int32_t tick = 0; tick < 39; ++tick) {
        CHECK(items.tickItems().empty());
    }
    CHECK(items.items().size() == 2);
    CHECK(items.tickItems() == std::vector<std::int32_t>{2});
}

TEST_CASE("what stops an item sideways, and what a ceiling does not do") {
    WorldMutex mutex;
    // A floor, and a wall east of x = 1.
    DroppedItemPhysics items{mutex, [](std::int32_t x, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                                 const registry::BlockState* stone = registry::blocks::named("stone");
                                 if (y <= 63 || (x >= 1 && y <= 66)) {
                                     return std::optional<std::int32_t>{stone->stateId};
                                 }
                                 return std::optional<std::int32_t>{registry::blocks::air().stateId};
                             }};
    DroppedItem sliding = drop(1, "stone", 1, 0.5, 64.0, 0.5);
    sliding.velocityX = 0.5;
    items.spawnItem(sliding);

    static_cast<void>(items.tickItems());
    // It stopped against the wall, and what it had left sideways is gone rather than kept for the
    // next tick to push it into the wall again.
    CHECK(items.items()[0].velocityX == 0.0);
    CHECK(items.items()[0].x < 1.0);
}

TEST_CASE("an item that hits a ceiling has not landed") {
    WorldMutex mutex;
    // Air below, a ceiling at y = 70: nothing to stand on, so nothing is ever on the ground.
    DroppedItemPhysics items{mutex, [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                                 return y >= 70 ? std::optional<std::int32_t>{registry::blocks::named("stone")->stateId}
                                                : std::optional<std::int32_t>{registry::blocks::air().stateId};
                             }};
    DroppedItem rising = drop(1, "stone", 1, 0.5, 69.5, 0.5);
    rising.velocityX = 0.5;
    rising.velocityY = 1.0;
    items.spawnItem(rising);

    static_cast<void>(items.tickItems());
    // The head stopped and the rise was cut short, but it is falling, not standing: the friction
    // of a block it is nowhere near must not be applied to it.
    CHECK(items.items()[0].velocityY == 0.0);
    CHECK(items.items()[0].velocityX == doctest::Approx(0.5 * 0.98));
    CHECK_FALSE(items.items()[0].resting);
}

TEST_CASE("what is nearby, and what is picked up") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    // Dropped furthest first, so the order that comes back is the distance and not the order they
    // were dropped in.
    items.spawnItem(drop(1, "stone", 1, 3.5, 64.0, 0.5));
    items.spawnItem(drop(2, "stone", 1, 0.5, 64.0, 0.5));
    items.spawnItem(drop(3, "stone", 1, 40.5, 64.0, 0.5));

    const std::vector<DroppedItem> near = items.nearbyItems(0.0, 64.0, 0.0, 5.0);
    REQUIRE(near.size() == 2);
    // Nearest first, which is the order a player picks them up in.
    CHECK(near[0].entityId == 2);
    CHECK(near[1].entityId == 1);
    CHECK(items.nearbyItems(0.0, 64.0, 0.0, 0.1).empty());

    const std::optional<DroppedItem> taken = items.removeItem(1);
    REQUIRE(taken.has_value());
    CHECK(taken->entityId == 1);
    CHECK(items.items().size() == 2);
    // And picking up what somebody else already took is not an error.
    CHECK_FALSE(items.removeItem(1).has_value());
}

TEST_CASE("an item outside the region being ticked is left where it is") {
    WorldMutex mutex;
    DroppedItemPhysics items{mutex, floorOf(stateOf("stone"))};
    items.spawnItem(drop(1, "stone", 1, 0.5, 68.0, 0.5));
    items.spawnItem(drop(2, "stone", 1, 5000.5, 68.0, 0.5));

    basalt::server::runtime::sched::RegionScheduler scheduler{true};
    basalt::server::runtime::sched::Region& region = scheduler.regionFor("world");
    // Split the world so the region owns the first item's ground and not the second's.
    scheduler.repartition("world", std::array<std::int64_t, 1>{0});
    REQUIRE_FALSE(region.sections().empty());
    REQUIRE(region.ownsPosition(0.5, 0.5));
    REQUIRE_FALSE(region.ownsPosition(5000.5, 0.5));

    static_cast<void>(items.tickItems(&region));
    // An item belongs to whoever owns the ground it is lying on: the other one did not age, did
    // not fall, and is not this region's business.
    CHECK(items.items()[0].age == 1);
    CHECK(items.items()[0].y < 68.0);
    CHECK(items.items()[1].age == 0);
    CHECK(items.items()[1].y == 68.0);

    // With the world in one piece there is no such thing as somebody else's item.
    basalt::server::runtime::sched::RegionScheduler whole;
    basalt::server::runtime::sched::Region& everything = whole.regionFor("world");
    static_cast<void>(items.tickItems(&everything));
    CHECK(items.items()[1].age == 1);
}
