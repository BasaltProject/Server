#pragma once

#include <basalt/server/runtime/world/Trades.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// One trade as the jar declares it: what it wants, what it gives, and how often.
struct VillagerTrade {
    wire::common::Identifier id;
    wire::common::Identifier wants;
    std::int32_t wantsCount = 1;
    std::optional<wire::common::Identifier> alsoWants;
    std::int32_t alsoWantsCount = 0;
    wire::common::Identifier gives;
    std::int32_t givesCount = 1;
    std::int32_t maxUses = 4;
    std::int32_t experience = 1;
    float priceMultiplier = 0.0F;
    /// What the given stack carries on top of its item. parity: `ItemStackTemplate.components`.
    wire::nbt::NbtCompound givesComponents;
    /// parity: `merchant_predicate`, asked of the villager before the trade is offered at all.
    std::optional<server::world::loot::LootCondition> merchantPredicate;
    /// parity: `given_item_modifier`, run over what it gives; an empty result offers nothing.
    std::vector<server::world::loot::LootFunction> givenItemModifier;
    /// parity: `double_trade_price_enchantments`, ids or a single `#tag`.
    std::vector<std::string> doubleTradePriceEnchantments;
};

/// parity: `TradeSet`, how many of a level's trades a villager picks and from which random sequence.
struct TradeSet {
    server::world::loot::IntProvider amount = server::world::loot::IntProvider::constant(2);
    bool allowDuplicates = false;
    std::optional<wire::common::Identifier> randomSequence;
    std::vector<VillagerTrade> trades;
};

class RealLootTables;

/// Every villager trade the operator's own `server.jar` ships, by profession and level.
///
/// parity: trades are data since 26.2: files under `data/minecraft/villager_trade/`, grouped into
/// levels by the `villager_trade/<profession>/level_<n>` tags. Read from the operator's jar, never committed.
class RealVillagerTrades {
public:
    /// parity: `VillagerTrade.CODEC`'s own defaults when a trade file does not say: `max_uses` 4
    /// and `xp` 1, not `MerchantOffer`'s.
    static constexpr std::int32_t kDefaultMaxUses = 4;
    static constexpr std::int32_t kDefaultExperience = 1;

    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// The jar arrives as a callback rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
    };

    explicit RealVillagerTrades(Jar jar, wire::common::Logger logger = wire::common::Logger{"VillagerTrades"})
        : jar_(std::move(jar)), logger_(std::move(logger)) {}

    /// The trades this profession offers at this level, in the order the tag lists them.
    [[nodiscard]] const std::vector<VillagerTrade>& forProfession(std::string_view profession, std::int32_t level);

    /// parity: `VillagerProfession.getTrades(level)`, the level's `trade_set` file.
    [[nodiscard]] const TradeSet& tradeSet(std::string_view profession, std::int32_t level);

    /// One trade file on its own, so the shape of a trade can be pinned without a jar to read from.
    [[nodiscard]] static std::optional<VillagerTrade> parse(const wire::common::Identifier& id, std::string_view json,
                                                            server::world::loot::LootReferences* references = nullptr);

    /// parity: `VillagerTrade.getOffer`: the predicate, the modifier, the extra cost the modifier
    /// priced in, and nothing when any of it comes to an empty stack or a price under one.
    [[nodiscard]] static std::optional<trades::Offer>
    offerOf(const VillagerTrade& trade, const server::world::loot::LootContext& context, random::RandomSource& random);

    /// Where the named modifiers and predicates come from, and what prepares a roll.
    void useLootTables(RealLootTables* tables) { lootTables_ = tables; }

    [[nodiscard]] RealLootTables* lootTables() const noexcept { return lootTables_; }

private:
    Jar jar_;
    wire::common::Logger logger_;
    std::map<std::string, std::vector<VillagerTrade>> cache_;
    std::map<std::string, TradeSet> sets_;
    RealLootTables* lootTables_ = nullptr;
};

} // namespace basalt::server::runtime::world
