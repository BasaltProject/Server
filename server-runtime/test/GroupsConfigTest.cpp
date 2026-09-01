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

/// A `config/` of its own holding one `groups.toml`, since that is what the loader reads.
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

TEST_CASE("a rule is a dotted name, a wildcard, or a denial of either") {
    const PermissionRule exact = PermissionRule::parse("basalt.command.stop");
    CHECK(exact.granted());
    CHECK(exact.specificity() == 3);
    CHECK(exact.matches("basalt.command.stop"));
    CHECK_FALSE(exact.matches("basalt.command.stopping"));
    CHECK_FALSE(exact.matches("basalt.command"));
    // Exact is exact: without a star it grants that one name and nothing under it.
    CHECK_FALSE(exact.matches("basalt.command.stop.now"));

    // A trailing star matches what is below it and the name itself.
    const PermissionRule wildcard = PermissionRule::parse("basalt.command.*");
    CHECK(wildcard.specificity() == 2);
    CHECK(wildcard.matches("basalt.command"));
    CHECK(wildcard.matches("basalt.command.stop"));
    CHECK(wildcard.matches("basalt.command.give.other"));
    CHECK_FALSE(wildcard.matches("basalt.world.load"));
    // Below it, not merely starting with it: `commandeer` is a different word.
    CHECK_FALSE(wildcard.matches("basalt.commandeer"));

    // The bare star is everything, and is the least specific thing there is.
    const PermissionRule everything = PermissionRule::parse("*");
    CHECK(everything.specificity() == 0);
    CHECK(everything.matches("anything.at.all"));
    CHECK(everything.matches(""));

    // A leading minus denies, and the space an operator left behind it is not part of the name.
    const PermissionRule denial = PermissionRule::parse("-basalt.command.seed");
    CHECK_FALSE(denial.granted());
    CHECK(denial.pattern() == "basalt.command.seed");
    CHECK(PermissionRule::parse("-  basalt.command.seed  ").pattern() == "basalt.command.seed");
    CHECK_FALSE(PermissionRule::parse("- basalt.*").granted());
}

TEST_CASE("the commented default is a server where nobody is an operator by accident") {
    const GroupsConfig config = loaded("groups-default", kGroupsDefaultToml);
    CHECK(config.names() == std::vector<std::string>{"default", "operator"});
    CHECK(config.defaultGroup().name == "default");
    CHECK(config.operatorGroup().name == "operator");

    // What a new player can do, and what they cannot.
    CHECK(config.resolve("default", "basalt.command.help"));
    CHECK(config.resolve("default", "basalt.command.list"));
    CHECK_FALSE(config.resolve("default", "basalt.command.stop"));
    CHECK_FALSE(config.resolve("default", "basalt.command.give"));

    // And an operator can do everything, which is what `permissions = ["*"]` says.
    CHECK(config.resolve("operator", "basalt.command.stop"));
    CHECK(config.resolve("operator", "anything.at.all"));
}

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
    // Everything under `basalt`, except the one command denied more specifically.
    CHECK(config.resolve("moderator", "basalt.command.kick"));
    CHECK(config.resolve("moderator", "basalt.world.load"));
    CHECK_FALSE(config.resolve("moderator", "basalt.command.seed"));
    CHECK_FALSE(config.resolve("moderator", "basalt.command.give.other"));
    // The wildcard above it still grants the rest of that family.
    CHECK(config.resolve("moderator", "basalt.command.give"));
    // Nothing outside `basalt` is granted at all, because nothing granted it.
    CHECK_FALSE(config.resolve("moderator", "other.plugin.thing"));
}

TEST_CASE("a group overrides what it inherits without having to be more specific") {
    const GroupsConfig config = loaded("groups-inherit", R"(
[groups.default]
default = true
permissions = ["basalt.command.help"]

[groups.operator]
operator = true
permissions = ["*"]

[groups.builder]
inherits = "default"
permissions = ["basalt.command.*"]

[groups.trainee]
inherits = "builder"
permissions = ["-basalt.command.*"]
)");
    // What it inherits.
    CHECK(config.resolve("builder", "basalt.command.help"));
    CHECK(config.resolve("builder", "basalt.command.kick"));

    // A rule of its own beats an inherited one of the same specificity, so a group can take away
    // what its parent gave without being more specific than the parent was.
    CHECK_FALSE(config.resolve("trainee", "basalt.command.kick"));
    // And what the grandparent granted more specifically still stands.
    CHECK(config.resolve("trainee", "basalt.command.help"));
}

TEST_CASE("a group nobody has heard of is the default one") {
    const GroupsConfig config = loaded("groups-unknown", kGroupsDefaultToml);
    CHECK(config.get(std::nullopt).name == "default");
    CHECK(config.get("nothing").name == "default");
    // However it was spelled: group names are written by hand in two places.
    CHECK(config.get("OPERATOR").name == "operator");
    CHECK(config.resolve("OPERATOR", "basalt.command.stop"));
    CHECK_FALSE(config.resolve(std::nullopt, "basalt.command.stop"));
    CHECK(config.resolve(std::nullopt, "basalt.command.help"));
}

TEST_CASE("a file that cannot mean what it says is refused rather than half read") {
    CHECK(failure("groups-empty", "# nothing here\n").ends_with("declares no [groups] table."));
    CHECK(failure("groups-none", "[groups]\n").ends_with("declares no groups."));

    // Exactly one of each, because "which group does a new player get" has one answer.
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

    // Inheriting from a group that is not there is a typo, and a silent one if it were allowed.
    CHECK(failure("groups-missing-parent", R"(
[groups.a]
default = true
operator = true
inherits = "nothing"
)") == "Group 'a' inherits from 'nothing', which does not exist.");

    // A cycle would otherwise be an infinite walk the first time anybody is checked.
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

TEST_CASE("a group name is the same name however it was capitalized") {
    // Written by hand here and stored in `players.db` from a command: the two have to agree, and
    // the file is the one an operator types into.
    const GroupsConfig config = loaded("groups-case", R"(
[groups.Default]
default = true
permissions = ["basalt.command.help"]

[groups.Operator]
operator = true
permissions = ["*"]

[groups.Moderator]
inherits = "Default"
permissions = ["basalt.command.kick"]
)");
    CHECK(config.names() == std::vector<std::string>{"default", "operator", "moderator"});
    CHECK(config.defaultGroup().name == "default");
    CHECK(config.get("moderator").name == "moderator");
    // The inheritance is found through the lowercased name too, so the group really did inherit.
    CHECK(config.resolve("moderator", "basalt.command.help"));
    CHECK(config.resolve("moderator", "basalt.command.kick"));
    CHECK_FALSE(config.resolve("moderator", "basalt.command.stop"));
}

TEST_CASE("a group that declares nothing grants nothing") {
    const GroupsConfig config = loaded("groups-bare", R"(
[groups.default]
default = true
operator = true
)");
    CHECK(config.names() == std::vector<std::string>{"default"});
    CHECK(config.defaultGroup().rules.empty());
    // A permission nobody granted has not been granted: there is no hidden ranking behind a group.
    CHECK_FALSE(config.resolve("default", "basalt.command.help"));
    CHECK_FALSE(config.resolve("default", ""));
}
