#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/CommandsConfig.hpp>
#include <basalt/server/runtime/config/CompatibilityConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::runtime::config;
namespace fileIO = basalt::wire::io::fileIO;
namespace multiversion = basalt::wire::multiversion;
namespace versions = basalt::wire::multiversion::protocolVersions;

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

[[nodiscard]] CompatibilityConfig compatibility(std::string_view name, std::string_view contents) {
    const std::expected<CompatibilityConfig, std::string> config =
        CompatibilityConfig::load(written(name, "server.toml", contents));
    REQUIRE(config.has_value());
    return *config;
}

[[nodiscard]] CommandsConfig commands(std::string_view name, std::string_view contents) {
    const std::expected<CommandsConfig, std::string> config =
        CommandsConfig::load(written(name, "commands.toml", contents));
    REQUIRE(config.has_value());
    return *config;
}

/// What the gate is asked in the end: whether that protocol may through, however it does it.
[[nodiscard]] bool accepts(const multiversion::ProtocolGate& gate, std::int32_t protocol) {
    return gate.supported().contains(protocol);
}

} // namespace

TEST_CASE("the commented default lets old clients in and keeps new ones out") {
    const CompatibilityConfig config = compatibility("compat-default", kServerDefaultToml);
    CHECK(config.backwards().all);
    // A newer client cannot finish joining, so it is turned away with an explanation rather than
    // let in to disconnect halfway through with a decoding error.
    CHECK(config.futureProtocols() == multiversion::UnknownProtocolPolicy::Refuse);
    CHECK(config.snapshots() == multiversion::UnknownProtocolPolicy::Refuse);
    CHECK(config.nameSubstitutes());
}

TEST_CASE("what the door is built from is what this build can actually translate") {
    const CompatibilityConfig all = compatibility("compat-all", "[network.java]\nbackwards = \"all\"\n");
    const std::set<std::int32_t> translatable{765, 766};
    const multiversion::ProtocolGate gate = all.gate(translatable);
    CHECK(accepts(gate, versions::kNative));
    CHECK(accepts(gate, 765));
    CHECK(accepts(gate, 766));
    CHECK_FALSE(accepts(gate, 700));

    // A list is exactly the list, plus the native version, which is never off: a server its own
    // clients cannot join is not a configuration anybody meant to write.
    const CompatibilityConfig some = compatibility("compat-list", "[network.java]\nbackwards = [765]\n");
    CHECK_FALSE(some.backwards().all);
    CHECK(some.backwards().protocols == std::set<std::int32_t>{765});
    const multiversion::ProtocolGate chosen = some.gate(translatable);
    CHECK(accepts(chosen, 765));
    CHECK_FALSE(accepts(chosen, 766));
    CHECK(accepts(chosen, versions::kNative));

    // However it was written: the word is a word, not a spelling.
    CHECK(compatibility("compat-caps", "[network.java]\nbackwards = \"  ALL  \"\n").backwards().all);

    // An empty list is not "all": it is somebody saying only the native version.
    const CompatibilityConfig none = compatibility("compat-empty", "[network.java]\nbackwards = []\n");
    CHECK_FALSE(none.backwards().all);
    const multiversion::ProtocolGate bare = none.gate(translatable);
    CHECK(accepts(bare, versions::kNative));
    CHECK_FALSE(accepts(bare, 765));
}

TEST_CASE("asking for a version this build cannot translate is said out loud, not silently dropped") {
    const CompatibilityConfig config = compatibility("compat-missing", "[network.java]\nbackwards = [765, 1]\n");
    const CapturedLog log;
    const multiversion::ProtocolGate gate = config.gate({765});
    CHECK(accepts(gate, 765));
    CHECK_FALSE(accepts(gate, 1));
    CHECK(log.saidSomethingWith("asks for protocols this build cannot translate"));
}

TEST_CASE("a list this build covers entirely is not warned about") {
    const CompatibilityConfig config = compatibility("compat-covered", "[network.java]\nbackwards = [765, 766]\n");
    const CapturedLog log;
    const multiversion::ProtocolGate gate = config.gate({765, 766});
    CHECK(accepts(gate, 765));
    CHECK(accepts(gate, 766));
    CHECK_FALSE(log.saidSomethingWith("cannot translate"));
}

