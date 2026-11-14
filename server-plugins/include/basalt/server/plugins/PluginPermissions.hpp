#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::server::plugins::permissions {

inline constexpr std::int32_t kUndefined = -1;
inline constexpr std::int32_t kFalse = 0;
inline constexpr std::int32_t kTrue = 1;

inline constexpr std::int32_t kDefaultFalse = 0;
inline constexpr std::int32_t kDefaultTrue = 1;
inline constexpr std::int32_t kDefaultOp = 2;
inline constexpr std::int32_t kDefaultNotOp = 3;

inline constexpr std::int32_t kPriorityPlugin = 100;
inline constexpr std::int32_t kPriorityAdapter = 200;

/// An empty uuid is the console. Answers kUndefined, kFalse or kTrue.
using Check = std::function<std::int32_t(std::string_view uuid, std::string_view node)>;
using Meta = std::function<std::optional<std::string>(std::string_view uuid, std::string_view key)>;

/// The server's permission chain without Wire types, for Lua and for the Java adapters. Callable from any thread.
class PluginPermissions {
public:
    PluginPermissions() = default;
    PluginPermissions(const PluginPermissions&) = delete;
    PluginPermissions& operator=(const PluginPermissions&) = delete;
    PluginPermissions(PluginPermissions&&) = delete;
    PluginPermissions& operator=(PluginPermissions&&) = delete;
    virtual ~PluginPermissions() = default;

    /// Asked before the server's groups and grants; `meta` may be empty. The handle takes it out again.
    [[nodiscard]] virtual PluginHandle provide(std::string_view id, std::int32_t priority, Check check, Meta meta) = 0;

    /// A plugin.yml permission: its default for players nothing else names it for, and what it sets on its children.
    virtual void declare(std::string_view node, std::int32_t defaultKind, std::span<const std::string_view> childNodes,
                         std::span<const bool> childValues) = 0;

    virtual void undeclare(std::string_view node) = 0;

    /// The whole chain; kUndefined only when the player is not online.
    [[nodiscard]] virtual std::int32_t check(std::string_view uuid, std::string_view node) = 0;

    [[nodiscard]] virtual std::optional<std::string> meta(std::string_view uuid, std::string_view key) = 0;

    /// A provider's answers changed: command trees are sent again.
    virtual void changed() = 0;

    [[nodiscard]] static PluginPermissions& unavailable();
};

} // namespace basalt::server::plugins::permissions
