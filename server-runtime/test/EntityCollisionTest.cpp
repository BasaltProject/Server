#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::entity;
namespace registry = basalt::server::registry;

namespace {

/// A world made of the blocks a test names, and air everywhere else.
class Blocks {
public:
    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        blocks_[std::array<std::int32_t, 3>{x, y, z}] = stateId;
    }

    void fill(std::int32_t minX, std::int32_t minY, std::int32_t minZ, std::int32_t maxX, std::int32_t maxY,
              std::int32_t maxZ, std::int32_t stateId) {
        for (std::int32_t x = minX; x <= maxX; ++x) {
            for (std::int32_t y = minY; y <= maxY; ++y) {
                for (std::int32_t z = minZ; z <= maxZ; ++z) {
                    put(x, y, z, stateId);
                }
            }
        }
    }

    [[nodiscard]] entityCollision::BlockAt accessor() const {
        return [this](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
            const auto found = blocks_.find(std::array<std::int32_t, 3>{x, y, z});
            return found == blocks_.end() ? std::optional<std::int32_t>{registry::blocks::air().stateId}
                                          : std::optional<std::int32_t>{found->second};
        };
    }

    /// A world that answers nothing at all, which is what an unloaded chunk looks like.
    [[nodiscard]] static entityCollision::BlockAt unloaded() {
        return [](std::int32_t, std::int32_t, std::int32_t) { return std::optional<std::int32_t>{}; };
    }

private:
    std::map<std::array<std::int32_t, 3>, std::int32_t> blocks_;
};

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

/// A player-sized box standing at these feet coordinates.
[[nodiscard]] Aabb standing(double x, double y, double z) { return Aabb::of(x, y, z, 0.6, 1.8); }

} // namespace

TEST_CASE("nothing in the way is the whole movement") {
    const Blocks world;
    const std::array<double, 3> moved =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.3, -0.1, 0.2);
    CHECK(moved[0] == doctest::Approx(0.3));
    CHECK(moved[1] == doctest::Approx(-0.1));
    CHECK(moved[2] == doctest::Approx(0.2));

    // And no movement at all is answered without asking the world anything.
    const std::array<double, 3> still =
        entityCollision::collide(Blocks::unloaded(), standing(0.5, 64.0, 0.5), 0.0, 0.0, 0.0);
    CHECK(still[0] == 0.0);
    CHECK(still[1] == 0.0);
    CHECK(still[2] == 0.0);
}

TEST_CASE("a floor stops a fall exactly at its surface") {
    Blocks world;
    world.fill(-2, 63, -2, 2, 63, 2, stateOf("stone"));

    // Falling from just above the floor: it lands on top of it, not inside it.
    const std::array<double, 3> landed =
        entityCollision::collide(world.accessor(), standing(0.5, 64.5, 0.5), 0.0, -1.0, 0.0);
    CHECK(landed[1] == doctest::Approx(-0.5));

    // Already standing on it: it cannot go down at all.
    const std::array<double, 3> resting =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.0, -1.0, 0.0);
    CHECK(resting[1] == 0.0);

    // And walking along it is not a collision: sliding on a floor is what a floor is for.
    const std::array<double, 3> walked =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.4, 0.0, 0.0);
    CHECK(walked[0] == doctest::Approx(0.4));
}

TEST_CASE("a wall stops the axis it is on and leaves the others alone") {
    Blocks world;
    world.fill(-2, 63, -2, 2, 63, 2, stateOf("stone"));
    world.fill(2, 64, -2, 2, 66, 2, stateOf("stone"));

    // Walking east into the wall at x = 2: it stops with its own half-width to spare.
    const std::array<double, 3> blocked =
        entityCollision::collide(world.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.0);
    CHECK(blocked[0] == doctest::Approx(0.7));
    // The other axes are untouched by a wall on this one.
    CHECK(blocked[2] == 0.0);

    const std::array<double, 3> alongside =
        entityCollision::collide(world.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.5);
    CHECK(alongside[0] == doctest::Approx(0.7));
    CHECK(alongside[2] == doctest::Approx(0.5));

    // Walking away from it is free.
    const std::array<double, 3> away =
        entityCollision::collide(world.accessor(), standing(1.0, 64.0, 0.5), -1.0, 0.0, 0.0);
    CHECK(away[0] == doctest::Approx(-1.0));
}

TEST_CASE("a short move toward a wall is a short move, not a walk to the wall") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    world.put(1, 64, 0, stateOf("stone"));

    // The wall is two tenths away and the step is one: the blocks are gathered a whole block at a
    // time, so the wall is looked at, and what comes back is still the tenth that was asked for.
    const std::array<double, 3> small =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.1, 0.0, 0.0);
    CHECK(small[0] == doctest::Approx(0.1));
}

