#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::ExplosionSystem;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace blocks = basalt::server::registry::blocks;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Fixture {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;
    std::vector<std::int32_t> packets;
    float hurt = 0.0F;
    std::map<std::string, std::string> lootFiles;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        PlayerConnection connection;
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.hurt = [this](float amount, std::string_view, const Identifier&,
                                 const std::optional<basalt::server::runtime::entity::DamageSource>&) {
            hurt += amount;
        };
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, std::move(connection));
        player->worldName = "world";
        registry.add(player.get());
        world->state().getOrGenerateChunk(0, 0);
    }

    [[nodiscard]] std::vector<PlayerSession*> watchers() { return {player.get()}; }

    [[nodiscard]] RealLootTables::Jar jar() {
        return {.readText = [this](std::string_view name) -> std::optional<std::string> {
                    const auto found = lootFiles.find(std::string{name});
                    return found == lootFiles.end() ? std::nullopt : std::optional{found->second};
                },
                .entriesUnder = [](std::string_view) { return std::vector<std::string>{}; }};
    }

    [[nodiscard]] std::int32_t sentCount(std::int32_t id) const {
        return static_cast<std::int32_t>(std::ranges::count(packets, id));
    }
};

} // namespace

TEST_CASE("a blast sends its own packet, empties the blocks and hurts whoever stood in it") {
    Fixture fixture{scratch("explosion-basic")};
    RealLootTables tables{fixture.jar()};
    ExplosionSystem system{fixture.registry, tables, {}, {.nextSeed = [] { return std::int64_t{9}; }}};
    REQUIRE(fixture.world->state().setBlockState(8, 64, 8, blocks::stateIdOf(Identifier::vanilla("dirt"), {}).value()));
    fixture.player->x = 8.0;
    fixture.player->y = 64.0;
    fixture.player->z = 8.0;

    system.blowUp(*fixture.world, fixture.watchers(), 8.0, 64.0, 8.0, 4.0F);

    CHECK(fixture.sentCount(play::ExplodePacket::kId) == 1);
    CHECK(fixture.world->state().blockState(8, 64, 8) == blocks::air().stateId);
    // Standing at the centre is the most damage the blast can do, so it is certainly not zero.
    CHECK(fixture.hurt > 0.0F);
}

TEST_CASE("a plugin that refuses a blast leaves the world untouched") {
    Fixture fixture{scratch("explosion-refused")};
    RealLootTables tables{fixture.jar()};
    const ExplosionSystem::Hooks refuse{.allowed = [](const LoadedWorld&, double, double, double, float,
                                                      std::string_view, std::int32_t) { return false; }};
    ExplosionSystem system{fixture.registry, tables, refuse, {}};
    const std::int32_t dirt = blocks::stateIdOf(Identifier::vanilla("dirt"), {}).value();
    REQUIRE(fixture.world->state().setBlockState(8, 64, 8, dirt));

    system.blowUp(*fixture.world, fixture.watchers(), 8.0, 64.0, 8.0, 4.0F, "tnt", 7);
    CHECK(fixture.world->state().blockState(8, 64, 8) == dirt);
    CHECK(fixture.packets.empty());
}

TEST_CASE("what a blast breaks drops as a few merged stacks rather than one item per block") {
    Fixture fixture{scratch("explosion-drops")};
    // A table that always drops, so nothing here depends on the survival roll.
    fixture.lootFiles["data/minecraft/loot_table/blocks/dirt.json"] =
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:dirt"}]}]})";
    RealLootTables tables{fixture.jar()};
    ExplosionSystem system{
        fixture.registry, tables, {}, {.nextSeed = [] { return std::int64_t{3}; }, .nextDouble = [] { return 0.5; }}};
    const std::int32_t dirt = blocks::stateIdOf(Identifier::vanilla("dirt"), {}).value();
    for (std::int32_t x = 5; x <= 11; x++) {
        for (std::int32_t z = 5; z <= 11; z++) {
            REQUIRE(fixture.world->state().setBlockState(x, 63, z, dirt));
        }
    }

    system.blowUp(*fixture.world, fixture.watchers(), 8.0, 64.0, 8.0, 4.0F);

    // parity: sixteen to a stack while the blast is holding them, so a big hole is a handful of
    // item entities and not one per block.
    const std::vector<basalt::server::runtime::world::DroppedItem> dropped = fixture.world->state().items();
    REQUIRE_FALSE(dropped.empty());
    for (const basalt::server::runtime::world::DroppedItem& item : dropped) {
        CHECK(item.count <= 16);
    }
}
