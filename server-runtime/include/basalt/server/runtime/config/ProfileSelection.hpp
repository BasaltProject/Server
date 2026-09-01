#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/PlaceholderCondition.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::config {

/// What every profile in every UI file has in common: a name, a priority, whether it is switched on
/// by hand, and the conditions under which it applies.
///
/// Extracted because the server list, the scoreboard, the tab list and the boss bar all select a
/// profile the same way, and four copies of "highest priority among those that apply" is four
/// places for the rule to drift apart. The files stay separate because they are separate domains;
/// the rule is one.
struct ProfileHeader {
    std::string name;
    std::int32_t priority = 0;
    bool isDefault = false;
    bool isActive = false;
    std::vector<wire::common::placeholder::PlaceholderCondition> conditions;

    /// The four shared keys, read out of a profile's table.
    [[nodiscard]] static ProfileHeader from(std::string name, const wire::common::toml::TomlTable& table);
};

/// A profile is anything carrying one of those headers. A concept rather than a base class: these
/// are read through a template, and a vtable buys nothing here.
template <typename T>
concept ConditionalProfile = requires(const T& profile) {
    { profile.header } -> std::convertible_to<const ProfileHeader&>;
};

/// Says a thing once, however many times it happens.
///
/// UI profiles are re-selected on a timer for every player, so a condition that cannot be decided
/// would otherwise fill the log at the refresh rate.
class OnceReporter {
public:
    void report(const wire::common::Logger& logger, const std::string& message);

private:
    mutable std::mutex mutex_;
    std::set<std::string, std::less<>> said_;
};

/// Whether a profile's conditions all hold.
///
/// A condition that cannot be decided at all does not count as a no. The profile is skipped and the
/// reason is reported once, because "your condition is nonsense here" and "your condition said no"
/// are different things and only one of them is a mistake.
[[nodiscard]] bool profileApplies(const ProfileHeader& header,
                                  const wire::common::placeholder::PlaceholderRegistry& registry,
                                  const wire::common::placeholder::PlaceholderContext& context,
                                  const wire::common::Logger& logger, OnceReporter& reporter, std::string_view what);

/// The profile a given situation gets: the highest priority among those switched on or whose every
/// condition holds, falling back to the one marked default.
template <ConditionalProfile T>
[[nodiscard]] const T& selectProfile(const std::vector<T>& profiles, const T& fallback,
                                     const wire::common::placeholder::PlaceholderRegistry& registry,
                                     const wire::common::placeholder::PlaceholderContext& context,
                                     const wire::common::Logger& logger, OnceReporter& reporter,
                                     std::string_view what) {
    const T* best = nullptr;
    for (const T& profile : profiles) {
        if (!profile.header.isActive && !profileApplies(profile.header, registry, context, logger, reporter, what)) {
            continue;
        }
        // The highest priority, and the first of them when several share it, which is the order
        // the file declares them in.
        if (best == nullptr || profile.header.priority > best->header.priority) {
            best = &profile;
        }
    }
    return best == nullptr ? fallback : *best;
}

/// Reads the profiles of a UI file, checking the one rule they all share: exactly one default.
///
/// `build` gets the header already parsed, so each file only writes out the keys that are its own.
template <typename Build>
[[nodiscard]] auto parseProfiles(const wire::common::toml::TomlTable& root, const std::string& path, Build&& build)
    -> std::expected<std::pair<std::vector<decltype(build(ProfileHeader{}, root))>, std::size_t>, std::string> {
    using Profile = decltype(build(ProfileHeader{}, root));

    const wire::common::toml::TomlTable* declared = root.table("profiles");
    if (declared == nullptr) {
        return std::unexpected(path + " declares no [profiles] table.");
    }
    std::vector<Profile> profiles;
    for (const std::string& name : declared->keys()) {
        const wire::common::toml::TomlTable* table = declared->table(name);
        if (table == nullptr) {
            continue;
        }
        profiles.push_back(build(ProfileHeader::from(name, *table), *table));
    }
    if (profiles.empty()) {
        return std::unexpected(path + " declares no profiles.");
    }

    std::size_t defaults = 0;
    std::size_t defaultAt = 0;
    for (std::size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].header.isDefault) {
            ++defaults;
            defaultAt = index;
        }
    }
    if (defaults != 1) {
        return std::unexpected(path + " must mark exactly one profile as `default`, found " + std::to_string(defaults) +
                               ".");
    }
    return std::pair<std::vector<Profile>, std::size_t>{std::move(profiles), defaultAt};
}

/// A block of text as a file writes it: a list of lines, or one string split on its newlines.
///
/// Nothing when the key is absent, which is not the same as an empty block: a profile that says
/// nothing about its footer inherits one, and a profile that says it has none has none.
[[nodiscard]] std::optional<std::vector<std::string>> linesOf(const wire::common::toml::TomlTable& table,
                                                              std::string_view key);

} // namespace basalt::server::runtime::config
