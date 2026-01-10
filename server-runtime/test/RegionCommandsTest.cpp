#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/RegionCommands.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;
namespace commands = basalt::server::runtime::command::commands;
namespace config = basalt::server::runtime::config;

namespace {

[[nodiscard]] std::string flatten(const Component& component) {
    std::string text = basalt::wire::text::toPlainText(component);
    if (const auto* translatable = std::get_if<Component::Translatable>(&component.payload());
        translatable != nullptr) {
        text += translatable->key;
        for (const Component& argument : translatable->args) {
            text += " " + flatten(argument);
        }
    }
    for (const Component& child : component.children()) {
        text += flatten(child);
    }
    return text;
}

class RecordingSource final : public CommandSource {
public:
    [[nodiscard]] std::string name() const override { return "Operator"; }

    [[nodiscard]] Component displayName() const override { return Component::text("Operator"); }

    [[nodiscard]] std::int32_t permissionLevel() const override { return 4; }

    [[nodiscard]] std::array<double, 3> position() const override { return at; }

    [[nodiscard]] PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const Component& message) const override { said.push_back(message); }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view>) const override { return true; }

    [[nodiscard]] std::string lastAsText() const { return said.empty() ? std::string{} : flatten(said.back()); }

    std::array<double, 3> at{0.0, 0.0, 0.0};
    mutable std::vector<Component> said;
};

/// One connection that keeps whatever was sent to it, packets included.
struct Client {
    std::vector<Component> messages;
    std::vector<std::int32_t> packets;
    std::vector<basalt::server::runtime::world::ActiveEffect> effects;
    std::vector<basalt::wire::common::Identifier> removed;
    float healed = 0.0F;
    float hurt = 0.0F;
    std::vector<std::string> switched;
    /// Set once the session exists, so a switch can move it the way the real connection does.
    PlayerSession* session = nullptr;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.sendEffect = [this](const basalt::server::runtime::world::ActiveEffect& effect) {
            effects.push_back(effect);
        };
        connection.sendEffectRemoved = [this](const basalt::wire::common::Identifier& effect) {
            removed.push_back(effect);
        };
        connection.heal = [this](float amount) { healed += amount; };
        connection.hurt = [this](float amount, std::string_view, const basalt::wire::common::Identifier&,
                                 const std::optional<basalt::server::runtime::entity::DamageSource>&) {
            hurt += amount;
        };
        connection.switchWorld = [this](basalt::server::runtime::world::LoadedWorld& world, double, double, double) {
            switched.push_back(world.name());
            if (session != nullptr) {
                session->worldName = world.name();
            }
            return true;
        };
        return connection;
    }

    [[nodiscard]] bool sent(std::int32_t id) const { return std::ranges::find(packets, id) != packets.end(); }
};

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

/// A real database in the scratch directory, since `AccessRepository` is not an interface to stub.
[[nodiscard]] std::unique_ptr<basalt::server::storage::PlayerStore> openStore(const std::string& root) {
    std::error_code ignored;
    std::filesystem::create_directories(root, ignored);
    auto opened = basalt::server::storage::PlayerStore::open(root + "/players.db");
    return opened.has_value() ? std::move(*opened) : nullptr;
}

struct Fixture {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    std::vector<std::unique_ptr<Client>> clients;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<basalt::server::storage::PlayerStore> store;
    CommandEnvironment environment;
    CommandDispatcher dispatcher;
    RecordingSource source;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          store(openStore(root)), environment(registry, options()), dispatcher(environment) {}

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.worlds = {{"world", world.get()}};
        return built;
    }

    PlayerSession& add(const std::string& name) {
        clients.push_back(std::make_unique<Client>());
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          clients.back()->connection()));
        players.back()->worldName = "world";
        clients.back()->session = players.back().get();
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

namespace mobEffects = basalt::server::runtime::world::mobEffects;

namespace blocks = basalt::server::registry::blocks;

/// A block by name, for the ones the vanilla table does not name individually.
[[nodiscard]] std::int32_t stateOf(const std::string& name) {
    return blocks::stateIdOf(basalt::wire::common::Identifier::vanilla(name), {}).value_or(-1);
}

TEST_CASE("clone copies a region and answers how many blocks landed") {
    Fixture fixture{scratch("region-commands-clone")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::CloneCommand>());

    REQUIRE(fixture.world->state().setBlockState(0, 40, 0, blocks::stone().stateId));
    REQUIRE(fixture.world->state().setBlockState(1, 40, 0, stateOf("dirt")));

    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 1 40 0 0 45 0") == 2);
    CHECK(fixture.world->state().blockState(0, 45, 0).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.world->state().blockState(1, 45, 0).value_or(0) == stateOf("dirt"));
    CHECK(fixture.source.lastAsText().find("commands.clone.success") != std::string::npos);
}

