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

/// The enchantments the operator's own `server.jar` ships, converted to the NBT the registry packet
/// carries.
///
/// `minecraft:enchantment` is a datapack registry, so the client knows only what it was sent, and
/// the definitions are content: read from the download, never committed.
///
/// All or nothing. An entry the client cannot parse drops the connection at login, so one bad file
/// means an empty registry instead of a partial one.
class RealEnchantments {
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

    explicit RealEnchantments(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Enchantments"});

    /// Id to definition, in sorted order: the index of an entry here is the id the wire carries.
    [[nodiscard]] const std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>>&
    entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    /// Every name this registry holds, for the completions a command offers.
    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    /// The registry index of an enchantment, which is what an item component references.
    [[nodiscard]] std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& id) const;

    /// Its definition as the jar wrote it, for the effects this server reads rather than forwards.
    [[nodiscard]] const wire::nbt::NbtCompound* definitionOf(const wire::common::Identifier& id) const;

    [[nodiscard]] bool contains(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
    std::map<wire::common::Identifier, std::int32_t> indices_;
};

} // namespace basalt::server::runtime::world
