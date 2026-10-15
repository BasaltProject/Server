#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/CommandPredicates.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>

#include <map>
#include <mutex>
#include <random>

namespace basalt::server::runtime::world::commandPredicates {

std::optional<bool> test(RealLootTables& tables, const wire::common::JsonValue& predicate, LoadedWorld* world,
                         const std::array<double, 3>& origin, std::optional<server::world::loot::EntityFacts> self) {
    if (const auto* text = predicate.get<std::string>(); text != nullptr) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(*text);
        if (!id.has_value() || !tables.references().read("predicate", *id).has_value()) {
            return std::nullopt;
        }
    }
    const std::expected<server::world::loot::LootCondition, server::world::loot::LootTableParseError> condition =
        server::world::loot::lootTableJson::parseCondition(predicate, &tables.references());
    if (!condition.has_value()) {
        return std::nullopt;
    }
    server::world::loot::LootContext context;
    if (world != nullptr) {
        tables.prepareAt(context, *world, origin[0], origin[1], origin[2]);
    } else {
        tables.prepare(context);
        context.origin = origin;
    }
    context.hasEntity = self.has_value();
    context.thisEntity = std::move(self);
    // parity: `LootContext.Builder.create(Optional.empty())` draws on the level's own random.
    static std::mutex mutex;
    static std::map<const LoadedWorld*, random::RandomSource> levels;
    const std::scoped_lock lock{mutex};
    auto found = levels.find(world);
    if (found == levels.end()) {
        found = levels.emplace(world, random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}())))
                    .first;
    }
    return condition->test(found->second, context);
}

} // namespace basalt::server::runtime::world::commandPredicates
