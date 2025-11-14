#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;
namespace world = basalt::server::world;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    return directory.string();
}

[[nodiscard]] ConfigDirectory writtenWorlds(std::string_view name, std::string_view contents) {
    const std::string root = scratch(name);
    fileIO::writeText(root + "/worlds.toml", contents);
    return ConfigDirectory{root};
}

[[nodiscard]] WorldsConfig loaded(std::string_view name, std::string_view contents) {
    const std::expected<WorldsConfig, std::string> config = WorldsConfig::load(writtenWorlds(name, contents));
    REQUIRE(config.has_value());
    return *config;
}

[[nodiscard]] std::string failure(std::string_view name, std::string_view contents) {
    const std::expected<WorldsConfig, std::string> config = WorldsConfig::load(writtenWorlds(name, contents));
    REQUIRE_FALSE(config.has_value());
    return config.error();
}

/// Every line the server logged while this is alive.
class CapturedLog {
public:
    CapturedLog() {
        basalt::wire::common::logOutput::addSink([this](std::string_view line) { lines_.emplace_back(line); });
    }

    CapturedLog(const CapturedLog&) = delete;
    CapturedLog& operator=(const CapturedLog&) = delete;

    ~CapturedLog() { basalt::wire::common::logOutput::clearSinks(); }

    [[nodiscard]] bool saidSomethingWith(std::string_view fragment) const {
        return std::ranges::any_of(
            lines_, [fragment](const std::string& line) { return line.find(fragment) != std::string::npos; });
    }

private:
    std::vector<std::string> lines_;
};

} // namespace

TEST_CASE("what a dimension is, which is what the generator and the registry ask it") {
    // parity: the three dimension types a vanilla server sends during configuration, in their own
    // registry order.
    CHECK(dimensionFacts(WorldDimension::Overworld).registryIndex == 0);
    CHECK(dimensionFacts(WorldDimension::Nether).registryIndex == 1);
    CHECK(dimensionFacts(WorldDimension::End).registryIndex == 2);
    CHECK(dimensionRegistryKey(WorldDimension::Overworld) == "minecraft:overworld");
    CHECK(dimensionRegistryKey(WorldDimension::Nether) == "minecraft:the_nether");
    CHECK(dimensionRegistryKey(WorldDimension::End) == "minecraft:the_end");

    CHECK(dimensionFacts(WorldDimension::Overworld).seaLevel == 63);
    CHECK(dimensionFacts(WorldDimension::Nether).seaLevel == 32);
    CHECK(dimensionFacts(WorldDimension::End).seaLevel == 0);
    CHECK(dimensionFacts(WorldDimension::Overworld).chunkHeight == world::kOverworldHeight);
    CHECK(dimensionFacts(WorldDimension::Nether).chunkHeight == world::kNetherHeight);
    CHECK(dimensionFacts(WorldDimension::End).chunkHeight == world::kEndHeight);

    // parity: the 1:8 that makes a Nether portal land where it does, and nowhere else.
    CHECK(dimensionFacts(WorldDimension::Nether).coordinateScale == 8.0);
    CHECK(dimensionFacts(WorldDimension::Overworld).coordinateScale == 1.0);
    CHECK(dimensionFacts(WorldDimension::End).coordinateScale == 1.0);

    // parity: only the overworld's own dimension type turns the piglin spawning on.
    CHECK(dimensionFacts(WorldDimension::Overworld).spawnsPortalPiglins);
    CHECK_FALSE(dimensionFacts(WorldDimension::Nether).spawnsPortalPiglins);

    // parity: it never rains in the Nether (a ceiling) or the End (which is neither).
    CHECK(canHaveWeather(WorldDimension::Overworld));
    CHECK_FALSE(canHaveWeather(WorldDimension::Nether));
    CHECK_FALSE(canHaveWeather(WorldDimension::End));

    // parity: the Nether names no clock, which is why its sky never moves.
    CHECK(hasClock(WorldDimension::Overworld));
    CHECK_FALSE(hasClock(WorldDimension::Nether));
    CHECK(hasClock(WorldDimension::End));
}

