#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/GroupCommands.hpp>
#include <basalt/server/runtime/command/commands/RandomCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
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
namespace play = basalt::wire::protocol::play;

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
    std::int32_t trees = 0;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.sendCommandTree = [this] { trees++; };
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
    config::GroupsConfig groups{{config::PermissionGroup{.name = "default", .isDefault = true},
                                 config::PermissionGroup{.name = "operator", .isOperator = true}},
                                0,
                                1};
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
        built.playerRepository = store == nullptr ? nullptr : &store->players();
        built.groups = [this]() -> const config::GroupsConfig& { return groups; };
        return built;
    }

    PlayerSession& add(const std::string& name) {
        clients.push_back(std::make_unique<Client>());
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          clients.back()->connection()));
        players.back()->worldName = "world";
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

TEST_CASE("op puts an online player in the operator group and refreshes their tree") {
    Fixture fixture{scratch("group-commands-op")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::OpCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "op Steve") == 1);
    REQUIRE(steve.group.has_value());
    CHECK(*steve.group == fixture.groups.operatorGroup().name);
    CHECK(fixture.clients[0]->trees == 1);
    CHECK(fixture.source.lastAsText().find("commands.op.success") != std::string::npos);
}

TEST_CASE("deop puts them back in the default group rather than in none") {
    Fixture fixture{scratch("group-commands-deop")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::OpCommand>());
    fixture.dispatcher.registerCommand(std::make_shared<commands::DeopCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "op Steve") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "deop Steve") == 1);
    REQUIRE(steve.group.has_value());
    CHECK(*steve.group == fixture.groups.defaultGroup().name);
    CHECK(fixture.source.lastAsText().find("commands.deop.success") != std::string::npos);
}

TEST_CASE("op refuses a name nobody here has ever used") {
    Fixture fixture{scratch("group-commands-op-unknown")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::OpCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "op Nobody") == 0);
    CHECK(fixture.source.lastAsText().find("has ever joined") != std::string::npos);
}

TEST_CASE("group refuses a name groups.toml never declared") {
    Fixture fixture{scratch("group-commands-group-unknown")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GroupCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "group Steve wizards") == 0);
    CHECK(fixture.source.lastAsText().find("Unknown group 'wizards'") != std::string::npos);
}

TEST_CASE("group puts a player in any declared group, case folded") {
    Fixture fixture{scratch("group-commands-group")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GroupCommand>());

    const std::string wanted = fixture.groups.operatorGroup().name;
    std::string shouted = wanted;
    std::ranges::transform(shouted, shouted.begin(),
                           [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
    CHECK(fixture.dispatcher.execute(fixture.source, "group Steve " + shouted) == 1);
    REQUIRE(steve.group.has_value());
    CHECK(*steve.group == wanted);
}

TEST_CASE("random value draws inside the range it was given") {
    Fixture fixture{scratch("random-command-value")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    for (int attempt = 0; attempt < 8; attempt++) {
        const std::int32_t rolled = fixture.dispatcher.execute(fixture.source, "random value 1..6 test:dice");
        CHECK(rolled >= 1);
        CHECK(rolled <= 6);
    }
    CHECK(fixture.source.lastAsText().find("commands.random.sample.success") != std::string::npos);
}

TEST_CASE("a bare number is read as one to that number") {
    Fixture fixture{scratch("random-command-bare")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    for (int attempt = 0; attempt < 8; attempt++) {
        const std::int32_t rolled = fixture.dispatcher.execute(fixture.source, "random value 3 test:dice");
        CHECK(rolled >= 1);
        CHECK(rolled <= 3);
    }
}

TEST_CASE("a named sequence advances while the unnamed one repeats") {
    Fixture fixture{scratch("random-command-sequence")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    // The unnamed stream is built fresh from the world seed on every draw, so it answers the same
    // number every time. parity: vanilla's own `/random value` with no sequence.
    const std::int32_t first = fixture.dispatcher.execute(fixture.source, "random value 1..1000000");
    CHECK(fixture.dispatcher.execute(fixture.source, "random value 1..1000000") == first);

    std::vector<std::int32_t> named;
    for (int attempt = 0; attempt < 4; attempt++) {
        named.push_back(fixture.dispatcher.execute(fixture.source, "random value 1..1000000 test:dice"));
    }
    CHECK(std::ranges::find_if(named, [&named](std::int32_t value) { return value != named.front(); }) != named.end());
}

TEST_CASE("random reset forgets a sequence, so it starts over") {
    Fixture fixture{scratch("random-command-reset")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    const std::int32_t first = fixture.dispatcher.execute(fixture.source, "random value 1..1000000 test:dice");
    CHECK(fixture.dispatcher.execute(fixture.source, "random value 1..1000000 test:dice") != first);
    CHECK(fixture.dispatcher.execute(fixture.source, "random reset test:dice") == 1);
    CHECK(fixture.source.lastAsText().find("commands.random.reset.success") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "random value 1..1000000 test:dice") == first);
}

TEST_CASE("random reset star answers how many it forgot") {
    Fixture fixture{scratch("random-command-reset-all")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "random value 1..6 test:one"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "random value 1..6 test:two"));
    CHECK(fixture.dispatcher.execute(fixture.source, "random reset *") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "random reset *") == 0);
}

TEST_CASE("random refuses a range that is backwards or too wide to draw from") {
    Fixture fixture{scratch("random-command-range")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "random value 6..1") == 0);
    CHECK(fixture.source.lastAsText().find("commands.random.error.range_too_small") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "random value ..") == 0);
    CHECK(fixture.source.lastAsText().find("commands.random.error.range_too_large") != std::string::npos);
}

TEST_CASE("random roll tells everybody rather than only the caller") {
    Fixture fixture{scratch("random-command-roll")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::RandomCommand>([] { return 42; }));

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "random roll 1..6"));
    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("commands.random.roll") != std::string::npos);
}