TEST_CASE("a word that is neither a policy nor a list stops the server") {
    const std::expected<CompatibilityConfig, std::string> word =
        CompatibilityConfig::load(written("compat-word", "server.toml", "[network.java]\nbackwards = \"none\"\n"));
    REQUIRE_FALSE(word.has_value());
    // "none" and "775" are both someone meaning something specific, and reading either as "all"
    // is the worst answer available.
    CHECK(word.error().ends_with(": backwards must be \"all\" or a list of protocol numbers"));

    const std::expected<CompatibilityConfig, std::string> policy = CompatibilityConfig::load(
        written("compat-policy", "server.toml", "[network.java]\nfuture-protocols = \"maybe\"\n"));
    REQUIRE_FALSE(policy.has_value());
    CHECK(policy.error().ends_with(": future-protocols must be accept, warn or refuse, not \"maybe\""));

    const std::expected<CompatibilityConfig, std::string> snapshots = CompatibilityConfig::load(
        written("compat-snapshots", "server.toml", "[network.java]\nsnapshots = \"sometimes\"\n"));
    REQUIRE_FALSE(snapshots.has_value());
    CHECK(snapshots.error().ends_with(": snapshots must be accept, warn or refuse, not \"sometimes\""));
}

TEST_CASE("a file with nothing in it is every default") {
    const CompatibilityConfig config = compatibility("compat-bare", "# nothing\n");
    CHECK(config.backwards().all);
    CHECK(config.futureProtocols() == multiversion::UnknownProtocolPolicy::Refuse);
    CHECK(config.snapshots() == multiversion::UnknownProtocolPolicy::Refuse);
    CHECK(config.nameSubstitutes());

    // A `[java]` table that says nothing is the same as no table at all.
    const CompatibilityConfig bare = compatibility("compat-empty-java", "[network.java]\n");
    CHECK(bare.backwards().all);
    CHECK(bare.nameSubstitutes());
    CHECK(bare.futureProtocols() == multiversion::UnknownProtocolPolicy::Refuse);

    const CompatibilityConfig accepting =
        compatibility("compat-accept", "[network.java]\nfuture-protocols = \"accept\"\nsnapshots = \"warn\"\n"
                                       "name-substitutes = false\n");
    CHECK(accepting.futureProtocols() == multiversion::UnknownProtocolPolicy::Accept);
    CHECK(accepting.snapshots() == multiversion::UnknownProtocolPolicy::Warn);
    CHECK_FALSE(accepting.nameSubstitutes());
}

TEST_CASE("which bedrock versions may in, which is all of them unless somebody says otherwise") {
    const CompatibilityConfig defaults = compatibility("compat-bedrock-default", "# nothing\n");

    // The window of live protocols, and what happens past its top end.
    CHECK_FALSE(defaults.bedrock().protocols.has_value());
    CHECK(defaults.bedrock().futureProtocols == basalt::wire::bedrock::BedrockFuturePolicy::Warn);

    const CompatibilityConfig window = compatibility(
        "compat-bedrock-window", "[network.bedrock]\nprotocols = [2168]\nfuture-protocols = \"refuse\"\n");
    REQUIRE(window.bedrock().protocols.has_value());
    CHECK(window.bedrock().protocols->contains(2168));
    CHECK(window.bedrock().futureProtocols == basalt::wire::bedrock::BedrockFuturePolicy::Refuse);

    const auto badPolicy = CompatibilityConfig::load(
        written("compat-bedrock-policy", "server.toml", "[network.bedrock]\nfuture-protocols = \"maybe\"\n"));
    REQUIRE_FALSE(badPolicy.has_value());
    CHECK(badPolicy.error().find("accept, warn or refuse") != std::string::npos);
}

TEST_CASE("a command nobody configured is a command with nothing changed") {
    const CommandsConfig config = commands("commands-default", kCommandsDefaultToml);
    const CommandOverride& untouched = config.get("teleport");
    CHECK_FALSE(untouched.permission.has_value());
    CHECK(untouched.enabled);
    CHECK(untouched.aliases.empty());
    CHECK(config.custom().empty());
    CHECK(config.message("nothing") == nullptr);
}

