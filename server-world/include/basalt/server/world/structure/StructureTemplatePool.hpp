#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::structure {

/// Loads a real NBT structure piece by its `minecraft:...` location string, or null if unavailable.
/// Separate from `TemplateLoader`, which the features key by identifier instead.
using PoolTemplateLoader = std::function<const StructureTemplate*(std::string_view)>;

/// parity: `StructureTemplatePool.Projection`.
enum class StructureProjection : std::uint8_t { TerrainMatching, Rigid };

/// parity: `Rotation.getShuffled` and `Rotation.getRandom`.
namespace structureRotations {

[[nodiscard]] StructureRotation random(random::RandomSource& random);
[[nodiscard]] std::vector<StructureRotation> shuffled(random::RandomSource& random);

} // namespace structureRotations

/// parity: `StructurePoolElement`, narrowed to the element kinds real village pools use. No mirror:
/// vanilla's jigsaw runtime never mirrors pieces, and pre-mirrored variants are baked as separate
/// NBT files.
///
/// A kind-tagged value rather than a class hierarchy, so a pool is a plain vector and an element can
/// be copied into one by weight without an allocation per repeat.
class StructurePoolElement {
public:
    enum class Kind : std::uint8_t { Empty, LegacySingle, List, Feature };

    /// parity: `EmptyPoolElement`.
    [[nodiscard]] static StructurePoolElement empty();

    /// parity: `SinglePoolElement` and `LegacySinglePoolElement`. `ignoreAir` captures their
    /// meaningful placement difference: legacy pieces use `STRUCTURE_AND_AIR` while modern single
    /// pieces only ignore structure blocks. Every real village piece is legacy; every bastion piece
    /// is modern.
    [[nodiscard]] static StructurePoolElement legacySingle(std::string location, StructureProcessorList processors,
                                                           StructureProjection projection, bool ignoreAir = true);

    /// parity: `ListPoolElement`: several sub-elements placed together as one unit.
    [[nodiscard]] static StructurePoolElement list(std::vector<StructurePoolElement> elements,
                                                   StructureProjection projection);

    /// parity: `FeaturePoolElement`: a placed worldgen feature as a village piece, used by the real
    /// desert, savanna, snowy and taiga `decor` and `trees` pools. Zero-sized like the real one and
    /// carrying its one synthetic downward connector.
    [[nodiscard]] static StructurePoolElement feature(std::string feature, StructureProjection projection);

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    [[nodiscard]] StructureProjection projection() const noexcept { return projection_; }

    [[nodiscard]] const std::string& location() const noexcept { return location_; }

    [[nodiscard]] std::int32_t groundLevelDelta() const noexcept { return kind_ == Kind::Empty ? 0 : 1; }
    [[nodiscard]] bool ignoreAir() const noexcept { return ignoreAir_; }
    [[nodiscard]] const StructureProcessorList& processors() const noexcept { return processors_; }
    [[nodiscard]] std::size_t listSize() const noexcept { return elements_ == nullptr ? 0 : elements_->size(); }

    [[nodiscard]] StructureBoundingBox getBoundingBox(const PoolTemplateLoader& loader, StructurePos position,
                                                      StructureRotation rotation) const;

    [[nodiscard]] std::vector<PlacedJigsaw> getShuffledJigsawBlocks(const PoolTemplateLoader& loader,
                                                                    StructurePos position, StructureRotation rotation,
                                                                    random::RandomSource& random) const;

    void place(const PoolTemplateLoader& loader, Chunk& chunk, StructurePos position, StructureRotation rotation,
               StructureBoundingBox clipTo, random::RandomSource& random,
               std::optional<std::int64_t> worldSeed = std::nullopt) const;

private:
    Kind kind_ = Kind::Empty;
    StructureProjection projection_ = StructureProjection::Rigid;
    std::string location_;
    StructureProcessorList processors_;
    bool ignoreAir_ = true;
    std::shared_ptr<const std::vector<StructurePoolElement>> elements_;
};

/// parity: `StructureTemplatePool`.
class StructureTemplatePool {
public:
    StructureTemplatePool() = default;

    StructureTemplatePool(std::string fallback,
                          const std::vector<std::pair<StructurePoolElement, std::int32_t>>& weighted);

    [[nodiscard]] const std::string& fallback() const noexcept { return fallback_; }

    [[nodiscard]] const StructurePoolElement& getRandomTemplate(random::RandomSource& random) const;

    [[nodiscard]] std::vector<StructurePoolElement> getShuffledTemplates(random::RandomSource& random) const;

    [[nodiscard]] std::size_t size() const noexcept { return templates_.size(); }

    /// parity: `StructureTemplatePool.getMaxSize`, used by the village pool expansion hack.
    [[nodiscard]] std::int32_t maxHeight(const PoolTemplateLoader& loader) const;

private:
    std::string fallback_;
    std::vector<StructurePoolElement> templates_;
};

} // namespace basalt::server::world::structure
