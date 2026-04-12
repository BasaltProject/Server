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

/// The tab list, scoreboard and boss bar of one player. Runs on the player's connection thread: it
/// writes to a socket, and a client that stops reading must never stall the shared tick thread.
/// Each piece redraws on its own interval and is **sent only when its rendered text changes**;
/// otherwise a scoreboard is a dozen redundant packets per player per tick.
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

    /// Where the drawing goes when the client does not speak Java's packets. Every one of these is
    /// optional: an edition that leaves one unset gets the Java packet for it, which is what makes
    /// this the same renderer for both doors rather than two.
    struct Surfaces {
        std::function<void(const std::string& title, const std::vector<std::string>& lines)> scoreboard;
        std::function<void()> clearScoreboard;
        std::function<void(const std::string& title, float progress)> bossBar;
        std::function<void()> clearBossBar;
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

    /// Takes everything down and draws it again on the next tick, for the world the player is now in.
    void worldChanged();

    /// Where the drawing goes. Set once, before the first tick.
    void drawInto(Surfaces surfaces) { surfaces_ = std::move(surfaces); }

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
    Surfaces surfaces_;
    Plugin plugin_;
    wire::common::Logger logger_;
    config::OnceReporter reporter_;

    std::optional<std::pair<std::string, std::string>> lastTabList_;
    bool redrawPending_ = false;
    std::optional<std::string> lastScoreboardTitle_;
    std::vector<std::string> lastScoreboardLines_;
    bool scoreboardShown_ = false;
    std::optional<wire::common::Uuid> bossBarId_;
    std::optional<std::string> lastBossText_;
    std::optional<float> lastBossProgress_;
    std::optional<std::pair<wire::protocol::play::BossBarColor, wire::protocol::play::BossBarOverlay>> lastBossStyle_;
};

} // namespace basalt::server::runtime::ui
