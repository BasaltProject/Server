#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/entity/ecs/ComponentStore.hpp>
#include <basalt/server/entity/ecs/MovementSystem.hpp>
#include <basalt/server/entity/ecs/World.hpp>

#include <doctest.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace basalt::server::entity;
using ecs::EntityId;

namespace {
[[nodiscard]] std::string describe(const std::vector<EntityId>& ids) {
    std::string text;
    for (const EntityId id : ids) {
        if (!text.empty()) {
            text += ',';
        }
        text += std::to_string(id.index()) + "#" + std::to_string(id.generation());
    }
    return text;
}
} // namespace

TEST_CASE("the entity ids recycle the way the frozen kotlin recycles them") {
    ecs::World world;
    std::vector<EntityId> first;
    for (std::int32_t index = 0; index < 5; ++index) {
        first.push_back(world.create());
    }
    CHECK(describe(first) == "0#0,1#0,2#0,3#0,4#0");

    world.destroy(first[1]);
    world.destroy(first[3]);
    world.destroy(first[1]);
    std::vector<EntityId> recycled;
    for (std::int32_t index = 0; index < 3; ++index) {
        recycled.push_back(world.create());
    }
    CHECK(describe(recycled) == "1#1,3#1,5#0");

    std::int32_t live = 0;
    std::vector<EntityId> stale;
    for (const EntityId id : first) {
        if (world.isAlive(id)) {
            ++live;
        } else {
            stale.push_back(id);
        }
    }
    CHECK(live == 3);
    CHECK(describe(stale) == "1#0,3#0");
    CHECK_FALSE(world.isAlive(EntityId::of(99, 0)));
    CHECK_FALSE(world.isAlive(EntityId::of(first[1].index(), 0)));

    const EntityId wide = EntityId::of(70000, 3);
    CHECK(describe({wide}) == "70000#3");
    CHECK(wide.raw == 12884971888ULL);

    ecs::EntityHandle handle{world, recycled[0]};
    CHECK(handle.isAlive());
    handle.destroy();
    CHECK_FALSE(handle.isAlive());
    CHECK(handle.id() == EntityId::of(1, 1));
}

TEST_CASE("the sparse set packs and reorders the way the frozen kotlin does") {
    ecs::ComponentStore<std::string> store;
    for (std::int32_t index = 0; index < 6; ++index) {
        store.set(index, "v" + std::to_string(index));
    }
    store.set(3, "V3");
    store.remove(0);
    store.remove(4);
    store.remove(4);
    store.set(9, "v9");

    std::string seen;
    store.forEach([&seen](std::int32_t entityIndex, const std::string& value) {
        seen += std::to_string(entityIndex) + "=" + value + ";";
    });
    CHECK(store.size() == 5);
    CHECK(seen == "5=v5;1=v1;2=v2;3=V3;9=v9;");
    CHECK_FALSE(store.has(0));
    CHECK(store.has(3));
    CHECK(*store.get(3) == "V3");
    CHECK(store.get(0) == nullptr);
    CHECK(*store.get(9) == "v9");
    CHECK_FALSE(store.has(1000));
    CHECK(store.has(5));
    CHECK(*store.get(5) == "v5");
}
