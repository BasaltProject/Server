#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/terrain/DesertPyramidStructureGenerator.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
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
using RuntimeDialog = basalt::server::runtime::world::dialogs::Dialog;
using basalt::wire::text::Component;
namespace commands = basalt::server::runtime::command::commands;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;
namespace terrain = basalt::server::world::terrain;
namespace world = basalt::server::runtime::world;

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
    std::int32_t syncs = 0;
    std::int32_t waypoints = 0;
    std::vector<std::int32_t> dialogs;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendSystemMessage = [this](const Component& message) { messages.push_back(message); };
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.syncExperience = [this] { syncs++; };
        connection.showDialog = [this](std::int32_t id, const RuntimeDialog&) { dialogs.push_back(id); };
        connection.sendWaypoint = [this](const Uuid&, std::int32_t, const std::string&,
                                         const std::optional<std::int32_t>&, double, double, double,
                                         bool) { waypoints++; };
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

    explicit Fixture(const std::string& root) : Fixture{root, {}} {}

    Fixture(const std::string& root, std::vector<world::StructurePlacer> placers)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"},
              LoadedWorld::Options{.structurePlacers = std::move(placers)})),
          store(openStore(root)), environment(registry, options()), dispatcher(environment) {}

    [[nodiscard]] CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        built.worlds = {{"world", world.get()}};
        built.access = store == nullptr ? nullptr : &store->access();
        return built;
    }

    PlayerSession& add(const std::string& name) {
        clients.push_back(std::make_unique<Client>());
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          clients.back()->connection()));
        players.back()->worldName = "world";
        registry.add(players.back().get());
        return *players.back();
    }
};

} // namespace

namespace play = basalt::wire::protocol::play;

using basalt::server::runtime::command::commands::FunctionRunner;
using basalt::server::runtime::function::FunctionLibrary;
using basalt::server::runtime::world::RealDialogs;
using basalt::server::runtime::world::RealEnchantments;
using basalt::server::runtime::world::RealTestInstances;
using basalt::wire::common::Identifier;

namespace {

/// A jar of named text entries, which is all three registry readers ever ask for.
struct TextJar {
    std::map<std::string, std::string> entries;

    [[nodiscard]] std::optional<std::string> read(std::string_view name) const {
        const auto found = entries.find(std::string{name});
        return found == entries.end() ? std::nullopt : std::optional{found->second};
    }

    [[nodiscard]] std::vector<std::string> under(std::string_view prefix) const {
        std::vector<std::string> names;
        for (const auto& [name, ignored] : entries) {
            if (name.starts_with(prefix)) {
                names.push_back(name);
            }
        }
        return names;
    }
};

} // namespace

TEST_CASE("waypoint tracks an entity once it has an icon, and pushes it to everyone") {
    Fixture fixture{scratch("waypoint")};
    fixture.add("Steve");
    fixture.dispatcher.registerCommand(std::make_shared<commands::WaypointCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "waypoint list") == 0);
    CHECK(fixture.source.lastAsText().find("commands.waypoint.list.empty") != std::string::npos);

    // parity: `/waypoint modify` answers 0 whether or not it changed anything.
    CHECK(fixture.dispatcher.execute(fixture.source, "waypoint modify Steve color hex #ff0000") == 0);
    CHECK(fixture.source.lastAsText().find("commands.waypoint.modify.color") != std::string::npos);
    CHECK(fixture.clients[0]->waypoints == 1);

    CHECK(fixture.dispatcher.execute(fixture.source, "waypoint list") == 1);
    CHECK(fixture.source.lastAsText().find("commands.waypoint.list.success") != std::string::npos);

    // The style is remembered next to the colour rather than replacing it, so both survive a change.
    CHECK(fixture.dispatcher.execute(fixture.source, "waypoint modify Steve style set minecraft:bowtie") == 0);
    CHECK(fixture.source.lastAsText().find("commands.waypoint.modify.style") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "waypoint list") == 1);
}

