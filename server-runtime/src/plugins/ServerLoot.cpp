#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/plugins/ServerLoot.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <random>

namespace basalt::server::runtime::plugins {

std::vector<std::string> ServerLoot::tables() {
    std::vector<std::string> out;
    if (hooks_.tables != nullptr) {
        for (const wire::common::Identifier& id : hooks_.tables->ids()) {
            out.push_back(id.toString());
        }
    }
    return out;
}

bool ServerLoot::has(std::string_view table) {
    const auto id = wire::common::Identifier::parse(std::string{table});
    return hooks_.tables != nullptr && id.has_value() && hooks_.tables->byId(*id) != nullptr;
}

std::optional<std::vector<server::plugins::PluginLootItem>>
ServerLoot::roll(std::string_view table, const server::plugins::PluginLootRoll& rolled) {
    const auto id = wire::common::Identifier::parse(std::string{table});
    const server::world::loot::LootTable* found =
        hooks_.tables == nullptr || !id.has_value() ? nullptr : hooks_.tables->byId(*id);
    if (found == nullptr) {
        return std::nullopt;
    }
    return generate(*found, rolled);
}

std::optional<std::vector<server::plugins::PluginLootItem>>
ServerLoot::rollJson(std::string_view json, const server::plugins::PluginLootRoll& rolled) {
    if (hooks_.tables == nullptr) {
        return std::nullopt;
    }
    const auto parsed = server::world::loot::lootTableJson::parse(json, &hooks_.tables->references());
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return generate(*parsed, rolled);
}

namespace {

server::plugins::PluginLootItem flatten(const server::world::loot::ItemStack& stack) {
    return server::plugins::PluginLootItem{
        .item = stack.item.toString(),
        .count = stack.count,
        .components = stack.components.empty()
                          ? std::string{}
                          : wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(stack.components))};
}

} // namespace

std::optional<std::vector<server::plugins::PluginTradeOffer>>
ServerLoot::trades(std::string_view profession, std::int32_t level, const server::plugins::PluginTradeRoll& roll) {
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(roll.world) : nullptr;
    const auto known = entity::villagers::professions();
    if (world == nullptr || hooks_.villagerTrades == nullptr ||
        (roll.listed.empty() && std::find(known.begin(), known.end(), profession) == known.end())) {
        return std::nullopt;
    }
    const std::int64_t seed = roll.seed != 0 ? roll.seed : static_cast<std::int64_t>(std::random_device{}());
    const std::array<double, 3> origin{roll.x, roll.y, roll.z};
    std::vector<world::trades::Offer> rolled;
    if (roll.listed.empty()) {
        rolled = entity::villagers::rollOffers(
            *hooks_.villagerTrades, profession,
            std::clamp(level, entity::villagers::kFirstLevel, entity::villagers::kMaxLevel), *world, origin,
            roll.variant, roll.amount < 0 ? std::nullopt : std::optional<std::int32_t>{roll.amount}, seed);
    } else {
        std::vector<wire::common::Identifier> ids;
        for (const std::string& name : roll.listed) {
            if (const auto id = wire::common::Identifier::parse(name); id.has_value()) {
                ids.push_back(*id);
            }
        }
        rolled = entity::villagers::rollListed(*hooks_.villagerTrades, ids, roll.amount < 0 ? 1 : roll.amount, *world,
                                               origin, roll.variant, seed);
    }
    std::vector<server::plugins::PluginTradeOffer> out;
    for (const world::trades::Offer& offer : rolled) {
        server::plugins::PluginTradeOffer made{.result = flatten(offer.result),
                                               .maxUses = offer.maxUses,
                                               .experience = offer.experience,
                                               .priceMultiplier = offer.priceMultiplier};
        made.costA = flatten(offer.costA);
        if (offer.costB.has_value()) {
            made.costB = flatten(*offer.costB);
        }
        out.push_back(std::move(made));
    }
    return out;
}

std::optional<double> ServerLoot::number(std::string_view json, bool asFloat, std::int64_t seed) {
    const auto parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    server::world::loot::LootReferences* references = hooks_.tables == nullptr ? nullptr : &hooks_.tables->references();
    random::RandomSource random =
        random::RandomSource::legacy(seed != 0 ? seed : static_cast<std::int64_t>(std::random_device{}()));
    const server::world::loot::LootContext context;
    if (asFloat) {
        const auto provider = server::world::loot::lootTableJson::parseFloatProvider(*parsed, references);
        return provider.has_value() ? std::optional<double>{provider->get(random, context)} : std::nullopt;
    }
    const auto provider = server::world::loot::lootTableJson::parseIntProvider(*parsed, references);
    return provider.has_value() ? std::optional<double>{provider->get(random, context)} : std::nullopt;
}

