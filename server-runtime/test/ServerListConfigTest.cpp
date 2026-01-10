#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
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
namespace placeholder = basalt::wire::common::placeholder;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    fileIO::createDirectories(directory.string());
    return directory.string();
}

[[nodiscard]] ConfigDirectory written(std::string_view name, std::string_view contents) {
    const std::string root = scratch(name);
    fileIO::writeText(root + "/server_list.toml", contents);
    return ConfigDirectory{root};
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

/// A registry answering the one placeholder the shipped file conditions on.
[[nodiscard]] placeholder::PlaceholderRegistry protocolRegistry(std::int64_t protocol) {
    placeholder::PlaceholderRegistry registry;
    registry.registerProvider("basalt_player",
                              [protocol](std::string_view params, const placeholder::PlaceholderContext&) {
                                  if (params == "protocol") {
                                      return placeholder::PlaceholderValue::of(protocol);
                                  }
                                  return placeholder::PlaceholderValue::unknown();
                              });
    return registry;
}

[[nodiscard]] ServerListConfig loaded(std::string_view name, std::string_view contents) {
    std::expected<ServerListConfig, std::string> config = ServerListConfig::load(written(name, contents));
    REQUIRE(config.has_value());
    return std::move(*config);
}

} // namespace

TEST_CASE("who is pinging is only looked up when a profile asks") {
    std::int32_t lookups = 0;
    const PingRequest lazy{770, "play.example.com", std::string{"203.0.113.7"}, [&lookups] {
                               ++lookups;
                               return std::optional<std::string>{"Steve"};
                           }};
    CHECK(lazy.protocolVersion() == 770);
    CHECK(lazy.host() == "play.example.com");
    CHECK(*lazy.address() == "203.0.113.7");
    // A ping is a request anyone on the internet can send as fast as they like, and the answer is
    // a database read.
    CHECK(lookups == 0);
    CHECK(*lazy.knownPlayer() == "Steve");
    CHECK(*lazy.knownPlayer() == "Steve");
    CHECK(lookups == 1);

    // A request that already knows asks nobody.
    const PingRequest known{770, "play.example.com", std::nullopt, std::optional<std::string>{"Alex"}};
    CHECK(*known.knownPlayer() == "Alex");
    CHECK_FALSE(known.address().has_value());

    // And one with nothing to look up answers nothing rather than calling an empty function.
    const PingRequest nobody{770, "play.example.com", std::nullopt};
    CHECK_FALSE(nobody.knownPlayer().has_value());
}

TEST_CASE("the shipped file greets old clients differently from new ones") {
    const ServerListConfig config = loaded("list-default", kServerListDefaultToml);
    CHECK(config.profiles().size() >= 3);
    CHECK(config.defaultProfile().header.name == "default");
    CHECK_FALSE(config.rememberIp());

    // 735 is 1.16, the first version with RGB text: below it the gradient is flat white, so the
    // same layout is built out of the sixteen colours those clients do have.
    const ServerListProfile modern = config.select(protocolRegistry(770));
    REQUIRE(modern.motd.has_value());
    CHECK(modern.motd->front().find("gradient") != std::string::npos);

    const ServerListProfile legacy = config.select(protocolRegistry(47));
    REQUIRE(legacy.motd.has_value());
    CHECK(legacy.motd->front().find("gradient") == std::string::npos);
    CHECK(legacy.motd->front().find("BASALT SERVER") != std::string::npos);
}

TEST_CASE("a profile switched on by hand is what everyone sees until it is switched off") {
    const CapturedLog log;
    const ServerListConfig config = loaded("list-active", R"(
[profiles.default]
default = true
motd = ["ordinary"]
icon = "server-icon.png"

[profiles.maintenance]
active = true
priority = 100
motd = ["under maintenance"]
hide-player-count = true
)");
    // Named at startup: a server showing its maintenance message because somebody left a flag on
    // is a confusing thing to debug from the outside.
    CHECK(log.saidSomethingWith("Server list profile 'maintenance' is switched on in"));

    const ServerListProfile chosen = config.select(protocolRegistry(770));
    CHECK(*chosen.motd == std::vector<std::string>{"under maintenance"});
    CHECK(chosen.hidePlayerCount == true);
    // What it did not restate comes from the default, which is the point of the overrides.
    CHECK(*chosen.icon == "server-icon.png");
}

