#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::runtime::config;
namespace forwarding = basalt::wire::auth::forwarding;
namespace fileIO = basalt::wire::io::fileIO;
namespace toml = basalt::wire::common::toml;
using basalt::wire::common::LogLevel;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] BasaltConfig parsed(std::string_view text) {
    const std::expected<toml::TomlTable, toml::TomlError> root = toml::parse(text);
    REQUIRE(root.has_value());
    const std::expected<BasaltConfig, std::string> config = BasaltConfig::fromToml(*root, "config/server.toml");
    REQUIRE(config.has_value());
    return *config;
}

/// Every line the server logged while this is alive, for the messages that are the whole point of
/// a warning.
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

    [[nodiscard]] std::size_t size() const noexcept { return lines_.size(); }

private:
    std::vector<std::string> lines_;
};

} // namespace

TEST_CASE("the commented default is what a vanilla server would have been") {
    const BasaltConfig config = parsed(kServerDefaultToml);
    CHECK(config.port == 25565);
    // Twenty is a number from 2010 and nothing in the server cares, so the default is eighty.
    CHECK(config.maxPlayers == 80);
    CHECK(config.onlineMode);
    CHECK(config.gamemode == "survival");
    CHECK_FALSE(config.whitelist);
    CHECK_FALSE(config.forwarding.enabled());
    CHECK(config.logLevel == LogLevel::Info);
    CHECK(config.logToFile);
    CHECK(config.logKeepDays == 14);
    CHECK(config.logColor == ColorMode::Auto);
    CHECK_FALSE(config.generationMetrics);
    // More than one region is not vanilla, so it is off until an operator asks.
    CHECK_FALSE(config.regional);
    CHECK(config.regionThreads == 0);
    CHECK(config.pluginsEnabled);
    CHECK(config.pluginInstructionsPerTurn == 250000);
    CHECK(config.pluginMemoryBytes == 32LL * 1024 * 1024);
    CHECK(config.pluginLanes == 1);
    CHECK(config.orphanData == "warn");
    CHECK(config.pluginAllowances.empty());
}

TEST_CASE("an option the file does not mention keeps its default") {
    const BasaltConfig config = parsed("[server]\nport = 25566\n");
    CHECK(config.port == 25566);
    CHECK(config.maxPlayers == 80);
    CHECK(config.onlineMode);
    CHECK(config.logLevel == LogLevel::Info);

    // A file with nothing in it at all is every default, not a failure.
    const BasaltConfig empty = parsed("");
    CHECK(empty.port == 25565);
    CHECK(empty.gamemode == "survival");
}

TEST_CASE("what the file says, read back") {
    const BasaltConfig config = parsed(R"(
[server]
port = 25570
max-players = 12
online-mode = false
gamemode = "creative"
whitelist = true

[logging]
level = "debug"
file = false
keep-days = 3
color = "never"

[performance]
generation-metrics = true
regional = true
region-threads = "4"

[plugins]
enabled = false
instructions-per-turn = 900000
memory-mb = 64
test-lanes = 3
orphan-data = "quiet"

[plugins.allow]
example = ["packets", "commands"]
other = []
)");
    CHECK(config.port == 25570);
    CHECK(config.maxPlayers == 12);
    CHECK_FALSE(config.onlineMode);
    CHECK(config.gamemode == "creative");
    CHECK(config.whitelist);

    // The level is named however the operator felt like spelling it.
    CHECK(config.logLevel == LogLevel::Debug);
    CHECK_FALSE(config.logToFile);
    CHECK(config.logKeepDays == 3);
    CHECK(config.logColor == ColorMode::Never);

    CHECK(config.generationMetrics);
    CHECK(config.regional);
    CHECK(config.regionThreads == 4);

    CHECK_FALSE(config.pluginsEnabled);
    CHECK(config.pluginInstructionsPerTurn == 900000);
    CHECK(config.pluginMemoryBytes == 64LL * 1024 * 1024);
    CHECK(config.pluginLanes == 3);
    CHECK(config.orphanData == "quiet");
    REQUIRE(config.pluginAllowances.size() == 2);
    CHECK(config.pluginAllowances.at("example") == std::vector<std::string>{"packets", "commands"});
    CHECK(config.pluginAllowances.at("other").empty());
}

