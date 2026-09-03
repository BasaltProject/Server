#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/EffectCommand.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
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

TEST_CASE("effect give carries the effect and tells the client about it") {
    Fixture fixture{scratch("effect-command-give")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EffectCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:speed") == 1);
    REQUIRE(steve.effects.contains(basalt::wire::common::Identifier::vanilla("speed")));
    // parity: thirty seconds when no length was named, at twenty ticks a second.
    CHECK(steve.effects.at(basalt::wire::common::Identifier::vanilla("speed")).remainingTicks == 600);
    CHECK(fixture.clients[0]->effects.size() == 1);
    CHECK(fixture.source.lastAsText().find("commands.effect.give.success.single") != std::string::npos);
}

TEST_CASE("effect give takes a length, a level and whether the particles show") {
    Fixture fixture{scratch("effect-command-give-forms")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EffectCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:speed 10 2 true") == 1);
    const auto& active = steve.effects.at(basalt::wire::common::Identifier::vanilla("speed"));
    CHECK(active.remainingTicks == 200);
    CHECK(active.amplifier == 2);
    // `hideParticles true` is what makes them invisible, so the flag on the effect is its opposite.
    CHECK_FALSE(active.visible);

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:speed infinite 1") == 1);
    CHECK(steve.effects.at(basalt::wire::common::Identifier::vanilla("speed")).infinite());
}

TEST_CASE("the two instantaneous effects act once and are never carried") {
    Fixture fixture{scratch("effect-command-instant")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EffectCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:instant_health") == 1);
    CHECK(steve.effects.empty());
    // parity: three hearts, doubled by each level.
    CHECK(fixture.clients[0]->healed == doctest::Approx(6.0F));

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:instant_damage 1 1") == 1);
    CHECK(fixture.clients[0]->hurt == doctest::Approx(12.0F));
    CHECK(steve.effects.empty());
}

TEST_CASE("an effect nobody registered is refused by name") {
    Fixture fixture{scratch("effect-command-unknown")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EffectCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:enlightenment") == 0);
    CHECK(fixture.source.lastAsText().find("effect.effectNotFound") != std::string::npos);
}

TEST_CASE("effect clear takes them all off, or only the one it names") {
    Fixture fixture{scratch("effect-command-clear")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::EffectCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:speed"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "effect give Steve minecraft:jump_boost"));

    CHECK(fixture.dispatcher.execute(fixture.source, "effect clear Steve minecraft:speed") == 1);
    CHECK(steve.effects.size() == 1);
    CHECK(fixture.clients[0]->removed.size() == 1);

    CHECK(fixture.dispatcher.execute(fixture.source, "effect clear Steve") == 1);
    CHECK(steve.effects.empty());
    CHECK(fixture.source.lastAsText().find("commands.effect.clear.everything.success.single") != std::string::npos);
}

TEST_CASE("world list names every loaded world and marks the one you are in") {
    Fixture fixture{scratch("world-command-list")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "world list") == 1);
    CHECK(flatten(fixture.source.said.front()).find("1 world(s) loaded") != std::string::npos);
    CHECK(fixture.source.lastAsText().find("world") != std::string::npos);
}

TEST_CASE("world info reports the facts an operator asks for") {
    Fixture fixture{scratch("world-command-info")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "world info world") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("world: world") != std::string::npos);
    CHECK(all.find("dimension: overworld") != std::string::npos);
    CHECK(all.find("chunks loaded: 0") != std::string::npos);
}

TEST_CASE("world moves whoever it names, and refuses when they are already there") {
    Fixture fixture{scratch("world-command-move")};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WorldCommand>());

    // Already in it, so a single target is refused rather than silently doing nothing.
    CHECK(fixture.dispatcher.execute(fixture.source, "world world Steve") == 0);
    CHECK(fixture.source.lastAsText().find("is already in world") != std::string::npos);

    steve.worldName = "elsewhere";
    CHECK(fixture.dispatcher.execute(fixture.source, "world world Steve") == 1);
    REQUIRE(fixture.clients[0]->switched.size() == 1);
    CHECK(fixture.clients[0]->switched[0] == "world");
    CHECK(fixture.source.lastAsText().find("Moved Steve to world.") != std::string::npos);
}
