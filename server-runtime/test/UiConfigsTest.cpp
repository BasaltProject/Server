#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/AnimationsConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;
namespace play = basalt::wire::protocol::play;

namespace {

[[nodiscard]] ConfigDirectory written(std::string_view name, std::string_view fileName, std::string_view contents) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    fileIO::writeText((directory / fileName).string(), contents);
    return ConfigDirectory{directory.string()};
}

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

TEST_CASE("the tab list as its commented default has it") {
    const std::expected<TabListConfig, std::string> config =
        TabListConfig::load(written("ui-tab", "tab_list.toml", kTabListDefaultToml));
    REQUIRE(config.has_value());
    CHECK(config->enabled);
    CHECK(config->updateIntervalTicks > 0);
    REQUIRE_FALSE(config->profiles.empty());
    CHECK(config->defaultProfile().header.isDefault);
    CHECK(config->defaultProfile().headerLines.has_value());
}

TEST_CASE("a profile takes from the default what it does not say itself") {
    const std::expected<TabListConfig, std::string> config =
        TabListConfig::load(written("ui-tab-inherit", "tab_list.toml", R"(
enabled = true
update-interval-ticks = 5

[profiles.plain]
default = true
header = ["a header"]
footer = "a footer"

[profiles.busy]
priority = 5
active = true
footer = ["another footer"]
)"));
    REQUIRE(config.has_value());
    CHECK(config->updateIntervalTicks == 5);
    REQUIRE(config->profiles.size() == 2);

    const TabListProfile& busy = config->profiles[1];
    CHECK_FALSE(busy.headerLines.has_value());
    const TabListProfile filled = busy.inheriting(config->defaultProfile());
    REQUIRE(filled.headerLines.has_value());
    CHECK(*filled.headerLines == std::vector<std::string>{"a header"});
    // What it did say is its own, not the default's.
    CHECK(*filled.footerLines == std::vector<std::string>{"another footer"});
    // A string is a block of lines cut at its newlines.
    CHECK(*config->defaultProfile().footerLines == std::vector<std::string>{"a footer"});

    // Inheriting from itself changes nothing, which is what the default profile does.
    const TabListProfile itself = config->defaultProfile().inheriting(config->defaultProfile());
    CHECK(*itself.headerLines == std::vector<std::string>{"a header"});
}

TEST_CASE("what a profile takes from the default is only what it did not say") {
    const std::expected<TabListConfig, std::string> config =
        TabListConfig::load(written("ui-tab-own", "tab_list.toml", R"(
[profiles.plain]
default = true
header = ["default header"]
footer = ["default footer"]

[profiles.own]
header = ["own header"]
)"));
    REQUIRE(config.has_value());
    // Its own header stays its own, and the footer it never mentioned comes from the default.
    const TabListProfile filled = config->profiles[1].inheriting(config->defaultProfile());
    CHECK(*filled.headerLines == std::vector<std::string>{"own header"});
    CHECK(*filled.footerLines == std::vector<std::string>{"default footer"});

    // A file that says nothing about being switched on is switched on: the tab list is what
    // vanilla already draws, so leaving it alone means leaving it as it was.
    CHECK(config->enabled);
    // And what it says nothing about refreshing is one second, not one tick and not never.
    CHECK(config->updateIntervalTicks == 20);
    CHECK_FALSE(TabListConfig::disabled().enabled);
}

TEST_CASE("an interval nobody can wait for is floored rather than counted backwards") {
    const std::expected<TabListConfig, std::string> negative = TabListConfig::load(
        written("ui-tab-negative", "tab_list.toml", "update-interval-ticks = -5\n[profiles.a]\ndefault = true\n"));
    REQUIRE(negative.has_value());
    CHECK(negative->updateIntervalTicks == 0);

    const std::expected<TabListConfig, std::string> asked = TabListConfig::load(
        written("ui-tab-asked", "tab_list.toml", "update-interval-ticks = 3\n[profiles.a]\ndefault = true\n"));
    REQUIRE(asked.has_value());
    CHECK(asked->updateIntervalTicks == 3);
}

TEST_CASE("a file that is switched off says so where somebody looking for it would read it") {
    const basalt::wire::common::LogLevel previous = basalt::wire::common::Logger::defaultMinLevel();
    basalt::wire::common::Logger::setDefaultMinLevel(basalt::wire::common::LogLevel::Debug);
    const CapturedLog log;
    const std::expected<TabListConfig, std::string> config =
        TabListConfig::load(written("ui-tab-off", "tab_list.toml", "enabled = false\n[profiles.a]\ndefault = true\n"));
    basalt::wire::common::Logger::setDefaultMinLevel(previous);
    REQUIRE(config.has_value());
    CHECK_FALSE(config->enabled);
    // Not a warning: an operator who turned it off knows, and one who is wondering why nothing is
    // drawn finds the line in a debug log.
    CHECK(log.saidSomethingWith("is switched off"));
}

TEST_CASE("a scoreboard longer than the client draws is said out loud") {
    const CapturedLog log;
    std::string lines;
    for (std::int32_t index = 0; index < 20; ++index) {
        lines += "  \"line " + std::to_string(index) + "\",\n";
    }
    const std::expected<ScoreboardConfig, std::string> config = ScoreboardConfig::load(
        written("ui-board-long", "scoreboard.toml",
                "[profiles.default]\ndefault = true\ntitle = \"Board\"\nlines = [\n" + lines + "]\n"));
    REQUIRE(config.has_value());
    // Not dropped here: the file is read as written and the drawing end takes the first fifteen.
    CHECK(config->defaultProfile().lines->size() == 20);
    CHECK(ScoreboardConfig::kMaxLines == 15);
    CHECK(log.saidSomethingWith("has 20 lines; the client draws 15"));

    // Off unless the operator turns it on, unlike the tab list.
    CHECK_FALSE(config->enabled);
    CHECK(ScoreboardConfig::disabled().enabled == false);
    CHECK(ScoreboardConfig::disabled().defaultProfile().header.name == "default");
    CHECK_FALSE(ScoreboardConfig::disabled().defaultProfile().title.has_value());
}

TEST_CASE("a board takes the title it does not have from the default one") {
    const std::expected<ScoreboardConfig, std::string> config =
        ScoreboardConfig::load(written("ui-board", "scoreboard.toml", R"(
enabled = true
[profiles.plain]
default = true
title = "A Server"
lines = ["one"]
[profiles.event]
priority = 3
lines = ["two"]
)"));
    REQUIRE(config.has_value());
    CHECK(config->enabled);
    const ScoreboardProfile filled = config->profiles[1].inheriting(config->defaultProfile());
    CHECK(*filled.title == "A Server");
    CHECK(*filled.lines == std::vector<std::string>{"two"});

    // And a profile with a title of its own and no rows takes the rows and keeps the title.
    const std::expected<ScoreboardConfig, std::string> other =
        ScoreboardConfig::load(written("ui-board-title", "scoreboard.toml", R"(
[profiles.plain]
default = true
title = "A Server"
lines = ["one"]
[profiles.event]
title = "An Event"
)"));
    REQUIRE(other.has_value());
    const ScoreboardProfile inherited = other->profiles[1].inheriting(other->defaultProfile());
    CHECK(*inherited.title == "An Event");
    CHECK(*inherited.lines == std::vector<std::string>{"one"});
    // A board of a size the client draws is not worth a word.
    CHECK_FALSE(other->enabled);
}

TEST_CASE("a board the client can draw is not complained about") {
    const CapturedLog log;
    const std::expected<ScoreboardConfig, std::string> config = ScoreboardConfig::load(written(
        "ui-board-short", "scoreboard.toml", "[profiles.default]\ndefault = true\nlines = [\"one\", \"two\"]\n"));
    REQUIRE(config.has_value());
    CHECK_FALSE(log.saidSomethingWith("the client draws"));
}

TEST_CASE("the boss bar as its commented default has it") {
    const std::expected<BossBarConfig, std::string> config =
        BossBarConfig::load(written("ui-bar", "boss_bar.toml", kBossBarDefaultToml));
    REQUIRE(config.has_value());
    // Off by default: it sits over the game and the client gives it to real bosses.
    CHECK_FALSE(config->enabled);
    const BossBarProfile& profile = config->defaultProfile();
    CHECK(profile.color == play::BossBarColor::White);
    CHECK(profile.overlay == play::BossBarOverlay::Notched20);
    REQUIRE(profile.progress.has_value());
    CHECK(*profile.progress == "1.0");
}

TEST_CASE("what a bar is called, and what a wrong word costs") {
    const CapturedLog log;
    const std::expected<BossBarConfig, std::string> config =
        BossBarConfig::load(written("ui-bar-words", "boss_bar.toml", R"(
enabled = true
[profiles.default]
default = true
text = "hello"
color = "PURPLE"
overlay = "notched_6"
progress = 1

[profiles.wrong]
color = "beige"
overlay = "dotted"
)"));
    REQUIRE(config.has_value());
    const BossBarProfile& fine = config->defaultProfile();
    // However it was capitalized.
    CHECK(fine.color == play::BossBarColor::Purple);
    CHECK(fine.overlay == play::BossBarOverlay::Notched6);
    // A number is kept as written, because the field can also be a placeholder.
    CHECK(*fine.progress == "1");

    // One wrong word in decoration is not a file that cannot be understood: it falls back and says
    // so, rather than stopping the server.
    const BossBarProfile& wrong = config->profiles[1];
    CHECK(wrong.color == play::BossBarColor::White);
    CHECK(wrong.overlay == play::BossBarOverlay::Progress);
    CHECK(log.saidSomethingWith("has an unknown colour 'beige'; using white"));
    CHECK(log.saidSomethingWith("has an unknown overlay 'dotted'; using progress"));

    // What it left out it takes from the default.
    const BossBarProfile filled = wrong.inheriting(fine);
    CHECK(*filled.text == "hello");
    CHECK(*filled.progress == "1");
    // And what it said stays wrong-but-drawn rather than being taken from the default.
    CHECK(filled.color == play::BossBarColor::White);

    CHECK(bossBarColorName(play::BossBarColor::Pink) == "pink");
    CHECK(bossBarColorName(play::BossBarColor::White) == "white");
}

TEST_CASE("a bar takes the colour and the overlay it never mentioned") {
    const std::expected<BossBarConfig, std::string> config =
        BossBarConfig::load(written("ui-bar-inherit", "boss_bar.toml", R"(
[profiles.plain]
default = true
text = "hello"
color = "red"
overlay = "notched_10"
progress = "0.5"

[profiles.quiet]
text = "quiet"
)"));
    REQUIRE(config.has_value());
    // Switched off unless the operator asks: the bar sits over the game.
    CHECK_FALSE(config->enabled);

    const BossBarProfile& quiet = config->profiles[1];
    CHECK_FALSE(quiet.color.has_value());
    CHECK_FALSE(quiet.overlay.has_value());
    const BossBarProfile filled = quiet.inheriting(config->defaultProfile());
    CHECK(filled.color == play::BossBarColor::Red);
    CHECK(filled.overlay == play::BossBarOverlay::Notched10);
    CHECK(*filled.progress == "0.5");
    CHECK(*filled.text == "quiet");
}

TEST_CASE("the three files together, and a server that has read none of them") {
    const UiConfigs none;
    CHECK_FALSE(none.anyEnabled());
    CHECK_FALSE(none.tabList.enabled);
    CHECK_FALSE(none.scoreboard.enabled);
    CHECK_FALSE(none.bossBar.enabled);
    // Asking a switched-off file for its default is an answer, not a crash.
    CHECK(none.tabList.defaultProfile().header.name == "default");
    CHECK(none.bossBar.defaultProfile().header.isDefault);

    UiConfigs some;
    some.scoreboard.enabled = true;
    CHECK(some.anyEnabled());
}

TEST_CASE("which frame an animation is showing is a function of the clock") {
    const Animation animation{.name = "spin", .frames = {"a", "b", "c"}, .changeIntervalMillis = 100};
    CHECK(animation.frameAt(0) == "a");
    CHECK(animation.frameAt(99) == "a");
    CHECK(animation.frameAt(100) == "b");
    CHECK(animation.frameAt(250) == "c");
    CHECK(animation.frameAt(300) == "a");
    // Everyone looking at it at the same moment sees the same frame, whenever they joined.
    CHECK(animation.frameAt(1'000'000'000'123) == animation.frameAt(1'000'000'000'123));

    // One frame is a still picture, and never divides by anything.
    const Animation still{.name = "still", .frames = {"only"}, .changeIntervalMillis = 100};
    CHECK(still.frameAt(0) == "only");
    CHECK(still.frameAt(123456) == "only");
}

TEST_CASE("animations are read by name, and a half-written one is skipped") {
    const CapturedLog log;
    const std::expected<AnimationsConfig, std::string> config =
        AnimationsConfig::load(written("ui-animations", "animations.toml", R"(
[animations.Website]
change-interval-ms = 100
frames = ["one", "two"]

[animations.slow]
frames = ["x"]

[animations.fast]
change-interval-ms = 0
frames = ["x", "y"]

[animations.empty]
change-interval-ms = 100
)"));
    REQUIRE(config.has_value());
    CHECK(config->names() == std::vector<std::string>{"website", "slow", "fast"});

    const Animation* website = config->get("WEBSITE");
    REQUIRE(website != nullptr);
    CHECK(website->name == "website");
    CHECK(website->changeIntervalMillis == 100);
    CHECK(website->frames.size() == 2);

    // Without one of its own, the interval is the one the file's comment promises.
    CHECK(config->get("slow")->changeIntervalMillis == 200);
    // Faster than a tick is a legitimate choice that skips frames, not a number to rewrite; only
    // zero is floored, because dividing by it is the one thing that cannot happen.
    CHECK(config->get("fast")->changeIntervalMillis == AnimationsConfig::kMinimumIntervalMillis);

    // An animation with no frames is a half-written entry: skipped and named, not fatal.
    CHECK(config->get("empty") == nullptr);
    CHECK(log.saidSomethingWith("Animation 'empty'"));
    CHECK(log.saidSomethingWith("has no frames; skipping it"));

    CHECK(config->get("nothing") == nullptr);
}

TEST_CASE("the commented animations are the ones the other files refer to") {
    const std::expected<AnimationsConfig, std::string> config =
        AnimationsConfig::load(written("ui-animations-default", "animations.toml", kAnimationsDefaultToml));
    REQUIRE(config.has_value());
    const Animation* website = config->get("website");
    REQUIRE(website != nullptr);
    CHECK(website->frames.size() > 1);
    CHECK(website->changeIntervalMillis == 100);
}
