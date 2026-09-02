#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// The timelines the operator's own `server.jar` ships, as the NBT the registry packet carries.
///
/// A timeline maps a clock's ticks to environment attributes: without one the client has a clock
/// that advances and nothing that reads it, so `/time set night` leaves the sky where it was.
///
/// Same shape and the same all-or-nothing rule as `RealDialogs` and `RealEnchantments`.
class RealTimelines {
public:
    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// Every entry name under a prefix, which is how a whole registry directory is listed.
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    /// The jar arrives as callbacks rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealTimelines(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Timelines"});

    /// Id to definition, in sorted order: the index of an entry here is the id the wire carries.
    [[nodiscard]] const std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>>&
    entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    /// Every name this registry holds, for the completions a command offers.
    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    /// The definition of one timeline, or nothing when the jar shipped no such thing.
    [[nodiscard]] const wire::nbt::NbtCompound* definitionOf(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
};

} // namespace basalt::server::runtime::world
