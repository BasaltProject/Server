#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/ui/PlayerUi.hpp>

#include <doctest.h>

#include <algorithm>
#include <vector>

using basalt::server::runtime::config::BossBarConfig;
using basalt::server::runtime::config::BossBarProfile;
using basalt::server::runtime::config::ScoreboardConfig;
using basalt::server::runtime::config::ScoreboardProfile;
using basalt::server::runtime::config::TabListConfig;
using basalt::server::runtime::config::TabListProfile;
using basalt::server::runtime::config::UiConfigs;
using basalt::server::runtime::ui::PlayerUi;
using basalt::server::runtime::ui::SurfaceContent;
using basalt::wire::common::placeholder::PlaceholderContext;
using basalt::wire::common::placeholder::PlaceholderRegistry;
using basalt::wire::common::placeholder::PlaceholderValue;
namespace play = basalt::wire::protocol::play;

namespace {

struct Fixture {
    UiConfigs configs;
    PlaceholderRegistry placeholders;
    std::vector<std::int32_t> sent;
    PlayerUi::Plugin plugin;
    std::string name = "Steve";

    Fixture() {
        placeholders.registerProvider(
            "basalt_test", [this](std::string_view, const PlaceholderContext&) { return PlaceholderValue::of(name); });
    }

    [[nodiscard]] PlayerUi ui() {
        return PlayerUi{[this] { return configs; }, placeholders,
                        [this](std::int32_t id, const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
                            basalt::wire::io::ByteBuffer buffer;
                            write(buffer);
                            sent.push_back(id);
                        },
                        plugin};
    }

    [[nodiscard]] std::int32_t count(std::int32_t id) const {
        return static_cast<std::int32_t>(std::ranges::count(sent, id));
    }
};

} // namespace

TEST_CASE("nothing is drawn while every file is switched off") {
    Fixture fixture;
    PlayerUi ui = fixture.ui();
    for (std::int64_t tick = 0; tick < 40; tick++) {
        ui.tick(tick, PlaceholderContext{});
    }
    CHECK(fixture.sent.empty());
}

TEST_CASE("a panel is sent once and only again when what it says changes") {
    Fixture fixture;
    fixture.configs.tabList =
        TabListConfig{.enabled = true,
                      .updateIntervalTicks = 1,
                      .profiles = {TabListProfile{.headerLines = std::vector<std::string>{"Hello %basalt_test_name%"}}},
                      .defaultIndex = 0};
    PlayerUi ui = fixture.ui();

    ui.tick(0, PlaceholderContext{});
    CHECK(fixture.count(play::TabListPacket::kId) == 1);

    // Twenty more ticks of exactly the same text are not worth a packet each.
    for (std::int64_t tick = 1; tick <= 20; tick++) {
        ui.tick(tick, PlaceholderContext{});
    }
    CHECK(fixture.count(play::TabListPacket::kId) == 1);

    fixture.name = "Alex";
    ui.tick(21, PlaceholderContext{});
    CHECK(fixture.count(play::TabListPacket::kId) == 2);
}

TEST_CASE("an interval of zero draws a static panel once and never again") {
    Fixture fixture;
    fixture.configs.tabList =
        TabListConfig{.enabled = true,
                      .updateIntervalTicks = 0,
                      .profiles = {TabListProfile{.headerLines = std::vector<std::string>{"Welcome"}}},
                      .defaultIndex = 0};
    PlayerUi ui = fixture.ui();
    for (std::int64_t tick = 0; tick < 100; tick++) {
        ui.tick(tick, PlaceholderContext{});
    }
    CHECK(fixture.count(play::TabListPacket::kId) == 1);
}

TEST_CASE("a sidebar draws its rows and takes away the ones it lost") {
    Fixture fixture;
    fixture.configs.scoreboard = ScoreboardConfig{
        .enabled = true,
        .updateIntervalTicks = 1,
        .profiles = {ScoreboardProfile{.title = "Board", .lines = std::vector<std::string>{"one", "two", "three"}}},
        .defaultIndex = 0};
    PlayerUi ui = fixture.ui();
    ui.tick(0, PlaceholderContext{});
    CHECK(fixture.count(play::SetObjectivePacket::kId) == 1);
    CHECK(fixture.count(play::SetDisplayObjectivePacket::kId) == 1);
    CHECK(fixture.count(play::SetScorePacket::kId) == 3);

    // A board that got shorter has to have its leftovers taken away.
    fixture.configs.scoreboard.profiles[0].lines = std::vector<std::string>{"one"};
    ui.tick(1, PlaceholderContext{});
    CHECK(fixture.count(play::ResetScorePacket::kId) == 2);
    // The row that did not change is not re-sent.
    CHECK(fixture.count(play::SetScorePacket::kId) == 3);
}

TEST_CASE("a plugin holding the sidebar turns it on where the file left it off") {
    Fixture fixture;
    std::optional<SurfaceContent> held{SurfaceContent{.title = "Plugin", .lines = {"a", "b"}}};
    fixture.plugin.holdsSidebar = [] { return true; };
    fixture.plugin.sidebar = [&held] { return held; };
    PlayerUi ui = fixture.ui();

    ui.tick(0, PlaceholderContext{});
    CHECK(fixture.count(play::SetObjectivePacket::kId) == 1);
    CHECK(fixture.count(play::SetScorePacket::kId) == 2);

    // Letting go with the file still off takes the board down again.
    held.reset();
    ui.tick(20, PlaceholderContext{});
    CHECK(fixture.count(play::SetObjectivePacket::kId) == 2);
}

TEST_CASE("a boss bar is nudged rather than re-added when its text changes") {
    Fixture fixture;
    fixture.configs.bossBar =
        BossBarConfig{.enabled = true,
                      .updateIntervalTicks = 1,
                      .profiles = {BossBarProfile{.text = "%basalt_test_name%", .progress = "0.5"}},
                      .defaultIndex = 0};
    PlayerUi ui = fixture.ui();
    ui.tick(0, PlaceholderContext{});
    CHECK(fixture.count(play::BossEventPacket::kId) == 1);

    fixture.name = "Alex";
    ui.tick(1, PlaceholderContext{});
    // One update, not a remove and an add: re-adding restarts the client's own fade.
    CHECK(fixture.count(play::BossEventPacket::kId) == 2);

    ui.clear();
    CHECK(fixture.count(play::BossEventPacket::kId) == 3);
}
