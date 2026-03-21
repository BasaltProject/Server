#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

using server::world::loot::ItemStack;
using server::world::loot::LootCondition;
using server::world::loot::LootFunction;
using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

/// parity: `DataComponents.ADDITIONAL_TRADE_COST`, which only lives on a stack until it is priced.
constexpr std::string_view kAdditionalTradeCost = "minecraft:additional_trade_cost";

/// A whole number written either way round: the jar writes counts as integers and the discount as a
/// fraction, and a field is read for what it is rather than for how it was spelled.
[[nodiscard]] std::optional<double> numberOf(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* integer = value->get<std::int64_t>(); integer != nullptr) {
        return static_cast<double>(*integer);
    }
    if (const double* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Identifier> identifierOf(const JsonObject* object, std::string_view key) {
    if (object == nullptr) {
        return std::nullopt;
    }
    const JsonValue* value = object->find(key);
    const std::string* text = value == nullptr ? nullptr : value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*text);
    return id.has_value() ? std::optional{*id} : std::nullopt;
}

[[nodiscard]] std::int32_t countOf(const JsonObject* object, std::int32_t fallback) {
    if (object == nullptr) {
        return fallback;
    }
    const std::optional<double> count = numberOf(object->find("count"));
    return count.has_value() ? static_cast<std::int32_t>(*count) : fallback;
}

/// parity: the `values` of a tag file, as ids. Nothing here resolves a nested tag: a level's tag
/// lists its trades outright.
[[nodiscard]] std::vector<Identifier> idsOf(std::string_view json) {
    std::vector<Identifier> ids;
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return ids;
    }
    const JsonObject* root = parsed->get<JsonObject>();
    const JsonValue* values = root == nullptr ? nullptr : root->find("values");
    const std::vector<JsonValue>* array = values == nullptr ? nullptr : values->get<std::vector<JsonValue>>();
    if (array == nullptr) {
        return ids;
    }
    for (const JsonValue& element : *array) {
        const std::string* text = element.get<std::string>();
        if (text == nullptr) {
            continue;
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*text);
        if (id.has_value()) {
            ids.push_back(*id);
        }
    }
    return ids;
}

/// A holder set as written: one id, a list of them, or a single `#tag`.
[[nodiscard]] std::vector<std::string> idsOrTagOf(const JsonValue* value) {
    std::vector<std::string> ids;
    if (value == nullptr) {
        return ids;
    }
    if (const std::string* one = value->get<std::string>(); one != nullptr) {
        ids.push_back(*one);
    } else if (const std::vector<JsonValue>* list = value->get<std::vector<JsonValue>>(); list != nullptr) {
        for (const JsonValue& each : *list) {
            if (const std::string* text = each.get<std::string>(); text != nullptr) {
                ids.push_back(*text);
            }
        }
    }
    return ids;
}

/// Whether an enchantment is one the set names, a tag being asked of the roll's own registries.
[[nodiscard]] bool listsEnchantment(const std::vector<std::string>& set, const std::string& enchantment,
                                    const server::world::loot::LootContext& context) {
    if (set.size() == 1 && set.front().starts_with('#')) {
        const std::expected<Identifier, wire::common::IdentifierError> tag =
            Identifier::parse(std::string_view{set.front()}.substr(1));
        if (!tag.has_value() || !context.tagMembers) {
            return false;
        }
        const std::vector<Identifier> members = context.tagMembers("enchantment", *tag);
        return std::ranges::any_of(members, [&](const Identifier& id) { return id.toString() == enchantment; });
    }
    return std::ranges::find(set, enchantment) != set.end();
}

} // namespace

