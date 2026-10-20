#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <array>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::scoreboard {
class Scoreboard;
} // namespace basalt::server::runtime::scoreboard

namespace basalt::server::runtime::world {

class LoadedWorld;
class LootRegistries;

/// Results are cached, misses included, so a table that fails to parse is not re-read for every block broken.
class RealLootTables {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealLootTables(Jar jar, wire::common::Logger logger = wire::common::Logger{"Loot"});

    RealLootTables(const RealLootTables&) = delete;
    RealLootTables& operator=(const RealLootTables&) = delete;

    /// parity: `/reload`.
    void reload(Jar jar);

    [[nodiscard]] const server::world::loot::LootTable* byId(const wire::common::Identifier& id);

    /// parity: an entity's death table, `entities/<type>`.
    [[nodiscard]] const server::world::loot::LootTable* forEntity(const wire::common::Identifier& type);

    [[nodiscard]] const server::world::loot::LootTable* forBlock(const wire::common::Identifier& block);

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    void useRegistries(const LootRegistries* registries) { registries_ = registries; }

    /// Only rolled on the tick thread.
    void useScoreboard(const scoreboard::Scoreboard* scoreboard) { scoreboard_ = scoreboard; }

    void useTextResolver(
        std::function<std::optional<wire::nbt::NbtTag>(const wire::nbt::NbtTag& component, std::string_view holder)>
            resolver) {
        textResolver_ = std::move(resolver);
    }

    /// Unset, the stack is left blank.
    using ExplorationMaps = std::function<void(LoadedWorld& world, server::world::loot::ItemStack& stack,
                                               const server::world::loot::ExplorationMapRequest& request,
                                               const std::array<double, 3>& origin)>;

    void useExplorationMaps(ExplorationMaps maps) { explorationMaps_ = std::move(maps); }

    void prepare(server::world::loot::LootContext& context);

    [[nodiscard]] server::world::loot::LootReferences& references() noexcept { return references_; }

    /// parity: `LootContextParams.ORIGIN`.
    void prepareAt(server::world::loot::LootContext& context, LoadedWorld& world, double x, double y, double z);

private:
    const server::world::loot::LootTable* read(const wire::common::Identifier& key, const std::string& path,
                                               std::string_view verb);

    Jar jar_;
    wire::common::Logger logger_;
    server::world::loot::LootReferences references_;
    const LootRegistries* registries_ = nullptr;
    const scoreboard::Scoreboard* scoreboard_ = nullptr;
    std::function<std::optional<wire::nbt::NbtTag>(const wire::nbt::NbtTag&, std::string_view)> textResolver_;
    ExplorationMaps explorationMaps_;
    std::mutex mutex_;
    std::map<wire::common::Identifier, std::optional<server::world::loot::LootTable>> cache_;
    /// The cache before the last reload, kept so a roll already under way finishes on it.
    std::map<wire::common::Identifier, std::optional<server::world::loot::LootTable>> retired_;
};

} // namespace basalt::server::runtime::world
