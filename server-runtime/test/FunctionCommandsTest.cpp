#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/DataCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

using basalt::server::runtime::command::CommandStorage;
using basalt::server::runtime::command::DataAccess;
using basalt::server::runtime::function::FunctionLibrary;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

/// A datapack on disk with one function in it, which is the only way the library reads any.
[[nodiscard]] std::string packWith(const std::string& root, const std::string& name,
                                   const std::vector<std::string>& lines) {
    const std::string dir = root + "/pack/data/test/function";
    std::error_code ignored;
    std::filesystem::create_directories(dir, ignored);
    std::ofstream meta{root + "/pack/pack.mcmeta"};
    meta << R"({"pack":{"pack_format":81,"description":"test"}})";
    meta.close();
    std::ofstream file{dir + "/" + name + ".mcfunction"};
    for (const std::string& line : lines) {
        file << line << "\n";
    }
    return root;
}

} // namespace

TEST_CASE("a function runs its lines and reports the last result") {
    Fixture fixture{scratch("function-run")};
    const std::string root = packWith(scratch("function-run-packs"), "greet", {"say hello", "seed"});
    FunctionLibrary library{root};
    library.reload();
    commands::FunctionRunner runner{library};
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(library, runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());
    fixture.dispatcher.registerCommand(std::make_shared<commands::SeedCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function test:greet"));
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("commands.function.result") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:missing") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.function.unknown") != std::string::npos);
}

TEST_CASE("a macro line takes its arguments, and refuses when it was given none") {
    Fixture fixture{scratch("function-macro")};
    const std::string root = packWith(scratch("function-macro-packs"), "greet", {"$say hello $(who)"});
    FunctionLibrary library{root};
    library.reload();
    // `/say` reaches players rather than the caller, so one has to be there to hear the macro.
    fixture.add("Steve");
    commands::FunctionRunner runner{library};
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(library, runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:greet") == 0);
    CHECK(fixture.source.lastAsText().find("commands.function.error.missing_arguments") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(function test:greet {who:"world"})"));
    std::string all;
    for (const Component& line : fixture.clients[0]->messages) {
        all += flatten(line);
    }
    CHECK(all.find("hello world") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, R"(function test:greet {other:"x"})") == 0);
    CHECK(fixture.source.lastAsText().find("commands.function.error.parameter_not_found") != std::string::npos);
}

TEST_CASE("function with storage reads its arguments out of a holder") {
    Fixture fixture{scratch("function-with")};
    const std::string root = packWith(scratch("function-with-packs"), "greet", {"$say hello $(who)"});
    FunctionLibrary library{root};
    library.reload();
    CommandStorage storage{scratch("function-with-storage") + "/command_storage.nbt"};
    NbtCompound arguments;
    arguments.putString("who", "world");
    storage.set(*Identifier::parse("test:args"), arguments);
    const DataAccess access{storage};
    fixture.add("Steve");
    commands::FunctionRunner runner{library};
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(library, runner, &access));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function test:greet with storage test:args"));
    std::string all;
    for (const Component& line : fixture.clients[0]->messages) {
        all += flatten(line);
    }
    CHECK(all.find("hello world") != std::string::npos);
}

TEST_CASE("return only works inside a function, and stops the rest of it") {
    Fixture fixture{scratch("function-return")};
    const std::string root = packWith(scratch("function-return-packs"), "early", {"return 7", "say never"});
    FunctionLibrary library{root};
    library.reload();
    commands::FunctionRunner runner{library};
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(library, runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ReturnCommand>(runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "return 5") == 0);
    CHECK(fixture.source.lastAsText().find("only works inside a function") != std::string::npos);

    fixture.add("Steve");
    CHECK(fixture.dispatcher.execute(fixture.source, "function test:early") == 7);
    std::string all;
    for (const Component& line : fixture.clients[0]->messages) {
        all += flatten(line);
    }
    CHECK(all.find("never") == std::string::npos);
}

TEST_CASE("a function that calls itself is stopped rather than left to recurse") {
    Fixture fixture{scratch("function-recursion")};
    const std::string root = packWith(scratch("function-recursion-packs"), "loop", {"function test:loop"});
    FunctionLibrary library{root};
    library.reload();
    commands::FunctionRunner runner{library};
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(library, runner));

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:loop") == 0);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line);
    }
    CHECK(all.find("recursed more than") != std::string::npos);
}

TEST_CASE("the schedule keeps entries until their tick comes round") {
    commands::FunctionSchedule schedule;
    const Identifier greet = *Identifier::parse("test:greet");
    const Identifier other = *Identifier::parse("test:other");

    CHECK(schedule.schedule(greet, 2, true) == 2);
    CHECK(schedule.scheduledIds().size() == 1);
    CHECK(schedule.tick().empty());
    const std::vector<Identifier> due = schedule.tick();
    REQUIRE(due.size() == 1);
    CHECK(due.front() == greet);
    CHECK(schedule.scheduledIds().empty());

    // `replace` drops what was waiting under that id; `append` leaves it alone.
    static_cast<void>(schedule.schedule(other, 10, true));
    static_cast<void>(schedule.schedule(other, 20, false));
    CHECK(schedule.clear(other) == 2);
    CHECK(schedule.clear(other) == 0);
}

TEST_CASE("schedule reports when it was cleared and refuses when there was nothing") {
    Fixture fixture{scratch("function-schedule")};
    const std::string root = packWith(scratch("function-schedule-packs"), "greet", {"say hi"});
    FunctionLibrary library{root};
    library.reload();
    commands::FunctionSchedule schedule;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScheduleCommand>(library, schedule));

    CHECK(fixture.dispatcher.execute(fixture.source, "schedule function test:greet 40") == 40);
    CHECK(fixture.source.lastAsText().find("commands.schedule.created.function") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule clear test:greet") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule clear test:greet") == 0);
    CHECK(fixture.source.lastAsText().find("commands.schedule.cleared.failure") != std::string::npos);
}

TEST_CASE("datapack lists, enables and disables what the library found") {
    Fixture fixture{scratch("function-datapack")};
    const std::string root = packWith(scratch("function-datapack-packs"), "greet", {"say hi"});
    FunctionLibrary library{root};
    library.reload();
    fixture.dispatcher.registerCommand(std::make_shared<commands::DataPackCommand>(library));

    CHECK(fixture.dispatcher.execute(fixture.source, "datapack list") == 1);
    CHECK(fixture.source.lastAsText().find("commands.datapack.list.available.success") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "datapack disable pack") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "datapack list enabled") == 0);
    CHECK(fixture.dispatcher.execute(fixture.source, "datapack enable pack") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "datapack enable pack") == 0);
    CHECK(fixture.source.lastAsText().find("commands.datapack.enable.failed") != std::string::npos);
}