TEST_CASE("place names what it cannot generate instead of failing silently") {
    Fixture fixture{scratch("place-refusals")};
    basalt::server::runtime::world::StructureTemplates templates{{}};
    commands::PlaceCommand command{templates, [] { return std::int64_t{1}; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceCommand>(std::move(command)));

    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:village_plains 0 64 0") == 0);
    CHECK(fixture.source.lastAsText().find("is not a structure this build generates") != std::string::npos);

    // An empty jar ships no structure files, so a template name resolves to nothing at all.
    CHECK(fixture.dispatcher.execute(fixture.source, "place template minecraft:igloo/top 0 64 0") == 0);
    CHECK(fixture.source.lastAsText().find("igloo/top") != std::string::npos);
}

TEST_CASE("place structure generates the placers' structures, and keeps vanilla's refusals") {
    // A hut answers to nothing but its own draws: biome and height lookups stand in for the world.
    auto hutBiome = [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:swamp"}; };
    auto flatSurface = [](std::int32_t, std::int32_t) { return 66; };
    terrain::SwampHutStructureGenerator huts{hutBiome, flatSurface};
    // A monument whose surrounding sweep never finds an ocean biome refuses every position, which
    // is `OceanMonumentStructure.findGenerationPoint`'s own condition.
    terrain::OceanMonumentStructureGenerator monuments{
        [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:plains"}; },
        {"minecraft:ocean"},
        {"minecraft:ocean"},
        [](std::int32_t, std::int32_t) { return 40; },
        63};
    // A pyramid on high ground places a 21 by 21 piece, so its start covers four chunks.
    terrain::DesertPyramidStructureGenerator pyramids{hutBiome, flatSurface};

    std::vector<world::StructurePlacer> placers;
    placers.push_back({.id = Identifier::vanilla("swamp_hut"),
                       .boundsAt = [&huts](std::int64_t seed, std::int32_t x,
                                           std::int32_t z) { return huts.placeBoundsAt(seed, x, z); },
                       .place = [&huts](basalt::server::world::Chunk& chunk, std::int64_t seed, std::int32_t x,
                                        std::int32_t z) { huts.placeAt(chunk, seed, x, z); }});
    placers.push_back({.id = Identifier::vanilla("monument"),
                       .boundsAt = [&monuments](std::int64_t seed, std::int32_t x,
                                                std::int32_t z) { return monuments.placeBoundsAt(seed, x, z); },
                       .place = [&monuments](basalt::server::world::Chunk& chunk, std::int64_t seed, std::int32_t x,
                                             std::int32_t z) { monuments.placeAt(chunk, seed, x, z); }});
    placers.push_back({.id = Identifier::vanilla("desert_pyramid"),
                       .boundsAt = [&pyramids](std::int64_t seed, std::int32_t x,
                                               std::int32_t z) { return pyramids.placeBoundsAt(seed, x, z); },
                       .place = [&pyramids](basalt::server::world::Chunk& chunk, std::int64_t seed, std::int32_t x,
                                            std::int32_t z) { pyramids.placeAt(chunk, seed, x, z); }});

    Fixture fixture{scratch("place-structures"), std::move(placers)};
    basalt::server::runtime::world::StructureTemplates templates{{}};
    commands::PlaceCommand command{templates, [] { return std::int64_t{1}; }};
    fixture.dispatcher.registerCommand(std::make_shared<commands::PlaceCommand>(std::move(command)));
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 0));

    // A single-chunk structure places into its chunk and reports success.
    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:swamp_hut 8 64 8") == 1);
    CHECK(fixture.source.lastAsText().find("commands.place.structure.success") != std::string::npos);
    // The fixture's flat world has no terrain beyond a floor at the bottom of the heightmap, so
    // the hut settles there, exactly `updateAverageGroundHeight` over what the chunk holds.
    const std::int32_t planks = basalt::server::registry::blocks::named("spruce_planks")->stateId;
    bool hutWritten = false;
    for (std::int32_t y = -64; y <= 100 && !hutWritten; ++y) {
        for (std::int32_t z = 0; z < 16 && !hutWritten; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                if (fixture.world->state().blockState(x, y, z).value_or(0) == planks) {
                    hutWritten = true;
                    break;
                }
            }
        }
    }
    CHECK(hutWritten);

    // The monument's own condition refuses, which is `commands.place.structure.failed`.
    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:monument 8 40 8") == 0);
    CHECK(fixture.source.lastAsText().find("commands.place.structure.failed") != std::string::npos);

    // The pyramid covers chunks the world has not loaded, which is `argument.pos.unloaded` before
    // anything is written.
    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:desert_pyramid 8 64 8") == 0);
    CHECK(fixture.source.lastAsText().find("argument.pos.unloaded") != std::string::npos);

    static_cast<void>(fixture.world->state().getOrGenerateChunk(1, 0));
    static_cast<void>(fixture.world->state().getOrGenerateChunk(0, 1));
    static_cast<void>(fixture.world->state().getOrGenerateChunk(1, 1));
    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:desert_pyramid 8 64 8") == 1);
    CHECK(fixture.source.lastAsText().find("commands.place.structure.success") != std::string::npos);

    // Nothing the placers hold answers for an id they do not know.
    CHECK(fixture.dispatcher.execute(fixture.source, "place structure minecraft:stronghold 8 64 8") == 0);
    CHECK(fixture.source.lastAsText().find("is not a structure this build generates") != std::string::npos);
}

