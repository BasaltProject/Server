#include <basalt/server/plugins/PluginSurfaces.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace basalt::server::plugins {
namespace {

constexpr std::array<PluginSurface, 5> kSurfaces{PluginSurface::Sidebar, PluginSurface::TabList, PluginSurface::BossBar,
                                                 PluginSurface::ChatFormat, PluginSurface::NameTag};

} // namespace

std::string_view surfaceName(PluginSurface surface) {
    switch (surface) {
    case PluginSurface::Sidebar:
        return "sidebar";
    case PluginSurface::TabList:
        return "tab_list";
    case PluginSurface::BossBar:
        return "boss_bar";
    case PluginSurface::ChatFormat:
        return "chat_format";
    case PluginSurface::NameTag:
        return "name_tag";
    }
    return "sidebar";
}

std::optional<PluginSurface> surfaceOf(std::string_view name) {
    for (const PluginSurface surface : kSurfaces) {
        if (surfaceName(surface) == name) {
            return surface;
        }
    }
    return std::nullopt;
}

std::span<const PluginSurface> allSurfaces() { return kSurfaces; }

PluginHandle PluginSurfaceRegistry::claim(PluginSurfaceClaim claim) {
    std::vector<const PluginSurfaceClaim*> rivals;
    for (const std::unique_ptr<PluginSurfaceClaim>& existing : claims_) {
        if (existing->surface == claim.surface && existing->pluginId != claim.pluginId) {
            rivals.push_back(existing.get());
        }
    }
    claims_.push_back(std::make_unique<PluginSurfaceClaim>(std::move(claim)));
    const PluginSurfaceClaim* added = claims_.back().get();
    for (const PluginSurfaceClaim* rival : rivals) {
        if (!warn_) {
            break;
        }
        const std::string surface{surfaceName(added->surface)};
        if (rival->priority == added->priority) {
            warn_(added->pluginId + " and " + rival->pluginId + " both claim the " + surface + " at priority " +
                  std::to_string(added->priority) + "; " + rival->pluginId +
                  " keeps it because it asked first. A tie means one of them was missing a priority.");
            continue;
        }
        const PluginSurfaceClaim* winner = added->priority > rival->priority ? added : rival;
        const PluginSurfaceClaim* loser = winner == added ? rival : added;
        warn_(winner->pluginId + " takes the " + surface + " from " + loser->pluginId + " (" +
              std::to_string(winner->priority) + " over " + std::to_string(loser->priority) + "); " + loser->pluginId +
              " gets it back if " + winner->pluginId + " lets go.");
    }
    return PluginHandle{[this, added] {
        std::erase_if(claims_,
                      [added](const std::unique_ptr<PluginSurfaceClaim>& held) { return held.get() == added; });
    }};
}

std::vector<const PluginSurfaceClaim*> PluginSurfaceRegistry::all() const {
    std::vector<const PluginSurfaceClaim*> sorted;
    sorted.reserve(claims_.size());
    for (const std::unique_ptr<PluginSurfaceClaim>& claim : claims_) {
        sorted.push_back(claim.get());
    }
    // Stable so declaration order breaks a tie, which is the rule the config files already use.
    std::ranges::stable_sort(sorted, [](const PluginSurfaceClaim* left, const PluginSurfaceClaim* right) {
        return left->priority > right->priority;
    });
    return sorted;
}

std::vector<const PluginSurfaceClaim*> PluginSurfaceRegistry::claimsOn(PluginSurface surface) const {
    std::vector<const PluginSurfaceClaim*> found;
    for (const std::unique_ptr<PluginSurfaceClaim>& claim : claims_) {
        if (claim->surface == surface) {
            found.push_back(claim.get());
        }
    }
    return found;
}

bool PluginSurfaceRegistry::anyClaimOn(PluginSurface surface) const {
    return std::ranges::any_of(claims_, [surface](const std::unique_ptr<PluginSurfaceClaim>& claim) {
        return claim->surface == surface && !claim->yielded;
    });
}

const PluginSurfaceClaim* PluginSurfaceRegistry::holder(PluginSurface surface, PluginPlayerRef& player) const {
    const PluginSurfaceClaim* best = nullptr;
    for (const std::unique_ptr<PluginSurfaceClaim>& claim : claims_) {
        if (claim->surface != surface || !claim->wants(player)) {
            continue;
        }
        // Strictly greater keeps the first of equals, which is declaration order.
        if (best == nullptr || claim->priority > best->priority) {
            best = claim.get();
        }
    }
    return best;
}

std::optional<std::pair<const PluginSurfaceClaim*, PluginSurfaceContent>>
PluginSurfaceRegistry::content(PluginSurface surface, PluginPlayerRef& player) const {
    std::vector<const PluginSurfaceClaim*> candidates;
    for (const std::unique_ptr<PluginSurfaceClaim>& claim : claims_) {
        if (claim->surface == surface && claim->wants(player)) {
            candidates.push_back(claim.get());
        }
    }
    std::ranges::stable_sort(candidates, [](const PluginSurfaceClaim* left, const PluginSurfaceClaim* right) {
        return left->priority > right->priority;
    });
    for (const PluginSurfaceClaim* claim : candidates) {
        if (!claim->render) {
            continue;
        }
        const std::optional<PluginSurfaceContent> drawn = claim->render(player);
        if (drawn.has_value()) {
            return std::pair{claim, *drawn};
        }
    }
    return std::nullopt;
}

void PluginSurfaceRegistry::yieldOf(std::string_view pluginId, std::string_view why) {
    bool any = false;
    for (const std::unique_ptr<PluginSurfaceClaim>& claim : claims_) {
        if (claim->pluginId == pluginId && !claim->yielded) {
            claim->yielded = true;
            any = true;
        }
    }
    if (any && warn_) {
        warn_(std::string{pluginId} + " let go of what it was drawing: " + std::string{why} +
              ". Whoever was below it takes over.");
    }
}

} // namespace basalt::server::plugins
