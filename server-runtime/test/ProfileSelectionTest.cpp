#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/ProfileSelection.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::config;
namespace placeholder = basalt::wire::common::placeholder;
namespace toml = basalt::wire::common::toml;

namespace {

/// One file's kind of profile: the shared header plus whatever that file adds.
struct Profile {
    ProfileHeader header;
    std::string text;
};

class CapturedLog {
public:
    CapturedLog() {
        basalt::wire::common::logOutput::addSink([this](std::string_view line) { lines_.emplace_back(line); });
    }

    CapturedLog(const CapturedLog&) = delete;
    CapturedLog& operator=(const CapturedLog&) = delete;

    ~CapturedLog() { basalt::wire::common::logOutput::clearSinks(); }

    [[nodiscard]] std::size_t countWith(std::string_view fragment) const {
        return static_cast<std::size_t>(std::ranges::count_if(
            lines_, [fragment](const std::string& line) { return line.find(fragment) != std::string::npos; }));
    }

private:
    std::vector<std::string> lines_;
};

[[nodiscard]] placeholder::PlaceholderRegistry serverRegistry(std::int64_t online) {
    placeholder::PlaceholderRegistry registry;
    registry.registerProvider("basalt_server",
                              [online](std::string_view params, const placeholder::PlaceholderContext&) {
                                  if (params == "online") {
                                      return placeholder::PlaceholderValue::of(online);
                                  }
                                  if (params == "nowhere") {
                                      return placeholder::PlaceholderValue::unavailable("there is no player here");
                                  }
                                  return placeholder::PlaceholderValue::unknown();
                              });
    return registry;
}

[[nodiscard]] toml::TomlTable parsed(std::string_view text) {
    const std::expected<toml::TomlTable, toml::TomlError> table = toml::parse(text);
    REQUIRE(table.has_value());
    return *table;
}

[[nodiscard]] auto profilesOf(const toml::TomlTable& root, const std::string& path) {
    return parseProfiles(root, path, [](ProfileHeader header, const toml::TomlTable& table) {
        return Profile{.header = std::move(header), .text = std::string{table.string("text").value_or("")}};
    });
}

} // namespace

TEST_CASE("the four keys every profile shares") {
    const toml::TomlTable table = parsed(R"(
priority = 5
default = true
active = true
when = ["%basalt_server_online% > 0", "true"]
)");
    const ProfileHeader header = ProfileHeader::from("busy", table);
    CHECK(header.name == "busy");
    CHECK(header.priority == 5);
    CHECK(header.isDefault);
    CHECK(header.isActive);
    REQUIRE(header.conditions.size() == 2);
    CHECK(header.conditions[0].raw() == "%basalt_server_online% > 0");

    // A profile that says none of them is an ordinary one nobody has switched on.
    const ProfileHeader bare = ProfileHeader::from("quiet", parsed("text = \"x\"\n"));
    CHECK(bare.priority == 0);
    CHECK_FALSE(bare.isDefault);
    CHECK_FALSE(bare.isActive);
    CHECK(bare.conditions.empty());
}

TEST_CASE("the highest priority among the ones that apply, and the default when none do") {
    const toml::TomlTable root = parsed(R"(
[profiles.quiet]
default = true
text = "quiet"

[profiles.busy]
priority = 5
when = ["%basalt_server_online% > 10"]
text = "busy"

[profiles.packed]
priority = 10
when = ["%basalt_server_online% > 50"]
text = "packed"
)");
    const auto read = profilesOf(root, "config/server_list.toml");
    REQUIRE(read.has_value());
    const std::vector<Profile>& profiles = read->first;
    const Profile& fallback = profiles[read->second];
    CHECK(fallback.text == "quiet");

    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    const placeholder::PlaceholderContext context;

    // Nobody online: no condition holds, so the default is what is left.
    CHECK(selectProfile(profiles, fallback, serverRegistry(0), context, logger, reporter, "server list").text ==
          "quiet");
    // Enough for one of them.
    CHECK(selectProfile(profiles, fallback, serverRegistry(20), context, logger, reporter, "server list").text ==
          "busy");
    // Enough for both: the higher priority wins, whatever order they were written in.
    CHECK(selectProfile(profiles, fallback, serverRegistry(80), context, logger, reporter, "server list").text ==
          "packed");
}

TEST_CASE("two profiles of the same priority are settled by the order the file declares them") {
    const toml::TomlTable root = parsed(R"(
[profiles.first]
priority = 5
when = ["true"]
text = "first"

[profiles.second]
priority = 5
when = ["true"]
text = "second"

[profiles.quiet]
default = true
text = "quiet"
)");
    const auto read = profilesOf(root, "config/server_list.toml");
    REQUIRE(read.has_value());
    // The default is not the first table in the file, so the fallback is found by what it says
    // rather than by where it is.
    CHECK(read->first[read->second].text == "quiet");

    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(1), placeholder::PlaceholderContext{},
                        logger, reporter, "server list")
              .text == "first");
}