std::optional<VillagerTrade> RealVillagerTrades::parse(const Identifier& id, std::string_view json,
                                                       server::world::loot::LootReferences* references) {
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::nullopt;
    }
    const auto objectAt = [root](std::string_view name) -> const JsonObject* {
        const JsonValue* value = root->find(name);
        return value == nullptr ? nullptr : value->get<JsonObject>();
    };
    const JsonObject* wants = objectAt("wants");
    const JsonObject* gives = objectAt("gives");
    const JsonObject* extra = objectAt("additional_wants");
    const std::optional<Identifier> wanted = identifierOf(wants, "id");
    const std::optional<Identifier> given = identifierOf(gives, "id");
    if (!wanted.has_value() || !given.has_value()) {
        return std::nullopt;
    }
    const std::optional<Identifier> alsoWanted = identifierOf(extra, "id");
    VillagerTrade trade{
        .id = id,
        .wants = *wanted,
        .wantsCount = countOf(wants, 1),
        .alsoWants = alsoWanted,
        .alsoWantsCount = alsoWanted.has_value() ? countOf(extra, 1) : 0,
        .gives = *given,
        .givesCount = countOf(gives, 1),
        // parity: `getOffer` floors each: at least one use, no negative experience or discount.
        .maxUses = std::max(static_cast<std::int32_t>(numberOf(root->find("max_uses")).value_or(kDefaultMaxUses)), 1),
        .experience = std::max(static_cast<std::int32_t>(numberOf(root->find("xp")).value_or(kDefaultExperience)), 0),
        .priceMultiplier =
            std::max(static_cast<float>(numberOf(root->find("reputation_discount")).value_or(0.0)), 0.0F),
    };
    if (const JsonValue* components = gives->find("components"); components != nullptr) {
        std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag = wire::nbt::jsonToNbt(*components);
        if (const wire::nbt::NbtCompound* compound = tag.has_value() ? tag->get<wire::nbt::NbtCompound>() : nullptr;
            compound != nullptr) {
            trade.givesComponents = *compound;
        }
    }
    // A predicate or modifier this build cannot read would offer the wrong thing, so the trade goes.
    if (const JsonValue* predicate = root->find("merchant_predicate"); predicate != nullptr) {
        std::expected<LootCondition, server::world::loot::LootTableParseError> parsedPredicate =
            server::world::loot::lootTableJson::parseCondition(*predicate, references);
        if (!parsedPredicate.has_value()) {
            return std::nullopt;
        }
        trade.merchantPredicate = std::move(*parsedPredicate);
    }
    if (const JsonValue* modifier = root->find("given_item_modifier"); modifier != nullptr) {
        std::expected<std::vector<LootFunction>, server::world::loot::LootTableParseError> parsedModifier =
            server::world::loot::lootTableJson::parseModifier(*modifier, references);
        if (!parsedModifier.has_value()) {
            return std::nullopt;
        }
        trade.givenItemModifier = std::move(*parsedModifier);
    }
    trade.doubleTradePriceEnchantments = idsOrTagOf(root->find("double_trade_price_enchantments"));
    return trade;
}

std::optional<trades::Offer> RealVillagerTrades::offerOf(const VillagerTrade& trade,
                                                         const server::world::loot::LootContext& context,
                                                         random::RandomSource& random) {
    if (trade.merchantPredicate.has_value() && !trade.merchantPredicate->test(random, context)) {
        return std::nullopt;
    }
    ItemStack result{.item = trade.gives, .count = trade.givesCount, .components = trade.givesComponents};
    for (const LootFunction& modifier : trade.givenItemModifier) {
        result = modifier.apply(std::move(result), random, std::nullopt, context);
    }
    if (result.count <= 0 || result.item.path() == "air") {
        return std::nullopt;
    }
    // parity: `ADDITIONAL_TRADE_COST` comes off the result and onto the first price, doubled for a
    // book storing one of the listed enchantments.
    std::int32_t additional = result.components.getInt(kAdditionalTradeCost);
    static_cast<void>(result.components.remove(kAdditionalTradeCost));
    if (!trade.doubleTradePriceEnchantments.empty()) {
        if (const wire::nbt::NbtCompound* stored = result.components.getCompound("minecraft:stored_enchantments");
            stored != nullptr) {
            const bool doubled = std::ranges::any_of(stored->names(), [&](const std::string& name) {
                return listsEnchantment(trade.doubleTradePriceEnchantments, name, context);
            });
            if (doubled) {
                additional *= 2;
            }
        }
    }
    const std::int32_t wanted = trade.wantsCount + additional;
    if (wanted < 1 || (trade.alsoWants.has_value() && trade.alsoWantsCount < 1)) {
        return std::nullopt;
    }
    trades::Offer offer{.costA = ItemStack{.item = trade.wants, .count = wanted},
                        .result = std::move(result),
                        .maxUses = trade.maxUses,
                        .experience = trade.experience,
                        .priceMultiplier = trade.priceMultiplier};
    if (trade.alsoWants.has_value()) {
        offer.costB = ItemStack{.item = *trade.alsoWants, .count = trade.alsoWantsCount};
    }
    return offer;
}

