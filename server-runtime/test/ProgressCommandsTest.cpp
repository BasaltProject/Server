#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/ProgressCommands.hpp>
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

namespace advancement = basalt::server::world::advancement;

using basalt::server::runtime::world::PlayerProgress;
using basalt::server::runtime::world::RealAdvancements;
using basalt::server::runtime::world::RealRecipes;
using basalt::wire::common::Identifier;

namespace {

/// A story with a root and two children, which is enough for all four selection modes.
[[nodiscard]] RealAdvancements testAdvancements() {
    const auto withCriteria = [](std::optional<Identifier> parent, std::vector<std::string> names) {
        advancement::Advancement built;
        built.parent = std::move(parent);
        for (std::string& name : names) {
            built.criteria.emplace_back(
                std::move(name), advancement::AdvancementCriterion{.trigger = Identifier::vanilla("impossible")});
        }
        return built;
    };
    std::map<Identifier, advancement::Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/root"), withCriteria(std::optional<Identifier>{}, {"first"}));
    definitions.emplace(Identifier::vanilla("story/mine"),
                        withCriteria(std::optional{Identifier::vanilla("story/root")}, {"stone", "coal"}));
    definitions.emplace(Identifier::vanilla("story/smelt"),
                        withCriteria(std::optional{Identifier::vanilla("story/mine")}, {"iron"}));
    return RealAdvancements{std::move(definitions)};
}

} // namespace

TEST_CASE("advancement grant only marks every criterion of the one it names") {
    Fixture fixture{scratch("advancement-only")};
    const RealAdvancements advancements = testAdvancements();
    PlayerProgress progress;
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AdvancementCommand>(advancements, progress));

    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve only minecraft:story/mine") == 2);
    CHECK(progress.grantedCriteria(steve.uuid().toString(), Identifier::vanilla("story/mine")).contains("stone"));
    CHECK(fixture.source.lastAsText().find("commands.advancement.grant.one.to.one.success") != std::string::npos);

    // Nothing left to grant, so vanilla refuses rather than reporting a grant of none.
    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve only minecraft:story/mine") == 0);
    CHECK(fixture.source.lastAsText().find("commands.advancement.grant.one.to.one.failure") != std::string::npos);
}

TEST_CASE("the four selection modes pick different parts of the tree") {
    Fixture fixture{scratch("advancement-modes")};
    const RealAdvancements advancements = testAdvancements();
    PlayerProgress progress;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AdvancementCommand>(advancements, progress));

    // `until` walks up to the root: mine has one criterion below it and one of its own pair.
    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve until minecraft:story/mine") == 3);
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "advancement revoke Steve everything"));

    // `from` walks down: mine's two plus smelt's one.
    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve from minecraft:story/mine") == 3);
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "advancement revoke Steve everything"));

    // `through` is both halves, without counting mine twice.
    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve through minecraft:story/mine") == 4);
}

TEST_CASE("advancement everything grants the whole book, and revoke takes it back") {
    Fixture fixture{scratch("advancement-everything")};
    const RealAdvancements advancements = testAdvancements();
    PlayerProgress progress;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AdvancementCommand>(advancements, progress));

    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve everything") == 4);
    // Revoking counts advancements rather than criteria, which is what vanilla's own key says.
    CHECK(fixture.dispatcher.execute(fixture.source, "advancement revoke Steve everything") == 3);
}

TEST_CASE("advancement only takes a single criterion by name") {
    Fixture fixture{scratch("advancement-criterion")};
    const RealAdvancements advancements = testAdvancements();
    PlayerProgress progress;
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AdvancementCommand>(advancements, progress));

    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve only minecraft:story/mine stone") == 1);
    CHECK(progress.grantedCriteria(steve.uuid().toString(), Identifier::vanilla("story/mine")).contains("stone"));
    CHECK_FALSE(progress.grantedCriteria(steve.uuid().toString(), Identifier::vanilla("story/mine")).contains("coal"));

    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve only minecraft:story/mine gold") == 0);
    CHECK(fixture.source.lastAsText().find("commands.advancement.criterionNotFound") != std::string::npos);
}

TEST_CASE("an advancement nobody declared is refused by name") {
    Fixture fixture{scratch("advancement-unknown")};
    const RealAdvancements advancements = testAdvancements();
    PlayerProgress progress;
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::AdvancementCommand>(advancements, progress));

    CHECK(fixture.dispatcher.execute(fixture.source, "advancement grant Steve only minecraft:story/fly") == 0);
    CHECK(fixture.source.lastAsText().find("advancement.advancementNotFound") != std::string::npos);
}

TEST_CASE("recipe give and take move what a player knows") {
    Fixture fixture{scratch("recipe-give")};
    PlayerProgress progress;
    // No jar, so the recipe book is empty and every id is accepted as vanilla's own would be.
    const RealRecipes recipes{RealRecipes::Jar{}};
    PlayerSession& steve = fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::RecipeCommand>(progress, recipes));

    CHECK(fixture.dispatcher.execute(fixture.source, "recipe give Steve minecraft:stick") == 1);
    CHECK(progress.knownRecipes(steve.uuid().toString()).contains(Identifier::vanilla("stick")));
    CHECK(fixture.source.lastAsText().find("commands.recipe.give.success.single") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "recipe give Steve minecraft:stick") == 0);
    CHECK(fixture.source.lastAsText().find("commands.recipe.give.failed") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "recipe take Steve minecraft:stick") == 1);
    CHECK_FALSE(progress.knownRecipes(steve.uuid().toString()).contains(Identifier::vanilla("stick")));
}
