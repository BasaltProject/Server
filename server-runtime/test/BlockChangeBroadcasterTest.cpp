#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
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
    std::vector<std::string> sounds;
    std::int32_t pickupAnimations = 0;
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
            if (id == play::SoundPacket::kId) {
                basalt::wire::io::ByteBuffer in = basalt::wire::io::ByteBuffer::wrap(buffer.toVector());
                if (const auto sound = play::SoundPacket::read(in); sound.has_value()) {
                    sounds.push_back(sound->sound);
                }
            }
        };
        connection.sendBlockEvent = [this](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                           std::int32_t) { packets.push_back(play::BlockEventPacket::kId); };
        connection.sendLevelEvent = [this](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {
            packets.push_back(play::LevelEventPacket::kId);
        };
        connection.sendPickupAnimation = [this](std::int32_t, std::int32_t) { ++pickupAnimations; };
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

TEST_CASE("container openers play the matching sound family and animate lids") {
    Fixture fixture{scratch("broadcaster-containers")};
    RealLootTables tables{fixture.jar()};
    BlockChangeBroadcaster broadcaster{fixture.registry, tables, {}, {}};

    basalt::server::runtime::player::OpenersContext context;
    context.blocks.registry = &fixture.registry;
    context.blocks.blocks = &broadcaster;
    context.nextFloat = [] { return 0.5F; };
    context.nextSeed = [] { return std::int64_t{7}; };

    SUBCASE("chest, copper chest variants, ender chest and shulker box emit open and close sounds") {
        REQUIRE(fixture.world->state().setBlockState(2, 64, 2, stateOf("chest")));
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 2, 64, 2, "chest", 1);
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 2, 64, 2, "chest", 0);

        REQUIRE(fixture.world->state().setBlockState(3, 64, 2, stateOf("weathered_copper_chest")));
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 3, 64, 2, "weathered_copper_chest",
                                                         1);
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 3, 64, 2, "weathered_copper_chest",
                                                         0);

        REQUIRE(fixture.world->state().setBlockState(4, 64, 2, stateOf("ender_chest")));
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 4, 64, 2, "ender_chest", 1);
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 4, 64, 2, "ender_chest", 0);

        REQUIRE(fixture.world->state().setBlockState(5, 64, 2, stateOf("shulker_box")));
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 5, 64, 2, "shulker_box", 1);
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 5, 64, 2, "shulker_box", 0);

        const std::vector<std::string> expected = {"block.chest.open",
                                                   "block.chest.close",
                                                   "block.copper_chest_weathered.open",
                                                   "block.copper_chest_weathered.close",
                                                   "block.ender_chest.open",
                                                   "block.ender_chest.close",
                                                   "block.shulker_box.open",
                                                   "block.shulker_box.close"};
        CHECK(fixture.sounds == expected);
    }

    SUBCASE("non-lidded containers emit neither lid events nor open sounds") {
        REQUIRE(fixture.world->state().setBlockState(2, 64, 2, stateOf("hopper")));
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 2, 64, 2, "hopper", 1);
        basalt::server::runtime::player::announceOpeners(context, *fixture.world, 2, 64, 2, "hopper", 0);
        CHECK(fixture.sounds.empty());
        CHECK(fixture.packets.empty());
    }
}

TEST_CASE("experience orb collection and level milestones emit their sounds") {
    Fixture fixture{scratch("broadcaster-xp")};
    fixture.player->x = 1.0;
    fixture.player->y = 64.0;
    fixture.player->z = 1.0;

    basalt::server::runtime::ExperienceOrbSystem orbs{fixture.registry, {}};
    orbs.spawn(*fixture.world, 1.0, 64.0, 1.0, 160);
    const std::vector<basalt::server::runtime::entity::ServerEntity*> spawned = fixture.world->state().entities();
    REQUIRE(spawned.size() == 1);
    orbs.tick(*fixture.world, fixture.watchers(), {}, *spawned.front());

    CHECK(fixture.pickupAnimations == 1);
    CHECK(std::ranges::find(fixture.sounds, "entity.experience_orb.pickup") != fixture.sounds.end());
    CHECK(std::ranges::find(fixture.sounds, "entity.player.levelup") != fixture.sounds.end());

    fixture.sounds.clear();
    fixture.player->aliveTicks = 200;
    fixture.player->experience.giveLevels(5);
    const std::vector<PlayerSession*> watchers = fixture.watchers();
    basalt::server::runtime::player::playLevelUpSoundIfDue(*fixture.player, watchers, 7);
    CHECK(std::ranges::find(fixture.sounds, "entity.player.levelup") != fixture.sounds.end());
}