TEST_CASE("enchant refuses a registry the client never got, and writes the component when it has one") {
    Fixture fixture{scratch("enchant")};
    basalt::server::runtime::player::PlayerSession& steve = fixture.add("Steve");
    steve.inventory.set(basalt::server::runtime::player::PlayerInventory::kHotbarStart,
                        basalt::server::storage::StoredItemStack{.id = "minecraft:diamond_sword", .count = 1});

    const TextJar empty;
    const RealEnchantments none{{.readText = [&empty](std::string_view name) { return empty.read(name); },
                                 .entriesUnder = [&empty](std::string_view prefix) { return empty.under(prefix); }}};
    fixture.dispatcher.registerCommand(std::make_shared<commands::EnchantCommand>(none));
    CHECK(fixture.dispatcher.execute(fixture.source, "enchant Steve minecraft:sharpness 3") == 0);
    CHECK(fixture.source.lastAsText().find("No enchantment registry") != std::string::npos);

    TextJar jar;
    jar.entries["data/minecraft/enchantment/sharpness.json"] = R"({"anvil_cost":1,"max_level":5})";
    const RealEnchantments one{{.readText = [&jar](std::string_view name) { return jar.read(name); },
                                .entriesUnder = [&jar](std::string_view prefix) { return jar.under(prefix); }}};
    CommandDispatcher second{fixture.environment};
    second.registerCommand(std::make_shared<commands::EnchantCommand>(one));
    CHECK(second.execute(fixture.source, "enchant Steve minecraft:sharpness 3") == 1);

    const auto& held = steve.inventory.at(basalt::server::runtime::player::PlayerInventory::kHotbarStart);
    REQUIRE(held.has_value());
    const auto* levels = held->components.find(basalt::server::runtime::player::kEnchantmentsComponent);
    REQUIRE(levels != nullptr);
    const auto* compound = levels->get<basalt::wire::nbt::NbtCompound>();
    REQUIRE(compound != nullptr);
    const auto* level = compound->find("minecraft:sharpness");
    REQUIRE(level != nullptr);
    CHECK(*level->get<std::int32_t>() == 3);

    // Level zero takes it back off, and the empty component goes with it.
    CHECK(second.execute(fixture.source, "enchant Steve minecraft:sharpness 0") == 1);
    CHECK(steve.inventory.at(basalt::server::runtime::player::PlayerInventory::kHotbarStart)
              ->components.find(basalt::server::runtime::player::kEnchantmentsComponent) == nullptr);
}

