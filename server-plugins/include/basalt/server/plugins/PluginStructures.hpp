#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginStructurePlacement {
    std::string world;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// Quarter turns clockwise, 0 to 3.
    std::int32_t rotation = 0;
    /// 0 none, 1 left-right, 2 front-back.
    std::int32_t mirror = 0;
    float integrity = 1.0F;
    std::int64_t seed = 0;
    bool entities = true;
    /// -1 picks one at random.
    std::int32_t palette = -1;
};

struct PluginTemplateScanRequest {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t rotation = 0;
    std::int32_t mirror = 0;
    std::int32_t pivotX = 0;
    std::int32_t pivotZ = 0;
    /// Only blocks of this id when set, such as `minecraft:jigsaw`.
    std::string block;
    /// False keeps the template-relative positions.
    bool absolute = true;
};

struct PluginTemplateBlock {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    /// The block entity data as an unnamed NBT compound, empty when the block has none.
    std::vector<std::uint8_t> nbt;
};

struct PluginTemplateJigsaw {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string front;
    std::string top;
    bool rollable = false;
    std::string name;
    std::string pool;
    std::string target;
    std::string finalState;
    std::int32_t placementPriority = 0;
    std::int32_t selectionPriority = 0;
};

struct PluginTemplateScan {
    std::int32_t sizeX = 0;
    std::int32_t sizeY = 0;
    std::int32_t sizeZ = 0;
    std::vector<PluginTemplateBlock> blocks;
    std::vector<PluginTemplateJigsaw> jigsaws;
};

/// Structure templates: those in the game data, those saved from a world, and in-memory ones.
class PluginStructures {
public:
    PluginStructures() = default;
    PluginStructures(const PluginStructures&) = delete;
    PluginStructures& operator=(const PluginStructures&) = delete;
    PluginStructures(PluginStructures&&) = delete;
    PluginStructures& operator=(PluginStructures&&) = delete;
    virtual ~PluginStructures() = default;

    [[nodiscard]] static PluginStructures& unavailable();

    [[nodiscard]] virtual std::vector<std::string> ids() { return {}; }

    /// The template as an unnamed NBT compound in the structure file format.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read(std::string_view id) {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// Keeps the template under this id, and in the world's generated folder when `toDisk`.
    virtual bool store(std::string_view id, std::span<const std::uint8_t> structure, bool toDisk) {
        static_cast<void>(id);
        static_cast<void>(structure);
        static_cast<void>(toDisk);
        return false;
    }

    /// Forgets the template, and deletes its generated file when `fromDisk`.
    virtual bool remove(std::string_view id, bool fromDisk) {
        static_cast<void>(id);
        static_cast<void>(fromDisk);
        return false;
    }

    [[nodiscard]] virtual std::string file(std::string_view id) {
        static_cast<void>(id);
        return {};
    }

    /// The blocks and, when asked, the entities of the box as a template.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> capture(std::string_view world, std::int32_t x,
                                                                           std::int32_t y, std::int32_t z,
                                                                           std::int32_t sizeX, std::int32_t sizeY,
                                                                           std::int32_t sizeZ, bool entities) {
        static_cast<void>(world);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(sizeX);
        static_cast<void>(sizeY);
        static_cast<void>(sizeZ);
        static_cast<void>(entities);
        return std::nullopt;
    }

    /// The blocks and jigsaws of a template as a placement would put them, with the world left alone.
    [[nodiscard]] virtual std::optional<PluginTemplateScan> scan(std::span<const std::uint8_t> structure,
                                                                 const PluginTemplateScanRequest& request) {
        static_cast<void>(structure);
        static_cast<void>(request);
        return std::nullopt;
    }

    /// Places a template given in the structure file format; the loaded chunks it covers change.
    virtual bool place(std::span<const std::uint8_t> structure, const PluginStructurePlacement& placement) {
        static_cast<void>(structure);
        static_cast<void>(placement);
        return false;
    }
};

inline PluginStructures& PluginStructures::unavailable() {
    static PluginStructures none;
    return none;
}

} // namespace basalt::server::plugins
