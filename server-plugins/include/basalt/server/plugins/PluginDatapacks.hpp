#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginDatapack {
    std::string id;
    /// JSON text components.
    std::string title;
    std::string description;
    bool required = false;
    bool enabled = false;
    /// "compatible", "too_old", "too_new" or "unknown".
    std::string compatibility = "compatible";
    /// "built_in", "feature" or "world".
    std::string source = "world";
    std::vector<std::string> features;
};

struct PluginScheduledFunction {
    /// The function id, or "#id" for a tag.
    std::string name;
    std::int64_t triggerTime = 0;
};

class PluginDatapacks {
public:
    PluginDatapacks() = default;
    PluginDatapacks(const PluginDatapacks&) = delete;
    PluginDatapacks& operator=(const PluginDatapacks&) = delete;
    PluginDatapacks(PluginDatapacks&&) = delete;
    PluginDatapacks& operator=(PluginDatapacks&&) = delete;
    virtual ~PluginDatapacks() = default;

    [[nodiscard]] static PluginDatapacks& unavailable();

    /// Looks for packs added to the world's folder since the last look.
    virtual void refresh() {}

    /// Finds new packs and reloads the data of the server, as `/reload` does; false when the reload failed.
    virtual bool reload() { return false; }

    [[nodiscard]] virtual std::vector<PluginDatapack> packs() { return {}; }

    [[nodiscard]] virtual std::vector<std::string> functionIds() { return {}; }

    [[nodiscard]] virtual std::vector<std::string> functionTagIds() { return {}; }

    /// The queue behind `/schedule`, in the order it will run.
    [[nodiscard]] virtual std::vector<PluginScheduledFunction> scheduled() { return {}; }

    /// Queues a function (or "#tag") `delay` ticks from now; false when the name is unknown or the delay is 0.
    virtual bool schedule(std::string_view function, std::int64_t delay, bool replace) {
        static_cast<void>(function);
        static_cast<void>(delay);
        static_cast<void>(replace);
        return false;
    }

    /// Drops every queued run of `name`, answering how many went.
    virtual std::int32_t unschedule(std::string_view name) {
        static_cast<void>(name);
        return 0;
    }

    /// The file `name` ("data/minecraft/recipe/stick.json") as the enabled packs show it, top pack first.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> resource(std::string_view name) {
        static_cast<void>(name);
        return std::nullopt;
    }

    /// Every visible file whose name starts with `prefix`.
    [[nodiscard]] virtual std::vector<std::string> resourcesUnder(std::string_view prefix) {
        static_cast<void>(prefix);
        return {};
    }

    /// The id of the pack a visible file comes from; empty when there is none.
    [[nodiscard]] virtual std::string resourcePack(std::string_view name) {
        static_cast<void>(name);
        return {};
    }
};

inline PluginDatapacks& PluginDatapacks::unavailable() {
    static PluginDatapacks none;
    return none;
}

} // namespace basalt::server::plugins
