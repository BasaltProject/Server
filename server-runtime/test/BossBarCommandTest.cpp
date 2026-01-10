#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
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

namespace play = basalt::wire::protocol::play;

using basalt::wire::common::Identifier;

TEST_CASE("bossbar add makes a bar and refuses a second one under the same id") {
    Fixture fixture{scratch("bossbar-add")};
    commands::BossBarRegistry registry;
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));

    CHECK(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})") == 1);
    CHECK(registry.get(Identifier::vanilla("raid")) == nullptr);
    REQUIRE(registry.get(*Identifier::parse("test:raid")) != nullptr);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.create.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.create.failed") != std::string::npos);
}

TEST_CASE("bossbar list names every bar and answers how many there are") {
    Fixture fixture{scratch("bossbar-list")};
    commands::BossBarRegistry registry;
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.list.bars.none") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:one {"text":"One"})"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:two {"text":"Two"})"));
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar list") == 2);
    // In insertion order, which is what vanilla's own map keeps.
    CHECK(fixture.source.lastAsText().find("test:one, test:two") != std::string::npos);
}

TEST_CASE("a bar nobody made is refused by name") {
    Fixture fixture{scratch("bossbar-unknown")};
    commands::BossBarRegistry registry;
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar get test:nothing value") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.unknown") != std::string::npos);
}

TEST_CASE("bossbar set moves each field once and refuses a repeat") {
    Fixture fixture{scratch("bossbar-set")};
    commands::BossBarRegistry registry;
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})"));
    commands::CustomBossBar* bar = registry.get(*Identifier::parse("test:raid"));
    REQUIRE(bar != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid value 40") == 40);
    CHECK(bar->value == 40);
    CHECK(bar->progress() == doctest::Approx(0.4F));
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid value 40") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.set.value.unchanged") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid max 200") == 200);
    CHECK(bar->progress() == doctest::Approx(0.2F));

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid color red") == 0);
    CHECK(bar->color == basalt::wire::protocol::play::BossBarColor::Red);
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid style notched_6") == 0);
    CHECK(bar->overlay == basalt::wire::protocol::play::BossBarOverlay::Notched6);
}

TEST_CASE("bossbar players adds and drops viewers, telling each of them") {
    Fixture fixture{scratch("bossbar-players")};
    commands::BossBarRegistry registry;
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})"));

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid players @a") == 2);
    CHECK(fixture.clients[0]->sent(basalt::wire::protocol::play::BossEventPacket::kId));
    CHECK(fixture.clients[1]->sent(basalt::wire::protocol::play::BossEventPacket::kId));
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar get test:raid players") == 2);

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid players Steve") == 1);
    CHECK(registry.get(*Identifier::parse("test:raid"))->players == std::set<std::string>{"Steve"});
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid players") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.set.players.success.none") != std::string::npos);
}

TEST_CASE("a hidden bar sends nothing until it is shown again") {
    Fixture fixture{scratch("bossbar-visible")};
    commands::BossBarRegistry registry;
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid players Steve"));

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid visible false") == 0);
    CHECK(registry.barsFor(steve).empty());
    const std::size_t sentSoFar = fixture.clients[0]->packets.size();
    // Hidden, so a change to it reaches nobody.
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid value 10") == 10);
    CHECK(fixture.clients[0]->packets.size() == sentSoFar);

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid visible true") == 0);
    CHECK(registry.barsFor(steve).size() == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid visible true") == 0);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.set.visibility.unchanged.visible") != std::string::npos);
}

TEST_CASE("bossbar remove takes the bar off every screen showing it") {
    Fixture fixture{scratch("bossbar-remove")};
    commands::BossBarRegistry registry;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::BossBarCommand>(registry));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(bossbar add test:raid {"text":"Raid"})"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "bossbar set test:raid players Steve"));
    const std::size_t sentSoFar = fixture.clients[0]->packets.size();

    CHECK(fixture.dispatcher.execute(fixture.source, "bossbar remove test:raid") == 0);
    CHECK(fixture.clients[0]->packets.size() > sentSoFar);
    CHECK(registry.get(*Identifier::parse("test:raid")) == nullptr);
    CHECK(fixture.source.lastAsText().find("commands.bossbar.remove.success") != std::string::npos);
}
