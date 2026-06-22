#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::structure {

struct StructureBlockInfo {
    StructurePos pos;
    std::int32_t stateId = 0;
    std::optional<wire::nbt::NbtCompound> blockEntityNbt;
    std::optional<StructureDirection> frontFacing;
    std::optional<StructureDirection> topFacing;
    std::map<std::string, std::string> properties;
};

/// parity: `StructureTemplate.StructureEntityInfo`.
struct StructureEntityInfo {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    StructurePos blockPos;
    wire::nbt::NbtCompound data;
};

/// parity: `StructureTemplate.JigsawBlockInfo`.
struct PlacedJigsaw {
    StructurePos pos;
    StructureDirection front = StructureDirection::North;
    StructureDirection top = StructureDirection::Up;
    bool rollable = false;
    std::string name;
    std::string pool;
    std::string target;
    std::string finalStateName;
    std::int32_t placementPriority = 0;
    std::int32_t selectionPriority = 0;
};

struct StructureDataMarker {
    StructurePos pos;
    std::string metadata;
};

/// parity: the processors added after `RuleProcessor`, such as `BlockAgeProcessor`.
using TemplateBlockProcessor = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, StructurePos)>;

struct TemplatePlacement {
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureMirror mirror = StructureMirror::None;
    StructurePos pivot;
    StructureBoundingBox clipTo;
    const StructureProcessorList* processors = nullptr;
    TemplateBlockProcessor blockProcessor;
    random::RandomSource* random = nullptr;
    /// parity: `StructurePlaceSettings.setRandom`.
    random::RandomSource* settingsRandom = nullptr;
    std::optional<std::int64_t> worldSeed;
    bool ignoreStructureBlocks = false;
    bool ignoreAir = false;
    /// parity: `StructurePlaceSettings.setIgnoreEntities`.
    bool ignoreEntities = false;
    /// the same stream. parity: `processBlockInfos` with no bounding box.
    bool wholePiece = false;
    std::optional<std::int32_t> gravityOffset;
    /// The palette to use in place of drawing one.
    std::optional<std::size_t> paletteIndex;
};

/// parity: `StructureTemplate`.
class StructureTemplate {
public:
    [[nodiscard]] StructurePos size() const noexcept { return size_; }

    [[nodiscard]] const std::vector<StructureBlockInfo>& blocks() const noexcept { return blocks_; }

    [[nodiscard]] const std::vector<StructureEntityInfo>& entities() const noexcept { return entities_; }

    [[nodiscard]] std::size_t paletteCount() const noexcept { return palettes_.size(); }

    void load(const wire::nbt::NbtCompound& nbt);

    /// parity: `StructureTemplate.getJigsaws`, world-space and rotated or mirrored.
    [[nodiscard]] std::vector<PlacedJigsaw> jigsawBlocks(StructurePos position, StructureRotation rotation,
                                                         StructureMirror mirror, StructurePos pivot) const;

    /// parity: `TemplateStructurePiece`'s transformed DATA-mode structure-block markers.
    [[nodiscard]] std::vector<StructureDataMarker> dataMarkers(StructurePos position, StructureRotation rotation,
                                                               StructureMirror mirror, StructurePos pivot) const;

    [[nodiscard]] StructureBoundingBox getBoundingBox(StructurePos position, StructureRotation rotation,
                                                      StructurePos pivot, StructureMirror mirror) const;

    /// parity: `StructureTemplate.placeInWorld`.
    void placeInto(Chunk& chunk, const TemplatePlacement& placement) const;

    bool placeDecorationInto(Chunk& chunk, StructurePos position, StructureRotation rotation,
                             random::RandomSource& random) const;

    /// parity: the block-state transform, exposed because the jigsaw assembly needs it on its own.
    [[nodiscard]] static std::int32_t transformedStateId(std::int32_t stateId,
                                                         const std::map<std::string, std::string>& properties,
                                                         StructureMirror mirror, StructureRotation rotation);

private:
    [[nodiscard]] const std::vector<StructureBlockInfo>& paletteBlocks(std::size_t index) const;

    void placeEntities(Chunk& chunk, StructurePos position, StructureRotation rotation, StructureMirror mirror,
                       StructurePos pivot, const StructureBoundingBox* clipTo) const;

    StructurePos size_;
    std::vector<StructureBlockInfo> blocks_;
    std::vector<std::vector<StructureBlockInfo>> palettes_;
    std::vector<StructureEntityInfo> entities_;
};

using TemplateLoader = std::function<const StructureTemplate*(const wire::common::Identifier&)>;

} // namespace basalt::server::world::structure
