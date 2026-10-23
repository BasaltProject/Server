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

} // namespace basalt::server::runtime::plugins
