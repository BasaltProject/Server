#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/AccessCommands.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
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
    std::vector<Component> disconnects;
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
        connection.address = "10.0.0.7";
        connection.disconnect = [this](const Component& reason) { disconnects.push_back(reason); };
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
        built.access = store == nullptr ? nullptr : &store->access();
        built.playerRepository = store == nullptr ? nullptr : &store->players();
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

using basalt::server::storage::StoredItemStack;

using basalt::server::storage::BanKind;

TEST_CASE("ban writes the record and disconnects whoever it names") {
    Fixture fixture{scratch("access-ban")};
    PlayerSession& steve = fixture.add("Steve");
    REQUIRE(fixture.store != nullptr);
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanCommand>([] { return 1000; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "ban Steve being rude") == 1);
    const auto record = fixture.store->access().banOf(steve.uuid().toString(), BanKind::Player);
    REQUIRE(record.has_value());
    CHECK(record->reason == "being rude");
    CHECK(record->createdAtEpochSeconds == 1000);
    CHECK(fixture.clients[0]->disconnects.size() == 1);
    CHECK(fixture.source.lastAsText().find("commands.ban.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "ban Steve") == 0);
    CHECK(fixture.source.lastAsText().find("commands.ban.failed") != std::string::npos);
}

TEST_CASE("ban with no reason uses the operator's default") {
    Fixture fixture{scratch("access-ban-default")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanCommand>([] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "ban Steve") == 1);
    CHECK(fixture.store->access().banOf(steve.uuid().toString(), BanKind::Player)->reason == "Banned by an operator.");
}

TEST_CASE("ban a name nobody has played under still writes a record") {
    Fixture fixture{scratch("access-ban-offline")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanCommand>([] { return 0; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "ban Nobody") == 1);
    // The offline UUID is what vanilla resolves an unseen name to, so the ban lands when they join.
    const std::string uuid = basalt::wire::common::Uuid::offlinePlayer("Nobody").toString();
    CHECK(fixture.store->access().banOf(uuid, BanKind::Player).has_value());
}

TEST_CASE("ban-ip takes an address, or the address of somebody online") {
    Fixture fixture{scratch("access-ban-ip")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanIpCommand>([] { return 0; }));

    // One connection is on that address, so the count is that plus the ban itself.
    CHECK(fixture.dispatcher.execute(fixture.source, "ban-ip Steve") == 2);
    CHECK(fixture.store->access().banOf("10.0.0.7", BanKind::Ip).has_value());
    CHECK(fixture.clients[0]->disconnects.size() == 1);

    CHECK(fixture.dispatcher.execute(fixture.source, "ban-ip 10.0.0.7") == 0);
    CHECK(fixture.source.lastAsText().find("commands.banip.failed") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "ban-ip 192.168.1.4") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "ban-ip Nobody") == 0);
    CHECK(fixture.source.lastAsText().find("commands.banip.invalid") != std::string::npos);
}

TEST_CASE("pardon lifts a ban and refuses when there was none") {
    Fixture fixture{scratch("access-pardon")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanCommand>([] { return 0; }));
    fixture.dispatcher.registerCommand(std::make_shared<commands::PardonCommand>(false));
    fixture.dispatcher.registerCommand(std::make_shared<commands::PardonCommand>(true));

    CHECK(fixture.dispatcher.execute(fixture.source, "pardon Steve") == 0);
    CHECK(fixture.source.lastAsText().find("commands.pardon.failed") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "ban Steve"));
    CHECK(fixture.dispatcher.execute(fixture.source, "pardon Steve") == 1);
    CHECK(fixture.source.lastAsText().find("commands.pardon.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "pardon-ip 10.0.0.7") == 0);
    CHECK(fixture.source.lastAsText().find("commands.pardonip.failed") != std::string::npos);
}

TEST_CASE("banlist reads both kinds, together or apart") {
    Fixture fixture{scratch("access-banlist")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanCommand>([] { return 0; }));
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanIpCommand>([] { return 0; }));
    fixture.dispatcher.registerCommand(std::make_shared<commands::BanListCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "banlist") == 0);
    CHECK(fixture.source.lastAsText().find("commands.banlist.none") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "ban Steve"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "ban-ip 1.2.3.4"));
    CHECK(fixture.dispatcher.execute(fixture.source, "banlist") == 2);
    CHECK(fixture.dispatcher.execute(fixture.source, "banlist players") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "banlist ips") == 1);
}

TEST_CASE("whitelist add and remove keep the list, and on and off the flag") {
    Fixture fixture{scratch("access-whitelist")};
    fixture.add("Steve");
    bool enforced = false;
    fixture.dispatcher.registerCommand(std::make_shared<commands::WhitelistCommand>(
        [&enforced] { return enforced; }, [&enforced](bool value) { enforced = value; }, [] { return 5; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist off") == 0);
    CHECK(fixture.source.lastAsText().find("commands.whitelist.alreadyOff") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist on") == 1);
    CHECK(enforced);

    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.whitelist.none") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist add Steve") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist list") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist add Steve") == 0);
    CHECK(fixture.source.lastAsText().find("commands.whitelist.add.failed") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist remove Steve") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist remove Steve") == 0);
    CHECK(fixture.source.lastAsText().find("commands.whitelist.remove.failed") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "whitelist reload") == 1);
}