TEST_CASE("every field a profile leaves out is the default profile's") {
    const ServerListConfig config = loaded("list-inherit", R"(
[profiles.default]
default = true
motd = ["ordinary"]
version-name = "Basalt 26.2"
version-protocol = 776
hide-player-count = false
max-players = 80
online-players = 3
hover = ["one", "two"]
icon = "server-icon.png"

[profiles.other]
active = true
motd = ["different"]
)");
    const ServerListProfile chosen = config.select(protocolRegistry(770));
    CHECK(*chosen.motd == std::vector<std::string>{"different"});
    CHECK(*chosen.versionName == "Basalt 26.2");
    CHECK(chosen.versionProtocol == 776);
    CHECK(chosen.hidePlayerCount == false);
    CHECK(chosen.maxPlayers == 80);
    CHECK(chosen.onlinePlayers == 3);
    CHECK(*chosen.hover == std::vector<std::string>{"one", "two"});
    CHECK(*chosen.icon == "server-icon.png");

    // The default profile inheriting from itself is itself, not a copy with holes in it.
    const ServerListProfile fallback = config.defaultProfile().inheriting(config.defaultProfile());
    CHECK(*fallback.motd == std::vector<std::string>{"ordinary"});
    CHECK(*fallback.versionName == "Basalt 26.2");

    // A profile that says a thing keeps it even when the default says otherwise.
    const ServerListProfile own = config.profiles()[1].inheriting(config.defaultProfile());
    CHECK(*own.motd == std::vector<std::string>{"different"});
}

TEST_CASE("a profile that says nothing at all is the default profile in every field") {
    const ServerListConfig config = loaded("list-empty-profile", R"(
[profiles.default]
default = true
motd = ["ordinary"]
version-name = "Basalt 26.2"
icon = "server-icon.png"

[profiles.quiet]
active = true
)");
    const ServerListProfile chosen = config.select(protocolRegistry(770));
    // Including the greeting itself, which is the field an operator is most likely to forget.
    CHECK(*chosen.motd == std::vector<std::string>{"ordinary"});
    CHECK(*chosen.versionName == "Basalt 26.2");
    CHECK(*chosen.icon == "server-icon.png");
}

TEST_CASE("the profile named at startup is the one that is on, not the one with the most priority") {
    const CapturedLog log;
    const ServerListConfig config = loaded("list-active-priority", R"(
[profiles.default]
default = true
motd = ["ordinary"]

[profiles.conditional]
priority = 500
when = ["%basalt_player_protocol% > 1000"]
motd = ["conditional"]

[profiles.maintenance]
active = true
priority = 1
motd = ["under maintenance"]
)");
    // A profile with a higher priority that nothing has selected is not switched on.
    CHECK(log.saidSomethingWith("Server list profile 'maintenance' is switched on in"));
    CHECK_FALSE(log.saidSomethingWith("'conditional' is switched on"));
    // And a ping that does select it wins on priority, because both of them then apply.
    CHECK(*config.select(protocolRegistry(2000)).motd == std::vector<std::string>{"conditional"});
    CHECK(*config.select(protocolRegistry(770)).motd == std::vector<std::string>{"under maintenance"});
}

