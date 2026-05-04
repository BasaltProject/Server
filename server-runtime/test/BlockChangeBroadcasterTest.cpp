#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::BlockChangeBroadcaster;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::BlockChange;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace blockEffect = basalt::server::runtime::world::blockEffect;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;
namespace blocks = basalt::server::registry::blocks;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::int32_t stateOf(const std::string& name) {
    return blocks::stateIdOf(Identifier::vanilla(name), {}).value_or(-1);
}

struct Fixture {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;
    std::vector<std::int32_t> packets;
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
        connection.sendBlockEvent = [this](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                           std::int32_t) { packets.push_back(play::BlockEventPacket::kId); };
        connection.sendLevelEvent = [this](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {
            packets.push_back(play::LevelEventPacket::kId);
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

    [[nodiscard]] bool sent(std::int32_t id) const { return std::ranges::find(packets, id) != packets.end(); }
};

} // namespace

TEST_CASE("a block change goes out as an update, and its effects as their own packets") {
    Fixture fixture{scratch("broadcaster-effects")};
    RealLootTables tables{fixture.jar()};
    BlockChangeBroadcaster broadcaster{fixture.registry, tables, {}, {}};

    broadcaster.announceBlock(fixture.watchers(), BlockChange{.x = 1, .y = 64, .z = 1, .stateId = stateOf("stone")});
    CHECK(fixture.sent(play::ClientboundBlockUpdatePacket::kId));
    CHECK(fixture.packets.size() == 1);

    fixture.packets.clear();
    broadcaster.announceBlock(
        fixture.watchers(),
        BlockChange{.x = 1,
                    .y = 64,
                    .z = 1,
                    .stateId = stateOf("note_block"),
                    .effects = {blockEffect::Sound{.name = "minecraft:block.note_block.harp"},
                                blockEffect::Event{.b0 = 0, .b1 = 1},
                                blockEffect::Effect{.type = blockEffect::Effect::kDispenserFail, .data = 0}}});
    CHECK(fixture.packets.size() == 4);
    CHECK(fixture.sent(play::SoundPacket::kId));
    CHECK(fixture.sent(play::BlockEventPacket::kId));
    CHECK(fixture.sent(play::LevelEventPacket::kId));
}

TEST_CASE("water washing a block away drops what breaking it would, and lava drops nothing") {
    Fixture fixture{scratch("broadcaster-washed")};
    fixture.lootFiles["data/minecraft/loot_table/blocks/torch.json"] =
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:torch"}]}]})";
    RealLootTables tables{fixture.jar()};
    BlockChangeBroadcaster broadcaster{fixture.registry, tables, {}, {.nextSeed = [] { return std::int64_t{5}; }}};

    REQUIRE(fixture.world->state().setBlockState(1, 64, 1, stateOf("water")));
    broadcaster.dropWashedAwayBlock(*fixture.world, 1, 64, 1, stateOf("torch"));
    CHECK(fixture.world->state().items().size() == 1);

    // parity: lava's own `beforeDestroyingBlock` only plays the fizz, so nothing drops.
    REQUIRE(fixture.world->state().setBlockState(2, 64, 2, stateOf("lava")));
    broadcaster.dropWashedAwayBlock(*fixture.world, 2, 64, 2, stateOf("torch"));
    CHECK(fixture.world->state().items().size() == 1);

    broadcaster.dropWashedAwayBlock(*fixture.world, 1, 64, 1, stateOf("water"));
    CHECK(fixture.world->state().items().size() == 1);
}

TEST_CASE("a plugin that refuses a piston stops it moving anything at all") {
    Fixture fixture{scratch("broadcaster-piston")};
    RealLootTables tables{fixture.jar()};
    const BlockChangeBroadcaster::Hooks refuse{.pistonAllowed = [](const LoadedWorld&, std::int32_t, std::int32_t,
                                                                   std::int32_t, bool,
                                                                   std::string_view) { return false; }};
    BlockChangeBroadcaster broadcaster{fixture.registry, tables, refuse, {}};

    CHECK(broadcaster.finishPiston(*fixture.world, {1, 64, 1}, fixture.watchers()).empty());
}