std::optional<std::vector<server::plugins::PluginLootItem>>
ServerLoot::generate(const server::world::loot::LootTable& found, const server::plugins::PluginLootRoll& rolled) {
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(rolled.world) : nullptr;
    if (world == nullptr) {
        return std::nullopt;
    }
    server::world::loot::LootContext context;
    context.luck = rolled.luck;
    if (!rolled.looter.empty()) {
        for (const player::PlayerSession* session :
             hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
            if (session->uuid().toString() != rolled.looter) {
                continue;
            }
            context.hasEntity = true;
            context.thisEntity = server::world::loot::EntityFacts{.type = wire::common::Identifier::vanilla("player"),
                                                                  .scoreName = session->name()};
        }
    }
    hooks_.tables->prepareAt(context, *world, rolled.x, rolled.y, rolled.z);
    random::RandomSource random = random::RandomSource::legacy(
        rolled.seed != 0 ? rolled.seed : static_cast<std::int64_t>(std::random_device{}()));
    std::vector<server::plugins::PluginLootItem> out;
    for (const server::world::loot::ItemStack& stack : found.generate(random, rolled.luck, std::nullopt, context)) {
        out.push_back(server::plugins::PluginLootItem{
            .item = stack.item.toString(),
            .count = stack.count,
            .components = stack.components.empty()
                              ? std::string{}
                              : wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(stack.components))});
    }
    return out;
}

std::optional<server::plugins::PluginLootItem> ServerLoot::enchant(const server::plugins::PluginLootItem& stack,
                                                                   const server::plugins::PluginLootEnchant& how) {
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(how.world) : nullptr;
    const auto item = wire::common::Identifier::parse(stack.item);
    if (world == nullptr || hooks_.tables == nullptr || !item.has_value()) {
        return std::nullopt;
    }
    server::world::loot::LootContext context;
    hooks_.tables->prepareAt(context, *world, 0.0, 0.0, 0.0);
    server::world::loot::ItemStack given{.item = *item, .count = stack.count};
    if (!stack.components.empty()) {
        const auto parsed = wire::nbt::snbt::parse(stack.components);
        const wire::nbt::NbtCompound* compound = parsed.has_value() ? parsed->get<wire::nbt::NbtCompound>() : nullptr;
        if (compound != nullptr) {
            given.components = *compound;
        }
    }
    random::RandomSource random =
        random::RandomSource::legacy(how.seed != 0 ? how.seed : static_cast<std::int64_t>(std::random_device{}()));
    const server::world::loot::LootFunction function = server::world::loot::LootFunction::enchantWithLevels(
        server::world::loot::IntProvider::constant(how.levels), {how.options});
    const server::world::loot::ItemStack made = function.apply(std::move(given), random, std::nullopt, context);
    return server::plugins::PluginLootItem{
        .item = made.item.toString(),
        .count = made.count,
        .components = made.components.empty()
                          ? std::string{}
                          : wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(made.components))};
}

std::optional<server::plugins::PluginLootItem> ServerLoot::apply(std::string_view functions,
                                                                 const server::plugins::PluginLootItem& stack,
                                                                 const server::plugins::PluginLootApply& how) {
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(how.world) : nullptr;
    const auto item = wire::common::Identifier::parse(stack.item);
    const auto json = wire::common::parseJson(functions);
    if (world == nullptr || hooks_.tables == nullptr || !item.has_value() || !json.has_value()) {
        return std::nullopt;
    }
    const auto parsed = server::world::loot::lootTableJson::parseModifier(*json, &hooks_.tables->references());
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    server::world::loot::LootContext context;
    hooks_.tables->prepareAt(context, *world, how.x, how.y, how.z);
    server::world::loot::ItemStack given{.item = *item, .count = stack.count};
    if (!stack.components.empty()) {
        const auto components = wire::nbt::snbt::parse(stack.components);
        const wire::nbt::NbtCompound* compound =
            components.has_value() ? components->get<wire::nbt::NbtCompound>() : nullptr;
        if (compound != nullptr) {
            given.components = *compound;
        }
    }
    random::RandomSource random =
        random::RandomSource::legacy(how.seed != 0 ? how.seed : static_cast<std::int64_t>(std::random_device{}()));
    for (const server::world::loot::LootFunction& function : *parsed) {
        given = function.apply(std::move(given), random, std::nullopt, context);
    }
    return server::plugins::PluginLootItem{
        .item = given.item.toString(),
        .count = given.count,
        .components = given.components.empty()
                          ? std::string{}
                          : wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(given.components))};
}

} // namespace basalt::server::runtime::plugins
