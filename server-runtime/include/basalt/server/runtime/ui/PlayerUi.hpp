#pragma once

#include <basalt/server/runtime/config/ProfileSelection.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::ui {

/// What a plugin wants drawn on a surface it has taken over: a title and its rows.
struct SurfaceContent {
    std::string title;
    std::vector<std::string> lines;
};

/// The tab list, scoreboard and boss bar of one player.
///
/// Lives on that player's own connection thread, driven from the same loop that already ticks
/// hunger, because everything here writes to a socket and the shared tick thread must never do
/// that: one client that stops reading would otherwise park saving, chunk unloading and every other
/// player.
///
/// Every piece is redrawn on its own interval and **sent only when the text it renders to changes**.
/// That is not a micro-optimisation: at twenty refreshes a second a scoreboard is a dozen packets
/// per player per tick, and almost every one of them would say exactly what the last one said.
class PlayerUi {
public:
    using Send = std::function<void(std::int32_t packetId, const std::function<void(wire::io::ByteBuffer&)>& write)>;

    /// What a plugin may take over. Kept as callbacks: asking whether one holds a surface is cheap,
    /// and only a due redraw runs the plugin's own code.
    struct Plugin {
        std::function<bool()> holdsSidebar;
        std::function<std::optional<SurfaceContent>()> sidebar;
        std::function<bool()> holdsBossBar;
        std::function<std::optional<SurfaceContent>()> bossBar;
    };

    PlayerUi(std::function<config::UiConfigs()> configs,
             const wire::common::placeholder::PlaceholderRegistry& placeholders, Send send, Plugin plugin = {},
             wire::common::Logger logger = wire::common::Logger{"Ui"});

    /// Called every tick from the connection's own loop. Only decides which pieces are due.
    ///
    /// Takes a context rather than a player because nothing in here needs to know what a player is:
    /// it renders whatever the placeholders resolve to, which is also what makes it testable
    /// without a socket and a login behind it.
    void tick(std::int64_t tick, const wire::common::placeholder::PlaceholderContext& context);

    /// Takes down anything this player was shown. Called when they leave or change worlds.
    void clear();

private:
    void updateTabList(const wire::common::placeholder::PlaceholderContext& context);

    void updateScoreboard(const wire::common::placeholder::PlaceholderContext& context);

    void drawScoreboard(const std::string& title, const std::vector<std::string>& lines);

    /// Takes the sidebar down and leaves the rest alone.
    void clearScoreboard();

    void updateBossBar(const wire::common::placeholder::PlaceholderContext& context);

    void drawBossBar(const std::string& text, float progress, wire::protocol::play::BossBarColor color,
                     wire::protocol::play::BossBarOverlay overlay);

    void clearBossBar();

    /// A number between 0 and 1, however the operator wrote it: a literal, or anything resolving
    /// to one.
    [[nodiscard]] float progressOf(const std::optional<std::string>& raw,
                                   const wire::common::placeholder::PlaceholderContext& context);

    /// The substituted text, for comparing against last time, and the parsed component, to send.
    [[nodiscard]] std::pair<std::string, wire::text::Component>
    render(const std::vector<std::string>& lines, const wire::common::placeholder::PlaceholderContext& context);

    [[nodiscard]] std::string substitute(std::string_view text,
                                         const wire::common::placeholder::PlaceholderContext& context);

    std::function<config::UiConfigs()> configs_;
    const wire::common::placeholder::PlaceholderRegistry* placeholders_;
    Send send_;
    Plugin plugin_;
    wire::common::Logger logger_;
    config::OnceReporter reporter_;

    std::optional<std::pair<std::string, std::string>> lastTabList_;
    std::optional<std::string> lastScoreboardTitle_;
    std::vector<std::string> lastScoreboardLines_;
    bool scoreboardShown_ = false;
    std::optional<wire::common::Uuid> bossBarId_;
    std::optional<std::string> lastBossText_;
    std::optional<float> lastBossProgress_;
    std::optional<std::pair<wire::protocol::play::BossBarColor, wire::protocol::play::BossBarOverlay>> lastBossStyle_;
};

} // namespace basalt::server::runtime::ui
