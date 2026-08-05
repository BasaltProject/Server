#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginMap {
    std::int32_t id = 0;
    /// The dimension the map shows, as its level key ("minecraft:overworld").
    std::string dimension;
    std::int32_t scale = 0;
    std::int32_t centerX = 0;
    std::int32_t centerZ = 0;
    bool tracking = true;
    bool unlimited = false;
    bool locked = false;
};

struct PluginMapCursor {
    std::int8_t x = 0;
    std::int8_t y = 0;
    /// 0 to 15, a sixteenth of a turn each.
    std::int8_t direction = 0;
    std::int32_t type = 0;
    /// A JSON text component.
    std::optional<std::string> caption;
};

struct PluginExploration {
    std::vector<std::string> destination;
    std::string decoration = "minecraft:mansion";
    std::int32_t zoom = 2;
    std::int32_t radius = 50;
    bool skipExisting = true;
};

/// Maps as the world keeps them (128 by 128 colour ids and their decorations) and a way to show a player another view.
class PluginMaps {
public:
    PluginMaps() = default;
    PluginMaps(const PluginMaps&) = delete;
    PluginMaps& operator=(const PluginMaps&) = delete;
    PluginMaps(PluginMaps&&) = delete;
    PluginMaps& operator=(PluginMaps&&) = delete;
    virtual ~PluginMaps() = default;

    [[nodiscard]] static PluginMaps& unavailable();

    static constexpr std::size_t kPixels = 128 * 128;

    [[nodiscard]] virtual std::optional<PluginMap> map(std::int32_t id) {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// A new map centred on the point; -1 when the world is unknown.
    virtual std::int32_t create(std::string_view world, std::int32_t centerX, std::int32_t centerZ,
                                std::int32_t scale) {
        static_cast<void>(world);
        static_cast<void>(centerX);
        static_cast<void>(centerZ);
        static_cast<void>(scale);
        return -1;
    }

    virtual bool update(const PluginMap& map) {
        static_cast<void>(map);
        return false;
    }

    /// The colour ids the world keeps for the map, `kPixels` of them.
    [[nodiscard]] virtual std::vector<std::uint8_t> colors(std::int32_t id) {
        static_cast<void>(id);
        return {};
    }

    [[nodiscard]] virtual std::vector<std::string> carriers(std::int32_t id) {
        static_cast<void>(id);
        return {};
    }

    /// The components of a filled map showing the nearest of these structures, as an unnamed NBT compound.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>>
    explorerMap(std::string_view world, double x, double y, double z, const PluginExploration& request) {
        static_cast<void>(world);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(request);
        return std::nullopt;
    }

    /// Shows the player this picture of the map, and these cursors, in place of what the world keeps.
    virtual void show(std::string_view uuid, std::int32_t id, std::span<const std::uint8_t> colors,
                      const std::vector<PluginMapCursor>& cursors) {
        static_cast<void>(uuid);
        static_cast<void>(id);
        static_cast<void>(colors);
        static_cast<void>(cursors);
    }
};

inline PluginMaps& PluginMaps::unavailable() {
    static PluginMaps none;
    return none;
}

} // namespace basalt::server::plugins