TEST_CASE("a dimension is named by any of the three spellings an operator might use") {
    CHECK(dimensionByName("nether") == WorldDimension::Nether);
    CHECK(dimensionByName("the_nether") == WorldDimension::Nether);
    CHECK(dimensionByName("minecraft:the_nether") == WorldDimension::Nether);
    CHECK(dimensionByName("NETHER") == WorldDimension::Nether);
    CHECK(dimensionByName("End") == WorldDimension::End);
    CHECK(dimensionByName("overworld") == WorldDimension::Overworld);
    CHECK(dimensionByName("minecraft:overworld") == WorldDimension::Overworld);
    CHECK_FALSE(dimensionByName("the_aether").has_value());
    CHECK_FALSE(dimensionByName("").has_value());

    CHECK(worldTypeByName("FLAT") == WorldType::Flat);
    CHECK(worldTypeByName("void") == WorldType::Void);
    CHECK_FALSE(worldTypeByName("superflat").has_value());
    // Declared, and refused where it is selected rather than quietly generating something else.
    CHECK(worldTypeByName("amplified") == WorldType::Amplified);
    CHECK_FALSE(isImplemented(WorldType::Amplified));
    CHECK(isImplemented(WorldType::Normal));
}

TEST_CASE("the commented default is a vanilla server's three worlds") {
    const WorldsConfig config = loaded("worlds-default", kWorldsDefaultToml);
    REQUIRE(config.worlds().size() == 3);
    CHECK(config.defaultWorld().name == "world");
    CHECK(config.defaultWorld().dimension == WorldDimension::Overworld);
    CHECK(config.worlds()[1].name == "world_nether");
    CHECK(config.worlds()[1].dimension == WorldDimension::Nether);
    CHECK(config.worlds()[2].dimension == WorldDimension::End);
    // Left to chance unless the operator wrote one down.
    CHECK_FALSE(config.defaultWorld().seed.has_value());

    // Every portal leads where a vanilla one does.
    const WorldDeclaration& overworld = config.worlds()[0];
    const WorldDeclaration& nether = config.worlds()[1];
    const WorldDeclaration& end = config.worlds()[2];
    CHECK(config.portalTarget(overworld, WorldDimension::Nether) == &nether);
    CHECK(config.portalTarget(overworld, WorldDimension::End) == &end);
    CHECK(config.portalTarget(nether, WorldDimension::Overworld) == &overworld);
    CHECK(config.portalTarget(end, WorldDimension::Overworld) == &overworld);
    CHECK(&config.overworldOf(nether) == &overworld);
    CHECK(&config.overworldOf(overworld) == &overworld);
}

TEST_CASE("a server can have several sets of dimensions, which on Paper takes a plugin") {
    const WorldsConfig config = loaded("worlds-sets", R"(
[worlds.world]
default = true
[worlds.world_nether]
dimension = "nether"
linked_to = "world"
[worlds.skyblock]
type = "void"
seed = 42
[worlds.skyblock_nether]
dimension = "nether"
linked_to = "skyblock"
[worlds.skyblock_the_end]
dimension = "end"
linked_to = "skyblock"
)");
    REQUIRE(config.worlds().size() == 5);
    const WorldDeclaration& skyblock = config.worlds()[2];
    CHECK(skyblock.type == WorldType::Void);
    CHECK(skyblock.seed == 42);

    // A portal out of skyblock leads to skyblock's own nether, not to whichever was declared first.
    const WorldDeclaration* nether = config.portalTarget(skyblock, WorldDimension::Nether);
    REQUIRE(nether != nullptr);
    CHECK(nether->name == "skyblock_nether");
    // And out of that nether, back to skyblock rather than to the default world.
    CHECK(config.portalTarget(*nether, WorldDimension::Overworld)->name == "skyblock");
    CHECK(config.overworldOf(*nether).name == "skyblock");

    // The first set is unaffected by the second.
    CHECK(config.portalTarget(config.worlds()[0], WorldDimension::Nether)->name == "world_nether");
    // Nothing declares an end for the default world, so a portal there leads nowhere yet.
    CHECK(config.portalTarget(config.worlds()[0], WorldDimension::End) == nullptr);
}

