#include <basalt/server/runtime/plugins/ServerLoot.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

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
    world::LoadedWorld* world = hooks_.worldFor ? hooks_.worldFor(rolled.world) : nullptr;
    const server::world::loot::LootTable* found =
        hooks_.tables == nullptr || !id.has_value() ? nullptr : hooks_.tables->byId(*id);
    if (found == nullptr || world == nullptr) {
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
    for (const server::world::loot::ItemStack& stack : found->generate(random, rolled.luck, std::nullopt, context)) {
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

} // namespace basalt::server::runtime::plugins
