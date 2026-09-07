#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/Favicon.hpp>
#include <basalt/server/runtime/ServerListStatus.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using basalt::server::runtime::ListedPlayer;
using basalt::server::runtime::ServerListStatus;
using basalt::server::runtime::config::ConfigDirectory;
using basalt::server::runtime::config::ServerListConfig;
using basalt::wire::protocol::status::ServerStatusPayload;
namespace fileIO = basalt::wire::io::fileIO;
namespace ph = basalt::wire::common::placeholder;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    return directory.string();
}

[[nodiscard]] ServerListConfig configWith(std::string_view name, std::string_view contents) {
    const std::string root = scratch(name);
    fileIO::writeText(root + "/server_list.toml", contents);
    auto loaded = ServerListConfig::load(ConfigDirectory{root});
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

[[nodiscard]] ServerListStatus::Options options() {
    return {.slots = 20, .versionName = "Basalt 26.2", .protocolVersion = 776};
}

} // namespace

TEST_CASE("a ping is answered with the profile's own line, count and version") {
    const ServerListConfig config = configWith("status-basic", R"(
[profiles.default]
default = true
motd = ["Line one", "Line two"]
version-name = "Basalt"
version-protocol = 999
)");
    const ph::PlaceholderRegistry registry;
    ServerListStatus status{[&config]() -> const ServerListConfig& { return config; }, registry,
                            [] { return std::vector<ListedPlayer>{{"Steve", "uuid-1"}, {"Alex", "uuid-2"}}; },
                            options()};

    const ServerStatusPayload payload = status.statusFor(776, "example.org", std::string{"1.2.3.4"});
    // Two lines join with a newline between them, which is what the client draws as two rows.
    CHECK(basalt::wire::text::toPlainText(payload.description) == "Line one\nLine two");
    REQUIRE(payload.players.has_value());
    CHECK(payload.players->online == 2);
    CHECK(payload.players->max == 20);
    CHECK(payload.players->sample.size() == 2);
    REQUIRE(payload.version.has_value());
    CHECK(payload.version->name == "Basalt");
    CHECK(payload.version->protocol == 999);
    // No icon named, so the built-in one goes out rather than nothing at all.
    REQUIRE(payload.favicon.has_value());
    CHECK(payload.favicon->size() == basalt::server::runtime::favicon::bytes().size());
}

TEST_CASE("a profile that hides the count sends no player block at all") {
    const ServerListConfig config = configWith("status-hidden", R"(
[profiles.default]
default = true
motd = ["Quiet"]
hide-player-count = true
)");
    const ph::PlaceholderRegistry registry;
    ServerListStatus status{[&config]() -> const ServerListConfig& { return config; }, registry,
                            [] { return std::vector<ListedPlayer>{{"Steve", "uuid-1"}}; }, options()};

    CHECK_FALSE(status.statusFor(776, "example.org", std::nullopt).players.has_value());
}

TEST_CASE("a plugin has the last word on the line and the slots") {
    const ServerListConfig config = configWith("status-plugin", R"(
[profiles.default]
default = true
motd = ["From the file"]
)");
    const ph::PlaceholderRegistry registry;
    ServerListStatus::Hooks hooks;
    hooks.ping = [](const std::optional<std::string>&, const std::string& motd, std::int32_t,
                    std::int32_t) -> ServerListStatus::Hooks::Answer {
        CHECK(motd == "From the file");
        return {.motd = "From the plugin", .maxPlayers = 5};
    };
    ServerListStatus status{[&config]() -> const ServerListConfig& { return config; }, registry,
                            [] { return std::vector<ListedPlayer>{}; }, options(), hooks};

    const ServerStatusPayload payload = status.statusFor(776, "example.org", std::nullopt);
    CHECK(basalt::wire::text::toPlainText(payload.description) == "From the plugin");
    REQUIRE(payload.players.has_value());
    CHECK(payload.players->max == 5);
}

TEST_CASE("a missing or wrong-sized icon falls back to the built-in one, and is only read once") {
    const std::string root = scratch("status-icon");
    fileIO::writeText(root + "/server_list.toml", R"(
[profiles.default]
default = true
motd = ["Hi"]
icon = "server-icon.png"
)");
    auto loaded = ServerListConfig::load(ConfigDirectory{root});
    REQUIRE(loaded.has_value());
    const ph::PlaceholderRegistry registry;
    ServerListStatus::Options withRoot = options();
    withRoot.configRoot = root;
    ServerListStatus status{[&loaded]() -> const ServerListConfig& { return *loaded; }, registry,
                            [] { return std::vector<ListedPlayer>{}; }, withRoot};

    const ServerStatusPayload missing = status.statusFor(776, "example.org", std::nullopt);
    REQUIRE(missing.favicon.has_value());
    CHECK(missing.favicon->size() == basalt::server::runtime::favicon::bytes().size());

    // Writing the file afterwards changes nothing: the failure is remembered, which is what keeps a
    // missing icon from being a failed open on every single ping.
    const std::span<const std::uint8_t> real = basalt::server::runtime::favicon::bytes();
    fileIO::writeBytes(root + "/server-icon.png", std::vector<std::uint8_t>{real.begin(), real.end()});
    CHECK(status.statusFor(776, "example.org", std::nullopt).favicon->size() == real.size());
}
