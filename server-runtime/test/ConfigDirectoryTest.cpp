#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ConfigDirectory.hpp>
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

    // The directory does not exist yet either: first run is one call, not a setup step.
    const std::expected<toml::TomlTable, std::string> loaded = directory.load("server.toml", kServerDefaultToml);
    REQUIRE(loaded.has_value());
    CHECK(fileIO::exists(root + "/server.toml"));

    // What was written is the default itself, comments and all: the file an operator opens on
    // first run explains itself.
    const std::optional<std::string> written = fileIO::readText(root + "/server.toml");
    REQUIRE(written.has_value());
    CHECK(*written == kServerDefaultToml);
    CHECK(written->find('#') != std::string::npos);

    const toml::TomlTable* server = loaded->table("server");
    REQUIRE(server != nullptr);
    REQUIRE(server->find("port") != nullptr);
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
    // With several files, "the configuration is broken" is not a useful thing to tell someone.
    CHECK(loaded.error().starts_with(root + "/groups.toml is not valid TOML: "));
    // And the broken file is left exactly as the operator wrote it: falling back to the default
    // would be a typo in `groups.toml` quietly granting nobody any permissions.
    CHECK(*fileIO::readText(root + "/groups.toml") == "[unclosed\n");
}

TEST_CASE("every commented default is valid TOML") {
    // The templates are edited by hand and compiled in, so a broken one would only show up on
    // somebody's first run. Here instead.
    const std::vector<std::pair<std::string, std::string_view>> templates = {
        {"animations.toml", kAnimationsDefaultToml}, {"boss_bar.toml", kBossBarDefaultToml},
        {"commands.toml", kCommandsDefaultToml},     {"compatibility.toml", kCompatibilityDefaultToml},
        {"groups.toml", kGroupsDefaultToml},         {"scoreboard.toml", kScoreboardDefaultToml},
        {"server.toml", kServerDefaultToml},         {"server_list.toml", kServerListDefaultToml},
        {"tab_list.toml", kTabListDefaultToml},      {"world_settings.toml", kWorldSettingsDefaultToml},
        {"worlds.toml", kWorldsDefaultToml},
    };
    for (const auto& [name, contents] : templates) {
        CAPTURE(name);
        CHECK_FALSE(contents.empty());
        // Written for an operator to read, not an empty skeleton.
        CHECK(contents.find('#') != std::string_view::npos);
        CHECK(toml::parse(contents).has_value());
    }
}
