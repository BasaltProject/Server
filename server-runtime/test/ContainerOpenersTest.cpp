#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/ContainerOpeners.hpp>

#include <doctest.h>

#include <cstdint>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::world;

TEST_CASE("nobody is looking until somebody opens one") {
    containerOpeners::clear();
    CHECK(containerOpeners::count("world", 1, 2, 3) == 0);

    CHECK(containerOpeners::open("world", 1, 2, 3) == 1);
    CHECK(containerOpeners::count("world", 1, 2, 3) == 1);
    CHECK(containerOpeners::open("world", 1, 2, 3) == 2);
    CHECK(containerOpeners::count("world", 1, 2, 3) == 2);

    // The chest next to it is a different chest.
    CHECK(containerOpeners::count("world", 2, 2, 3) == 0);
    CHECK(containerOpeners::count("world", 1, 3, 3) == 0);
    CHECK(containerOpeners::count("world", 1, 2, 4) == 0);
    // And so is the one at the same place in another world.
    CHECK(containerOpeners::count("nether", 1, 2, 3) == 0);

    CHECK(containerOpeners::close("world", 1, 2, 3) == 1);
    CHECK(containerOpeners::close("world", 1, 2, 3) == 0);
    CHECK(containerOpeners::count("world", 1, 2, 3) == 0);
    containerOpeners::clear();
}

TEST_CASE("closing what nobody had open is not a negative count") {
    containerOpeners::clear();
    CHECK(containerOpeners::close("world", 5, 5, 5) == 0);
    CHECK(containerOpeners::count("world", 5, 5, 5) == 0);
    // And it did not leave an entry behind either: the next open starts from one.
    CHECK(containerOpeners::open("world", 5, 5, 5) == 1);
    containerOpeners::clear();
}

TEST_CASE("positions that pack the same are the same chest, and no others do") {
    containerOpeners::clear();
    // Negative and far coordinates, where a packing that dropped a sign would collide.
    CHECK(containerOpeners::open("world", -1, -1, -1) == 1);
    CHECK(containerOpeners::count("world", -1, -1, -1) == 1);
    CHECK(containerOpeners::count("world", 1, 1, 1) == 0);
    CHECK(containerOpeners::count("world", -1, -1, 1) == 0);
    CHECK(containerOpeners::count("world", 33554431, -1, -1) == 0);

    CHECK(containerOpeners::open("world", 30000000, 319, -30000000) == 1);
    CHECK(containerOpeners::count("world", 30000000, 319, -30000000) == 1);
    CHECK(containerOpeners::count("world", 30000000, 318, -30000000) == 0);
    containerOpeners::clear();
}

TEST_CASE("a world unloading takes its open containers with it") {
    containerOpeners::clear();
    static_cast<void>(containerOpeners::open("world", 1, 1, 1));
    static_cast<void>(containerOpeners::open("nether", 2, 2, 2));
    CHECK(containerOpeners::count("world", 1, 1, 1) == 1);
    CHECK(containerOpeners::count("nether", 2, 2, 2) == 1);

    containerOpeners::clear();
    CHECK(containerOpeners::count("world", 1, 1, 1) == 0);
    CHECK(containerOpeners::count("nether", 2, 2, 2) == 0);

    // Still nothing once something else is open: what was cleared is gone, not merely hidden
    // behind the shortcut that answers while nothing is open anywhere.
    CHECK(containerOpeners::open("world", 9, 9, 9) == 1);
    CHECK(containerOpeners::count("world", 1, 1, 1) == 0);
    CHECK(containerOpeners::count("nether", 2, 2, 2) == 0);
    containerOpeners::clear();
}

TEST_CASE("the count survives everybody opening and closing at once") {
    containerOpeners::clear();
    // Every connection thread opens and closes the same chest; the count has to come back to zero
    // and nothing may be lost on the way.
    std::vector<std::thread> threads;
    for (std::int32_t worker = 0; worker < 8; ++worker) {
        threads.emplace_back([] {
            for (std::int32_t at = 0; at < 200; ++at) {
                static_cast<void>(containerOpeners::open("world", 7, 8, 9));
                static_cast<void>(containerOpeners::count("world", 7, 8, 9));
                static_cast<void>(containerOpeners::close("world", 7, 8, 9));
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    CHECK(containerOpeners::count("world", 7, 8, 9) == 0);

    // And one that stays open is still open afterwards.
    CHECK(containerOpeners::open("world", 7, 8, 9) == 1);
    containerOpeners::clear();
}
