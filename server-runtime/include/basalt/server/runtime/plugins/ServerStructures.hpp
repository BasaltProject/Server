#pragma once

#include <basalt/server/plugins/PluginStructures.hpp>

#include <functional>
#include <string>

namespace basalt::server::runtime {
class StructureBlockSystem;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
class StructureTemplates;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::plugins {

class ServerStructures final : public server::plugins::PluginStructures {
public:
    struct Hooks {
        world::StructureTemplates* templates = nullptr;
        std::function<StructureBlockSystem*()> blocks;
        std::function<world::LoadedWorld*(const std::string& name)> worldFor;
    };

    explicit ServerStructures(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::vector<std::string> ids() override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view id) override;
    bool store(std::string_view id, std::span<const std::uint8_t> structure, bool toDisk) override;
    bool remove(std::string_view id, bool fromDisk) override;
    [[nodiscard]] std::string file(std::string_view id) override;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> capture(std::string_view world, std::int32_t x,
                                                                   std::int32_t y, std::int32_t z, std::int32_t sizeX,
                                                                   std::int32_t sizeY, std::int32_t sizeZ,
                                                                   bool entities) override;
    bool place(std::span<const std::uint8_t> structure,
               const server::plugins::PluginStructurePlacement& placement) override;

private:
    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
