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
#include <map>
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

/// A library read out of an in-memory pack, one function per entry.
void load(FunctionLibrary& library, const std::map<std::string, std::vector<std::string>>& functions,
          const std::map<std::string, std::string>& tags = {}) {
    auto files = std::make_shared<std::map<std::string, std::string>>();
    for (const auto& [name, lines] : functions) {
        std::string text;
        for (const std::string& line : lines) {
            text += line + "\n";
        }
        (*files)["data/test/function/" + name + ".mcfunction"] = text;
    }
    for (const auto& [name, json] : tags) {
        (*files)["data/test/tags/function/" + name + ".json"] = json;
    }
    static_cast<void>(library.load(
        [files](std::string_view prefix) {
            std::vector<std::string> names;
            for (const auto& [name, text] : *files) {
                if (name.starts_with(prefix)) {
                    names.push_back(name);
                }
            }
            return names;
        },
        [files](std::string_view name) -> std::optional<std::string> {
            const auto found = files->find(std::string{name});
            return found == files->end() ? std::nullopt : std::optional{found->second};
        }));
}

[[nodiscard]] commands::FunctionRunner runnerOf(const FunctionLibrary& library) {
    return commands::FunctionRunner{library, commands::FunctionRunner::Settings{.permissionLevel = 2,
                                                                                .commandLimit = {}}};
}

[[nodiscard]] std::string heard(const std::vector<Component>& lines) {
    std::string all;
    for (const Component& line : lines) {
        all += flatten(line) + "\n";
    }
    return all;
}

} // namespace

TEST_CASE("function runs a function or a tag, says so first, and names what it cannot find") {
    Fixture fixture{scratch("function-run")};
    FunctionLibrary library;
    load(library, {{"greet", {"say hello"}}, {"other", {"say there"}}},
         {{"both", R"({"values":["test:greet","test:other","test:greet"]})"}, {"empty", R"({"values":[]})"}});
    fixture.add("Steve");
    commands::FunctionRunner runner = runnerOf(library);
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function test:greet"));
    CHECK(fixture.source.lastAsText().find("commands.function.scheduled.single") != std::string::npos);
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function #test:both"));
    CHECK(fixture.source.lastAsText().find("commands.function.scheduled.multiple") != std::string::npos);
    CHECK(heard(fixture.clients[0]->messages).find("there") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:missing") == 0);
    CHECK(fixture.source.lastAsText().find("arguments.function.unknown") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "function #test:empty") == 0);
    CHECK(fixture.source.lastAsText().find("commands.function.scheduled.no_functions") != std::string::npos);
}

TEST_CASE("a macro is filled in before it runs, and refuses what it cannot fill") {
    Fixture fixture{scratch("function-macro")};
    FunctionLibrary library;
    load(library, {{"greet", {"$say hello $(who)"}}});
    fixture.add("Steve");
    commands::FunctionRunner runner = runnerOf(library);
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:greet") == 0);
    CHECK(fixture.source.lastAsText().find("commands.function.error.missing_arguments") != std::string::npos);

    static_cast<void>(fixture.dispatcher.execute(fixture.source, R"(function test:greet {who:"world"})"));
    CHECK(heard(fixture.clients[0]->messages).find("hello world") != std::string::npos);
    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function test:greet {who:1.5d}"));
    CHECK(heard(fixture.clients[0]->messages).find("hello 1.5") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, R"(function test:greet {other:"x"})") == 0);
    CHECK(fixture.source.lastAsText().find("commands.function.error.missing_argument") != std::string::npos);
}

TEST_CASE("function with storage reads its arguments out of a holder") {
    Fixture fixture{scratch("function-with")};
    FunctionLibrary library;
    load(library, {{"greet", {"$say hello $(who)"}}});
    CommandStorage storage{scratch("function-with-storage") + "/command_storage.nbt"};
    NbtCompound arguments;
    arguments.putString("who", "world");
    storage.set(*Identifier::parse("test:args"), arguments);
    const DataAccess access{storage};
    fixture.add("Steve");
    commands::FunctionRunner runner = runnerOf(library);
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(runner, &access));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    static_cast<void>(fixture.dispatcher.execute(fixture.source, "function test:greet with storage test:args"));
    CHECK(heard(fixture.clients[0]->messages).find("hello world") != std::string::npos);
}

TEST_CASE("return ends a function with its value, and outside one only ends the command") {
    Fixture fixture{scratch("function-return")};
    FunctionLibrary library;
    load(library, {{"early", {"return 7", "say never"}}, {"forwarded", {"return run say first", "say never"}}});
    commands::FunctionRunner runner = runnerOf(library);
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::ReturnCommand>(runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "return 5") == 5);

    fixture.add("Steve");
    CHECK(fixture.dispatcher.execute(fixture.source, "function test:early") == 7);
    CHECK(fixture.source.lastAsText().find("commands.function.result") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "function test:forwarded") == 1);
    const std::string all = heard(fixture.clients[0]->messages);
    CHECK(all.find("first") != std::string::npos);
    CHECK(all.find("never") == std::string::npos);
}

TEST_CASE("a function that calls itself stops instead of running forever") {
    Fixture fixture{scratch("function-recursion")};
    FunctionLibrary library;
    load(library, {{"loop", {"function test:loop"}}});
    commands::FunctionRunner runner = runnerOf(library);
    fixture.dispatcher.registerCommand(std::make_shared<commands::FunctionCommand>(runner));

    CHECK(fixture.dispatcher.execute(fixture.source, "function test:loop") == 0);
}

TEST_CASE("the schedule runs what is due, in trigger then schedule order") {
    commands::FunctionSchedule schedule;
    const Identifier greet = *Identifier::parse("test:greet");
    const Identifier other = *Identifier::parse("test:other");

    schedule.schedule(greet, false, 12);
    schedule.schedule(other, true, 10);
    CHECK(schedule.names() == std::vector<std::string>{"test:greet", "#test:other"});
    CHECK(schedule.due(9).empty());
    const std::vector<commands::FunctionSchedule::Event> due = schedule.due(12);
    REQUIRE(due.size() == 2);
    CHECK(due[0].name == "#test:other");
    CHECK(due[1].name == "test:greet");

    schedule.schedule(other, false, 20);
    schedule.schedule(other, false, 30);
    CHECK(schedule.remove("test:other") == 2);
    CHECK(schedule.remove("test:other") == 0);
}

TEST_CASE("schedule answers the game time it runs at and refuses the same tick and macros") {
    Fixture fixture{scratch("function-schedule")};
    FunctionLibrary library;
    load(library, {{"greet", {"say hi"}}, {"macro", {"$say $(x)"}}});
    commands::FunctionSchedule schedule;
    fixture.dispatcher.registerCommand(std::make_shared<commands::ScheduleCommand>(
        library, schedule, [](const basalt::server::runtime::command::CommandContext&) { return 100; }));

    CHECK(fixture.dispatcher.execute(fixture.source, "schedule function test:greet 40") == 140);
    CHECK(fixture.source.lastAsText().find("commands.schedule.created.function") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule function test:greet 0") == 0);
    CHECK(fixture.source.lastAsText().find("commands.schedule.same_tick") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule function test:macro 5") == 0);
    CHECK(fixture.source.lastAsText().find("commands.schedule.macro") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule clear test:greet") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "schedule clear test:greet") == 0);
    CHECK(fixture.source.lastAsText().find("commands.schedule.cleared.failure") != std::string::npos);
}