TEST_CASE("an overlapping clone needs force, and force reads the whole source first") {
    Fixture fixture{scratch("region-commands-clone-overlap")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::CloneCommand>());

    REQUIRE(fixture.world->state().setBlockState(0, 40, 0, blocks::stone().stateId));
    REQUIRE(fixture.world->state().setBlockState(1, 40, 0, stateOf("dirt")));

    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 2 40 0 1 40 0") == 0);
    CHECK(fixture.source.lastAsText().find("commands.clone.overlap") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 2 40 0 1 40 0 force") == 2);
    // Read first, write after: the dirt came from the source as it was, not as it had become.
    CHECK(fixture.world->state().blockState(1, 40, 0).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.world->state().blockState(2, 40, 0).value_or(0) == stateOf("dirt"));
}

TEST_CASE("clone masked skips air and move empties the source") {
    Fixture fixture{scratch("region-commands-clone-modes")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::CloneCommand>());

    REQUIRE(fixture.world->state().setBlockState(0, 40, 0, blocks::stone().stateId));
    REQUIRE(fixture.world->state().setBlockState(0, 45, 1, stateOf("dirt")));

    // The second block of the source is air, so only one lands and the dirt below is left alone.
    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 0 40 1 0 45 0 masked") == 1);
    CHECK(fixture.world->state().blockState(0, 45, 1).value_or(0) == stateOf("dirt"));

    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 0 40 0 0 50 0 replace move") == 1);
    CHECK(fixture.world->state().blockState(0, 40, 0).value_or(0) == blocks::air().stateId);
    CHECK(fixture.world->state().blockState(0, 50, 0).value_or(0) == blocks::stone().stateId);
}

TEST_CASE("clone filtered copies only what the filter matches") {
    Fixture fixture{scratch("region-commands-clone-filtered")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::CloneCommand>());

    REQUIRE(fixture.world->state().setBlockState(0, 40, 0, blocks::stone().stateId));
    REQUIRE(fixture.world->state().setBlockState(1, 40, 0, stateOf("dirt")));

    CHECK(fixture.dispatcher.execute(fixture.source, "clone 0 40 0 1 40 0 0 45 0 filtered minecraft:dirt") == 1);
    CHECK(fixture.world->state().blockState(1, 45, 0).value_or(0) == stateOf("dirt"));
    CHECK(fixture.world->state().blockState(0, 45, 0).value_or(0) == blocks::air().stateId);
}

TEST_CASE("fillbiome works in quarts, so a single block changes the whole one it falls in") {
    Fixture fixture{scratch("region-commands-fillbiome")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillBiomeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "fillbiome 0 40 0 0 40 0 minecraft:desert") == 1);
    const std::optional<std::int32_t> desert =
        basalt::server::registry::biomes::registry().idOf(basalt::wire::common::Identifier::vanilla("desert"));
    REQUIRE(desert.has_value());
    basalt::server::world::Chunk* chunk = fixture.world->state().chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    // The quart holds sixteen blocks across, so its neighbour inside the same quart moved too.
    CHECK(chunk->biomeId(0, (40 - basalt::server::world::kOverworldHeight.minY()) >> 2, 0) == *desert);
    CHECK(fixture.source.lastAsText().find("commands.fillbiome.success.count") != std::string::npos);
}

TEST_CASE("fillbiome replace changes only the quarts already holding the filter") {
    Fixture fixture{scratch("region-commands-fillbiome-replace")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillBiomeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "fillbiome 0 40 0 0 40 0 minecraft:desert replace minecraft:the_void") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "fillbiome 0 40 0 0 40 0 minecraft:desert") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "fillbiome 0 40 0 0 40 0 minecraft:badlands replace minecraft:desert") == 1);
}

TEST_CASE("a biome nobody registered is refused by name") {
    Fixture fixture{scratch("region-commands-fillbiome-unknown")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillBiomeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "fillbiome 0 40 0 0 40 0 minecraft:cheese") == 0);
    CHECK(fixture.source.lastAsText().find("argument.resource.not_found") != std::string::npos);
}

TEST_CASE("forceload takes block coordinates that name a chunk") {
    Fixture fixture{scratch("region-commands-forceload")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::ForceLoadCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "forceload add 0 0") == 1);
    CHECK(fixture.world->forcedChunks().size() == 1);
    // 16, 16 is chunk 1, 1, not chunk 16, 16.
    CHECK(fixture.dispatcher.execute(fixture.source, "forceload add 16 16") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "forceload query 16 16") == 1);
    CHECK(fixture.source.lastAsText().find("1, 1") != std::string::npos);

    // Already forced, so nothing changed and vanilla refuses.
    CHECK(fixture.dispatcher.execute(fixture.source, "forceload add 0 0") == 0);
    CHECK(fixture.source.lastAsText().find("commands.forceload.added.failure") != std::string::npos);
}

TEST_CASE("forceload remove takes one, a range or all of them") {
    Fixture fixture{scratch("region-commands-forceload-remove")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::ForceLoadCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "forceload add 0 0 32 32") == 9);
    CHECK(fixture.dispatcher.execute(fixture.source, "forceload remove 0 0") == 1);
    CHECK(fixture.world->forcedChunks().size() == 8);
    CHECK(fixture.dispatcher.execute(fixture.source, "forceload remove all") == 0);
    CHECK(fixture.world->forcedChunks().empty());

    CHECK(fixture.dispatcher.execute(fixture.source, "forceload query") == 0);
    CHECK(fixture.source.lastAsText().find("commands.forceload.added.none") != std::string::npos);
}

TEST_CASE("a forceload wider than vanilla allows is refused") {
    Fixture fixture{scratch("region-commands-forceload-toobig")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::ForceLoadCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "forceload add 0 0 512 512") == 0);
    CHECK(fixture.source.lastAsText().find("commands.forceload.toobig") != std::string::npos);
    CHECK(fixture.world->forcedChunks().empty());
}
