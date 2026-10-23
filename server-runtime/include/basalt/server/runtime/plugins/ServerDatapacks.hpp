#pragma once

#include <basalt/server/plugins/PluginDatapacks.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>

#include <functional>

namespace basalt::server::runtime::plugins {

class ServerDatapacks final : public server::plugins::PluginDatapacks {
public:
    explicit ServerDatapacks(std::function<datapack::DataPacks*()> packs) : packs_(std::move(packs)) {}

    void refresh() override;
    [[nodiscard]] std::vector<server::plugins::PluginDatapack> packs() override;

private:
    std::function<datapack::DataPacks*()> packs_;
};

} // namespace basalt::server::runtime::plugins
