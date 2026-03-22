#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::terrain {

/// What a placed Nether ruined portal is, without the blocks it wrote.
struct NetherRuinedPortalStartSnapshot {
    std::string templateName;
    structure::StructurePos position;
    structure::StructureRotation rotation = structure::StructureRotation::None;
    structure::StructureMirror mirror = structure::StructureMirror::None;
    structure::StructureBoundingBox boundingBox;
    bool airPocket = false;
};

/// parity: `RuinedPortalStructure` and `RuinedPortalPiece` for the 26.2 `ruined_portal_nether`
/// setup. The official piece runs once in its template-centre chunk and writes through a
/// `WorldGenRegion` into neighbouring chunks. Basalt owns one chunk at a time, so this replays that
/// one global operation from the centre chunk's random and clips only the final writes to the target.
class NetherRuinedPortalGenerator {
public:
    using BaseStateAt = std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)>;
    using PlaceEarlierBastions = std::function<void(Chunk&, std::int64_t)>;
    using CarveBase = std::function<void(Chunk&)>;

    NetherRuinedPortalGenerator(const NetherBiomeSource& biomeSource, structure::TemplateLoader templateLoader,
                                BaseStateAt baseStateAt, PlaceEarlierBastions placeEarlierBastions,
                                CarveBase carveBase);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// What the candidate chunk would start, without replaying the writes.
    [[nodiscard]] std::optional<NetherRuinedPortalStartSnapshot>
    startSnapshot(std::int64_t seed, std::int32_t candidateChunkX, std::int32_t candidateChunkZ);

private:
    struct PortalStart;
    struct PortalWorld;

    struct PortalBlockEntity {
        structure::StructurePos position;
        wire::nbt::NbtCompound data;
    };

    struct PortalReplay {
        std::vector<std::pair<structure::StructurePos, std::int32_t>> writes;
        std::vector<PortalBlockEntity> blockEntities;
    };

    struct PortalCacheKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const PortalCacheKey&, const PortalCacheKey&) = default;
    };

    [[nodiscard]] std::optional<PortalStart> createStart(std::int64_t seed, std::int32_t candidateChunkX,
                                                         std::int32_t candidateChunkZ) const;

    [[nodiscard]] std::int32_t findSuitableY(random::RandomSource& random, bool airPocket,
                                             const structure::StructureBoundingBox& boundingBox) const;

    [[nodiscard]] const PortalReplay& replay(std::int64_t seed, std::int32_t candidateChunkX,
                                             std::int32_t candidateChunkZ, const PortalStart& start);

    [[nodiscard]] PortalReplay createReplay(std::int64_t seed, const PortalStart& start) const;

    void fillBaseTerrain(Chunk& chunk) const;

    void placeTemplate(const PortalStart& start, PortalWorld& world, random::RandomSource& random) const;

    static void spreadNetherrack(const structure::StructureBoundingBox& boundingBox, PortalWorld& world,
                                 random::RandomSource& random);

    static void addNetherrackDripColumnsBelowPortal(const structure::StructureBoundingBox& boundingBox,
                                                    PortalWorld& world, random::RandomSource& random);

    static void addNetherrackDripColumn(random::RandomSource& random, PortalWorld& world,
                                        structure::StructurePos start);

    static void placeNetherrackOrMagma(random::RandomSource& random, PortalWorld& world,
                                       structure::StructurePos position);

    static void applyReplay(Chunk& chunk, const PortalReplay& replay);

    const NetherBiomeSource* biomeSource_;
    structure::TemplateLoader templateLoader_;
    BaseStateAt baseStateAt_;
    CarveBase carveBase_;
    PlaceEarlierBastions placeEarlierBastions_;
    std::mutex mutex_;
    std::map<PortalCacheKey, PortalReplay> replays_;
};

} // namespace basalt::server::world::terrain
