#pragma once

#include <basalt/server/plugins/PluginDatapacks.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>

#include <functional>

namespace basalt::server::runtime::plugins {

class ServerDatapacks final : public server::plugins::PluginDatapacks {
public:
    explicit ServerDatapacks(std::function<datapack::DataPacks*()> packs,
                             std::function<bool(const std::vector<std::string>&)> reload = {})
        : packs_(std::move(packs)), reload_(std::move(reload)) {}

    void refresh() override;
    bool reload() override;
    [[nodiscard]] std::vector<server::plugins::PluginDatapack> packs() override;

private:
    std::function<datapack::DataPacks*()> packs_;
    std::function<bool(const std::vector<std::string>&)> reload_;
};

} // namespace basalt::server::runtime::plugins