TEST_CASE("a secondary dimension that names nothing belongs to the default world") {
    const WorldsConfig config = loaded("worlds-implicit", R"(
[worlds.first]
[worlds.second]
default = true
[worlds.a_nether]
dimension = "nether"
)");
    // The default world is the one that says so, not the first one written.
    CHECK(config.defaultWorld().name == "second");
    CHECK(config.overworldOf(config.worlds()[2]).name == "second");
    CHECK(config.portalTarget(config.worlds()[1], WorldDimension::Nether)->name == "a_nether");
    // And the world that did not claim it has no nether of its own.
    CHECK(config.portalTarget(config.worlds()[0], WorldDimension::Nether) == nullptr);
}

TEST_CASE("links that cannot mean one thing are refused rather than guessed") {
    CHECK(failure("worlds-unknown-link", R"(
[worlds.world]
default = true
[worlds.nether]
dimension = "nether"
linked_to = "nothing"
)")
              .starts_with("World 'nether' is linked to 'nothing', which "));

    CHECK(failure("worlds-link-to-nether", R"(
[worlds.world]
default = true
[worlds.a_nether]
dimension = "nether"
[worlds.an_end]
dimension = "end"
linked_to = "a_nether"
)") == "World 'an_end' is linked to 'a_nether', which is a nether rather than an overworld.");

    // Two Nethers for one overworld is the guessing game this refuses to play.
    CHECK(failure("worlds-two-nethers", R"(
[worlds.world]
default = true
[worlds.one]
dimension = "nether"
linked_to = "world"
[worlds.two]
dimension = "nether"
linked_to = "world"
)") == "Worlds 'one' and 'two' are both the nether of 'world'. A world has one of each.");

    // Including when one of them claimed it by saying nothing at all.
    CHECK(failure("worlds-implicit-clash", R"(
[worlds.world]
default = true
[worlds.one]
dimension = "nether"
[worlds.two]
dimension = "nether"
linked_to = "world"
)") == "Worlds 'one' and 'two' are both the nether of 'world'. A world has one of each.");

    // An overworld has no portals of its own to name.
    CHECK(failure("worlds-overworld-link", R"(
[worlds.world]
default = true
linked_to = "other"
[worlds.other]
)") == "World 'world' is an overworld and declares linked_to = \"other\". Only a nether or an end names the "
       "overworld its portals lead to.");
}

TEST_CASE("a world nobody can generate stops the server rather than generating something else") {
    CHECK(failure("worlds-unknown-type", "[worlds.world]\ntype = \"superflat\"\n")
              .starts_with("World 'world' declares an unknown type 'superflat'. Known types: normal, flat, void, "
                           "amplified."));
    // Declared and not implemented is its own message, because it is not a typo.
    CHECK(failure("worlds-amplified", "[worlds.world]\ntype = \"amplified\"\n")
              .starts_with("World 'world' declares type 'amplified', which is not implemented yet."));
    CHECK(failure("worlds-unknown-dimension", "[worlds.world]\ndimension = \"aether\"\n")
              .starts_with("World 'world' declares an unknown dimension 'aether'. Known dimensions: overworld, "
                           "nether, end."));

    CHECK(failure("worlds-none", "[defaults]\ndifficulty = \"hard\"\n")
              .ends_with(" declares no worlds: a server needs at least one."));
    CHECK(failure("worlds-two-defaults", R"(
[worlds.a]
default = true
[worlds.b]
default = true
)")
              .ends_with(" marks more than one world as the default."));
}

TEST_CASE("with no world marked as the default the first one is it, and the log says which") {
    const CapturedLog log;
    const WorldsConfig config = loaded("worlds-no-default", "[worlds.first]\n[worlds.second]\n");
    CHECK(config.defaultWorld().name == "first");
    // Which world players join into is not something to leave anybody guessing about.
    CHECK(log.saidSomethingWith("No world is marked as the default"));
    CHECK(log.saidSomethingWith("using 'first'"));
}

TEST_CASE("a world that declares nothing is a normal overworld") {
    const WorldsConfig config = loaded("worlds-bare", "[worlds.world]\n");
    REQUIRE(config.worlds().size() == 1);
    CHECK(config.worlds().front().type == WorldType::Normal);
    CHECK(config.worlds().front().dimension == WorldDimension::Overworld);
    CHECK_FALSE(config.worlds().front().linkedTo.has_value());
}

TEST_CASE("settings are inherited, and overridden by the world that has its own") {
    const std::string worldsRoot = scratch("worlds-settings-root");
    const ConfigDirectory directory = writtenWorlds("worlds-settings", R"(
[worlds.world]
default = true
[defaults]
difficulty = "hard"
view-distance = 6
)");
    const std::expected<WorldsConfig, std::string> config = WorldsConfig::load(directory, worldsRoot);
    REQUIRE(config.has_value());
    const WorldDeclaration& world = config->worlds().front();

    // What `[defaults]` says, and the vanilla value for what it does not.
    const WorldSettings inherited = config->settingsFor(world);
    CHECK(inherited.difficulty == "hard");
    CHECK(inherited.viewDistance == 6);
    CHECK(inherited.saveIntervalSeconds == 300);
    CHECK_FALSE(inherited.openEndExitPortal);

    // The world has never been created, so nothing was written into a directory that does not
    // exist: an existing directory is how "this world was already created" is decided elsewhere.
    CHECK_FALSE(fileIO::exists(worldsRoot + "/world/settings.toml"));

    // Once it exists, the commented default is put beside it.
    fileIO::createDirectories(worldsRoot + "/world");
    CHECK(config->settingsFor(world) == inherited);
    REQUIRE(fileIO::exists(worldsRoot + "/world/settings.toml"));
    CHECK(*fileIO::readText(worldsRoot + "/world/settings.toml") == kWorldSettingsDefaultToml);

    // And what it says wins over what it inherits, key by key.
    fileIO::writeText(worldsRoot + "/world/settings.toml", "view-distance = 2\nopen-end-exit-portal = true\n");
    const WorldSettings own = config->settingsFor(world);
    CHECK(own.viewDistance == 2);
    CHECK(own.openEndExitPortal);
    CHECK(own.difficulty == "hard");
    CHECK(own.saveIntervalSeconds == 300);

    // A save interval cannot run backwards; zero is what turns it off.
    fileIO::writeText(worldsRoot + "/world/settings.toml", "save-interval-seconds = -5\n");
    CHECK(config->settingsFor(world).saveIntervalSeconds == 0);

    // One world's preferences failing to parse is not the server's configuration failing: what it
    // says is lost, what it inherits is not.
    fileIO::writeText(worldsRoot + "/world/settings.toml", "[unclosed\n");
    CHECK(config->settingsFor(world) == inherited);
}

TEST_CASE("the vanilla settings are what a world with nothing configured gets") {
    const WorldSettings vanilla = WorldSettings::vanilla();
    CHECK(vanilla.difficulty == "easy");
    CHECK(vanilla.viewDistance == 10);
    CHECK(vanilla.saveIntervalSeconds == 300);
    // NOT VANILLA and off: vanilla opens the exit portal when the dragon dies, and there is no
    // dragon in this build yet.
    CHECK_FALSE(vanilla.openEndExitPortal);
}