TEST_CASE("what a command asks for, whether it exists, and what else it is called") {
    const CommandsConfig config = commands("commands-overrides", R"(
[commands.Teleport]
permission = "basalt.command.moderator"
aliases = ["tpa", "tp"]

[commands.seed]
enabled = false
)");
    // Written however the operator capitalized it, asked for however the caller spells it.
    const CommandOverride& teleport = config.get("teleport");
    REQUIRE(teleport.permission.has_value());
    CHECK(*teleport.permission == "basalt.command.moderator");
    CHECK(teleport.enabled);
    CHECK(teleport.aliases == std::vector<std::string>{"tpa", "tp"});
    CHECK(config.get("TELEPORT").aliases.size() == 2);

    // Off means it stops existing rather than existing and refusing.
    CHECK_FALSE(config.get("seed").enabled);
    CHECK_FALSE(config.get("seed").permission.has_value());
}

TEST_CASE("a command the operator invented, and one they left half written") {
    const CapturedLog log;
    const CommandsConfig config = commands("commands-custom", R"(
[custom.Discord]
message = ["Join us at", "example.invalid"]
aliases = ["dc"]

[custom.rules]
permission = "basalt.command.rules"
message = "one\ntwo"

[custom.empty]
permission = "basalt.command.empty"
)");
    REQUIRE(config.custom().size() == 2);
    const CustomCommand& discord = config.custom()[0];
    CHECK(discord.name == "discord");
    // Without one of its own, the permission is the one every command has by default.
    CHECK(discord.permission == "basalt.command.discord");
    CHECK(discord.message == std::vector<std::string>{"Join us at", "example.invalid"});
    CHECK(discord.aliases == std::vector<std::string>{"dc"});

    // One string is cut at its newlines, so a short answer needs no list.
    const CustomCommand& rules = config.custom()[1];
    CHECK(rules.name == "rules");
    CHECK(rules.permission == "basalt.command.rules");
    CHECK(rules.message == std::vector<std::string>{"one", "two"});

    // A command with nothing to say is a half-written entry: skipped and named, not fatal.
    CHECK(log.saidSomethingWith("Custom command 'empty'"));
    CHECK(log.saidSomethingWith("has no message; skipping it"));
    // And what did load is counted, so an operator can see their file took effect.
    CHECK(log.saidSomethingWith("Loaded 2 custom command(s) from"));
}

TEST_CASE("what a command says instead of what it says by default") {
    const CommandsConfig config = commands("commands-messages", R"(
[messages]
"command.unknown" = "I do not know that one."
"command.no-permission" = "Not for you."
)");
    REQUIRE(config.message("command.unknown") != nullptr);
    CHECK(*config.message("command.unknown") == "I do not know that one.");
    CHECK(*config.message("command.no-permission") == "Not for you.");
    CHECK(config.message("command.missing") == nullptr);
    CHECK(config.messages().size() == 2);

    // A message that is not text is not a message: it is left out rather than turned into an
    // empty line the operator never wrote.
    const CommandsConfig odd = commands("commands-odd-message", R"(
[messages]
"command.unknown" = 5
"command.ok" = "fine"
)");
    CHECK(odd.message("command.unknown") == nullptr);
    CHECK(odd.messages().size() == 1);
}

TEST_CASE("a file with no commands of its own says nothing about them") {
    const CapturedLog log;
    const CommandsConfig config = commands("commands-quiet", "[commands.help]\nenabled = true\n");
    CHECK(config.custom().empty());
    // Neither "loaded 0 custom commands" nor "disabled none of them": a line saying nothing
    // happened is a line to read on every start.
    CHECK_FALSE(log.saidSomethingWith("custom command"));
    CHECK_FALSE(log.saidSomethingWith("Disabled by"));
}

TEST_CASE("what the log says about a file that switched commands off") {
    const CapturedLog log;
    const CommandsConfig config = commands("commands-disabled", R"(
[commands.seed]
enabled = false
[commands.gamemode]
enabled = false
[commands.help]
enabled = true
)");
    CHECK_FALSE(config.get("seed").enabled);
    // Named and in a fixed order, because "some commands are missing" is the report otherwise.
    CHECK(log.saidSomethingWith("Disabled by "));
    CHECK(log.saidSomethingWith(": gamemode, seed."));
}
