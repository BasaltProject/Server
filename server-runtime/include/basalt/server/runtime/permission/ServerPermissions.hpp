#pragma once

#include <basalt/server/plugins/PluginPermissions.hpp>
#include <basalt/wire/permissions/PermissionChain.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::config {
class GroupsConfig;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::permission {

/// The server's permission chain: plugin grants over groups.toml by default, with providers and plugin.yml
/// declarations on top.
class ServerPermissions final : public server::plugins::permissions::PluginPermissions {
public:
    struct Options {
        std::function<const config::GroupsConfig&()> groups;
        std::function<const player::PlayerSession*(std::string_view uuid)> online;
        /// Runs after the answers may have changed, to send command trees again.
        std::function<void()> changed;
    };

    explicit ServerPermissions(Options options);
    ServerPermissions(const ServerPermissions&) = delete;
    ServerPermissions& operator=(const ServerPermissions&) = delete;
    ~ServerPermissions() override;

    [[nodiscard]] wire::permissions::PermissionChain& chain() noexcept { return chain_; }

    [[nodiscard]] bool has(const player::PlayerSession& player, const config::GroupsConfig& groups,
                           std::string_view node) const;

    [[nodiscard]] wire::permissions::Tristate check(const player::PlayerSession& player,
                                                    const config::GroupsConfig& groups, std::string_view node) const;

    /// What the server answers on its own when nothing is provided or declared.
    [[nodiscard]] static bool byDefault(const player::PlayerSession& player, const config::GroupsConfig& groups,
                                        std::string_view node);

    server::plugins::PluginHandle provide(std::string_view id, std::int32_t priority,
                                          server::plugins::permissions::Check check,
                                          server::plugins::permissions::Meta meta) override;

    void declare(std::string_view node, std::int32_t defaultKind, std::span<const std::string_view> childNodes,
                 std::span<const bool> childValues) override;

    void undeclare(std::string_view node) override;

    std::int32_t check(std::string_view uuid, std::string_view node) override;

    std::optional<std::string> meta(std::string_view uuid, std::string_view key) override;

    void changed() override;

private:
    struct Declared {
        std::int32_t defaultKind = server::plugins::permissions::kDefaultOp;
        std::vector<std::pair<std::string, bool>> children;
    };

    [[nodiscard]] bool plain() const noexcept;

    [[nodiscard]] bool answer(const player::PlayerSession& player, const config::GroupsConfig& groups,
                              std::string_view node) const;

    [[nodiscard]] std::optional<bool> declared(const player::PlayerSession& player, const config::GroupsConfig& groups,
                                               std::string_view node, std::int32_t depth) const;

    Options options_;
    wire::permissions::PermissionChain chain_;
    wire::permissions::ProviderHandle default_;
    mutable std::shared_mutex declarationsMutex_;
    std::map<std::string, Declared, std::less<>> declarations_;
    std::map<std::string, std::vector<std::pair<std::string, bool>>, std::less<>> parents_;
    std::atomic<bool> anyDeclared_{false};
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

/// The session's chain, or the plugin grants over the group for a session not yet registered.
[[nodiscard]] bool has(const player::PlayerSession& player, const config::GroupsConfig& groups, std::string_view node);

} // namespace basalt::server::runtime::permission