TEST_CASE("a number that would stop every plugin is floored rather than taken as written") {
    // A budget of five instructions reads as "plugins are broken" rather than as the typo it is.
    const BasaltConfig tiny = parsed("[plugins]\ninstructions-per-turn = 5\nmemory-mb = 0\n");
    CHECK(tiny.pluginInstructionsPerTurn == 1000);
    CHECK(tiny.pluginMemoryBytes == 1024 * 1024);
    CHECK(parsed("[plugins]\ntest-lanes = 0\n").pluginLanes == 1);
    CHECK(parsed("[plugins]\ntest-lanes = -4\n").pluginLanes == 1);

    // Above the floor the number is the operator's.
    CHECK(parsed("[plugins]\ninstructions-per-turn = 1001\n").pluginInstructionsPerTurn == 1001);
}

TEST_CASE("region threads is a number or the word that means work it out") {
    // parity of shape with Folia's own `threads = -1`: anything that is not a number means "decide
    // for me", which the scheduler does rather than the file.
    CHECK(parsed("[performance]\nregion-threads = \"auto\"\n").regionThreads == 0);
    CHECK(parsed("[performance]\nregion-threads = \"6\"\n").regionThreads == 6);
    CHECK(parsed("[performance]\nregion-threads = \"6 threads\"\n").regionThreads == 0);
    CHECK(parsed("[performance]\n").regionThreads == 0);
}

TEST_CASE("a spelling nobody recognizes keeps the default rather than stopping the server") {
    // A log level is not worth refusing to start over: the file says what it says and the server
    // says what it read.
    CHECK(parsed("[logging]\nlevel = \"chatty\"\n").logLevel == LogLevel::Info);
    CHECK(parsed("[logging]\ncolor = \"beige\"\n").logColor == ColorMode::Auto);
    // However it was spelled: an operator writing DEBUG in capitals meant the same thing.
    CHECK(parsed("[logging]\nlevel = \"DEBUG\"\n").logLevel == LogLevel::Debug);
    CHECK(parsed("[logging]\nlevel = \"Warn\"\n").logLevel == LogLevel::Warn);
    CHECK(parsed("[logging]\ncolor = \"NEVER\"\n").logColor == ColorMode::Never);
    CHECK(parsed("[logging]\ncolor = \"Always\"\n").logColor == ColorMode::Always);
    CHECK(colorModeName(ColorMode::Auto) == "auto");
    CHECK(colorModeName(ColorMode::Always) == "always");
    CHECK(colorModeName(ColorMode::Never) == "never");
}

TEST_CASE("a forwarding mode that is not one stops the server rather than being ignored") {
    const std::expected<toml::TomlTable, toml::TomlError> root =
        toml::parse("[network.forwarding]\nmode = \"velocitty\"\n");
    REQUIRE(root.has_value());
    const std::expected<BasaltConfig, std::string> config = BasaltConfig::fromToml(*root, "config/server.toml");
    REQUIRE_FALSE(config.has_value());
    // Named, and with the list of what it could have been: a typo here silently accepts every
    // login as whoever asks.
    CHECK(config.error() ==
          "config/server.toml: forwarding mode must be one of none, bungeecord, bungeeguard, safenet, "
          "velocity, not \"velocitty\"");
}

TEST_CASE("a forwarding mode without the secret it needs is refused at startup") {
    const std::expected<toml::TomlTable, toml::TomlError> root =
        toml::parse("[network.forwarding]\nmode = \"velocity\"\n");
    REQUIRE(root.has_value());
    const std::expected<forwarding::ProxyForwarding, std::string> read =
        forwardingConfig::fromToml(*root, "config/server.toml");
    // Finding out one player at a time is worse than being told here.
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().starts_with("config/server.toml: "));
}

