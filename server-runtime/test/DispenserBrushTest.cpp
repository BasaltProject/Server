#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::server::world::ChunkBlockEntity;
using basalt::server::world::loot::ItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace blockInventory = basalt::server::runtime::world::blockInventory;
namespace config = basalt::server::runtime::config;
namespace dispensers = basalt::server::runtime::world::dispensers;
namespace world = basalt::server::runtime::world;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::int32_t stateOf(const std::string& name, const std::map<std::string, std::string>& properties = {}) {
    return basalt::server::registry::blocks::stateIdOf(Identifier::vanilla(name), properties).value_or(-1);
}

struct Fixture {
    std::unique_ptr<LoadedWorld> world;
    std::int32_t calls = 0;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        world->state().getOrGenerateChunk(0, 0);
        // A dispenser one above the ground, facing east, holding a brush.
        static_cast<void>(world->state().setBlockState(1, 1, 1, stateOf("dispenser", {{"facing", "east"}})));
        static_cast<void>(world->state().setBlockState(1, 0, 1, stateOf("bedrock")));
        static_cast<void>(world->state().setBlockState(2, 0, 1, stateOf("bedrock")));
        basalt::wire::nbt::NbtCompound data;
        blockInventory::write(data, {{ItemStack{.item = Identifier::vanilla("brush"), .count = 1}}});
        if (basalt::server::world::Chunk* chunk = world->state().chunkIfLoaded(0, 0); chunk != nullptr) {
            chunk->setBlockEntity(ChunkBlockEntity(1, 1, 1, Identifier::vanilla("dispenser"), std::move(data)));
            chunk->markDirty();
        }
    }

    void spawnArmadillo(double x, double y, double z) {
        const basalt::server::registry::EntityTypeEntry* entry =
            basalt::server::registry::entityTypes::of(Identifier::vanilla("armadillo"));
        REQUIRE(entry != nullptr);
        auto armadillo = std::make_unique<ServerEntity>(1001, Uuid::random(), Identifier::vanilla("armadillo"),
                                                        entry->protocolId, x, y, z);
        static_cast<void>(world->state().spawnEntity(std::move(armadillo)));
    }

    [[nodiscard]] dispensers::Result fire() {
        RandomSource rng = RandomSource::legacy(42);
        const world::fluidFlow::Context context{.world = world->state(),
                                                .gameRules = world->gameRules(),
                                                .random = rng,
                                                .ultrawarm = false,
                                                .blockTags = nullptr};
        return dispensers::fire(context, 1, 1, 1, {.gameTime = 0, .brushArmadillo = [this](ServerEntity& armadillo) {
                                                       ++calls;
                                                       return armadillo.type().path() == "armadillo";
                                                   }});
    }

    [[nodiscard]] std::optional<ItemStack> brushLeft() {
        const basalt::server::world::Chunk* chunk = world->state().chunkIfLoaded(0, 0);
        if (chunk == nullptr) {
            return std::nullopt;
        }
        const ChunkBlockEntity* entity = chunk->blockEntityAt(1, 1, 1);
        if (entity == nullptr) {
            return std::nullopt;
        }
        const blockInventory::Slots slots = blockInventory::read(entity->data(), dispensers::kSize);
        return slots.front();
    }
};

TEST_CASE("the dispenser brush brushes an armadillo in the front square") {
    Fixture fixture{scratch("brush-hit")};
    fixture.spawnArmadillo(2.5, 1.0, 1.5);
    const dispensers::Result result = fixture.fire();
    CHECK(result.fired);
    CHECK(fixture.calls == 1);
    const std::optional<ItemStack> left = fixture.brushLeft();
    REQUIRE(left.has_value());
    // parity: `ItemStack.hurtAndBreak(16, ...)`, the brush keeps its count and takes the damage.
    CHECK(left->count == 1);
    CHECK(left->components.getInt("minecraft:damage", 0) == 16);
}

TEST_CASE("the dispenser brush refuses with nothing in front") {
    Fixture fixture{scratch("brush-miss")};
    const dispensers::Result result = fixture.fire();
    CHECK(!result.fired);
    CHECK(fixture.calls == 0);
    const std::optional<ItemStack> left = fixture.brushLeft();
    REQUIRE(left.has_value());
    CHECK(left->components.getInt("minecraft:damage", 0) == 0);
}

} // namespace
