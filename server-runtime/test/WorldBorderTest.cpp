#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/WorldBorder.hpp>

#include <doctest.h>

using namespace basalt::server::runtime::world;
namespace play = basalt::wire::protocol::play;

TEST_CASE("a fresh border is the whole world, centred on the origin") {
    const WorldBorder border;
    CHECK(border.centerX() == doctest::Approx(0.0));
    CHECK(border.centerZ() == doctest::Approx(0.0));
    CHECK(border.size() == doctest::Approx(WorldBorder::kDefaultSize));
    CHECK(border.targetSize() == doctest::Approx(WorldBorder::kDefaultSize));
    CHECK(border.lerpTimeTicks() == 0);
    CHECK(border.warningBlocks == 5);
    CHECK(border.warningSeconds == 15);
    CHECK(border.damageBuffer == doctest::Approx(5.0));
    CHECK(border.damagePerBlock == doctest::Approx(0.2));
    // parity: the default is a shade under the maximum, which is a shade under the coordinate cap.
    CHECK(WorldBorder::kDefaultSize < WorldBorder::kMaxSize);
    CHECK(WorldBorder::kMaxCenterCoordinate < WorldBorder::kMaxSize);
}

TEST_CASE("setting a size is immediate, moving one is not") {
    WorldBorder border;
    border.setSize(100.0);
    CHECK(border.size() == doctest::Approx(100.0));
    CHECK(border.targetSize() == doctest::Approx(100.0));
    CHECK(border.lerpTimeTicks() == 0);

    // parity: while it moves, the border reports the size it started from and the one it is going
    // to, which is what lets the client draw the shrink itself.
    border.moveSize(50.0, 200);
    CHECK(border.size() == doctest::Approx(100.0));
    CHECK(border.targetSize() == doctest::Approx(50.0));
    CHECK(border.lerpTimeTicks() == 200);

    border.settle();
    CHECK(border.size() == doctest::Approx(50.0));
    CHECK(border.targetSize() == doctest::Approx(50.0));
    CHECK(border.lerpTimeTicks() == 0);

    // A new `set` in the middle of a move cancels it.
    border.moveSize(500.0, 1000);
    border.setSize(75.0);
    CHECK(border.size() == doctest::Approx(75.0));
    CHECK(border.targetSize() == doctest::Approx(75.0));
    CHECK(border.lerpTimeTicks() == 0);
}

TEST_CASE("how far outside a position is") {
    WorldBorder border;
    border.setSize(100.0);
    border.center(0.0, 0.0);

    // Inside is zero, however near the edge.
    CHECK(border.distanceOutside(0.0, 0.0) == doctest::Approx(0.0));
    CHECK(border.distanceOutside(49.9, 49.9) == doctest::Approx(0.0));
    CHECK(border.distanceOutside(-50.0, 0.0) == doctest::Approx(0.0));

    // A square border, so the distance is the larger of the two axes, not the diagonal.
    CHECK(border.distanceOutside(60.0, 0.0) == doctest::Approx(10.0));
    CHECK(border.distanceOutside(0.0, -60.0) == doctest::Approx(10.0));
    CHECK(border.distanceOutside(60.0, 55.0) == doctest::Approx(10.0));
    CHECK(border.distanceOutside(-70.0, 60.0) == doctest::Approx(20.0));

    // Measured from where it is centred, not from the origin.
    border.center(1000.0, -1000.0);
    CHECK(border.distanceOutside(1000.0, -1000.0) == doctest::Approx(0.0));
    CHECK(border.distanceOutside(1060.0, -1000.0) == doctest::Approx(10.0));
    CHECK(border.distanceOutside(0.0, 0.0) > 900.0);

    // A border on the move measures against where it is going, which is what pushes a player in
    // ahead of the shrink rather than after it.
    border.center(0.0, 0.0);
    border.moveSize(20.0, 100);
    CHECK(border.distanceOutside(40.0, 0.0) == doctest::Approx(30.0));
}

TEST_CASE("the buffer outside the border where nothing happens yet") {
    WorldBorder border;
    border.setSize(100.0);

    // Inside, and inside the five-block buffer: no damage at all.
    CHECK(border.damageAt(0.0, 0.0) == doctest::Approx(0.0F));
    CHECK(border.damageAt(52.0, 0.0) == doctest::Approx(0.0F));
    CHECK(border.damageAt(55.0, 0.0) == doctest::Approx(0.0F));

    // Past it, a fifth of a heart per block, with a floor of one so the first blocks still hurt.
    CHECK(border.damageAt(56.0, 0.0) == doctest::Approx(1.0F));
    CHECK(border.damageAt(60.0, 0.0) == doctest::Approx(1.0F));
    // Ten blocks past the buffer is two, which is where the floor stops deciding.
    CHECK(border.damageAt(65.0, 0.0) == doctest::Approx(2.0F));
    CHECK(border.damageAt(105.0, 0.0) == doctest::Approx(10.0F));

    // The numbers an operator set are the numbers used.
    border.damageBuffer = 0.0;
    border.damagePerBlock = 2.0;
    CHECK(border.damageAt(51.0, 0.0) == doctest::Approx(2.0F));
    CHECK(border.damageAt(50.0, 0.0) == doctest::Approx(0.0F));
}

TEST_CASE("what a joining client is told") {
    WorldBorder border;
    border.center(12.0, -34.0);
    border.setSize(200.0);
    border.moveSize(100.0, 400);
    border.warningBlocks = 7;
    border.warningSeconds = 21;

    const play::InitializeBorderPacket packet = border.initializePacket();
    CHECK(packet.centerX == doctest::Approx(12.0));
    CHECK(packet.centerZ == doctest::Approx(-34.0));
    // Where it is and where it is going, in that order: swapping them makes the client draw the
    // shrink backwards.
    CHECK(packet.oldSize == doctest::Approx(200.0));
    CHECK(packet.newSize == doctest::Approx(100.0));
    CHECK(packet.lerpTimeTicks == 400);
    CHECK(packet.warningBlocks == 7);
    CHECK(packet.warningSeconds == 21);
    // parity: vanilla always sends its own absolute maximum here, not this border's size.
    CHECK(packet.absoluteMaxSize == play::InitializeBorderPacket::kAbsoluteMaxSize);
}
