#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginServerLink {
    /// The vanilla kind of link ("bug_report", "community_guidelines", ...), or empty with a label of its own.
    std::string kind;
    /// A JSON text component; only a link without a kind has one.
    std::string label;
    std::string url;
};

struct PluginResourcePack {
    std::string id;
    std::string url;
    std::string hash;
    bool required = false;
    /// A JSON text component, or empty.
    std::string prompt;
};

/// The pace of the server, the links its clients list in their pause menu, and the pack it asks them to use.
class PluginServerControl {
public:
    PluginServerControl() = default;
    PluginServerControl(const PluginServerControl&) = delete;
    PluginServerControl& operator=(const PluginServerControl&) = delete;
    PluginServerControl(PluginServerControl&&) = delete;
    PluginServerControl& operator=(PluginServerControl&&) = delete;
    virtual ~PluginServerControl() = default;

    [[nodiscard]] static PluginServerControl& unavailable();

    [[nodiscard]] virtual float tickRate() { return 20.0F; }

    /// Ticks per second over the last minute, five minutes and fifteen minutes, then the mean tick in milliseconds.
    [[nodiscard]] virtual std::array<double, 4> pace() { return {20.0, 20.0, 20.0, 0.0}; }

    virtual void setTickRate(float rate) { static_cast<void>(rate); }

    [[nodiscard]] virtual bool frozen() { return false; }

    virtual void setFrozen(bool value) { static_cast<void>(value); }

    /// Runs this many ticks while frozen; false when the server is not frozen.
    virtual bool step(std::int32_t ticks) {
        static_cast<void>(ticks);
        return false;
    }

    [[nodiscard]] virtual std::int32_t stepsOwed() { return 0; }

    virtual bool stopStepping() { return false; }

    virtual bool sprint(std::int32_t ticks) {
        static_cast<void>(ticks);
        return false;
    }

    [[nodiscard]] virtual bool sprinting() { return false; }

    virtual bool stopSprinting() { return false; }

    [[nodiscard]] virtual std::vector<PluginServerLink> links() { return {}; }

    virtual void setLinks(std::vector<PluginServerLink> links) { static_cast<void>(links); }

    [[nodiscard]] virtual std::optional<PluginResourcePack> resourcePack() { return std::nullopt; }
};

inline PluginServerControl& PluginServerControl::unavailable() {
    static PluginServerControl none;
    return none;
}

} // namespace basalt::server::plugins
