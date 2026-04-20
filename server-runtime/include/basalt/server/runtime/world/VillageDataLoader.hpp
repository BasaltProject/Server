#pragma once

#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/world/structure/StructureTemplatePool.hpp>
#include <basalt/server/world/structure/VillagePoolJson.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class VillageDataLoader {
public:
    VillageDataLoader(StructureTemplates& templates, StructureTemplates::ReadEntry readEntry);

    [[nodiscard]] const server::world::structure::StructureTemplatePool* pool(std::string_view name);

    [[nodiscard]] std::optional<std::set<std::int32_t>> blockTag(std::string_view name);

    [[nodiscard]] std::optional<std::string> processorListText(std::string_view name);

    [[nodiscard]] server::world::structure::TemplateLoader templateLoader();

    [[nodiscard]] server::world::structure::villagePoolJson::TagResolver tagResolver();

    [[nodiscard]] server::world::structure::villagePoolJson::ProcessorListResolver processorResolver();

private:
    [[nodiscard]] std::optional<std::set<std::int32_t>> expandBlockTag(std::string_view name,
                                                                       std::set<std::string>& seen);

    [[nodiscard]] std::optional<std::string> readDataText(const std::string& path) const;

    StructureTemplates* templates_;
    StructureTemplates::ReadEntry readEntry_;
    /// One lock for three caches, as three locks would be three chances to take them in a different order.
    std::mutex mutex_;
    std::map<std::string, std::optional<server::world::structure::StructureTemplatePool>, std::less<>> pools_;
    std::map<std::string, std::optional<std::string>, std::less<>> processorTexts_;
    std::map<std::string, std::optional<std::set<std::int32_t>>, std::less<>> tags_;
};

} // namespace basalt::server::runtime::world
