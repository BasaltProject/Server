#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/DataCommand.hpp>
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

using basalt::server::runtime::command::CommandStorage;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

TEST_CASE("data get reads a storage compound, whole or by path") {
    Fixture fixture{scratch("data-get")};
    CommandStorage storage{scratch("data-get-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    CHECK(fixture.dispatcher.execute(fixture.source, R"(data merge storage test:box {count:7,name:"hat"})") == 1);
    CHECK(fixture.source.lastAsText().find("commands.data.storage.modified") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box count") == 7);
    CHECK(fixture.source.lastAsText().find("commands.data.storage.query") != std::string::npos);
    // A whole compound has no number to report, so vanilla answers one.
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box") == 1);

    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box missing") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.nbtpath.nothing_found") != std::string::npos);
}

TEST_CASE("data get with a scale multiplies the number it found") {
    Fixture fixture{scratch("data-scale")};
    CommandStorage storage{scratch("data-scale-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "data merge storage test:box {count:7}"));
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box count 3") == 21);
    CHECK(fixture.source.lastAsText().find("commands.data.get.query.scaled") != std::string::npos);
}

TEST_CASE("data remove drops a path and refuses one that was never there") {
    Fixture fixture{scratch("data-remove")};
    CommandStorage storage{scratch("data-remove-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "data merge storage test:box {a:1,b:2}"));
    CHECK(fixture.dispatcher.execute(fixture.source, "data remove storage test:box a") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box a") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box b") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "data remove storage test:box a") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.nbtpath.nothing_found") != std::string::npos);
}

TEST_CASE("data modify set and merge write a value at a path") {
    Fixture fixture{scratch("data-modify")};
    CommandStorage storage{scratch("data-modify-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box a.b set value 5") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box a.b") == 5);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(data modify storage test:box c set value {x:1})"));
    static_cast<void>(
        fixture.dispatcher.execute(fixture.source, R"(data modify storage test:box c merge value {y:2})"));
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box c.x") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box c.y") == 2);
}

TEST_CASE("data modify append, prepend and insert keep one element type") {
    Fixture fixture{scratch("data-modify-list")};
    CommandStorage storage{scratch("data-modify-list-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box list append value 1") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box list append value 2") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box list prepend value 0") == 3);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box list[0]") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box list[2]") == 2);

    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box list insert 1 value 9") == 4);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:box list[1]") == 9);

    // parity: a list holds one type, so a string into a list of ints is refused.
    CHECK(fixture.dispatcher.execute(fixture.source, R"(data modify storage test:box list append value "no")") == 0);
    CHECK(fixture.source.lastAsText().find("commands.data.modify.expected_list") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "data modify storage test:box list insert 99 value 1") == 0);
    CHECK(fixture.source.lastAsText().find("commands.data.modify.invalid_index") != std::string::npos);
}

TEST_CASE("data modify from copies out of another holder") {
    Fixture fixture{scratch("data-modify-from")};
    CommandStorage storage{scratch("data-modify-from-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(data merge storage test:src {deep:{n:42}})"));
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "data modify storage test:dst here set from storage test:src deep.n") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:dst here") == 42);
}

TEST_CASE("data modify string takes the value as text, and a slice of it") {
    Fixture fixture{scratch("data-modify-string")};
    CommandStorage storage{scratch("data-modify-string-file") + "/command_storage.nbt"};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(data merge storage test:src {word:"basalt"})"));
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "data modify storage test:dst a set string storage test:src word 0 3") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "data get storage test:dst a") == 1);
    const NbtTag* value = storage.get(*Identifier::parse("test:dst")).find("a");
    REQUIRE(value != nullptr);
    REQUIRE(value->get<std::string>() != nullptr);
    CHECK(*value->get<std::string>() == "bas");

    // parity: a negative index counts back from the end.
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "data modify storage test:dst b set string storage test:src word -3") == 1);
    CHECK(*storage.get(*Identifier::parse("test:dst")).find("b")->get<std::string>() == "alt");

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "data modify storage test:dst c set string storage test:src word 4 1") == 0);
    CHECK(fixture.source.lastAsText().find("commands.data.modify.invalid_substring") != std::string::npos);
}

TEST_CASE("a player is a read-only holder, which the command says out loud") {
    Fixture fixture{scratch("data-player")};
    CommandStorage storage{scratch("data-player-file") + "/command_storage.nbt"};
    PlayerSession& steve = fixture.add("Steve");
    steve.x = 8.5;
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataCommand>(storage));

    CHECK(fixture.dispatcher.execute(fixture.source, "data get entity Steve Name") == 1);
    CHECK(fixture.source.lastAsText().find("commands.data.entity.query") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("Steve") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, R"(data merge entity Steve {Name:"Alex"})") == 0);
    CHECK(fixture.source.lastAsText().find("read-only view of live state") != std::string::npos);
}

TEST_CASE("storage survives a save and a load") {
    const std::string root = scratch("data-storage-file");
    {
        CommandStorage storage{root + "/command_storage.nbt"};
        NbtCompound value;
        value.putInt("count", 3);
        storage.set(*Identifier::parse("test:box"), value);
        storage.save();
    }
    CommandStorage reopened{root + "/command_storage.nbt"};
    reopened.load();
    CHECK(reopened.ids().size() == 1);
    CHECK(reopened.get(*Identifier::parse("test:box")).getInt("count") == 3);
}