TEST_CASE("the forwarding an operator configured, read back") {
    const std::expected<toml::TomlTable, toml::TomlError> root = toml::parse(R"(
[network.forwarding]
mode = "bungeeguard"
tokens = ["one", "", "two"]
property-name = "custom"
)");
    REQUIRE(root.has_value());
    const std::expected<forwarding::ProxyForwarding, std::string> read =
        forwardingConfig::fromToml(*root, "config/server.toml");
    REQUIRE(read.has_value());
    CHECK(read->mode == forwarding::ForwardingMode::BungeeGuard);
    // An empty token would accept an empty token, so it is not one.
    CHECK(read->tokens == std::vector<std::string>{"one", "two"});
    CHECK(read->propertyName == "custom");
    CHECK(read->enabled());
    CHECK(read->authenticated());

    // Without a property name it is SafeNET's own, which is what the mode expects to find.
    const std::expected<toml::TomlTable, toml::TomlError> plain =
        toml::parse("[network.forwarding]\nmode = \"safenet\"\ntokens = [\"one\"]\n");
    REQUIRE(plain.has_value());
    const std::expected<forwarding::ProxyForwarding, std::string> defaulted =
        forwardingConfig::fromToml(*plain, "config/server.toml");
    REQUIRE(defaulted.has_value());
    CHECK(defaulted->propertyName == forwarding::ProxyForwarding::kSafeNetProperty);

    // A mode of `none` is nobody forwarding whatever else the table holds: the tokens beside it
    // are not a configuration, they are leftovers.
    const std::expected<toml::TomlTable, toml::TomlError> off =
        toml::parse("[network.forwarding]\nmode = \"none\"\ntokens = [\"one\"]\nsecret = \"s\"\n");
    REQUIRE(off.has_value());
    const std::expected<forwarding::ProxyForwarding, std::string> disabled =
        forwardingConfig::fromToml(*off, "config/server.toml");
    REQUIRE(disabled.has_value());
    CHECK_FALSE(disabled->enabled());
    CHECK(disabled->tokens.empty());
    CHECK(disabled->secret.empty());

    // Nothing configured at all is nobody forwarding, which is what a server without a proxy is.
    const std::expected<toml::TomlTable, toml::TomlError> bare = toml::parse("[server]\nport = 1\n");
    REQUIRE(bare.has_value());
    const std::expected<forwarding::ProxyForwarding, std::string> none =
        forwardingConfig::fromToml(*bare, "config/server.toml");
    REQUIRE(none.has_value());
    CHECK_FALSE(none->enabled());
    CHECK(none->mode == forwarding::ForwardingMode::None);
    CHECK(none->propertyName == forwarding::ProxyForwarding::kSafeNetProperty);
}

TEST_CASE("a vanilla server.properties is read the way java writes it") {
    const std::map<std::string, std::string, std::less<>> values = parseJavaProperties("# a comment\n"
                                                                                       "!ignored=yes\n"
                                                                                       "\n"
                                                                                       "server-port=25599\n"
                                                                                       "  max-players = 40  \n"
                                                                                       "motd:A vanilla server\n"
                                                                                       "no-separator\n"
                                                                                       "online-mode=false\n");
    CHECK(values.size() == 4);
    CHECK(values.at("server-port") == "25599");
    // Whitespace around the key and before the value goes; what is inside the value stays.
    CHECK(values.at("max-players") == "40");
    CHECK(values.at("motd") == "A vanilla server");
    CHECK(values.at("online-mode") == "false");
    CHECK(values.find("no-separator") == values.end());
    // Java's own comment characters, both of them.
    CHECK(values.find("!ignored") == values.end());
    CHECK(values.find("ignored") == values.end());
}

TEST_CASE("moving a vanilla server over is copy the folder and start") {
    const std::string root = scratch("config-import");
    const std::string properties = root + "/server.properties";
    fileIO::createDirectories(root);
    fileIO::writeText(properties, "server-port=25599\nmax-players=40\nonline-mode=false\nmotd=A vanilla server\n");

    const ConfigDirectory directory{root + "/config"};
    const std::expected<BasaltConfig, std::string> config =
        BasaltConfig::load(directory, properties, basalt::wire::common::Logger{"test"}, false);
    REQUIRE(config.has_value());
    CHECK(config->port == 25599);
    CHECK(config->maxPlayers == 40);
    CHECK_FALSE(config->onlineMode);
    // Everything it does not answer is Basalt's own default, not vanilla's.
    CHECK(config->gamemode == "survival");

    // The generated file is still the commented default, with the imported values written into it.
    const std::optional<std::string> written = fileIO::readText(directory.pathOf("server.toml"));
    REQUIRE(written.has_value());
    CHECK(written->find("port = 25599") != std::string::npos);
    CHECK(written->find("port = 25565") == std::string::npos);
    CHECK(written->find('#') != std::string::npos);

    // It is an import, not a compatibility layer: what the properties file says afterwards is not
    // read again.
    fileIO::writeText(properties, "server-port=25000\n");
    const std::expected<BasaltConfig, std::string> again =
        BasaltConfig::load(directory, properties, basalt::wire::common::Logger{"test"}, false);
    REQUIRE(again.has_value());
    CHECK(again->port == 25599);

    // The message of the day is the one thing read back out of it on demand, for whoever writes
    // the server list file.
    CHECK(BasaltConfig::legacyMotd(properties) == std::nullopt);
    fileIO::writeText(properties, "motd=A vanilla server\n");
    CHECK(BasaltConfig::legacyMotd(properties) == "A vanilla server");
    CHECK(BasaltConfig::legacyMotd(root + "/nothing.properties") == std::nullopt);
}

