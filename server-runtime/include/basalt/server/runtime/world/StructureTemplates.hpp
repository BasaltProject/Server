#pragma once

#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class StructureTemplates {
public:
    using ReadEntry = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)>;

    using DataIds =
        std::function<std::vector<wire::common::Identifier>(std::string_view directory, std::string_view extension)>;

    /// Callbacks rather than `ServerJarAssets`: only this needs the operator's copy of the game, and a test has none.
    struct Jar {
        ReadEntry readEntry;
        DataIds dataIds;
    };

    explicit StructureTemplates(Jar jar, wire::common::Logger logger = wire::common::Logger{"Structures"})
        : jar_(std::move(jar)), logger_(std::move(logger)) {}

    /// parity: `StructureTemplateManager.onResourceManagerReload`.
    void reload(Jar jar);

    /// The answer is kept either way, so a missing name is only looked for once.
    [[nodiscard]] const server::world::structure::StructureTemplate* find(const wire::common::Identifier& id);

    /// Every structure file the jar ships. parity: `PlaceCommand.SUGGEST_TEMPLATES`.
    [[nodiscard]] const std::vector<wire::common::Identifier>& templateIds();

    [[nodiscard]] const std::vector<wire::common::Identifier>& poolIds();

    /// parity: `LevelResource.GENERATED_DIR`.
    void setGeneratedRoot(std::filesystem::path root);

    /// parity: `StructureTemplateManager.getOrCreate`.
    bool store(const wire::common::Identifier& id, const wire::nbt::NbtCompound& structure, bool toDisk);

    /// parity: `StructureTemplateManager.remove`, which a corner block's signal asks for.
    void forget(const wire::common::Identifier& id);

private:
    [[nodiscard]] const std::vector<wire::common::Identifier>& idsUnder(const std::string& directory,
                                                                        std::string_view extension);

    Jar jar_;
    std::optional<std::filesystem::path> generatedRoot_;
    wire::common::Logger logger_;
    std::mutex mutex_;
    /// A map rather than a vector: a pointer handed out has to survive the next read.
    std::map<wire::common::Identifier, std::optional<server::world::structure::StructureTemplate>> cache_;
    std::map<std::string, std::vector<wire::common::Identifier>> listings_;
    std::map<wire::common::Identifier, std::optional<server::world::structure::StructureTemplate>> retired_;
};

} // namespace basalt::server::runtime::world