TEST_CASE("a rise cut short by a ceiling is not a landing") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    world.put(1, 64, 0, stateOf("stone_slab"));
    world.fill(-2, 67, -2, 4, 67, 4, stateOf("stone"));

    // Jumping into the ceiling while walking over the slab: the head stops at the ceiling, the
    // rise carries the whole body clear of the slab, and nothing is stepped onto because nothing
    // is in the way any more.
    const std::array<double, 3> stepped =
        entityCollision::collideWithStep(world.accessor(), standing(2.5, 64.0, 0.5), -1.5, 1.5, 0.0, 0.6, true);
    CHECK(stepped[0] == doctest::Approx(-1.5));
    CHECK(stepped[1] == doctest::Approx(1.2));
}

TEST_CASE("landing happens before walking, which is what keeps a mob out of the air") {
    Blocks world;
    // A floor, and a wall one block above it. Moving down and sideways at once has to land first:
    // resolving x before y would stop it against the wall while it was still in the air.
    world.fill(-2, 63, -2, 4, 63, 2, stateOf("stone"));
    world.fill(2, 64, -2, 2, 66, 2, stateOf("stone"));

    const std::array<double, 3> moved =
        entityCollision::collide(world.accessor(), standing(1.0, 64.6, 0.5), 1.0, -0.6, 0.0);
    // It landed on the floor.
    CHECK(moved[1] == doctest::Approx(-0.6));
    // And then stopped against the wall it was going to hit anyway.
    CHECK(moved[0] == doctest::Approx(0.7));
}

TEST_CASE("a slab is walked onto and a block is not") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 2, stateOf("stone"));
    world.put(2, 64, 0, stateOf("stone_slab"));

    // Flat against the slab: stopped short of it.
    const std::array<double, 3> flat =
        entityCollision::collide(world.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.0);
    CHECK(flat[0] == doctest::Approx(0.7));

    // With a step of half a block it climbs onto it instead, and comes out further along.
    const std::array<double, 3> stepped =
        entityCollision::collideWithStep(world.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.0, 0.6, true);
    CHECK(stepped[0] > flat[0]);
    CHECK(stepped[1] == doctest::Approx(0.5));

    // A whole block is too tall for the same step.
    Blocks tall;
    tall.fill(-2, 63, -2, 4, 63, 2, stateOf("stone"));
    tall.put(2, 64, 0, stateOf("stone"));
    const std::array<double, 3> refused =
        entityCollision::collideWithStep(tall.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.0, 0.6, true);
    CHECK(refused[0] == doctest::Approx(0.7));
    CHECK(refused[1] == 0.0);

    // Nothing steps while it is in the air.
    const std::array<double, 3> airborne =
        entityCollision::collideWithStep(world.accessor(), standing(1.0, 65.0, 0.5), 1.0, 0.0, 0.0, 0.6, false);
    CHECK(airborne[0] == doctest::Approx(1.0));

    // And a step of nothing is the flat move.
    const std::array<double, 3> noStep =
        entityCollision::collideWithStep(world.accessor(), standing(1.0, 64.0, 0.5), 1.0, 0.0, 0.0, 0.0, true);
    CHECK(noStep[0] == doctest::Approx(0.7));
}

TEST_CASE("the smaller horizontal move is resolved first, which decides where a corner sends you") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    // One block, diagonally ahead: whichever axis is resolved first walks into its side, and the
    // other one is then free. parity: `Direction.axisStepOrder`, which resolves the smaller move
    // first so the larger one is the one that gets clipped.
    world.put(1, 64, 1, stateOf("stone"));

    // Moving mostly east: the north half goes first and comes out whole, and east is clipped.
    const std::array<double, 3> mostlyX =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.6, 0.0, 0.4);
    CHECK(mostlyX[0] == doctest::Approx(0.2));
    CHECK(mostlyX[2] == doctest::Approx(0.4));

    // And the mirror image, which is the same block and the same corner the other way round.
    const std::array<double, 3> mostlyZ =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.4, 0.0, 0.6);
    CHECK(mostlyZ[0] == doctest::Approx(0.4));
    CHECK(mostlyZ[2] == doctest::Approx(0.2));
}

TEST_CASE("a step is tried from the lowest height up, and only while it gets further") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    // A slab to climb and a whole block behind it: the lowest step that helps is the one taken,
    // and it is not the one that clears everything.
    world.put(1, 64, 0, stateOf("stone_slab"));
    world.put(0, 64, 0, stateOf("stone"));

    const std::array<double, 3> flat =
        entityCollision::collide(world.accessor(), standing(2.5, 64.0, 0.5), -1.5, 0.0, 0.0);
    CHECK(flat[0] == doctest::Approx(-0.2));

    // Half a block up clears the slab and stops against the block behind it: further than the flat
    // move, so it is taken, and the whole block's own height is never tried.
    const std::array<double, 3> stepped =
        entityCollision::collideWithStep(world.accessor(), standing(2.5, 64.0, 0.5), -1.5, 0.0, 0.0, 1.0, true);
    CHECK(stepped[0] == doctest::Approx(-1.2));
    CHECK(stepped[1] == doctest::Approx(0.5));
}

