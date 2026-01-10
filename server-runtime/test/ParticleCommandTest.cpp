#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/ParticleArgument.hpp>
#include <basalt/server/runtime/command/commands/ParticleCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
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
    std::vector<std::array<double, 5>> teleports;
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
        connection.teleportTo = [this](double x, double y, double z, float yaw, float pitch) {
            teleports.push_back({x, y, z, static_cast<double>(yaw), static_cast<double>(pitch)});
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

namespace play = basalt::wire::protocol::play;

using basalt::server::runtime::command::ParsedParticle;
using basalt::server::runtime::command::StringReader;

namespace {

/// Parses one particle argument on its own, which is where every option shape is decided.
[[nodiscard]] std::optional<ParsedParticle> parseParticle(Fixture& fixture, const std::string& input) {
    StringReader reader{input};
    const auto value =
        basalt::server::runtime::command::particleArgument()->parse(reader, fixture.source, fixture.environment);
    if (!value.has_value()) {
        return std::nullopt;
    }
    const auto* parsed = value->as<ParsedParticle>();
    return parsed == nullptr ? std::nullopt : std::optional{*parsed};
}

} // namespace

TEST_CASE("a particle with no options parses to the type alone") {
    Fixture fixture{scratch("particle-plain")};
    const std::optional<ParsedParticle> parsed = parseParticle(fixture, "minecraft:flame");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->type != nullptr);
    CHECK(parsed->type->id == basalt::wire::common::Identifier::vanilla("flame"));
    CHECK_FALSE(parsed->options.has_value());

    CHECK_FALSE(parseParticle(fixture, "minecraft:sparkles").has_value());
}

TEST_CASE("a block particle takes its state either way round") {
    Fixture fixture{scratch("particle-block")};
    const std::optional<ParsedParticle> plain = parseParticle(fixture, R"(minecraft:block{block_state:"stone"})");
    REQUIRE(plain.has_value());
    REQUIRE(plain->options.has_value());
    const auto* state = plain->options->as<play::ParticleOptions::BlockState>();
    REQUIRE(state != nullptr);
    CHECK(state->stateId == basalt::server::registry::blocks::stone().stateId);

    const std::optional<ParsedParticle> compound =
        parseParticle(fixture, R"(minecraft:block{block_state:{Name:"stone"}})");
    REQUIRE(compound.has_value());
    CHECK(compound->options->as<play::ParticleOptions::BlockState>()->stateId ==
          basalt::server::registry::blocks::stone().stateId);

    CHECK_FALSE(parseParticle(fixture, R"(minecraft:block{block_state:"cheese"})").has_value());
    CHECK_FALSE(parseParticle(fixture, "minecraft:block").has_value());
}

TEST_CASE("a colour is an int or three floats, packed the way vanilla packs one") {
    Fixture fixture{scratch("particle-colour")};
    const std::optional<ParsedParticle> packed = parseParticle(fixture, "minecraft:entity_effect{color:16711680}");
    REQUIRE(packed.has_value());
    CHECK(packed->options->as<play::ParticleOptions::Color>()->color == 16711680);

    // parity: `ARGB.colorFromFloat`, so a fully opaque red is 0xFFFF0000.
    const std::optional<ParsedParticle> floats =
        parseParticle(fixture, "minecraft:entity_effect{color:[1.0f,0.0f,0.0f]}");
    REQUIRE(floats.has_value());
    CHECK(floats->options->as<play::ParticleOptions::Color>()->color == static_cast<std::int32_t>(0xFFFF0000));
}

TEST_CASE("dust takes a colour and a scale, and spell defaults both of its fields") {
    Fixture fixture{scratch("particle-dust")};
    const std::optional<ParsedParticle> dust =
        parseParticle(fixture, "minecraft:dust{color:[1.0f,1.0f,1.0f],scale:2.0f}");
    REQUIRE(dust.has_value());
    const auto* options = dust->options->as<play::ParticleOptions::Dust>();
    REQUIRE(options != nullptr);
    CHECK(options->scale == doctest::Approx(2.0F));

    // parity: both of spell's fields are optional, defaulting to -1 and a power of one.
    const std::optional<ParsedParticle> spell = parseParticle(fixture, "minecraft:effect");
    REQUIRE(spell.has_value());
    const auto* cast = spell->options->as<play::ParticleOptions::Spell>();
    REQUIRE(cast != nullptr);
    CHECK(cast->color == -1);
    CHECK(cast->power == doctest::Approx(1.0F));
}

TEST_CASE("a geyser needs the field it is measured by") {
    Fixture fixture{scratch("particle-geyser")};
    CHECK_FALSE(parseParticle(fixture, "minecraft:geyser").has_value());
    const std::optional<ParsedParticle> parsed = parseParticle(fixture, "minecraft:geyser{water_blocks:4}");
    REQUIRE(parsed.has_value());
    CHECK(parsed->options->as<play::ParticleOptions::Geyser>()->waterBlocks == 4);
}

TEST_CASE("particle reaches whoever is in the world, or the viewers it names") {
    Fixture fixture{scratch("particle-command")};
    fixture.add("Steve");
    fixture.add("Alex");
    fixture.dispatcher.registerCommand(std::make_shared<commands::ParticleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "particle minecraft:flame") == 2);
    CHECK(fixture.clients[0]->sent(play::LevelParticlesPacket::kId));
    CHECK(fixture.clients[1]->sent(play::LevelParticlesPacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.particle.success") != std::string::npos);

    fixture.clients[1]->packets.clear();
    CHECK(fixture.dispatcher.execute(fixture.source, "particle minecraft:flame 0 64 0 1 1 1 0.1 5 force Steve") == 1);
    CHECK_FALSE(fixture.clients[1]->sent(play::LevelParticlesPacket::kId));
}

TEST_CASE("a particle nobody can see is refused rather than reported as sent") {
    Fixture fixture{scratch("particle-nobody")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::ParticleCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "particle minecraft:flame") == 0);
    CHECK(fixture.source.lastAsText().find("commands.particle.failed") != std::string::npos);
}
