#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

/// One permission rule: a dotted name, possibly ending in `*`, granting or denying.
///
/// Specificity is the number of segments before any wildcard, so `basalt.command.stop` (3) beats
/// `basalt.command.*` (2) beats `basalt.*` (1). That ordering is what lets a group be written as
/// "everything except this one thing" without depending on the order of the list.
class PermissionRule {
public:
    PermissionRule(std::string pattern, bool granted);

    /// A leading `-` denies; everything else grants.
    [[nodiscard]] static PermissionRule parse(std::string_view raw);

    [[nodiscard]] const std::string& pattern() const noexcept { return pattern_; }

    [[nodiscard]] bool granted() const noexcept { return granted_; }

    [[nodiscard]] std::int32_t specificity() const noexcept { return specificity_; }

    [[nodiscard]] bool matches(std::string_view node) const;

    friend bool operator==(const PermissionRule&, const PermissionRule&) = default;

private:
    std::string pattern_;
    bool granted_ = true;
    std::string prefix_;
    bool isWildcard_ = false;
    std::int32_t specificity_ = 0;
};

/// A named set of permissions.
struct PermissionGroup {
    std::string name;
    std::vector<PermissionRule> rules;
    std::optional<std::string> inherits;
    bool isDefault = false;
    bool isOperator = false;
};

/// `config/groups.toml`, resolved: every group with its inheritance already flattened.
///
/// Membership is deliberately not here. A group definition is something the operator writes; which
/// group a player is in is something the server writes, so it belongs in `players.db`. Keeping the
/// two apart is what removed the old `[server] operators` list, which the server had to rewrite by
/// hand every time somebody ran `op`.
class GroupsConfig {
public:
    static constexpr std::string_view kFileName = "groups.toml";

    GroupsConfig(std::vector<PermissionGroup> groups, std::size_t defaultIndex, std::size_t operatorIndex)
        : groups_(std::move(groups)), defaultIndex_(defaultIndex), operatorIndex_(operatorIndex) {}

    [[nodiscard]] static std::expected<GroupsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    /// The names, in the order the file declares them.
    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] const PermissionGroup& defaultGroup() const noexcept { return groups_[defaultIndex_]; }

    [[nodiscard]] const PermissionGroup& operatorGroup() const noexcept { return groups_[operatorIndex_]; }

    /// The named group, or the default one when there is no such group and when there is no name.
    [[nodiscard]] const PermissionGroup& get(std::optional<std::string_view> name) const;

    /// Whether `groupName` grants `node`.
    ///
    /// Rules from the group itself win over inherited ones at equal specificity, so a group can
    /// override what it inherits without having to be more specific than its parent. Nothing
    /// matching at all is a denial: what a group can do is exactly what its file says, and a
    /// permission nobody granted has not been granted.
    [[nodiscard]] bool resolve(std::optional<std::string_view> groupName, std::string_view node) const;

private:
    [[nodiscard]] const PermissionGroup* find(std::string_view name) const;

    std::vector<PermissionGroup> groups_;
    std::size_t defaultIndex_ = 0;
    std::size_t operatorIndex_ = 0;
};

} // namespace basalt::server::runtime::config
