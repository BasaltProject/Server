#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;
namespace toml = basalt::wire::common::toml;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

} // namespace

TEST_CASE("a missing file is written from its commented default and then read") {
    const std::string root = scratch("config-generated");
    const ConfigDirectory directory{root};
    CHECK(directory.root() == root);
    CHECK(directory.pathOf("server.toml") == root + "/server.toml");

    const std::expected<toml::TomlTable, std::string> loaded = directory.load("server.toml", kServerDefaultToml);
    REQUIRE(loaded.has_value());
    CHECK(fileIO::exists(root + "/server.toml"));

    const std::optional<std::string> written = fileIO::readText(root + "/server.toml");
    REQUIRE(written.has_value());
    CHECK(*written == kServerDefaultToml);
    CHECK(written->find('#') != std::string::npos);

    const toml::TomlTable* network = loaded->table("network");
    REQUIRE(network != nullptr);
    const toml::TomlTable* java = network->table("java");
    REQUIRE(java != nullptr);
    REQUIRE(java->find("port") != nullptr);
}

TEST_CASE("a file that is already there is not overwritten by the default") {
    const std::string root = scratch("config-existing");
    fileIO::createDirectories(root);
    fileIO::writeText(root + "/server.toml", "[server]\nport = 25566\n");

    const ConfigDirectory directory{root};
    const std::expected<toml::TomlTable, std::string> loaded = directory.load("server.toml", kServerDefaultToml);
    REQUIRE(loaded.has_value());
    const toml::TomlTable* server = loaded->table("server");
    REQUIRE(server != nullptr);
    REQUIRE(server->find("port") != nullptr);
    CHECK(*fileIO::readText(root + "/server.toml") == "[server]\nport = 25566\n");
}

TEST_CASE("a file that does not parse names itself rather than being replaced") {
    const std::string root = scratch("config-broken");
    fileIO::createDirectories(root);
    fileIO::writeText(root + "/groups.toml", "[unclosed\n");

    const ConfigDirectory directory{root};
    const std::expected<toml::TomlTable, std::string> loaded = directory.load("groups.toml", kGroupsDefaultToml);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().starts_with(root + "/groups.toml is not valid TOML: "));
    CHECK(*fileIO::readText(root + "/groups.toml") == "[unclosed\n");
}

TEST_CASE("every commented default is valid TOML") {
    const std::vector<std::pair<std::string, std::string_view>> templates = {
        {"animations.toml", kAnimationsDefaultToml},
        {"boss_bar.toml", kBossBarDefaultToml},
        {"commands.toml", kCommandsDefaultToml},
        {"groups.toml", kGroupsDefaultToml},
        {"scoreboard.toml", kScoreboardDefaultToml},
        {"server.toml", kServerDefaultToml},
        {"server_list.toml", kServerListDefaultToml},
        {"tab_list.toml", kTabListDefaultToml},
        {"world_mobs.toml", kWorldMobsDefaultToml},
        {"world_mobs_local.toml", kWorldMobsLocalDefaultToml},
        {"world_settings.toml", kWorldSettingsDefaultToml},
        {"worlds.toml", kWorldsDefaultToml},
    };
    for (const auto& [name, contents] : templates) {
        CAPTURE(name);
        CHECK_FALSE(contents.empty());
        CHECK(contents.find('#') != std::string_view::npos);
        CHECK(toml::parse(contents).has_value());
    }
}

TEST_CASE("the generated mob defaults are vanilla's") {
    const std::string root = scratch("config-mobs");
    const std::expected<MobSettings, std::string> loaded = loadMobDefaults(ConfigDirectory{root});
    REQUIRE(loaded.has_value());
    CHECK(*loaded == MobSettings::vanilla());
}

TEST_CASE("an override naming an unknown field is refused with the file") {
    const std::expected<toml::TomlTable, toml::TomlError> parsed = toml::parse("[overrides.zombie]\nmax-helth = 5\n");
    REQUIRE(parsed.has_value());
    const std::expected<MobSettings, std::string> settings =
        MobSettings::from(*parsed, MobSettings::vanilla(), "mobs.toml");
    REQUIRE_FALSE(settings.has_value());
    CHECK(settings.error().starts_with("mobs.toml: overrides.zombie.max-helth"));
}