const TradeSet& RealVillagerTrades::tradeSet(std::string_view profession, std::int32_t level) {
    const std::string key = std::string{profession} + "/" + std::to_string(level);
    if (const auto cached = sets_.find(key); cached != sets_.end()) {
        return cached->second;
    }
    TradeSet set;
    const std::optional<std::string> text = jar_.readText
                                                ? jar_.readText("data/minecraft/trade_set/" + std::string{profession} +
                                                                "/level_" + std::to_string(level) + ".json")
                                                : std::nullopt;
    const std::expected<JsonValue, wire::common::JsonError> parsed =
        text.has_value() ? wire::common::parseJson(*text) : std::unexpected(wire::common::JsonError::SyntaxError);
    const JsonObject* root = parsed.has_value() ? parsed->get<JsonObject>() : nullptr;
    server::world::loot::LootReferences* references = lootTables_ == nullptr ? nullptr : &lootTables_->references();
    if (root != nullptr) {
        if (const JsonValue* amount = root->find("amount"); amount != nullptr) {
            if (auto provider = server::world::loot::lootTableJson::parseIntProvider(*amount, references);
                provider.has_value()) {
                set.amount = std::move(*provider);
            }
        }
        if (const JsonValue* duplicates = root->find("allow_duplicates"); duplicates != nullptr) {
            const bool* flag = duplicates->get<bool>();
            set.allowDuplicates = flag != nullptr && *flag;
        }
        set.randomSequence = identifierOf(root, "random_sequence");
    }
    // parity: the set's `trades` is a holder set; every vanilla one is the level's own tag.
    std::vector<Identifier> ids;
    const std::vector<std::string> named =
        root == nullptr ? std::vector<std::string>{} : idsOrTagOf(root->find("trades"));
    if (named.size() == 1 && named.front().starts_with('#')) {
        const std::expected<Identifier, wire::common::IdentifierError> tag =
            Identifier::parse(std::string_view{named.front()}.substr(1));
        const std::optional<std::string> tagText =
            tag.has_value() && jar_.readText
                ? jar_.readText("data/" + tag->namespaceName() + "/tags/villager_trade/" + tag->path() + ".json")
                : std::nullopt;
        ids = tagText.has_value() ? idsOf(*tagText) : std::vector<Identifier>{};
    } else if (root != nullptr) {
        for (const std::string& one : named) {
            if (const auto id = Identifier::parse(one); id.has_value()) {
                ids.push_back(*id);
            }
        }
    } else if (jar_.readText) {
        const std::optional<std::string> tagText =
            jar_.readText("data/minecraft/tags/villager_trade/" + std::string{profession} + "/level_" +
                          std::to_string(level) + ".json");
        ids = tagText.has_value() ? idsOf(*tagText) : std::vector<Identifier>{};
    }
    for (const Identifier& id : ids) {
        const std::optional<std::string> file =
            jar_.readText ? jar_.readText("data/" + id.namespaceName() + "/villager_trade/" + id.path() + ".json")
                          : std::nullopt;
        if (!file.has_value()) {
            continue;
        }
        std::optional<VillagerTrade> trade = parse(id, *file, references);
        if (trade.has_value()) {
            set.trades.push_back(std::move(*trade));
        } else {
            logger_.debug([&] { return "Villager trade " + id.toString() + " is not offered: it did not parse"; });
        }
    }
    return sets_.emplace(key, std::move(set)).first->second;
}

const std::vector<VillagerTrade>& RealVillagerTrades::forProfession(std::string_view profession, std::int32_t level) {
    const std::string key = std::string{profession} + "/" + std::to_string(level);
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        return cached->second;
    }
    std::vector<VillagerTrade> loaded;
    const std::optional<std::string> tag =
        jar_.readText ? jar_.readText("data/minecraft/tags/villager_trade/" + std::string{profession} + "/level_" +
                                      std::to_string(level) + ".json")
                      : std::nullopt;
    if (!tag.has_value()) {
        logger_.debug([&] { return "No trades for " + key + ": no such tag"; });
        return cache_.emplace(key, std::move(loaded)).first->second;
    }
    for (const Identifier& id : idsOf(*tag)) {
        const std::optional<std::string> text = jar_.readText("data/minecraft/villager_trade/" + id.path() + ".json");
        if (!text.has_value()) {
            continue;
        }
        const std::optional<VillagerTrade> trade = parse(id, *text);
        if (trade.has_value()) {
            loaded.push_back(*trade);
        }
    }
    return cache_.emplace(key, std::move(loaded)).first->second;
}

} // namespace basalt::server::runtime::world
