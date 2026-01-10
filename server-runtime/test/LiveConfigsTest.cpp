#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/LiveConfigs.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

[[nodiscard]] ConfigDirectory freshDirectory(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    return ConfigDirectory{directory.string()};
}

} // namespace

TEST_CASE("what a reload re-reads, named rather than judged") {
    // The line is a property of the code: these are read every time they are used. `server.toml`
    // and `worlds.toml` become sockets, thread pools and loaded worlds, so they are not here.
    const std::vector<std::string_view> files{LiveConfigs::kReloadableFiles.begin(),
                                              LiveConfigs::kReloadableFiles.end()};
    CHECK(files == std::vector<std::string_view>{"groups.toml", "server_list.toml", "commands.toml", "animations.toml",
                                                 "tab_list.toml", "scoreboard.toml", "boss_bar.toml"});
    CHECK(std::ranges::find(files, "server.toml") == files.end());
    CHECK(std::ranges::find(files, "worlds.toml") == files.end());
}

TEST_CASE("a first read generates every file it needs and answers all of them") {
    const ConfigDirectory directory = freshDirectory("live-fresh");
    const std::expected<ConfigSnapshot, std::string> snapshot = LiveConfigs::read(directory);
    REQUIRE(snapshot.has_value());

    // Every reloadable file is now on disk, commented, for the operator to edit.
    for (const std::string_view name : LiveConfigs::kReloadableFiles) {
        CAPTURE(name);
        CHECK(fileIO::exists(directory.pathOf(name)));
    }
    CHECK(snapshot->groups.defaultGroup().name == "default");
    CHECK(snapshot->serverList.defaultProfile().header.name == "default");
    CHECK(snapshot->commands.custom().empty());
    CHECK(snapshot->animations.get("website") != nullptr);
    CHECK(snapshot->ui.tabList.enabled);
}

TEST_CASE("one broken file is the whole read failing, not three files applied and one skipped") {
    // Whichever one it is: a reload that applied the files it managed to read would leave a server
    // in a state no file describes.
    for (const std::string_view broken : LiveConfigs::kReloadableFiles) {
        CAPTURE(broken);
        const ConfigDirectory directory = freshDirectory("live-broken");
        // Generate them all first, so the only difference is the one file that does not parse.
        REQUIRE(LiveConfigs::read(directory).has_value());
        fileIO::writeText(directory.pathOf(broken), "[unclosed\n");

        const std::expected<ConfigSnapshot, std::string> snapshot = LiveConfigs::read(directory);
        REQUIRE_FALSE(snapshot.has_value());
        CHECK(snapshot.error().find(std::string{broken} + " is not valid TOML") != std::string::npos);
    }
}

TEST_CASE("a reader sees the whole old set or the whole new one") {
    const ConfigDirectory directory = freshDirectory("live-swap");
    std::expected<ConfigSnapshot, std::string> initial = LiveConfigs::read(directory);
    REQUIRE(initial.has_value());
    LiveConfigs live{std::move(*initial)};
    CHECK(live.current()->ui.scoreboard.enabled == false);

    // A snapshot held by a reader stays as it was, whatever is swapped in behind it.
    const std::shared_ptr<const ConfigSnapshot> held = live.current();
    fileIO::writeText(directory.pathOf("scoreboard.toml"),
                      "enabled = true\n[profiles.default]\ndefault = true\ntitle = \"Board\"\n");
    std::expected<ConfigSnapshot, std::string> reloaded = LiveConfigs::read(directory);
    REQUIRE(reloaded.has_value());
    live.replace(std::move(*reloaded));

    CHECK(live.current()->ui.scoreboard.enabled);
    CHECK_FALSE(held->ui.scoreboard.enabled);
    CHECK(live.current() != held);
}

TEST_CASE("reading and reloading at the same time never hands out half a set") {
    const ConfigDirectory directory = freshDirectory("live-threads");
    std::expected<ConfigSnapshot, std::string> initial = LiveConfigs::read(directory);
    REQUIRE(initial.has_value());
    LiveConfigs live{std::move(*initial)};

    std::atomic<bool> running{true};
    std::atomic<std::int32_t> reads{0};
    std::vector<std::thread> readers;
    readers.reserve(4);
    for (std::int32_t index = 0; index < 4; ++index) {
        readers.emplace_back([&live, &running, &reads] {
            while (running.load()) {
                const std::shared_ptr<const ConfigSnapshot> snapshot = live.current();
                // Whatever is being swapped in, what a reader holds is one whole set.
                CHECK(snapshot->groups.defaultGroup().name == "default");
                CHECK(snapshot->serverList.defaultProfile().header.name == "default");
                reads.fetch_add(1);
            }
        });
    }
    for (std::int32_t swap = 0; swap < 20; ++swap) {
        std::expected<ConfigSnapshot, std::string> next = LiveConfigs::read(directory);
        REQUIRE(next.has_value());
        live.replace(std::move(*next));
    }
    running.store(false);
    for (std::thread& reader : readers) {
        reader.join();
    }
    CHECK(reads.load() > 0);
}
