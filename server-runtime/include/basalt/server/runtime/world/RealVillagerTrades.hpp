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
    wire::nbt::NbtCompound givesComponents;
    /// parity: `merchant_predicate`.
    std::optional<server::world::loot::LootCondition> merchantPredicate;
    /// parity: `given_item_modifier`, run over what it gives; an empty result offers nothing.
    std::vector<server::world::loot::LootFunction> givenItemModifier;
    /// parity: `double_trade_price_enchantments`, ids or a single `#tag`.
    std::vector<std::string> doubleTradePriceEnchantments;
};

/// parity: `TradeSet`.
struct TradeSet {
    server::world::loot::IntProvider amount = server::world::loot::IntProvider::constant(2);
    bool allowDuplicates = false;
    std::optional<wire::common::Identifier> randomSequence;
    std::vector<VillagerTrade> trades;
};

class RealLootTables;

/// parity: `data/minecraft/villager_trade/`.
class RealVillagerTrades {
public:
    /// parity: `VillagerTrade.CODEC`.
    static constexpr std::int32_t kDefaultMaxUses = 4;
    static constexpr std::int32_t kDefaultExperience = 1;

    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// A callback because only this needs the operator's jar and a test has none.
    struct Jar {
        ReadText readText;
    };

    explicit RealVillagerTrades(Jar jar, wire::common::Logger logger = wire::common::Logger{"VillagerTrades"})
        : jar_(std::move(jar)), logger_(std::move(logger)) {}

    /// In the order the tag lists them.
    [[nodiscard]] const std::vector<VillagerTrade>& forProfession(std::string_view profession, std::int32_t level);

    /// parity: `VillagerProfession.getTrades(level)`, the level's `trade_set` file.
    [[nodiscard]] const TradeSet& tradeSet(std::string_view profession, std::int32_t level);

    [[nodiscard]] static std::optional<VillagerTrade> parse(const wire::common::Identifier& id, std::string_view json,
                                                            server::world::loot::LootReferences* references = nullptr);

    /// parity: `VillagerTrade.getOffer`.
    [[nodiscard]] static std::optional<trades::Offer>
    offerOf(const VillagerTrade& trade, const server::world::loot::LootContext& context, random::RandomSource& random);

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
