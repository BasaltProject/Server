#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::command::CommandTarget;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::ExecutionSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::DroppedItem;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;

namespace {

[[nodiscard]] PlayerSession sessionOf(const std::string& name) {
    return PlayerSession{Uuid{1, 2}, name, 7, PlayerConnection{}};
}

} // namespace

TEST_CASE("a player target answers about the player") {
    PlayerSession steve = sessionOf("Steve");
    steve.worldName = "world";
    steve.x = 1.5;
    steve.tags.emplace("chosen");

    const CommandTarget target{steve};

    CHECK(target.player() == &steve);
    CHECK(target.entity() == nullptr);
    CHECK(target.name() == "Steve");
    CHECK(target.entityId() == 7);
    CHECK(target.uuid() == steve.uuid().toString());
    CHECK(target.type() == Identifier::vanilla("player"));
    CHECK(target.worldName() == "world");
    CHECK(target.x() == doctest::Approx(1.5));
    CHECK(target.tags().contains("chosen"));
    CHECK(target.alive());
    // parity: the scoreboard knows a player by name and everything else by uuid.
    CHECK(target.scoreboardName() == "Steve");
    CHECK(target.width() == doctest::Approx(CommandTarget::kPlayerWidth));
}

TEST_CASE("a dead player is not alive, whatever else is true of them") {
    PlayerSession steve = sessionOf("Steve");
    steve.dead = true;

    CHECK_FALSE(CommandTarget{steve}.alive());
}

TEST_CASE("an entity target answers about the entity, and is named after its type") {
    ServerEntity cow{3, Uuid{5, 6}, Identifier::vanilla("cow"), 11, 2.5, 64.0, -1.5};
    cow.tags.emplace_back("marked");

    const CommandTarget target{cow, "nether"};

    CHECK(target.player() == nullptr);
    CHECK(target.entity() == &cow);
    CHECK(target.name() == "cow");
    CHECK(target.type() == Identifier::vanilla("cow"));
    CHECK(target.worldName() == "nether");
    CHECK(target.y() == doctest::Approx(64.0));
    CHECK(target.scoreboardName() == cow.uuid().toString());
    CHECK(target.tags().contains("marked"));

    cow.customName = "Bessie";
    CHECK(CommandTarget{cow}.name() == "Bessie");
}

TEST_CASE("an entity that was removed or has no health left is not alive") {
    ServerEntity cow{3, Uuid{5, 6}, Identifier::vanilla("cow"), 11, 0.0, 0.0, 0.0};
    CHECK(CommandTarget{cow}.alive());

    cow.health = 0.0;
    CHECK_FALSE(CommandTarget{cow}.alive());

    cow.health = 10.0;
    cow.removed = true;
    CHECK_FALSE(CommandTarget{cow}.alive());
}

TEST_CASE("an item on the floor is a target like any other") {
    DroppedItem item;
    item.entityId = 12;
    item.uuid = Uuid{9, 9};
    item.item = Identifier::vanilla("diamond");
    item.x = 4.5;

    const CommandTarget target{item, "world"};

    CHECK(target.name() == "diamond");
    CHECK(target.entityId() == 12);
    CHECK(target.type() == CommandTarget::itemType());
    CHECK(target.x() == doctest::Approx(4.5));
    CHECK(target.yaw() == doctest::Approx(0.0F));
    CHECK(target.tags().empty());
    CHECK(target.alive());
    CHECK(target.width() == doctest::Approx(CommandTarget::kItemWidth));
    CHECK(target.player() == nullptr);
    CHECK(target.entity() == nullptr);
}

TEST_CASE("an execute link keeps everything it was not told to change") {
    const ConsoleCommandSource console;
    PlayerSession steve = sessionOf("Steve");
    steve.x = 10.0;

    const ExecutionSource asSteve{console, ExecutionSource::Changes{.player = &steve}};

    CHECK(asSteve.name() == "Steve");
    CHECK(asSteve.displayName() == Component::text("Steve"));
    // parity: `execute as` runs as somebody without running with their permissions.
    CHECK(asSteve.permissionLevel() == console.permissionLevel());
    CHECK(asSteve.hasPermission("anything.at.all"));
    // The position is the console's until something moves it.
    CHECK(asSteve.position()[0] == doctest::Approx(console.position()[0]));

    const ExecutionSource moved{asSteve, ExecutionSource::Changes{.position = std::array<double, 3>{1.0, 2.0, 3.0}}};
    CHECK(moved.position()[1] == doctest::Approx(2.0));
    CHECK(moved.player() == &steve);
}

TEST_CASE("what a link changed is inherited by the links after it") {
    const ConsoleCommandSource console;
    const ExecutionSource anchored{console,
                                   ExecutionSource::Changes{.rotation = std::pair{90.0F, 10.0F}, .anchorEyes = true}};
    const ExecutionSource after{anchored, {}};

    CHECK(after.rotation().first == doctest::Approx(90.0F));
    CHECK(after.anchorEyes());
}

TEST_CASE("feedback still belongs to whoever started the chain") {
    const ConsoleCommandSource console;
    const ExecutionSource first{console, {}};
    const ExecutionSource second{first, {}};

    CHECK(&second.root() == &console);
    CHECK(second.asExecution() == &second);
    CHECK(console.asExecution() == nullptr);
}

TEST_CASE("a store reports the result to every link that asked, innermost first") {
    const ConsoleCommandSource console;
    std::vector<std::string> reported;
    const ExecutionSource outer{console,
                                ExecutionSource::Changes{.onResult = [&reported](bool success, std::int32_t result) {
                                    reported.push_back("outer " + std::to_string(result) + (success ? " ok" : " no"));
                                }}};
    const ExecutionSource inner{outer,
                                ExecutionSource::Changes{.onResult = [&reported](bool success, std::int32_t result) {
                                    reported.push_back("inner " + std::to_string(result) + (success ? " ok" : " no"));
                                }}};

    inner.report(3);

    REQUIRE(reported.size() == 2);
    CHECK(reported[0] == "inner 3 ok");
    CHECK(reported[1] == "outer 3 ok");

    reported.clear();
    inner.report(0);
    CHECK(reported[0] == "inner 0 no");
}
