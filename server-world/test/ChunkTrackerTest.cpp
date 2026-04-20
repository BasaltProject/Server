#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/ChunkTracker.hpp>

#include <doctest.h>

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
    // parity: ChunkTrackingView.isWithinDistance with the neighbours.
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
    CHECK(ChunkTracker::isWithinDistance(0, 0, 10, 10, 0, false));
    CHECK_FALSE(ChunkTracker::isWithinDistance(0, 0, 10, 11, 0, false));

    const ChunkTracker::Delta moved = tracker.update(1, 0, 10);
    CHECK(moved.toLoad.size() == 23);
    CHECK(moved.toUnload.size() == 23);
    CHECK(render(moved.toLoad).find("12,11") == std::string::npos);
    CHECK(tracker.loaded().size() == 473);
}
