#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/placeholder/PlayerPlaceholders.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <doctest.h>

using basalt::server::runtime::config::GroupsConfig;
using basalt::server::runtime::config::PingRequest;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::common::placeholder::PlaceholderContext;
using basalt::wire::common::placeholder::PlaceholderKind;
using basalt::wire::common::placeholder::PlaceholderValue;
namespace placeholder = basalt::server::runtime::placeholder;

namespace {

/// Two groups: everybody, and one that may broadcast.
[[nodiscard]] GroupsConfig groupsConfig() {
    using basalt::server::runtime::config::PermissionGroup;
    using basalt::server::runtime::config::PermissionRule;
    PermissionGroup everyone{
        .name = "default", .rules = {PermissionRule::parse("basalt.command.help")}, .isDefault = true};
    PermissionGroup admins{
        .name = "admins", .rules = {PermissionRule::parse("basalt.admin.broadcast")}, .isOperator = true};
    return GroupsConfig{{everyone, admins}, 0, 1};
}

struct Fixture {
    GroupsConfig groups = groupsConfig();
    PlayerSession player{
        Uuid{1, 2}, "Steve", 7,
        PlayerConnection{.protocolVersion = 776, .connectedHost = "play.example.org", .address = "203.0.113.7"}};

    Fixture() {
        player.worldName = "world";
        player.x = 21.70704236703265;
        player.y = 64.0;
        player.z = -3.5;
        player.yaw = 90.0F;
    }

    [[nodiscard]] basalt::wire::common::placeholder::PlaceholderProvider provider() {
        return placeholder::playerPlaceholders(placeholder::PlayerPlaceholderSources{
            .groups = [this]() -> const GroupsConfig& { return groups; },
            .world = [](std::string_view) { return nullptr; },
        });
    }

    [[nodiscard]] PlaceholderContext ofPlayer() const {
        return PlaceholderContext{}.with(placeholder::playerSlot(), &player);
    }
};

std::string text(const PlaceholderValue& value) {
    REQUIRE(value.isText());
    return value.text;
}

} // namespace

TEST_CASE("a player answers about itself") {
    Fixture fixture;
    const auto provider = fixture.provider();
    const PlaceholderContext context = fixture.ofPlayer();
    CHECK(text(provider("name", context)) == "Steve");
    CHECK(text(provider("world", context)) == "world");
    CHECK(text(provider("protocol", context)) == "776");
    CHECK(text(provider("host", context)) == "play.example.org");
    CHECK(text(provider("ip", context)) == "203.0.113.7");
    CHECK(text(provider("known", context)) == "true");
    CHECK(text(provider("gamemode", context)) == "survival");
    CHECK(text(provider("health_rounded", context)) == "20");
    // parity: the compass vanilla's F3 screen shows, from the same yaw ranges.
    CHECK(text(provider("direction", context)) == "W");
    CHECK(text(provider("direction_xz", context)) == "W");
}

TEST_CASE("a numeric placeholder rounds to the digits asked for") {
    Fixture fixture;
    const auto provider = fixture.provider();
    const PlaceholderContext context = fixture.ofPlayer();
    CHECK(text(provider("x_2", context)) == "21.71");
    CHECK(text(provider("x_0", context)) == "22");
    CHECK(text(provider("z_1", context)) == "-3.5");
    // A real name that happens to end in a digit is not a precision.
    CHECK(provider("nonsense_2", context).kind == PlaceholderKind::Unknown);
}

TEST_CASE("a server list ping answers what a handshake carries and no more") {
    Fixture fixture;
    const auto provider = fixture.provider();
    const PingRequest ping{770, "play.example.org", std::optional<std::string>{"198.51.100.4"},
                           std::optional<std::string>{"Alex"}};
    const PlaceholderContext context = PlaceholderContext{}.with(placeholder::pingSlot(), &ping);

    CHECK(text(provider("protocol", context)) == "770");
    CHECK(text(provider("ip", context)) == "198.51.100.4");
    CHECK(text(provider("name", context)) == "Alex");
    CHECK(text(provider("known", context)) == "true");

    // There is no player, so a player's own question says so rather than inventing a value.
    const PlaceholderValue position = provider("x", context);
    CHECK(position.kind == PlaceholderKind::Unavailable);
    REQUIRE(position.reason.has_value());
    CHECK(position.reason->find("server list ping") != std::string::npos);
}

TEST_CASE("a ping from a stranger says why it cannot greet them") {
    Fixture fixture;
    const PingRequest ping{770, "play.example.org", std::optional<std::string>{"198.51.100.4"},
                           std::optional<std::string>{}};
    const PlaceholderContext context = PlaceholderContext{}.with(placeholder::pingSlot(), &ping);

    const PlaceholderValue name = fixture.provider()("name", context);
    CHECK(name.kind == PlaceholderKind::Unavailable);
    REQUIRE(name.reason.has_value());
    CHECK(name.reason->find("nobody has connected from that address") != std::string::npos);
    CHECK(text(fixture.provider()("known", context)) == "false");
}

TEST_CASE("text about nobody says it is about nobody") {
    Fixture fixture;
    const PlaceholderValue name = fixture.provider()("name", PlaceholderContext{});
    CHECK(name.kind == PlaceholderKind::Unavailable);
    REQUIRE(name.reason.has_value());
    CHECK(*name.reason == "this text is not about any player");
}

TEST_CASE("a placeholder waiting on a system this build lacks says which") {
    Fixture fixture;
    const PlaceholderValue held = fixture.provider()("item_in_hand", fixture.ofPlayer());
    CHECK(held.kind == PlaceholderKind::Unavailable);
    REQUIRE(held.reason.has_value());
    CHECK(*held.reason == "there is no inventory system yet");

    CHECK(fixture.provider()("not_a_placeholder", fixture.ofPlayer()).kind == PlaceholderKind::Unknown);
}

TEST_CASE("a statistic is named without its namespaces") {
    CHECK(placeholder::expandStatKey("mined_stone") == "minecraft:mined/minecraft:stone");
    CHECK(placeholder::expandStatKey("deaths") == "minecraft:custom/minecraft:deaths");
    // A key already carrying a namespace is left alone.
    CHECK(placeholder::expandStatKey("minecraft:custom/minecraft:jump") == "minecraft:custom/minecraft:jump");

    Fixture fixture;
    fixture.player.stats.increment("minecraft:custom/minecraft:deaths", 3);
    CHECK(text(fixture.provider()("stat_deaths", fixture.ofPlayer())) == "3");
}
