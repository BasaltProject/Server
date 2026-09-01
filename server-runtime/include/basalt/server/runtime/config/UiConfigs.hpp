#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/runtime/config/ProfileSelection.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

/// One look for the player list. Anything left out is taken from the default profile.
struct TabListProfile {
    ProfileHeader header;
    std::optional<std::vector<std::string>> headerLines;
    std::optional<std::vector<std::string>> footerLines;

    [[nodiscard]] TabListProfile inheriting(const TabListProfile& fallback) const;
};

/// `config/tab_list.toml`.
struct TabListConfig {
    static constexpr std::string_view kFileName = "tab_list.toml";

    bool enabled = false;
    std::int32_t updateIntervalTicks = 0;
    std::vector<TabListProfile> profiles;
    std::size_t defaultIndex = 0;

    /// What a server that never read the file has: nothing drawn and nothing to draw.
    [[nodiscard]] static TabListConfig disabled();

    [[nodiscard]] static std::expected<TabListConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const TabListProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

/// One board: a title and its rows. Anything left out is taken from the default profile.
struct ScoreboardProfile {
    ProfileHeader header;
    std::optional<std::string> title;
    std::optional<std::vector<std::string>> lines;

    [[nodiscard]] ScoreboardProfile inheriting(const ScoreboardProfile& fallback) const;
};

/// `config/scoreboard.toml`.
struct ScoreboardConfig {
    static constexpr std::string_view kFileName = "scoreboard.toml";

    /// The client only draws this many rows; more would be written and never seen.
    static constexpr std::int32_t kMaxLines = 15;

    bool enabled = false;
    std::int32_t updateIntervalTicks = 0;
    std::vector<ScoreboardProfile> profiles;
    std::size_t defaultIndex = 0;

    [[nodiscard]] static ScoreboardConfig disabled();

    [[nodiscard]] static std::expected<ScoreboardConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const ScoreboardProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

/// One bar. Anything left out is taken from the default profile.
struct BossBarProfile {
    ProfileHeader header;
    std::optional<std::string> text;
    std::optional<wire::protocol::play::BossBarColor> color;
    std::optional<wire::protocol::play::BossBarOverlay> overlay;
    /// Kept as written so it can be a placeholder; resolved and clamped when the bar is drawn.
    std::optional<std::string> progress;

    [[nodiscard]] BossBarProfile inheriting(const BossBarProfile& fallback) const;
};

/// `config/boss_bar.toml`.
struct BossBarConfig {
    static constexpr std::string_view kFileName = "boss_bar.toml";

    bool enabled = false;
    std::int32_t updateIntervalTicks = 0;
    std::vector<BossBarProfile> profiles;
    std::size_t defaultIndex = 0;

    [[nodiscard]] static BossBarConfig disabled();

    [[nodiscard]] static std::expected<BossBarConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const BossBarProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

/// What `/bossbar set <id> color` takes, which is the enum's own name in lower case.
[[nodiscard]] std::string_view bossBarColorName(wire::protocol::play::BossBarColor color) noexcept;

/// The three UI files as one thing, because they are read, reloaded and handed around together.
struct UiConfigs {
    TabListConfig tabList = TabListConfig::disabled();
    ScoreboardConfig scoreboard = ScoreboardConfig::disabled();
    BossBarConfig bossBar = BossBarConfig::disabled();

    [[nodiscard]] bool anyEnabled() const noexcept { return tabList.enabled || scoreboard.enabled || bossBar.enabled; }
};

} // namespace basalt::server::runtime::config
