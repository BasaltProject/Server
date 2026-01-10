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

/// Resolves structure pool and piece names the way vanilla's `StructureTemplateManager` does, from
/// the operator's own downloaded jar, never embedded here.
///
/// Results are cached, since one jigsaw assembly asks for the same pool once per attempted
/// connection. Assemblies run on several threads, so each cache parses outside the lock and inserts
/// under a brief one, discarding a duplicate if another thread won the same key.
///
/// The templates themselves are `StructureTemplates`' business: this is the half that is pools,
/// processor lists and block tags.
class VillageDataLoader {
public:
    /// The templates come from the same jar and are already cached there; what this adds is the
    /// pools, the processor lists and the block tags, which nothing else reads.
    VillageDataLoader(StructureTemplates& templates, StructureTemplates::ReadEntry readEntry);

    /// The pool by name, or nothing when the jar ships none or it does not parse.
    [[nodiscard]] const server::world::structure::StructureTemplatePool* pool(std::string_view name);

    /// Resolves a block tag name to its member state ids, expanding nested `#tag` references.
    [[nodiscard]] std::optional<std::set<std::int32_t>> blockTag(std::string_view name);

    /// A named processor list's own JSON text, which is what a pool's string form refers to.
    [[nodiscard]] std::optional<std::string> processorListText(std::string_view name);

    [[nodiscard]] server::world::structure::TemplateLoader templateLoader();

    [[nodiscard]] server::world::structure::villagePoolJson::TagResolver tagResolver();

    [[nodiscard]] server::world::structure::villagePoolJson::ProcessorListResolver processorResolver();

private:
    [[nodiscard]] std::optional<std::set<std::int32_t>> expandBlockTag(std::string_view name,
                                                                       std::set<std::string>& seen);

    /// One `data/minecraft/<path>` entry as text, or nothing when the jar ships none.
    [[nodiscard]] std::optional<std::string> readDataText(const std::string& path) const;

    StructureTemplates* templates_;
    StructureTemplates::ReadEntry readEntry_;
    /// One lock for the three caches: they are only touched around a parse, and three locks would
    /// be three chances to take them in a different order.
    std::mutex mutex_;
    std::map<std::string, std::optional<server::world::structure::StructureTemplatePool>, std::less<>> pools_;
    std::map<std::string, std::optional<std::string>, std::less<>> processorTexts_;
    std::map<std::string, std::optional<std::set<std::int32_t>>, std::less<>> tags_;
};

} // namespace basalt::server::runtime::world
