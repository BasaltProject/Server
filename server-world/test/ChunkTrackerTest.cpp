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

TEST_CASE("the view delta matches the frozen kotlin") {
    ChunkTracker tracker;
    {
        const ChunkTracker::Delta delta = tracker.update(0, 0, 2);
        CHECK(render(delta.toLoad) ==
              "-2,-2;-2,-1;-2,0;-2,1;-2,2;-1,-2;-1,-1;-1,0;-1,1;-1,2;0,-2;0,-1;0,0;0,1;0,2;1,-2;1,-1;1,0;1,1;1,2;"
              "2,-2;2,-1;2,0;2,1;2,2");
        CHECK(render(delta.toUnload) == "");
        CHECK(tracker.loaded().size() == 25);
    }
    {
        const ChunkTracker::Delta delta = tracker.update(1, 0, 2);
        CHECK(render(delta.toLoad) == "3,-2;3,-1;3,0;3,1;3,2");
        CHECK(render(delta.toUnload) == "-2,-2;-2,-1;-2,0;-2,1;-2,2");
        CHECK(tracker.loaded().size() == 25);
    }
    {
        const ChunkTracker::Delta delta = tracker.update(1, 0, 3);
        CHECK(render(delta.toLoad) ==
              "-2,-3;-2,-2;-2,-1;-2,0;-2,1;-2,2;-2,3;-1,-3;-1,3;0,-3;0,3;1,-3;1,3;2,-3;2,3;3,-3;3,3;4,-3;4,-2;4,-1;"
              "4,0;4,1;4,2;4,3");
        CHECK(render(delta.toUnload) == "");
        CHECK(tracker.loaded().size() == 49);
    }
    {
        const ChunkTracker::Delta delta = tracker.update(-4, 7, 1);
        CHECK(render(delta.toLoad) == "-5,6;-5,7;-5,8;-4,6;-4,7;-4,8;-3,6;-3,7;-3,8");
        CHECK(render(delta.toUnload) ==
              "-2,-3;-2,-2;-2,-1;-2,0;-2,1;-2,2;-2,3;-1,-3;-1,-2;-1,-1;-1,0;-1,1;-1,2;-1,3;0,-3;0,-2;0,-1;0,0;0,1;"
              "0,2;0,3;1,-3;1,-2;1,-1;1,0;1,1;1,2;1,3;2,-3;2,-2;2,-1;2,0;2,1;2,2;2,3;3,-3;3,-2;3,-1;3,0;3,1;3,2;"
              "3,3;4,-3;4,-2;4,-1;4,0;4,1;4,2;4,3");
        CHECK(tracker.loaded().size() == 9);
    }
    {
        const ChunkTracker::Delta delta = tracker.update(-4, 7, 0);
        CHECK(render(delta.toLoad) == "");
        CHECK(render(delta.toUnload) == "-5,6;-5,7;-5,8;-4,6;-4,8;-3,6;-3,7;-3,8");
        CHECK(tracker.loaded().size() == 1);
    }
    tracker.reset();
    {
        const ChunkTracker::Delta delta = tracker.update(-4, 7, 0);
        CHECK(render(delta.toLoad) == "-4,7");
        CHECK(render(delta.toUnload) == "");
        CHECK(tracker.loaded().size() == 1);
    }
}

TEST_CASE("a negative radius is refused") {
    ChunkTracker tracker;
    CHECK_THROWS_AS(static_cast<void>(tracker.update(0, 0, -1)), std::invalid_argument);
}