TEST_CASE("a ping nothing matches gets the default, and the greeting is off unless asked for") {
    const ServerListConfig config = loaded("list-none", R"(
[profiles.default]
default = true
motd = ["ordinary"]

[profiles.busy]
priority = 5
when = ["%basalt_player_protocol% > 1000"]
motd = ["busy"]

[greeting]
remember-ip = true
)");
    CHECK(config.rememberIp());
    // A greeting table that says nothing does not remember anybody: keeping the address somebody
    // pinged from is a decision an operator makes, not a default.
    const ServerListConfig quiet = loaded("list-greeting-bare", R"(
[profiles.default]
default = true
motd = ["ordinary"]

[greeting]
)");
    CHECK_FALSE(quiet.rememberIp());
    CHECK(*config.select(protocolRegistry(770)).motd == std::vector<std::string>{"ordinary"});
    CHECK(*config.select(protocolRegistry(2000)).motd == std::vector<std::string>{"busy"});
}

TEST_CASE("a vanilla message of the day is imported into both greetings") {
    const basalt::wire::common::Logger logger{"test"};
    const std::string imported = serverListDefaultContents(std::string{"A vanilla server"}, logger);
    // Both, so the same server does not greet two people differently for no reason they chose.
    CHECK(imported.find("A vanilla server") != std::string::npos);
    CHECK(imported.find("Welcome to your new server") == std::string::npos);
    // The maintenance profile is not a greeting: its own text stays, headline and all.
    CHECK(imported.find("The server is under maintenance") != std::string::npos);
    CHECK(imported.find("BASALT SERVER") != std::string::npos);

    // A properties file writes a second line as the two characters `\n`, not as a newline.
    const std::string twoLines = serverListDefaultContents(std::string{"first\\nsecond"}, logger);
    CHECK(twoLines.find("    \"first\",\n    \"second\",\n") != std::string::npos);

    // What TOML cannot hold as written is escaped rather than breaking the file it is written into.
    const std::string quoted = serverListDefaultContents(std::string{"a \"quoted\" \\ server"}, logger);
    CHECK(quoted.find(R"(\"quoted\")") != std::string::npos);
    const std::expected<basalt::wire::common::toml::TomlTable, basalt::wire::common::toml::TomlError> parsed =
        basalt::wire::common::toml::parse(quoted);
    CHECK(parsed.has_value());

    // Nothing to import leaves the shipped file exactly as it ships.
    CHECK(serverListDefaultContents(std::nullopt, logger) == kServerListDefaultToml);
    CHECK(serverListDefaultContents(std::string{"   "}, logger) == kServerListDefaultToml);
}

TEST_CASE("an imported message with section signs is reported rather than silently mangled") {
    const CapturedLog log;
    const basalt::wire::common::Logger logger{"test"};
    const std::string imported = serverListDefaultContents(std::string{"§cA red server"}, logger);
    CHECK(imported.find("A red server") != std::string::npos);
    // Not translated: they are the one thing the modern format has no place for.
    CHECK(log.saidSomethingWith("section-sign colour codes"));
    CHECK(log.saidSomethingWith("server_list.toml"));
}

TEST_CASE("a first run beside a vanilla server writes the imported greeting into the file") {
    const std::string root = scratch("list-import");
    const ConfigDirectory directory{root};
    const std::expected<ServerListConfig, std::string> config =
        ServerListConfig::load(directory, std::string{"An imported server"}, basalt::wire::common::Logger{"test"});
    REQUIRE(config.has_value());
    const std::optional<std::string> onDisk = fileIO::readText(directory.pathOf("server_list.toml"));
    REQUIRE(onDisk.has_value());
    CHECK(onDisk->find("An imported server") != std::string::npos);
    CHECK(*config->defaultProfile().motd == std::vector<std::string>{"An imported server"});

    // It is a first-run import: the file on disk wins from then on.
    const std::expected<ServerListConfig, std::string> again =
        ServerListConfig::load(directory, std::string{"Something else"}, basalt::wire::common::Logger{"test"});
    REQUIRE(again.has_value());
    CHECK(*again->defaultProfile().motd == std::vector<std::string>{"An imported server"});
}
