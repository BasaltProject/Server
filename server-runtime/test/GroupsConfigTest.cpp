#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

[[nodiscard]] ConfigDirectory writtenGroups(std::string_view name, std::string_view contents) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    fileIO::writeText((directory / "groups.toml").string(), contents);
    return ConfigDirectory{directory.string()};
}

[[nodiscard]] GroupsConfig loaded(std::string_view name, std::string_view contents) {
    const std::expected<GroupsConfig, std::string> config = GroupsConfig::load(writtenGroups(name, contents));
    REQUIRE(config.has_value());
    return *config;
}

[[nodiscard]] std::string failure(std::string_view name, std::string_view contents) {
    const std::expected<GroupsConfig, std::string> config = GroupsConfig::load(writtenGroups(name, contents));
    REQUIRE_FALSE(config.has_value());
    return config.error();
}

} // namespace

TEST_CASE("the most specific rule wins whatever order they are written in") {
    const GroupsConfig config = loaded("groups-specificity", R"(
[groups.default]
default = true
permissions = ["basalt.command.help"]

[groups.operator]
operator = true
permissions = ["*"]

[groups.moderator]
permissions = [
    "-basalt.command.seed",
    "basalt.command.*",
    "basalt.*",
    "-basalt.command.give.other",
]
)");
    CHECK(config.resolve("moderator", "basalt.command.kick"));
    CHECK(config.resolve("moderator", "basalt.world.load"));
    CHECK_FALSE(config.resolve("moderator", "basalt.command.seed"));
    CHECK_FALSE(config.resolve("moderator", "basalt.command.give.other"));
    CHECK(config.resolve("moderator", "basalt.command.give"));
    CHECK_FALSE(config.resolve("moderator", "other.plugin.thing"));
}

TEST_CASE("a file that cannot mean what it says is refused rather than half read") {
    CHECK(failure("groups-empty", "# nothing here\n").ends_with("declares no [groups] table."));
    CHECK(failure("groups-none", "[groups]\n").ends_with("declares no groups."));

    CHECK(failure("groups-two-defaults", R"(
[groups.a]
default = true
[groups.b]
default = true
operator = true
)")
              .ends_with("must mark exactly one group as `default`, found 2."));
    CHECK(failure("groups-no-default", R"(
[groups.a]
operator = true
)")
              .ends_with("must mark exactly one group as `default`, found 0."));
    CHECK(failure("groups-no-operator", R"(
[groups.a]
default = true
)")
              .ends_with("must mark exactly one group as `operator`, found 0."));

    CHECK(failure("groups-missing-parent", R"(
[groups.a]
default = true
operator = true
inherits = "nothing"
)") == "Group 'a' inherits from 'nothing', which does not exist.");

    const std::string cycle = failure("groups-cycle", R"(
[groups.a]
default = true
operator = true
inherits = "b"
[groups.b]
inherits = "a"
)");
    CHECK(cycle.find("forms a cycle through") != std::string::npos);
}