TEST_CASE("a profile switched on by hand needs no condition, and one with none is never chosen") {
    const toml::TomlTable root = parsed(R"(
[profiles.quiet]
default = true
text = "quiet"

[profiles.maintenance]
active = true
priority = 1
text = "maintenance"

[profiles.unused]
priority = 99
text = "unused"
)");
    const auto read = profilesOf(root, "config/server_list.toml");
    REQUIRE(read.has_value());
    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;

    // Switched on by hand beats a higher priority that nothing selects: a profile with no
    // conditions and no switch is one nobody asked for.
    CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(0), placeholder::PlaceholderContext{},
                        logger, reporter, "server list")
              .text == "maintenance");
}

TEST_CASE("every condition of a profile has to hold, not just one of them") {
    const toml::TomlTable root = parsed(R"(
[profiles.quiet]
default = true
text = "quiet"

[profiles.both]
priority = 5
when = ["%basalt_server_online% > 10", "%basalt_server_online% < 20"]
text = "both"
)");
    const auto read = profilesOf(root, "config/scoreboard.toml");
    REQUIRE(read.has_value());
    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    const placeholder::PlaceholderContext context;

    CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(15), context, logger, reporter,
                        "scoreboard")
              .text == "both");
    CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(50), context, logger, reporter,
                        "scoreboard")
              .text == "quiet");
}

TEST_CASE("a condition nobody can decide is reported once and counts as a no") {
    const toml::TomlTable root = parsed(R"(
[profiles.quiet]
default = true
text = "quiet"

[profiles.broken]
priority = 5
when = ["%basalt_server_nowhere% = 1"]
text = "broken"
)");
    const auto read = profilesOf(root, "config/tab_list.toml");
    REQUIRE(read.has_value());
    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    const CapturedLog log;

    // Profiles are re-selected on a timer for every player, so the same complaint would otherwise
    // arrive at the refresh rate.
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(1),
                            placeholder::PlaceholderContext{}, logger, reporter, "tab list")
                  .text == "quiet");
    }
    CHECK(log.countWith("was skipped") == 1);
    // Said in full: which condition, of which profile, and why it could not be decided.
    CHECK(log.countWith("Condition '%basalt_server_nowhere% = 1' of tab list profile 'broken' was skipped: "
                        "%basalt_server_nowhere% cannot be used here: there is no player here.") == 1);
}

TEST_CASE("the same complaint from several threads at once is still said once") {
    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    const CapturedLog log;

    std::vector<std::thread> callers;
    callers.reserve(8);
    for (std::int32_t index = 0; index < 8; ++index) {
        callers.emplace_back([&reporter, &logger] {
            for (std::int32_t attempt = 0; attempt < 50; ++attempt) {
                reporter.report(logger, "the same thing");
                reporter.report(logger, "something else");
            }
        });
    }
    for (std::thread& caller : callers) {
        caller.join();
    }
    CHECK(log.countWith("the same thing") == 1);
    CHECK(log.countWith("something else") == 1);
}

TEST_CASE("a file of profiles that cannot mean what it says") {
    const std::expected<std::pair<std::vector<Profile>, std::size_t>, std::string> none =
        profilesOf(parsed("# nothing\n"), "config/boss_bar.toml");
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error() == "config/boss_bar.toml declares no [profiles] table.");

    CHECK(profilesOf(parsed("[profiles]\n"), "config/boss_bar.toml").error() ==
          "config/boss_bar.toml declares no profiles.");

    CHECK(profilesOf(parsed("[profiles.a]\n[profiles.b]\n"), "config/boss_bar.toml").error() ==
          "config/boss_bar.toml must mark exactly one profile as `default`, found 0.");

    CHECK(profilesOf(parsed("[profiles.a]\ndefault = true\n[profiles.b]\ndefault = true\n"), "config/boss_bar.toml")
              .error() == "config/boss_bar.toml must mark exactly one profile as `default`, found 2.");
}

TEST_CASE("a block of text is a list of lines or one string with newlines in it") {
    const toml::TomlTable table = parsed(R"(
listed = ["one", "two"]
single = "one\ntwo"
plain = "just one"
number = 5
)");
    CHECK(linesOf(table, "listed") == std::vector<std::string>{"one", "two"});
    CHECK(linesOf(table, "single") == std::vector<std::string>{"one", "two"});
    CHECK(linesOf(table, "plain") == std::vector<std::string>{"just one"});
    // Absent is not empty: a profile that says nothing about a block inherits one, and that is
    // the difference this answer carries.
    CHECK_FALSE(linesOf(table, "missing").has_value());
    // Not text at all is not a block of text.
    CHECK_FALSE(linesOf(table, "number").has_value());
}
