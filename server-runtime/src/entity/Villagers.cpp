#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::server::runtime::entity::villagers {
namespace {

constexpr std::array<std::string_view, 13> kProfessions{
    "armorer",       "butcher",   "cartographer", "cleric",   "farmer",    "fisherman",  "fletcher",
    "leatherworker", "librarian", "mason",        "shepherd", "toolsmith", "weaponsmith"};

[[nodiscard]] wire::nbt::NbtCompound stackTag(const server::world::loot::ItemStack& stack) {
    wire::nbt::NbtCompound tag;
    tag.putString("id", stack.item.toString());
    tag.putInt("count", stack.count);
    if (stack.components.size() > 0) {
        tag.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    }
    return tag;
}

[[nodiscard]] std::optional<server::world::loot::ItemStack> stackOf(const wire::nbt::NbtCompound* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(tag->getString("id"));
    if (!id.has_value()) {
        return std::nullopt;
    }
    server::world::loot::ItemStack stack{.item = *id, .count = tag->getInt("count", 1)};
    if (const wire::nbt::NbtCompound* components = tag->getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

} // namespace

void writeOffers(const ServerEntity& villager, wire::nbt::NbtCompound& tag) {
    if (villager.offers == nullptr || villager.offers->level == 0) {
        return;
    }
    wire::nbt::NbtList recipes{wire::nbt::NbtType::Compound};
    for (const world::trades::Offer& offer : villager.offers->list) {
        wire::nbt::NbtCompound recipe;
        recipe.set("buy", wire::nbt::NbtTag::ofCompound(stackTag(offer.costA)));
        if (offer.costB.has_value()) {
            recipe.set("buyB", wire::nbt::NbtTag::ofCompound(stackTag(*offer.costB)));
        }
        recipe.set("sell", wire::nbt::NbtTag::ofCompound(stackTag(offer.result)));
        recipe.putInt("uses", offer.uses);
        recipe.putInt("maxUses", offer.maxUses);
        recipe.putBoolean("rewardExp", true);
        recipe.putInt("specialPrice", offer.specialPrice);
        recipe.putInt("demand", offer.demand);
        recipe.putFloat("priceMultiplier", offer.priceMultiplier);
        recipe.putInt("xp", offer.experience);
        static_cast<void>(recipes.push(wire::nbt::NbtTag::ofCompound(std::move(recipe))));
    }
    wire::nbt::NbtCompound offers;
    offers.set("Recipes", wire::nbt::NbtTag::ofList(std::move(recipes)));
    tag.set("Offers", wire::nbt::NbtTag::ofCompound(std::move(offers)));
}

void readOffers(const wire::nbt::NbtCompound& tag, ServerEntity& villager) {
    const wire::nbt::NbtCompound* offers = tag.getCompound("Offers");
    if (offers == nullptr) {
        return;
    }
    auto read = std::make_shared<Offers>();
    read->level = std::clamp(villager.villagerLevel, kFirstLevel, kMaxLevel);
    if (const wire::nbt::NbtList* recipes = offers->getList("Recipes"); recipes != nullptr) {
        for (const wire::nbt::NbtTag& entry : recipes->elements()) {
            const wire::nbt::NbtCompound* recipe = entry.get<wire::nbt::NbtCompound>();
            const std::optional<server::world::loot::ItemStack> buy =
                recipe == nullptr ? std::nullopt : stackOf(recipe->getCompound("buy"));
            const std::optional<server::world::loot::ItemStack> sell =
                recipe == nullptr ? std::nullopt : stackOf(recipe->getCompound("sell"));
            if (!buy.has_value() || !sell.has_value()) {
                continue;
            }
            world::trades::Offer offer{.costA = *buy,
                                       .costB = stackOf(recipe->getCompound("buyB")),
                                       .result = *sell,
                                       .maxUses = recipe->getInt("maxUses", 4),
                                       .experience = recipe->getInt("xp", 1),
                                       .priceMultiplier = recipe->getFloat("priceMultiplier", 0.0F),
                                       .uses = recipe->getInt("uses"),
                                       .demand = recipe->getInt("demand"),
                                       .specialPrice = recipe->getInt("specialPrice")};
            read->list.push_back(std::move(offer));
        }
    }
    villager.offers = std::move(read);
}

const wire::common::Identifier& type() {
    static const wire::common::Identifier kType = wire::common::Identifier::vanilla("villager");
    return kType;
}

std::span<const std::string_view> professions() { return kProfessions; }

world::trades::Offer asOffer(const world::VillagerTrade& trade) {
    world::trades::Offer offer{.costA = server::world::loot::ItemStack{.item = trade.wants, .count = trade.wantsCount},
                               .result = server::world::loot::ItemStack{.item = trade.gives, .count = trade.givesCount},
                               .maxUses = trade.maxUses,
                               .experience = trade.experience,
                               .priceMultiplier = trade.priceMultiplier};
    if (trade.alsoWants.has_value()) {
        offer.costB = server::world::loot::ItemStack{.item = *trade.alsoWants, .count = trade.alsoWantsCount};
    }
    return offer;
}

namespace {

void rollTrades(world::RealVillagerTrades& trades, ServerEntity& villager, world::LoadedWorld& world,
                std::int32_t level, std::optional<std::int32_t> fixedAmount) {
    Offers& offers = *villager.offers;
    const world::TradeSet& set = trades.tradeSet(villager.profession, level);
    // parity: the roll's `THIS_ENTITY` and `ORIGIN`.
    server::world::loot::LootContext context;
    if (world::RealLootTables* tables = trades.lootTables(); tables != nullptr) {
        tables->prepareAt(context, world, villager.x, villager.y, villager.z);
    }
    context.hasEntity = true;
    context.additionalCostAllowed = true;
    server::world::loot::EntityFacts facts{.type = villager.type()};
    const wire::nbt::NbtCompound* data =
        villager.sourceNbt.has_value() ? villager.sourceNbt->getCompound("VillagerData") : nullptr;
    const std::string_view variant = data == nullptr ? std::string_view{} : data->getString("type");
    facts.components.emplace("minecraft:villager/variant", variant.empty() ? "minecraft:plains" : variant);
    context.thisEntity = std::move(facts);
    random::RandomSource fallback = random::RandomSource::legacy(0);
    random::RandomSource& random =
        set.randomSequence.has_value() ? world.randomSequences().of(*set.randomSequence) : fallback;
    const std::int32_t wanted = fixedAmount.has_value() ? *fixedAmount : set.amount.get(random, context);
    std::vector<const world::VillagerTrade*> pool;
    for (const world::VillagerTrade& trade : set.trades) {
        pool.push_back(&trade);
    }
    std::int32_t found = 0;
    while (found < wanted && !pool.empty()) {
        const auto roll = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(pool.size())));
        const world::VillagerTrade* trade = pool[roll];
        // parity: without duplicates the pick leaves the pool first; with them, only a pick that gave nothing does.
        if (!set.allowDuplicates) {
            pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(roll));
        }
        std::optional<world::trades::Offer> offer = world::RealVillagerTrades::offerOf(*trade, context, random);
        if (!offer.has_value()) {
            if (set.allowDuplicates) {
                pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(roll));
            }
            continue;
        }
        offers.list.push_back(std::move(*offer));
        ++found;
    }
}

} // namespace

std::vector<world::trades::Offer>& offersOf(world::RealVillagerTrades& trades, ServerEntity& villager,
                                            world::LoadedWorld& world) {
    if (villager.offers == nullptr) {
        villager.offers = std::make_shared<Offers>();
    }
    Offers& offers = *villager.offers;
    const std::int32_t reached = std::clamp(villager.villagerLevel, kFirstLevel, kMaxLevel);
    while (offers.level < reached) {
        ++offers.level;
        rollTrades(trades, villager, world, offers.level, std::nullopt);
    }
    return offers.list;
}

std::int32_t addTrades(world::RealVillagerTrades& trades, ServerEntity& villager, world::LoadedWorld& world,
                       std::int32_t amount) {
    const std::size_t before = offersOf(trades, villager, world).size();
    constexpr std::int32_t kMaxAdded = 64;
    rollTrades(trades, villager, world, villager.offers->level, std::clamp(amount, 0, kMaxAdded));
    return static_cast<std::int32_t>(villager.offers->list.size() - before);
}

void resetOffers(ServerEntity& villager) { villager.offers.reset(); }

} // namespace basalt::server::runtime::entity::villagers
