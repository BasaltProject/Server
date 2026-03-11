#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/EntityCommands.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
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

using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;

namespace {

/// The entity a summon left behind, found by the type it was asked for.
[[nodiscard]] ServerEntity* summoned(Fixture& fixture, const std::string& path) {
    for (ServerEntity* what : fixture.world->state().entities()) {
        if (what->type() == Identifier::vanilla(path)) {
            return what;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("summon puts a real entity of the named type where it was asked for") {
    Fixture fixture{scratch("entity-summon")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));

    CHECK(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 10 64 -3") == 1);
    ServerEntity* pig = summoned(fixture, "pig");
    REQUIRE(pig != nullptr);
    // parity: a bare integer in a vec3 is centre corrected.
    CHECK(pig->x == doctest::Approx(10.5));
    CHECK(fixture.source.lastAsText().find("commands.summon.success") != std::string::npos);
}

TEST_CASE("summon refuses a player and a type nobody registered") {
    Fixture fixture{scratch("entity-summon-refused")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));

    CHECK(fixture.dispatcher.execute(fixture.source, "summon minecraft:player") == 0);
    CHECK(fixture.source.lastAsText().find("commands.summon.failed.uuid") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "summon minecraft:dragonfly") == 0);
    CHECK(fixture.source.lastAsText().find("commands.summon.failed") != std::string::npos);
}

TEST_CASE("summon reads the tag through the same decoder a saved entity goes through") {
    Fixture fixture{scratch("entity-summon-nbt")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));

    CHECK(fixture.dispatcher.execute(
              fixture.source,
              R"(summon minecraft:pig 0 64 0 {CustomName:'{"text":"Wilbur"}',Health:5.0f,Glowing:1b})") == 1);
    ServerEntity* pig = summoned(fixture, "pig");
    REQUIRE(pig != nullptr);
    // `CustomName` arrives as a text component and only its plain text becomes the name.
    REQUIRE(pig->customName.has_value());
    CHECK(*pig->customName == "Wilbur");
    CHECK(pig->health == doctest::Approx(5.0));
    // The whole tag stays on the entity even so: it is where a reader that knows a field looks, and
    // a chest minecart's `Items` is read from exactly there.
    REQUIRE(pig->sourceNbt.has_value());
    CHECK(pig->sourceNbt->getByte("Glowing", 0) == 1);
}

TEST_CASE("a summoned projectile flies the way its tag says") {
    Fixture fixture{scratch("entity-summon-motion")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));

    CHECK(fixture.dispatcher.execute(
              fixture.source, "summon minecraft:arrow 1.5 -60.0 0.5 {Motion:[0.6d,0.0d,0.0d],NoGravity:1b}") == 1);
    ServerEntity* arrow = summoned(fixture, "arrow");
    REQUIRE(arrow != nullptr);
    // parity: `SummonCommand` goes through the save codec, so `Motion` and `NoGravity` mean here what
    // they mean in a saved world. Building the entity by hand read `CustomName` and `Health` and
    // nothing else: measured in `flechavibra`, where the arrow hung in the air where it appeared.
    CHECK(arrow->velocityX == doctest::Approx(0.6));
    CHECK(arrow->velocityY == doctest::Approx(0.0));
    CHECK(arrow->noGravity);
}

TEST_CASE("damage takes health off an entity and kills it at zero") {
    Fixture fixture{scratch("entity-damage")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));
    fixture.dispatcher.registerCommand(std::make_shared<commands::DamageCommand>());
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 0 64 0"));
    ServerEntity* pig = summoned(fixture, "pig");
    REQUIRE(pig != nullptr);
    const double before = pig->health;

    CHECK(fixture.dispatcher.execute(fixture.source, "damage @e[type=minecraft:pig] 3") == 1);
    CHECK(pig->health == doctest::Approx(before - 3.0));
    CHECK(fixture.source.lastAsText().find("commands.damage.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "damage @e[type=minecraft:pig] 1000") == 1);
    CHECK(pig->killed);
    // parity: `LivingEntity.tickDeath`, twenty ticks of lying there. The body is not gone yet, and
    // everything that can feel an entity keeps feeling it until it is.
    CHECK(pig->dying);
    CHECK_FALSE(pig->removed);
}

TEST_CASE("ride mounts and dismounts, and refuses riding yourself") {
    Fixture fixture{scratch("entity-ride")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));
    fixture.dispatcher.registerCommand(std::make_shared<commands::RideCommand>());
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 0 64 0"));
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:oak_boat 0 64 0"));
    ServerEntity* pig = summoned(fixture, "pig");
    ServerEntity* boat = summoned(fixture, "oak_boat");
    REQUIRE(pig != nullptr);
    REQUIRE(boat != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source, "ride @e[type=minecraft:pig] mount @e[type=minecraft:oak_boat]") ==
          1);
    REQUIRE(pig->vehicleId.has_value());
    CHECK(*pig->vehicleId == boat->entityId());
    CHECK(boat->passengerIds.size() == 1);
    CHECK(fixture.clients[0]->sent(play::SetPassengersPacket::kId));

    CHECK(fixture.dispatcher.execute(fixture.source, "ride @e[type=minecraft:pig] dismount") == 1);
    CHECK_FALSE(pig->vehicleId.has_value());
    CHECK(boat->passengerIds.empty());
    CHECK(fixture.dispatcher.execute(fixture.source, "ride @e[type=minecraft:pig] dismount") == 0);
    CHECK(fixture.source.lastAsText().find("commands.ride.not_riding") != std::string::npos);
}