TEST_CASE("only what a properties file really answers is imported") {
    const std::string root = scratch("config-import-partial");
    const std::string properties = root + "/server.properties";
    fileIO::createDirectories(root);
    // `yes` is not what Java writes for a boolean, so it is not a decision to import: the option
    // stays at Basalt's default rather than being read as some third thing.
    fileIO::writeText(properties, "online-mode=yes\nserver-port=notanumber\n");

    const ConfigDirectory directory{root + "/config"};
    const std::expected<BasaltConfig, std::string> config =
        BasaltConfig::load(directory, properties, basalt::wire::common::Logger{"test"}, false);
    REQUIRE(config.has_value());
    CHECK(config->onlineMode);
    CHECK(config->port == 25565);
}

TEST_CASE("what the operator is told about a proxy speaking for its players") {
    const basalt::wire::common::Logger logger{"test"};

    {
        // Nothing forwarding, nothing to say.
        const CapturedLog log;
        forwardingConfig::announce(forwarding::ProxyForwarding{}, true, 25565, logger);
        CHECK(log.size() == 0);
    }
    {
        // Unsigned forwarding: anybody who can reach the port can claim to be any player, and the
        // port is named because firewalling it is the answer.
        const CapturedLog log;
        forwardingConfig::announce(forwarding::ProxyForwarding{.mode = forwarding::ForwardingMode::BungeeCord}, false,
                                   25577, logger);
        CHECK(log.saidSomethingWith("forwarded by a bungeecord proxy"));
        CHECK(log.saidSomethingWith("port 25577"));
        CHECK_FALSE(log.saidSomethingWith("online-mode is on"));
    }
    {
        // Signed forwarding with online mode still on: the proxy authenticates, so this server
        // will not, and that is worth saying out loud.
        const CapturedLog log;
        forwardingConfig::announce(
            forwarding::ProxyForwarding{.mode = forwarding::ForwardingMode::Velocity, .secret = "a secret"}, true,
            25565, logger);
        CHECK(log.saidSomethingWith("forwarded by a velocity proxy"));
        CHECK(log.saidSomethingWith("online-mode is on"));
        CHECK_FALSE(log.saidSomethingWith("carries no secret"));
    }
}

TEST_CASE("loading says where the configuration came from, unless it was the silent boot read") {
    const std::string root = scratch("config-announce");
    const ConfigDirectory directory{root + "/config"};
    const basalt::wire::common::Logger logger{"test"};
    {
        const CapturedLog log;
        CHECK(BasaltConfig::load(directory, root + "/none.properties", logger, true).has_value());
        CHECK(log.saidSomethingWith("Loaded configuration from"));
    }
    {
        // The read that only exists to set up logging says nothing, so startup shows one line.
        const CapturedLog log;
        CHECK(BasaltConfig::load(directory, root + "/none.properties", logger, false).has_value());
        CHECK_FALSE(log.saidSomethingWith("Loaded configuration from"));
    }
}

TEST_CASE("a server with no properties beside it gets the untouched default") {
    const std::string root = scratch("config-fresh");
    const ConfigDirectory directory{root + "/config"};
    const std::expected<BasaltConfig, std::string> config =
        BasaltConfig::load(directory, root + "/server.properties", basalt::wire::common::Logger{"test"}, false);
    REQUIRE(config.has_value());
    CHECK(config->port == 25565);
    CHECK(*fileIO::readText(directory.pathOf("server.toml")) == kServerDefaultToml);
}
