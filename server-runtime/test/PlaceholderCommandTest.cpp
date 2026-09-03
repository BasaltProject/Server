#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/PlaceholderCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
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

    basalt::wire::common::placeholder::PlaceholderRegistry placeholders;

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.placeholders = &placeholders;
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

namespace play = basalt::wire::protocol::play;

namespace play = basalt::wire::protocol::play;

namespace placeholders = basalt::wire::common::placeholder;

namespace {

/// One provider that answers a name and refuses another, which is what the two failures look like.
void registerTestPlaceholders(placeholders::PlaceholderRegistry& registry) {
    registry.registerProvider(
        "test",
        [](std::string_view params, const placeholders::PlaceholderContext&) {
            if (params == "name") {
                return placeholders::PlaceholderValue::of(std::string{"Basalt"});
            }
            if (params == "later") {
                return placeholders::PlaceholderValue::unavailable("needs the inventory system");
            }
            return placeholders::PlaceholderValue::unknown();
        },
        [] { return std::vector<std::string>{"name", "later"}; });
}

} // namespace

TEST_CASE("placeholder parse fills a line in and reports what it could not") {
    Fixture fixture{scratch("placeholder-parse")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parse --null Hello %test_name%") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Hello Basalt") != std::string::npos);

    fixture.source.said.clear();
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parse --null %test_later%") == 1);
    all.clear();
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    // Kept as written, and the reason said out loud: a blank is exactly what nobody can debug.
    CHECK(all.find("needs the inventory system") != std::string::npos);
    CHECK(all.find("%test_later%") != std::string::npos);
}

TEST_CASE("a subject has to be online, and the console is not a player") {
    Fixture fixture{scratch("placeholder-subject")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parse Steve %test_name%") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parse Nobody %test_name%") == 0);
    CHECK(fixture.source.lastAsText().find("is not online") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parse me %test_name%") == 0);
    CHECK(fixture.source.lastAsText().find("The console is not a player") != std::string::npos);
}

TEST_CASE("bcparse reaches everybody rather than only the caller") {
    Fixture fixture{scratch("placeholder-bcparse")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder bcparse --null hi %test_name%") == 1);
    REQUIRE(fixture.clients[0]->messages.size() == 1);
    CHECK(flatten(fixture.clients[0]->messages[0]).find("hi Basalt") != std::string::npos);
}

TEST_CASE("cmdparse runs the line it built, as whoever typed it") {
    Fixture fixture{scratch("placeholder-cmdparse")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());
    fixture.dispatcher.registerCommand(std::make_shared<commands::SeedCommand>());

    // The leading slash is stripped before the line runs, and the hint prints it back as typed.
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "placeholder cmdparse --null /seed"));
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Running: //seed") != std::string::npos);
    CHECK(all.find("commands.seed.success") != std::string::npos);

    // Without the slash it runs just the same, which is the form the hint reads naturally for.
    fixture.source.said.clear();
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "placeholder cmdparse --null seed"));
    all.clear();
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Running: /seed") != std::string::npos);
    CHECK(all.find("commands.seed.success") != std::string::npos);
}

TEST_CASE("placeholder info says which identifier serves a name") {
    Fixture fixture{scratch("placeholder-info")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    // The percent signs are trimmed, since an operator pastes the name as it appears in a file.
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder info %test_name%") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("served by 'test'") != std::string::npos);
    CHECK(all.find("Here it says: Basalt") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder info nonsense_name") == 0);
    CHECK(fixture.source.lastAsText().find("is not a placeholder this server knows") != std::string::npos);
}

TEST_CASE("placeholder list names the identifiers, and one identifier's names") {
    Fixture fixture{scratch("placeholder-list")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder list") == 0);
    CHECK(fixture.source.lastAsText().find("Nothing has registered a placeholder") != std::string::npos);

    registerTestPlaceholders(fixture.placeholders);
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder list") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder list test") == 2);
    CHECK(fixture.source.lastAsText().find("%test_name%, %test_later%") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder list nothing") == 0);
}

TEST_CASE("parserel carries the second player for a plugin to read") {
    Fixture fixture{scratch("placeholder-parserel")};
    registerTestPlaceholders(fixture.placeholders);
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceholderCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "placeholder parserel Steve Alex %test_name%") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("Basalt") != std::string::npos);
    CHECK(all.find("No built-in placeholder is relational yet") != std::string::npos);
}
