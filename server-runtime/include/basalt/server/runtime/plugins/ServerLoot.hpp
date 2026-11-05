#pragma once

#include <basalt/server/plugins/PluginLoot.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

class ServerLoot final : public server::plugins::PluginLoot {
public:
    struct Hooks {
        world::RealLootTables* tables = nullptr;
        world::RealVillagerTrades* villagerTrades = nullptr;
        std::function<world::LoadedWorld*(const std::string& name)> worldFor;
        std::function<std::vector<player::PlayerSession*>()> sessions;
    };

    explicit ServerLoot(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::vector<std::string> tables() override;
    [[nodiscard]] bool has(std::string_view table) override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginLootItem>>
    roll(std::string_view table, const server::plugins::PluginLootRoll& roll) override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginLootItem>>
    rollJson(std::string_view json, const server::plugins::PluginLootRoll& roll) override;
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginTradeOffer>>
    trades(std::string_view profession, std::int32_t level, const server::plugins::PluginTradeRoll& roll) override;
    [[nodiscard]] std::optional<double> number(std::string_view json, bool asFloat, std::int64_t seed) override;
    [[nodiscard]] std::optional<server::plugins::PluginLootItem>
    enchant(const server::plugins::PluginLootItem& stack, const server::plugins::PluginLootEnchant& how) override;

private:
    [[nodiscard]] std::optional<std::vector<server::plugins::PluginLootItem>>
    generate(const server::world::loot::LootTable& table, const server::plugins::PluginLootRoll& rolled);

    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
