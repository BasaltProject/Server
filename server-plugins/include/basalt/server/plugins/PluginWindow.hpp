#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {
struct PluginWindowSpec {
    std::string kind = "chest";
    std::int32_t size = 27;
    wire::nbt::NbtTag title = wire::nbt::NbtTag::ofString("");
    std::vector<std::optional<PluginItem>> items;
    bool editable = true;
};

struct PluginWindowInfo {
    std::int32_t containerId = 0;
    std::string kind;
    std::int32_t slots = 0;
    bool plugin = false;
    std::optional<std::array<std::int32_t, 3>> block;
    std::string world;
};

/// The container window one player has open, addressed by the protocol's slot numbers, and the item on their cursor.
class PluginWindow {
public:
    PluginWindow() = default;
    PluginWindow(const PluginWindow&) = delete;
    PluginWindow& operator=(const PluginWindow&) = delete;
    PluginWindow(PluginWindow&&) = delete;
    PluginWindow& operator=(PluginWindow&&) = delete;
    virtual ~PluginWindow() = default;

    /// Opens a window of plugin-held contents and returns its container id, or zero when the kind is unknown.
    virtual std::int32_t openWindow(const PluginWindowSpec& spec) = 0;

    /// Opens the screen the container block at the position has, as if the player used it; the container id, or zero.
    virtual std::int32_t openBlockWindow(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0;
    }

    /// Opens the chest of a chest minecart or chest boat; the container id, or zero.
    virtual std::int32_t openEntityWindow(std::int32_t entityId) {
        static_cast<void>(entityId);
        return 0;
    }

    virtual void closeWindow() = 0;

    [[nodiscard]] virtual PluginWindowInfo windowInfo() const = 0;

    [[nodiscard]] virtual std::optional<PluginItem> windowItem(std::int32_t slot) const = 0;

    virtual void setWindowItem(std::int32_t slot, const std::optional<PluginItem>& item) = 0;

    [[nodiscard]] virtual std::optional<PluginItem> cursorItem() const = 0;

    virtual void setCursorItem(const std::optional<PluginItem>& item) = 0;

    virtual void resyncWindow() = 0;

    /// Sends the open window one of its data values (furnace progress, enchantment costs, anvil cost...).
    virtual bool setWindowProperty(std::int32_t id, std::int32_t value) {
        static_cast<void>(id);
        static_cast<void>(value);
        return false;
    }
};
} // namespace basalt::server::plugins
