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

    CHECK(selectProfile(profiles, fallback, serverRegistry(0), context, logger, reporter, "server list").text ==
          "quiet");
    CHECK(selectProfile(profiles, fallback, serverRegistry(20), context, logger, reporter, "server list").text ==
          "busy");
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
    CHECK(read->first[read->second].text == "quiet");

    const basalt::wire::common::Logger logger{"test"};
    OnceReporter reporter;
    CHECK(selectProfile(read->first, read->first[read->second], serverRegistry(1), placeholder::PlaceholderContext{},
                        logger, reporter, "server list")
              .text == "first");
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
