#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BlockCommands.hpp>
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

TEST_CASE("setblock writes the state it was given and tells whoever is watching") {
    Fixture fixture{scratch("block-commands-setblock")};
    fixture.add("Steve");
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:stone") == 1);
    CHECK(fixture.world->state().blockState(1, 40, 1).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.source.lastAsText().find("commands.setblock.success") != std::string::npos);
    CHECK_FALSE(fixture.clients[0]->packets.empty());
}

TEST_CASE("setblock keep refuses a position that already holds something") {
    Fixture fixture{scratch("block-commands-keep")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:stone keep") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:dirt keep") == 0);
    CHECK(fixture.source.lastAsText().find("commands.setblock.failed") != std::string::npos);
    CHECK(fixture.world->state().blockState(1, 40, 1).value_or(0) == blocks::stone().stateId);
}

TEST_CASE("setblock refuses a position outside the world or outside a loaded chunk") {
    Fixture fixture{scratch("block-commands-bounds")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 5000 1 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("argument.pos.outofbounds") != std::string::npos);
    // parity: `BlockPosArgument.getLoadedBlockPos`, so a command never runs the generator.
    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 5000 40 5000 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("argument.pos.unloaded") != std::string::npos);
}

TEST_CASE("setblock of the state that is already there changes nothing") {
    Fixture fixture{scratch("block-commands-same")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:stone") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("commands.setblock.failed") != std::string::npos);
    // parity: `destroy` breaks the old block first, so writing the same state still counts.
    CHECK(fixture.dispatcher.execute(fixture.source, "setblock 1 40 1 minecraft:stone destroy") == 1);
}

TEST_CASE("fill writes the whole box and answers how many blocks moved") {
    Fixture fixture{scratch("block-commands-fill")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 42 2 minecraft:stone") == 27);
    CHECK(fixture.world->state().blockState(1, 41, 1).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.source.lastAsText().find("commands.fill.success") != std::string::npos);

    // Nothing left to change, so vanilla refuses rather than reporting a fill of none.
    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 42 2 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("commands.fill.failed") != std::string::npos);
}

TEST_CASE("fill outline writes only the shell, and hollow empties the inside") {
    Fixture fixture{scratch("block-commands-fill-shapes")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillCommand>());

    // A 3x3x3 box has 26 blocks on its surface and one inside.
    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 42 2 minecraft:stone outline") == 26);
    CHECK(fixture.world->state().blockState(1, 41, 1).value_or(0) == blocks::air().stateId);

    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 42 2 minecraft:dirt hollow") == 26);
    CHECK(fixture.world->state().blockState(0, 40, 0).value_or(0) == stateOf("dirt"));
    CHECK(fixture.world->state().blockState(1, 41, 1).value_or(0) == blocks::air().stateId);
}

TEST_CASE("fill replace with a filter writes only over what the filter matches") {
    Fixture fixture{scratch("block-commands-fill-filter")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 40 2 minecraft:stone"));
    REQUIRE(fixture.world->state().setBlockState(1, 40, 1, stateOf("dirt")));

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "fill 0 40 0 2 40 2 minecraft:gold_block replace minecraft:dirt") == 1);
    CHECK(fixture.world->state().blockState(1, 40, 1).value_or(0) == stateOf("gold_block"));
    CHECK(fixture.world->state().blockState(0, 40, 0).value_or(0) == blocks::stone().stateId);
}

TEST_CASE("fill keep writes only where there was air") {
    Fixture fixture{scratch("block-commands-fill-keep")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillCommand>());

    REQUIRE(fixture.world->state().setBlockState(1, 40, 1, stateOf("dirt")));
    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 40 2 minecraft:stone keep") == 8);
    CHECK(fixture.world->state().blockState(1, 40, 1).value_or(0) == stateOf("dirt"));
}

TEST_CASE("a fill wider than the game rule allows is refused before anything is written") {
    Fixture fixture{scratch("block-commands-fill-toobig")};
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.world->gameRules().set(basalt::server::runtime::world::gameRules::kMaxBlockModifications, "8");
    fixture.dispatcher.registerCommand(std::make_shared<commands::FillCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "fill 0 40 0 2 42 2 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("commands.fill.toobig") != std::string::npos);
    CHECK(fixture.world->state().blockState(1, 41, 1).value_or(0) == blocks::air().stateId);
}
