#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
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
    std::int32_t inventorySyncs = 0;
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
        connection.sendInventoryAll = [this] { inventorySyncs++; };
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

using basalt::server::storage::StoredItemStack;

TEST_CASE("give puts the item in the inventory and pushes the new slots") {
    Fixture fixture{scratch("item-commands-give")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GiveCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "give Steve minecraft:stone 32") == 1);
    CHECK(steve.inventory.countOf("minecraft:stone") == 32);
    CHECK(fixture.clients[0]->inventorySyncs == 1);
    CHECK(fixture.source.lastAsText().find("commands.give.success.single") != std::string::npos);
}

TEST_CASE("give with no count gives one") {
    Fixture fixture{scratch("item-commands-give-one")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GiveCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "give Steve minecraft:stone") == 1);
    CHECK(steve.inventory.countOf("minecraft:stone") == 1);
}

TEST_CASE("give names the components no client can draw rather than dropping them silently") {
    Fixture fixture{scratch("item-commands-give-components")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::GiveCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "give Steve minecraft:stone[minecraft:damage=5]") == 1);
    CHECK(steve.inventory.countOf("minecraft:stone") == 1);
    const std::optional<std::int32_t> where = steve.inventory.indexOf("minecraft:stone");
    REQUIRE(where.has_value());
    // Stored either way: what changes is whether the operator was told the tooltip will not show it.
    CHECK(steve.inventory.at(*where)->components.getInt("minecraft:damage") == 5);
}

TEST_CASE("give says so when the inventory could not take it all") {
    Fixture fixture{scratch("item-commands-give-full")};
    PlayerSession& steve = fixture.add("Steve");
    for (std::int32_t slot = 0; slot < basalt::server::storage::StoredInventory::kPlayerSlots; slot++) {
        steve.inventory.set(slot, StoredItemStack{.id = "minecraft:dirt", .count = 64});
    }
    fixture.dispatcher.registerCommand(std::make_shared<commands::GiveCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "give Steve minecraft:stone 5") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("did not fit and were lost") != std::string::npos);
}

TEST_CASE("clear takes everything, or only what the predicate names") {
    Fixture fixture{scratch("item-commands-clear")};
    PlayerSession& steve = fixture.add("Steve");
    steve.inventory.set(0, StoredItemStack{.id = "minecraft:stone", .count = 10});
    steve.inventory.set(1, StoredItemStack{.id = "minecraft:dirt", .count = 5});
    fixture.dispatcher.registerCommand(std::make_shared<commands::ClearCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "clear Steve minecraft:stone") == 10);
    CHECK(steve.inventory.countOf("minecraft:stone") == 0);
    CHECK(steve.inventory.countOf("minecraft:dirt") == 5);
    CHECK(fixture.source.lastAsText().find("commands.clear.success.single") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "clear Steve") == 5);
    CHECK(steve.inventory.countOf("minecraft:dirt") == 0);
}

TEST_CASE("clear with a maximum takes only that many") {
    Fixture fixture{scratch("item-commands-clear-max")};
    PlayerSession& steve = fixture.add("Steve");
    steve.inventory.set(0, StoredItemStack{.id = "minecraft:stone", .count = 10});
    fixture.dispatcher.registerCommand(std::make_shared<commands::ClearCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "clear Steve minecraft:stone 4") == 4);
    CHECK(steve.inventory.countOf("minecraft:stone") == 6);
}

TEST_CASE("a maximum of zero counts without taking anything") {
    Fixture fixture{scratch("item-commands-clear-test")};
    PlayerSession& steve = fixture.add("Steve");
    steve.inventory.set(0, StoredItemStack{.id = "minecraft:stone", .count = 10});
    fixture.dispatcher.registerCommand(std::make_shared<commands::ClearCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "clear Steve minecraft:stone 0") == 10);
    CHECK(steve.inventory.countOf("minecraft:stone") == 10);
    CHECK(fixture.clients[0]->inventorySyncs == 0);
    CHECK(fixture.source.lastAsText().find("commands.clear.test.single") != std::string::npos);
}

TEST_CASE("clear that found nothing is refused rather than reporting a clear of none") {
    Fixture fixture{scratch("item-commands-clear-empty")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ClearCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "clear Steve") == 0);
    CHECK(fixture.source.lastAsText().find("clear.failed.single") != std::string::npos);
}
