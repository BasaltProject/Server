#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/BlockCommands.hpp>
#include <basalt/server/runtime/command/commands/ExecuteCommand.hpp>
#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>
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
    /// A second world, for what `execute in` has to reach.
    std::unique_ptr<LoadedWorld> other;
    std::unique_ptr<basalt::server::storage::PlayerStore> store;
    CommandEnvironment environment;
    CommandDispatcher dispatcher;
    RecordingSource source;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})),
          other(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "other", .type = config::WorldType::Flat}, config::WorldSettings{},
              WorldMetadata{}, root + "/other", basalt::wire::common::Logger{"Test"})),
          store(openStore(root)), environment(registry, options()), dispatcher(environment) {}

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.worlds = {{"world", world.get()}, {"other", other.get()}};
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

using basalt::server::runtime::command::CommandStorage;
using basalt::server::runtime::command::DataAccess;
using basalt::server::runtime::scoreboard::Scoreboard;
using basalt::wire::common::Identifier;

TEST_CASE("execute as and at change who and where the rest of the line runs") {
    Fixture fixture{scratch("execute-as")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    PlayerSession& steve = fixture.add("Steve");
    steve.x = 8.0;
    steve.y = 40.0;
    steve.z = 8.0;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    // `at` takes the target's position, so `~ ~ ~` is theirs rather than the console's.
    CHECK(fixture.dispatcher.execute(fixture.source, "execute at Steve run setblock ~ ~ ~ minecraft:stone") == 1);
    CHECK(fixture.world->state().blockState(8, 40, 8).value_or(0) == blocks::stone().stateId);

    // `@s` after `as` is whoever `as` handed on, once per player, not the console that typed it.
    PlayerSession& alex = fixture.add("Alex");
    alex.x = 4.0;
    alex.y = 40.0;
    alex.z = 4.0;
    CHECK(fixture.dispatcher.execute(fixture.source, "execute as @a at @s run setblock ~ ~2 ~ minecraft:stone") == 2);
    CHECK(fixture.world->state().blockState(8, 42, 8).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.world->state().blockState(4, 42, 4).value_or(0) == blocks::stone().stateId);
}

TEST_CASE("execute positioned and align move the position the rest of the line sees") {
    Fixture fixture{scratch("execute-positioned")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "execute positioned 3.7 40.9 5.2 run setblock ~ ~ ~ minecraft:stone") == 1);
    CHECK(fixture.world->state().blockState(3, 40, 5).value_or(0) == blocks::stone().stateId);

    // `align` floors the axes it names and leaves the rest alone.
    CHECK(fixture.dispatcher.execute(
              fixture.source, "execute positioned 3.7 40.9 5.2 align xyz run setblock ~ ~ ~ minecraft:dirt") == 1);
    CHECK(fixture.world->state().blockState(3, 40, 5).value_or(0) == stateOf("dirt"));
}

TEST_CASE("execute in runs the line in the world it names") {
    Fixture fixture{scratch("execute-in")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "execute in world run setblock 1 40 1 minecraft:stone") == 1);
    CHECK(fixture.world->state().blockState(1, 40, 1).value_or(0) == blocks::stone().stateId);

    // The world the console stands in is not the one the line was told to act in.
    static_cast<void>(fixture.other->state().getOrGenerateChunk(0, 0));
    CHECK(fixture.dispatcher.execute(fixture.source, "execute in other run setblock 2 40 2 minecraft:stone") == 1);
    CHECK(fixture.other->state().blockState(2, 40, 2).value_or(0) == blocks::stone().stateId);
    CHECK(fixture.world->state().blockState(2, 40, 2).value_or(0) != blocks::stone().stateId);
}

TEST_CASE("if and unless are the same test read two ways") {
    Fixture fixture{scratch("execute-if")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    REQUIRE(fixture.world->state().setBlockState(1, 40, 1, blocks::stone().stateId));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));

    CHECK(fixture.dispatcher.execute(fixture.source, "execute if block 1 40 1 minecraft:stone") == 1);
    CHECK(fixture.source.lastAsText().find("commands.execute.conditional.pass_count") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute unless block 1 40 1 minecraft:stone") == 0);
    CHECK(fixture.source.lastAsText().find("commands.execute.conditional.fail") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "execute if block 1 41 1 minecraft:stone") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute unless block 1 41 1 minecraft:stone") == 1);
}

