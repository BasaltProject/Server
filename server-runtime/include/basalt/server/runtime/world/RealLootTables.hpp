#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// Resolves a block's break drops from the real `data/minecraft/loot_table/blocks/<path>.json` the
/// operator's own downloaded jar carries, never embedded in this repo.
///
/// Results are cached, misses included: a table that fails to parse once is not worth re-reading on
/// every single block broken.
class RealLootTables {
public:
    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// Every entry name under a prefix, which is how a whole table directory is listed.
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    /// The jar arrives as callbacks rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealLootTables(Jar jar, wire::common::Logger logger = wire::common::Logger{"Loot"})
        : jar_(std::move(jar)), logger_(std::move(logger)) {}

    RealLootTables(const RealLootTables&) = delete;
    RealLootTables& operator=(const RealLootTables&) = delete;

    /// Any table by its own id, which is what `/loot ... loot <table>` rolls.
    [[nodiscard]] const server::world::loot::LootTable* byId(const wire::common::Identifier& id);

    /// parity: an entity's death table, `entities/<type>`.
    [[nodiscard]] const server::world::loot::LootTable* forEntity(const wire::common::Identifier& type);

    /// The block's own loot table, or nothing if it breaks empty-handed: no table, or one this build
    /// cannot parse yet.
    [[nodiscard]] const server::world::loot::LootTable* forBlock(const wire::common::Identifier& block);

    /// Every table the jar ships, for the completions the client asks for.
    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

private:
    /// `verb` is how the log names what the caller wanted, since a block drops and a table rolls.
    const server::world::loot::LootTable* read(const wire::common::Identifier& key, const std::string& path,
                                               std::string_view verb);

    Jar jar_;
    wire::common::Logger logger_;
    // Several connection threads can break a block at once, so the cache has one owner and it is
    // this lock. Map nodes never move, so a table handed out stays valid while another thread reads.
    std::mutex mutex_;
    std::map<wire::common::Identifier, std::optional<server::world::loot::LootTable>> cache_;
};

} // namespace basalt::server::runtime::world