TEST_CASE("dialog shows one the registry knows and clears whatever is open") {
    Fixture fixture{scratch("dialog")};
    fixture.add("Steve");

    TextJar jar;
    jar.entries["data/minecraft/dialog/server_links.json"] = R"({"type":"minecraft:server_links"})";
    const RealDialogs dialogs{{.readText = [&jar](std::string_view name) { return jar.read(name); },
                               .entriesUnder = [&jar](std::string_view prefix) { return jar.under(prefix); }}};
    fixture.dispatcher.registerCommand(std::make_shared<commands::DialogCommand>(dialogs));

    CHECK(fixture.dispatcher.execute(fixture.source, "dialog show Steve minecraft:server_links") == 1);
    CHECK(fixture.clients[0]->dialogs == std::vector<std::int32_t>{0});
    CHECK(fixture.source.lastAsText().find("commands.dialog.show.single") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "dialog show Steve minecraft:nothing") == 0);
    CHECK(fixture.source.lastAsText().find("argument.resource.not_found") != std::string::npos);

    CHECK(fixture.dispatcher.execute(fixture.source, "dialog clear Steve") == 1);
    CHECK(fixture.clients[0]->sent(play::kClearDialogId));
}

TEST_CASE("test runs a function test, skips one of another type, and remembers what failed") {
    Fixture fixture{scratch("test-command")};
    // `/say` reaches players rather than the caller, so a test function needs one to have an effect.
    fixture.add("Steve");
    TextJar functions;
    functions.entries["data/test/function/passes.mcfunction"] = "say hello\n";
    FunctionLibrary library;
    static_cast<void>(library.load([&functions](std::string_view prefix) { return functions.under(prefix); },
                                   [&functions](std::string_view name) { return functions.read(name); }));
    FunctionRunner runner{library, FunctionRunner::Settings{.permissionLevel = 2, .commandLimit = {}}};

    TextJar jar;
    jar.entries["data/minecraft/test_instance/passes.json"] =
        R"({"type":"minecraft:function","function":"test:passes"})";
    jar.entries["data/minecraft/test_instance/fails.json"] =
        R"({"type":"minecraft:function","function":"test:missing"})";
    jar.entries["data/minecraft/test_instance/blocks.json"] = R"({"type":"minecraft:block_based"})";
    const RealTestInstances instances{{.readText = [&jar](std::string_view name) { return jar.read(name); },
                                       .entriesUnder = [&jar](std::string_view prefix) { return jar.under(prefix); }}};
    REQUIRE(instances.all().size() == 3);

    fixture.dispatcher.registerCommand(std::make_shared<commands::TestCommand>(instances, runner));
    fixture.dispatcher.registerCommand(std::make_shared<commands::SayCommand>());

    CHECK(fixture.dispatcher.execute(fixture.source, "test runall") == 1);
    std::string all;
    for (const Component& line : fixture.source.said) {
        all += flatten(line) + "\n";
    }
    CHECK(all.find("minecraft:blocks: skipped") != std::string::npos);
    CHECK(all.find("minecraft:passes: passed") != std::string::npos);
    CHECK(all.find("minecraft:fails: failed") != std::string::npos);

    // Only the one that failed runs again, and clearing forgets it.
    fixture.source.said.clear();
    CHECK(fixture.dispatcher.execute(fixture.source, "test runfailed") == 0);
    CHECK(fixture.source.lastAsText().find("Ran 1 test(s)") != std::string::npos);
    CHECK(fixture.dispatcher.execute(fixture.source, "test clearall") == 1);
    CHECK(fixture.dispatcher.execute(fixture.source, "test runfailed") == 0);
    CHECK(fixture.source.lastAsText().find("commands.test.no_tests") != std::string::npos);
}