TEST_CASE("rotate takes an angle, a point or another entity") {
    Fixture fixture{scratch("entity-rotate")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));
    fixture.dispatcher.registerCommand(std::make_shared<commands::RotateCommand>());
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 0.0 64.0 0.0"));
    ServerEntity* pig = summoned(fixture, "pig");
    REQUIRE(pig != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source, "rotate @e[type=minecraft:pig] 90 45") == 1);
    CHECK(pig->yaw == doctest::Approx(90.0F));
    CHECK(pig->pitch == doctest::Approx(45.0F));

    // Due south is +Z, which vanilla writes as yaw zero.
    CHECK(fixture.dispatcher.execute(fixture.source, "rotate @e[type=minecraft:pig] facing 0.0 64.0 10.0") == 1);
    CHECK(pig->yaw == doctest::Approx(0.0F).epsilon(0.01));
    CHECK(pig->pitch == doctest::Approx(0.0F).epsilon(0.01));
}

TEST_CASE("swing tells everyone in the world which arm moved") {
    Fixture fixture{scratch("entity-swing")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::SwingCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "swing Steve offhand") == 1);
    CHECK(fixture.clients[0]->sent(play::AnimatePacket::kId));
    CHECK(fixture.source.lastAsText().find("commands.swing.success.single") != std::string::npos);
}

TEST_CASE("attribute reads and writes a base value, and resets it to the default") {
    Fixture fixture{scratch("entity-attribute")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));
    fixture.dispatcher.registerCommand(std::make_shared<commands::AttributeCommand>());
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 0 64 0"));
    ServerEntity* pig = summoned(fixture, "pig");
    REQUIRE(pig != nullptr);

    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "attribute @e[type=minecraft:pig] minecraft:max_health base set 30") == 30);
    CHECK(pig->attribute("minecraft:max_health") == doctest::Approx(30.0));
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "attribute @e[type=minecraft:pig] minecraft:max_health base get") == 30);
    // Back to the pig's own registered base rather than to the generic default of twenty.
    CHECK(fixture.dispatcher.execute(fixture.source,
                                     "attribute @e[type=minecraft:pig] minecraft:max_health base reset") == 10);

    CHECK(fixture.dispatcher.execute(fixture.source, "attribute @e[type=minecraft:pig] minecraft:nonsense base get") ==
          0);
    CHECK(fixture.source.lastAsText().find("attribute.unknown") != std::string::npos);
}

TEST_CASE("attribute modifiers apply in vanilla's own order") {
    Fixture fixture{scratch("entity-attribute-modifier")};
    fixture.dispatcher.registerCommand(std::make_shared<commands::SummonCommand>(fixture.registry));
    fixture.dispatcher.registerCommand(std::make_shared<commands::AttributeCommand>());
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "summon minecraft:pig 0 64 0"));

    const std::string prefix = "attribute @e[type=minecraft:pig] minecraft:max_health modifier ";
    CHECK(fixture.dispatcher.execute(fixture.source, prefix + "add test:flat 10 add_value") == 1);
    // A pig's own ten plus ten flat.
    CHECK(fixture.dispatcher.execute(fixture.source, "attribute @e[type=minecraft:pig] minecraft:max_health get") ==
          20);

    CHECK(fixture.dispatcher.execute(fixture.source, prefix + "add test:flat 10 add_value") == 0);
    CHECK(fixture.source.lastAsText().find("modifier_already_present") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, prefix + "value get test:flat 2") == 20);
    CHECK(fixture.dispatcher.execute(fixture.source, prefix + "remove test:flat") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, prefix + "remove test:flat") == 0);
    CHECK(fixture.source.lastAsText().find("commands.attribute.failed.no_modifier") != std::string::npos);
}

TEST_CASE("a player's attributes are a stated gap rather than a wrong number") {
    Fixture fixture{scratch("entity-attribute-player")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AttributeCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "attribute Steve minecraft:max_health get") == 0);
    CHECK(fixture.source.lastAsText().find("only works on a summoned entity") != std::string::npos);
}
