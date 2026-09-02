#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::PlayerCommandSource;
using basalt::server::runtime::config::GroupsConfig;
using basalt::server::runtime::config::PermissionGroup;
using basalt::server::runtime::config::PermissionRule;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;
namespace permissionLevel = basalt::server::runtime::command::permissionLevel;

namespace {

/// Two groups: everybody, and one that may broadcast and stop the server.
[[nodiscard]] GroupsConfig groupsConfig() {
    PermissionGroup everyone{
        .name = "default", .rules = {PermissionRule::parse("basalt.command.help")}, .isDefault = true};
    PermissionGroup admins{
        .name = "admins",
        .rules = {PermissionRule::parse("basalt.admin.broadcast"), PermissionRule::parse("basalt.command.stop")},
        .isOperator = true};
    return GroupsConfig{{everyone, admins}, 0, 1};
}

[[nodiscard]] PlayerSession sessionOf(const std::string& name, std::vector<Component>* sent = nullptr) {
    PlayerConnection connection;
    if (sent != nullptr) {
        connection.sendSystemMessage = [sent](const Component& message) { sent->push_back(message); };
    }
    return PlayerSession{Uuid{1, 1}, name, 1, std::move(connection)};
}

} // namespace

TEST_CASE("the console may do anything, named or not") {
    const ConsoleCommandSource console;

    CHECK(console.name() == "Server");
    CHECK(console.displayName() == Component::text("Server"));
    CHECK(console.permissionLevel() == permissionLevel::kOwners);
    CHECK(console.player() == nullptr);
    // Everything, unconditionally: a typo in groups.toml must not lock the operator out.
    CHECK(console.hasPermission("basalt.command.stop"));
    CHECK(console.hasPermission("nothing.anybody.declared"));
    CHECK(console.hasPermission(std::nullopt));
}

TEST_CASE("the console stands where the world says its spawn is") {
    const ConsoleCommandSource origin;
    CHECK(origin.position()[0] == doctest::Approx(0.0));

    const ConsoleCommandSource moved{basalt::wire::common::Logger{"Console"}, basalt::wire::text::LangTable{},
                                     [] { return std::array<double, 3>{8.5, 64.0, -12.5}; }};
    CHECK(moved.position()[0] == doctest::Approx(8.5));
    CHECK(moved.position()[2] == doctest::Approx(-12.5));
}

TEST_CASE("a player's permissions are their group's, and nothing else") {
    const GroupsConfig groups = groupsConfig();
    PlayerSession steve = sessionOf("Steve");

    const PlayerCommandSource source{steve, groups};
    CHECK(source.hasPermission("basalt.command.help"));
    CHECK_FALSE(source.hasPermission("basalt.command.stop"));
    // A node nobody named is denied rather than granted.
    CHECK_FALSE(source.hasPermission(std::nullopt));

    steve.group = "admins";
    CHECK(source.hasPermission("basalt.command.stop"));
}

TEST_CASE("a player's vanilla level says only whether they hear other people's commands") {
    const GroupsConfig groups = groupsConfig();
    PlayerSession steve = sessionOf("Steve");
    const PlayerCommandSource source{steve, groups};

    CHECK(source.permissionLevel() == permissionLevel::kAll);

    steve.group = "admins";
    CHECK(source.permissionLevel() == permissionLevel::kOwners);
}

TEST_CASE("a player's name and where they stand are read live off the session") {
    const GroupsConfig groups = groupsConfig();
    PlayerSession steve = sessionOf("Steve");
    steve.x = 1.5;
    steve.y = 64.0;
    steve.z = -2.5;
    const PlayerCommandSource source{steve, groups};

    CHECK(source.name() == "Steve");
    CHECK(source.displayName() == Component::text("Steve"));
    CHECK(source.position()[0] == doctest::Approx(1.5));
    CHECK(source.player() == &steve);

    steve.x = 100.0;
    CHECK(source.position()[0] == doctest::Approx(100.0));
}

TEST_CASE("a player's feedback goes back to them as a system message") {
    const GroupsConfig groups = groupsConfig();
    std::vector<Component> sent;
    PlayerSession steve = sessionOf("Steve", &sent);
    const PlayerCommandSource source{steve, groups};

    source.sendSystemMessage(Component::text("done"));

    REQUIRE(sent.size() == 1);
    CHECK(sent[0] == Component::text("done"));
}

TEST_CASE("a source is asked about through the base, whichever kind it is") {
    const GroupsConfig groups = groupsConfig();
    PlayerSession steve = sessionOf("Steve");
    const PlayerCommandSource player{steve, groups};
    const ConsoleCommandSource console;
    const std::vector<const CommandSource*> sources{&player, &console};

    CHECK(sources[0]->player() != nullptr);
    CHECK(sources[1]->player() == nullptr);
    CHECK(sources[1]->permissionLevel() > sources[0]->permissionLevel());
}