TEST_CASE("a step that gets nowhere is not taken, and does not lift anybody") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    world.put(1, 64, 0, stateOf("stone_slab"));
    // A ceiling low enough that the lift itself is clipped, so the step ends no further along than
    // the flat move did.
    world.fill(-2, 66, -2, 4, 66, 4, stateOf("stone"));

    const std::array<double, 3> stepped =
        entityCollision::collideWithStep(world.accessor(), standing(2.5, 64.0, 0.5), -1.5, 0.0, 0.0, 0.6, true);
    CHECK(stepped[0] == doctest::Approx(-0.2));
    // And it stays on the floor: a step that was refused is not half taken.
    CHECK(stepped[1] == 0.0);
}

TEST_CASE("a step taken while landing counts the drop it already took") {
    Blocks world;
    world.fill(-2, 63, -2, 4, 63, 4, stateOf("stone"));
    world.put(1, 64, 0, stateOf("stone_slab"));

    // Falling the last third of a block while walking into the slab: it lands, then steps from
    // where it landed, and what comes back is the drop and the step together.
    const std::array<double, 3> stepped =
        entityCollision::collideWithStep(world.accessor(), standing(2.5, 64.3, 0.5), -1.5, -0.5, 0.0, 0.6, false);
    CHECK(stepped[0] == doctest::Approx(-1.5));
    CHECK(stepped[1] == doctest::Approx(0.2));

    // In the air with nothing to land on, the same move does not step at all.
    Blocks air;
    air.put(1, 66, 0, stateOf("stone_slab"));
    const std::array<double, 3> airborne =
        entityCollision::collideWithStep(air.accessor(), standing(2.5, 66.0, 0.5), -1.5, 0.0, 0.0, 0.6, false);
    CHECK(airborne[0] == doctest::Approx(-0.2));
    CHECK(airborne[1] == 0.0);
}

TEST_CASE("what overlaps a box, and what merely touches it") {
    Blocks world;
    world.put(0, 64, 0, stateOf("stone"));

    // A box inside the block collides; one resting exactly on its face does not.
    CHECK(entityCollision::collides(world.accessor(), Aabb::of(0.5, 64.5, 0.5, 0.6, 0.6)));
    CHECK_FALSE(entityCollision::collides(world.accessor(), Aabb::of(0.5, 65.0, 0.5, 0.6, 1.8)));
    CHECK_FALSE(entityCollision::collides(world.accessor(), Aabb::of(5.5, 64.0, 5.5, 0.6, 1.8)));

    // The boxes come back in world space, one per collision shape.
    const std::vector<Aabb> boxes =
        entityCollision::collidingBoxes(world.accessor(), Aabb::of(0.5, 64.0, 0.5, 2.0, 2.0));
    REQUIRE(boxes.size() == 1);
    CHECK(boxes[0].minY == doctest::Approx(64.0));
    CHECK(boxes[0].maxY == doctest::Approx(65.0));

    // Air is not a collider, and neither is a block whose shape is empty.
    Blocks empty;
    empty.put(0, 64, 0, stateOf("short_grass"));
    CHECK_FALSE(entityCollision::collides(empty.accessor(), Aabb::of(0.5, 64.5, 0.5, 0.6, 0.6)));
}

TEST_CASE("an unloaded chunk is not a wall") {
    // Nothing loaded is nothing in the way: an entity walking into ungenerated terrain keeps its
    // movement rather than being stopped by a world that has not caught up.
    const std::array<double, 3> moved =
        entityCollision::collide(Blocks::unloaded(), standing(0.5, 64.0, 0.5), 1.0, -1.0, 1.0);
    CHECK(moved[0] == doctest::Approx(1.0));
    CHECK(moved[1] == doctest::Approx(-1.0));
    CHECK_FALSE(entityCollision::collides(Blocks::unloaded(), standing(0.5, 64.0, 0.5)));
}

TEST_CASE("a movement nobody could have asked for is refused rather than walked") {
    Blocks world;
    world.fill(-2, 63, -2, 2, 63, 2, stateOf("stone"));
    // Past the span bound the blocks are not gathered at all, so nothing stops it: a move this
    // long is a bug upstream, and scanning for it would cost more than it can be worth.
    const std::array<double, 3> huge =
        entityCollision::collide(world.accessor(), standing(0.5, 64.0, 0.5), 0.0, -100.0, 0.0);
    CHECK(huge[1] == doctest::Approx(-100.0));
}
