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
enum class PluginSurface : std::uint8_t { Sidebar, TabList, BossBar, ChatFormat, NameTag };

[[nodiscard]] std::string_view surfaceName(PluginSurface surface);

[[nodiscard]] std::optional<PluginSurface> surfaceOf(std::string_view name);

[[nodiscard]] std::span<const PluginSurface> allSurfaces();

struct PluginSurfaceContent {
    std::string title;
    std::vector<std::string> lines;

    friend bool operator==(const PluginSurfaceContent&, const PluginSurfaceContent&) = default;
};

struct PluginSurfaceClaim {
    std::string pluginId;
    PluginSurface surface = PluginSurface::Sidebar;
    std::int32_t priority = 0;
    std::function<bool(PluginPlayerRef&)> appliesTo;
    std::function<std::optional<PluginSurfaceContent>(PluginPlayerRef&)> render;
    bool yielded = false;

    [[nodiscard]] bool wants(PluginPlayerRef& player) const { return !yielded && (!appliesTo || appliesTo(player)); }
};

class PluginSurfaceRegistry {
public:
    using Warn = std::function<void(const std::string& message)>;

    explicit PluginSurfaceRegistry(Warn warn = {}) : warn_(std::move(warn)) {}

    PluginHandle claim(PluginSurfaceClaim claim);

    [[nodiscard]] std::vector<const PluginSurfaceClaim*> all() const;

    [[nodiscard]] std::vector<const PluginSurfaceClaim*> claimsOn(PluginSurface surface) const;

    [[nodiscard]] bool anyClaimOn(PluginSurface surface) const;

    [[nodiscard]] const PluginSurfaceClaim* holder(PluginSurface surface, PluginPlayerRef& player) const;

    [[nodiscard]] std::optional<std::pair<const PluginSurfaceClaim*, PluginSurfaceContent>>
    content(PluginSurface surface, PluginPlayerRef& player) const;

    void yieldOf(std::string_view pluginId, std::string_view why);

private:
    std::vector<std::unique_ptr<PluginSurfaceClaim>> claims_;
    Warn warn_;
};
} // namespace basalt::server::plugins