TEST_CASE("a condition gates the rest of the line") {
    Fixture fixture{scratch("execute-gate")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SetBlockCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "execute if block 1 40 1 minecraft:air run setblock 2 40 2 minecraft:stone") == 1);
    CHECK(fixture.world->state().blockState(2, 40, 2).value_or(0) == blocks::stone().stateId);

    CHECK(fixture.dispatcher.execute(
              fixture.source, "execute if block 1 40 1 minecraft:stone run setblock 3 40 3 minecraft:stone") == 0);
    CHECK(fixture.world->state().blockState(3, 40, 3).value_or(0) == blocks::air().stateId);
}

TEST_CASE("if blocks counts what matched and masked ignores air in the source") {
    Fixture fixture{scratch("execute-if-blocks")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));

    REQUIRE(fixture.world->state().setBlockState(0, 40, 0, blocks::stone().stateId));
    REQUIRE(fixture.world->state().setBlockState(4, 40, 0, blocks::stone().stateId));
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if blocks 0 40 0 0 40 0 4 40 0 all") == 1);

    // The second block of the source is air and the destination's is not, so `all` fails.
    REQUIRE(fixture.world->state().setBlockState(5, 40, 0, stateOf("dirt")));
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if blocks 0 40 0 1 40 0 4 40 0 all") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if blocks 0 40 0 1 40 0 4 40 0 masked") == 1);
}

TEST_CASE("if score compares two scores and matches a range") {
    Fixture fixture{scratch("execute-if-score")};
    Scoreboard scoreboard;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    REQUIRE(scoreboard.add({.name = "a", .criterion = "dummy"}));
    scoreboard.setScore("#x", "a", 5);
    scoreboard.setScore("#y", "a", 9);

    CHECK(fixture.dispatcher.execute(fixture.source, "execute if score #x a < #y a") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if score #x a > #y a") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if score #x a matches 1..6") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if score #x a matches 6..") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if score #x a matches 5") == 1);
}

TEST_CASE("if entity, dimension and loaded read the world the source acts in") {
    Fixture fixture{scratch("execute-if-world")};
    Scoreboard scoreboard;
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));

    CHECK(fixture.dispatcher.execute(fixture.source, "execute if entity @a") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if dimension world") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if loaded 1 40 1") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if loaded 5000 40 5000") == 0);
}

TEST_CASE("store result writes what the line answered, and store success writes whether it worked") {
    Fixture fixture{scratch("execute-store")};
    Scoreboard scoreboard;
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ListPlayersCommand>());
    REQUIRE(scoreboard.add({.name = "a", .criterion = "dummy"}));

    CHECK(fixture.dispatcher.execute(fixture.source, "execute store result score #n a run list") == 2);
    CHECK(scoreboard.score("#n", "a") == 2);

    CHECK(fixture.dispatcher.execute(fixture.source, "execute store success score #ok a run list") == 2);
    CHECK(scoreboard.score("#ok", "a") == 1);
}

TEST_CASE("store storage writes the typed number the scale produced") {
    Fixture fixture{scratch("execute-store-storage")};
    Scoreboard scoreboard;
    CommandStorage storage{scratch("execute-store-file") + "/command_storage.nbt"};
    const DataAccess access{storage};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::ExecuteCommand>(scoreboard, commands::ExecuteCommand::Hooks{.data = &access}));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ListPlayersCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "execute store result storage test:box count int 10 run list") ==
          1);
    CHECK(storage.get(*Identifier::parse("test:box")).getInt("count") == 10);
}

TEST_CASE("if data reads a path out of a holder") {
    Fixture fixture{scratch("execute-if-data")};
    Scoreboard scoreboard;
    CommandStorage storage{scratch("execute-if-data-file") + "/command_storage.nbt"};
    basalt::wire::nbt::NbtCompound value;
    value.putInt("count", 1);
    storage.set(*Identifier::parse("test:box"), value);
    const DataAccess access{storage};
    fixture.dispatcher.registerCommand(
        std::make_shared<commands::ExecuteCommand>(scoreboard, commands::ExecuteCommand::Hooks{.data = &access}));

    CHECK(fixture.dispatcher.execute(fixture.source, "execute if data storage test:box count") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if data storage test:box missing") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute unless data storage test:box missing") == 1);
}

TEST_CASE("the clauses that need a system this build has not got read as false") {
    Fixture fixture{scratch("execute-inert")};
    Scoreboard scoreboard;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ExecuteCommand>(scoreboard));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SeedCommand>());

    // Parsed and false rather than missing from the tree, so the line is not a syntax error.
    CHECK(fixture.dispatcher.execute(fixture.source, "execute if predicate test:always run seed") == 0);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Unknown command") == std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "execute on owner run seed") == 0);
}
