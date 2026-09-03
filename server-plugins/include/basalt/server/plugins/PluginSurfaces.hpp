#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginWorldAccess.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::plugins {

/// The things only one plugin can be drawing at a time.
///
/// The sidebar, the tab list header and footer, a boss bar, the MOTD, the chat format, the name over
/// a head. Elsewhere the last one to write wins, there is no way to find out who that was, and the
/// result is two plugins fighting over the scoreboard.
enum class PluginSurface : std::uint8_t { Sidebar, TabList, BossBar, ChatFormat, NameTag };

[[nodiscard]] std::string_view surfaceName(PluginSurface surface);

[[nodiscard]] std::optional<PluginSurface> surfaceOf(std::string_view name);

/// Every surface, in declaration order.
[[nodiscard]] std::span<const PluginSurface> allSurfaces();

/// A title and some lines, which is what every one of these surfaces comes down to.
struct PluginSurfaceContent {
    std::string title;
    std::vector<std::string> lines;

    friend bool operator==(const PluginSurfaceContent&, const PluginSurfaceContent&) = default;
};

/// One plugin's claim on one surface.
///
/// `priority` is asked for and never defaulted: a hidden default is how two plugins end up tied at
/// zero and the winner becomes whoever the file system listed first.
struct PluginSurfaceClaim {
    std::string pluginId;
    PluginSurface surface = PluginSurface::Sidebar;
    std::int32_t priority = 0;
    /// Whether it wants the surface for this player at all. Unset means always.
    std::function<bool(PluginPlayerRef&)> appliesTo;
    /// What to draw. Nothing means the claim is up but has nothing to say, which counts as not
    /// applying.
    std::function<std::optional<PluginSurfaceContent>(PluginPlayerRef&)> render;
    /// Set when the owner dies or oversteps: it keeps its claim but stops being asked.
    bool yielded = false;

    [[nodiscard]] bool wants(PluginPlayerRef& player) const { return !yielded && (!appliesTo || appliesTo(player)); }
};

/// Who is drawing what, and who is next in line.
///
/// The rule the config files already use: highest priority among the claims that apply, declaration
/// order breaking a tie. Losing is said out loud rather than silently, the loser keeps its claim and
/// takes the surface back when the one above lets go, and a holder that falls over lets go.
class PluginSurfaceRegistry {
public:
    using Warn = std::function<void(const std::string& message)>;

    explicit PluginSurfaceRegistry(Warn warn = {}) : warn_(std::move(warn)) {}

    PluginHandle claim(PluginSurfaceClaim claim);

    /// Everything registered, holder first, for `/plugins surfaces`.
    [[nodiscard]] std::vector<const PluginSurfaceClaim*> all() const;

    [[nodiscard]] std::vector<const PluginSurfaceClaim*> claimsOn(PluginSurface surface) const;

    /// Whether anybody at all is claiming `surface`.
    ///
    /// The cheap question, asked every tick from a connection thread: it looks at nothing but the
    /// list, and in particular never runs a plugin's own predicate, which is what `holder` does.
    [[nodiscard]] bool anyClaimOn(PluginSurface surface) const;

    /// Who draws `surface` for `player` right now, or nothing if nobody wants it.
    [[nodiscard]] const PluginSurfaceClaim* holder(PluginSurface surface, PluginPlayerRef& player) const;

    /// What to draw, skipping past a claim that applies but has nothing to say.
    [[nodiscard]] std::optional<std::pair<const PluginSurfaceClaim*, PluginSurfaceContent>>
    content(PluginSurface surface, PluginPlayerRef& player) const;

    /// The holder fell over: it keeps its claim and stops being drawn until it is loaded again.
    void yieldOf(std::string_view pluginId, std::string_view why);

private:
    /// Held by pointer so a claim's address survives the list growing, which the handles rely on.
    std::vector<std::unique_ptr<PluginSurfaceClaim>> claims_;
    Warn warn_;
};

} // namespace basalt::server::plugins
