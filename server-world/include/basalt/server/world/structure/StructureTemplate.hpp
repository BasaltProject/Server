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

/// parity: `StructureTemplate.StructureEntityInfo`, before piece transform and chunk clipping.
struct StructureEntityInfo {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    StructurePos blockPos;
    wire::nbt::NbtCompound data;
};

/// parity: `StructureTemplate.JigsawBlockInfo`, in world space. `finalStateName` is what vanilla's
/// `JigsawReplacementProcessor` swaps the jigsaw block itself for once placement is done.
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

/// A transformed structure-block DATA marker consumed by a template structure piece callback.
struct StructureDataMarker {
    StructurePos pos;
    std::string metadata;
};

/// Runs after the processor list and may drop a block outright, which a rule list cannot do.
/// parity: the processors added after `RuleProcessor`, such as `BlockAgeProcessor`.
using TemplateBlockProcessor = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, StructurePos)>;

/// Everything a placement can vary, so the call site does not carry ten positional arguments.
struct TemplatePlacement {
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureMirror mirror = StructureMirror::None;
    StructurePos pivot;
    StructureBoundingBox clipTo;
    const StructureProcessorList* processors = nullptr;
    TemplateBlockProcessor blockProcessor;
    random::RandomSource* random = nullptr;
    /// parity: `StructurePlaceSettings.setRandom`, used only by the template and fossil features:
    /// the palette pick and every processor draw then come in template order.
    random::RandomSource* settingsRandom = nullptr;
    /// The world's seed, which a capped processor forks positionally to pick its blocks.
    std::optional<std::int64_t> worldSeed;
    bool ignoreStructureBlocks = false;
    bool ignoreAir = false;
    std::optional<std::int32_t> gravityOffset;
};

/// parity: `StructureTemplate`, scoped to what this project can represent. The NBT `Properties`
/// compound is retained and resolved through the block registry; facing, axis, cardinal
/// connections, numeric rotations and rail shapes are mirrored and rotated at placement. Block
/// entity NBT is preserved for the blocks the chunk can store, loot seeds included, and template
/// entities are emitted as persistent generated descriptors.
class StructureTemplate {
public:
    [[nodiscard]] StructurePos size() const noexcept { return size_; }

    [[nodiscard]] const std::vector<StructureBlockInfo>& blocks() const noexcept { return blocks_; }

    [[nodiscard]] const std::vector<StructureEntityInfo>& entities() const noexcept { return entities_; }

    /// How many palettes this template offers, which is what the placement draw picks between.
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

    /// parity: `StructureTemplate.placeInWorld`, narrowed only by the missing fluid spread. The clip
    /// mirrors vanilla's per-chunk clipping: a piece spanning several chunks is placed once per
    /// intersecting chunk, each time keeping only the blocks that land inside that one.
    void placeInto(Chunk& chunk, const TemplatePlacement& placement) const;

    /// Places a configured-feature template from one owning chunk without wrapping the part that
    /// crosses its border. Unlike a structure piece, a configured feature runs once, so its
    /// out-of-chunk blocks travel as deferred decoration writes.
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
    /// Every palette's blocks. One entry for a plain `palette`, several for `palettes`.
    std::vector<std::vector<StructureBlockInfo>> palettes_;
    std::vector<StructureEntityInfo> entities_;
};

/// Loads a real NBT structure piece by its `minecraft:...` location, or null when unavailable.
using TemplateLoader = std::function<const StructureTemplate*(const wire::common::Identifier&)>;

} // namespace basalt::server::world::structure
