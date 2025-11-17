#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// The dialogs the operator's own `server.jar` ships, as the NBT the registry packet carries.
///
/// Same shape and the same all-or-nothing rule as `RealEnchantments`: `minecraft:dialog` is a
/// datapack registry, a client knows only what it was sent, and an entry it cannot parse drops the
/// connection at login.
class RealDialogs {
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

    explicit RealDialogs(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Dialogs"});

    /// Id to definition, in sorted order: the index of an entry here is the id the wire carries.
    [[nodiscard]] const std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>>&
    entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    /// Every name this registry holds, for the completions a command offers.
    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& id) const;

    [[nodiscard]] bool contains(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
    std::map<wire::common::Identifier, std::int32_t> indices_;
};

} // namespace basalt::server::runtime::world
