#pragma once

#include <basalt/server/plugins/PluginDatapacks.hpp>
#include <basalt/server/runtime/command/commands/FunctionRunner.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>

#include <functional>

namespace basalt::server::runtime::plugins {

class ServerDatapacks final : public server::plugins::PluginDatapacks {
public:
    explicit ServerDatapacks(std::function<datapack::DataPacks*()> packs,
                             std::function<bool(const std::vector<std::string>&)> reload = {},
                             std::function<const function::FunctionLibrary*()> functions = {},
                             command::commands::FunctionSchedule* schedule = nullptr,
                             std::function<std::int64_t()> gameTime = {})
        : packs_(std::move(packs)), reload_(std::move(reload)), functions_(std::move(functions)), schedule_(schedule),
          gameTime_(std::move(gameTime)) {}

    void refresh() override;
    bool reload() override;
    [[nodiscard]] std::vector<server::plugins::PluginDatapack> packs() override;
    [[nodiscard]] std::vector<std::string> functionIds() override;
    [[nodiscard]] std::vector<std::string> functionTagIds() override;
    [[nodiscard]] std::vector<server::plugins::PluginScheduledFunction> scheduled() override;
    bool schedule(std::string_view function, std::int64_t delay, bool replace) override;
    std::int32_t unschedule(std::string_view name) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> resource(std::string_view name) override;
    [[nodiscard]] std::vector<std::string> resourcesUnder(std::string_view prefix) override;
    [[nodiscard]] std::string resourcePack(std::string_view name) override;

private:
    std::function<datapack::DataPacks*()> packs_;
    std::function<bool(const std::vector<std::string>&)> reload_;
    std::function<const function::FunctionLibrary*()> functions_;
    command::commands::FunctionSchedule* schedule_;
    std::function<std::int64_t()> gameTime_;
};

} // namespace basalt::server::runtime::plugins
