#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/ChunkTracker.hpp>

#include <doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

using namespace basalt::server::world;

namespace {

[[nodiscard]] std::string render(const std::vector<ChunkPos>& positions) {
    std::string out;
    for (const ChunkPos& position : positions) {
        if (!out.empty()) {
            out += ";";
        }
        out += std::to_string(position.x) + "," + std::to_string(position.z);
    }
    return out;
}

} // namespace

TEST_CASE("the view is vanilla's cylinder, one ring of neighbours past the distance") {
    // parity: `ChunkTrackingView.isWithinDistance` with the neighbours: every axis less two,
    // squared, strictly under the distance squared. At two that is the whole seven by seven.
    ChunkTracker tracker;
    CHECK(tracker.update(0, 0, 2).toLoad.size() == 49);
    tracker.reset();
    const ChunkTracker::Delta ten = tracker.update(0, 0, 10);
    CHECK(ten.toLoad.size() == 473);
    CHECK(ChunkTracker::isWithinDistance(0, 0, 10, 11, 0, true));
    CHECK_FALSE(ChunkTracker::isWithinDistance(0, 0, 10, 12, 0, true));
    CHECK(ChunkTracker::isWithinDistance(0, 0, 10, 11, 6, true));
    CHECK_FALSE(ChunkTracker::isWithinDistance(0, 0, 10, 11, 7, true));
    CHECK_FALSE(ChunkTracker::isWithinDistance(0, 0, 10, 11, 11, true));
    // Without the neighbours the client's own view is one ring smaller.
    CHECK(ChunkTracker::isWithinDistance(0, 0, 10, 10, 0, false));
    CHECK_FALSE(ChunkTracker::isWithinDistance(0, 0, 10, 11, 0, false));

    // A step east brings in as many columns as it drops, and no square corners.
    const ChunkTracker::Delta moved = tracker.update(1, 0, 10);
    CHECK(moved.toLoad.size() == 23);
    CHECK(moved.toUnload.size() == 23);
    CHECK(render(moved.toLoad).find("12,11") == std::string::npos);
    CHECK(tracker.loaded().size() == 473);
}

TEST_CASE("a negative radius is refused") {
    ChunkTracker tracker;
    CHECK_THROWS_AS(static_cast<void>(tracker.update(0, 0, -1)), std::invalid_argument);
}
