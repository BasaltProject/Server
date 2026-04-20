#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

using namespace basalt::server::registry;
using basalt::wire::common::Identifier;

TEST_CASE("a state id and its properties convert into each other") {
    for (const auto& [stateId, path] : defaultBlockStatesTable()) {
        const Identifier block = Identifier::vanilla(std::string{path});
        const blockStateDefinitions::Definition* definition = blockStateDefinitions::of(block);
        if (definition == nullptr) {
            continue;
        }
        for (std::int32_t offset = 0; offset < definition->stateCount(); ++offset) {
            const std::int32_t id = definition->firstStateId() + offset;
            const std::map<std::string, std::string> properties = blockStateDefinitions::propertiesOf(id);
            REQUIRE_FALSE(properties.empty());
            const std::optional<std::int32_t> back = blockStateDefinitions::stateIdOf(block, properties);
            REQUIRE(back.has_value());
            CHECK(*back == id);
        }
    }
}

TEST_CASE("every state id belongs to exactly the block whose range covers it") {
    for (const auto& [stateId, path] : defaultBlockStatesTable()) {
        const Identifier* owner = blockStateDefinitions::blockOf(stateId);
        REQUIRE(owner != nullptr);
        CHECK(owner->path() == path);
    }
}

TEST_CASE("an id table names each thing once and numbers it once") {
    const auto unique = [](auto table) {
        std::set<std::int32_t> ids;
        std::set<std::string_view> names;
        for (const auto& [id, name] : table) {
            CHECK(ids.insert(id).second);
            CHECK(names.insert(name).second);
        }
        return ids.size();
    };
    CHECK(unique(itemIdsTable()) == itemIdsTable().size());
    CHECK(unique(entityTypeIdsTable()) == entityTypeIdsTable().size());
    CHECK(unique(particleTypeIdsTable()) == particleTypeIdsTable().size());
    CHECK(unique(dataComponentIdsTable()) == dataComponentIdsTable().size());
}

TEST_CASE("a particle's option shape decides how the packet after it is read") {
    using basalt::wire::protocol::play::ParticleOptionShape;

    const ParticleType* dust = particleTypes::of(Identifier::vanilla("dust"));
    REQUIRE(dust != nullptr);
    CHECK(dust->optionShape == ParticleOptionShape::Dust);

    const ParticleType* flame = particleTypes::of(Identifier::vanilla("flame"));
    REQUIRE(flame != nullptr);
    CHECK_FALSE(flame->needsOptions());
}

TEST_CASE("a statistic's id is its position, which is protocol") {
    CHECK(customStats::idOf("leave_game") == 0);
    CHECK(customStats::nameOf(0) == "leave_game");
    CHECK_FALSE(customStats::idOf("not_a_statistic").has_value());
    CHECK_FALSE(customStats::nameOf(static_cast<std::int32_t>(customStats::count())).has_value());
}

TEST_CASE("the biome registry follows vanilla's bootstrap order") {
    CHECK(biomes::registry().idOf(Identifier::vanilla("plains")) == 0);
    CHECK(biomes::plains().id == Identifier::vanilla("plains"));
    CHECK(biomes::registry().size() == 67);
}

TEST_CASE("a tag that refers to another tag is flattened through it") {
    // parity: TagLoader.build, resolving what a tag refers to before the tag itself.
    using namespace basalt::server::registry::tags;
    const Identifier planks = Identifier::vanilla("planks");
    const Identifier wood = Identifier::vanilla("wood");
    const Identifier oak = Identifier::vanilla("oak_planks");
    const Identifier log = Identifier::vanilla("oak_log");

    const std::map<Identifier, TagFile> files = {
        {planks, TagFile{{TagEntry{oak, false, true}}, false}},
        {wood, TagFile{{TagEntry{planks, true, true}, TagEntry{log, false, true}}, false}},
    };
    const auto resolved = resolve(files, [&](const Identifier& id) { return id == oak || id == log; });
    REQUIRE(resolved.has_value());
    CHECK(resolved->at(wood) == std::set<Identifier>{oak, log});
    CHECK(resolved->at(planks) == std::set<Identifier>{oak});
}

TEST_CASE("a cycle is refused rather than followed forever") {
    using namespace basalt::server::registry::tags;
    const Identifier first = Identifier::vanilla("first");
    const Identifier second = Identifier::vanilla("second");
    const std::map<Identifier, TagFile> files = {
        {first, TagFile{{TagEntry{second, true, true}}, false}},
        {second, TagFile{{TagEntry{first, true, true}}, false}},
    };
    const auto resolved = resolve(files, [](const Identifier&) { return true; });
    REQUIRE_FALSE(resolved.has_value());
    CHECK(resolved.error().kind == TagError::Kind::Cycle);
}
