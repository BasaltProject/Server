#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>
#include <basalt/server/world/terrain/decoration/RootedSulfurSpringFeature.hpp>
#include <basalt/server/world/terrain/decoration/SurfaceFreezingDecoration.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;
using structure::StructureTemplate;

/// parity: `FossilFeature`'s own two structure lists, lower and upper. They hold the same pieces,
/// and are still two lists because the two features draw their index separately.
constexpr std::array<std::string_view, 8> kFossilLower{"fossil/spine_1", "fossil/spine_2", "fossil/spine_3",
                                                       "fossil/spine_4", "fossil/skull_1", "fossil/skull_2",
                                                       "fossil/skull_3", "fossil/skull_4"};
constexpr std::array<std::string_view, 8> kFossilUpper = kFossilLower;

/// parity: `FossilFeature`, which picks one of its structures and buries it. The pieces are
/// structure files from the operator's own jar, so this needs the template loader.
[[nodiscard]] placedFeatures::ContextualPlacement fossil(const std::array<std::string_view, 8>& structures) {
    return [&structures](Chunk& chunk, random::RandomSource& random, std::int32_t localX, std::int32_t y,
                         std::int32_t localZ, const placedFeatures::FeatureContext& context) {
        if (!context.templates) {
            return false;
        }
        const std::string_view chosen =
            structures[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(structures.size())))];
        const StructureTemplate* found = context.templates(wire::common::Identifier::vanilla(std::string{chosen}));
        if (found == nullptr) {
            return false;
        }
        const StructurePos position{chunk.chunkX() * 16 + (localX & 15), y, chunk.chunkZ() * 16 + (localZ & 15)};
        const StructurePos pivot{0, 0, 0};
        found->placeInto(chunk,
                         structure::TemplatePlacement{.position = position,
                                                      .pivot = pivot,
                                                      .clipTo = found->getBoundingBox(position, StructureRotation::None,
                                                                                      pivot, StructureMirror::None),
                                                      .ignoreStructureBlocks = true});
        return true;
    };
}

const std::map<wire::common::Identifier, placedFeatures::ContextualPlacement>& contextualRegistry() {
    static const std::map<wire::common::Identifier, placedFeatures::ContextualPlacement> built = [] {
        std::map<wire::common::Identifier, placedFeatures::ContextualPlacement> registry;
        registry.emplace(wire::common::Identifier::vanilla("end_spike"),
                         [](Chunk& chunk, random::RandomSource&, std::int32_t, std::int32_t, std::int32_t,
                            const placedFeatures::FeatureContext& context) {
                             return end::endSurfaceStructuresDecoration::placeSpikesIn(chunk, context.seed);
                         });
        registry.emplace(wire::common::Identifier::vanilla("chorus_plant"),
                         [](Chunk& chunk, random::RandomSource& random, std::int32_t localX, std::int32_t y,
                            std::int32_t localZ, const placedFeatures::FeatureContext& context) {
                             if (context.endRouter == nullptr) {
                                 return false;
                             }
                             return end::endVegetalDecoration::placeAt(chunk, random, *context.endRouter, localX, y,
                                                                       localZ);
                         });
        registry.emplace(wire::common::Identifier::vanilla("freeze_top_layer"),
                         [](Chunk& chunk, random::RandomSource&, std::int32_t localX, std::int32_t, std::int32_t localZ,
                            const placedFeatures::FeatureContext& context) {
                             if (context.surface == nullptr) {
                                 return false;
                             }
                             surfaceFreezingDecoration::placeAt(chunk, *context.surface, localX, localZ);
                             return true;
                         });
        registry.emplace(wire::common::Identifier::vanilla("fossil_lower"), fossil(kFossilLower));
        registry.emplace(wire::common::Identifier::vanilla("fossil_upper"), fossil(kFossilUpper));
        registry.emplace(wire::common::Identifier::vanilla("rooted_sulfur_spring"),
                         [](Chunk& chunk, random::RandomSource& random, std::int32_t localX, std::int32_t y,
                            std::int32_t localZ, const placedFeatures::FeatureContext& context) {
                             if (!context.templates || !context.blockStateAt) {
                                 return false;
                             }
                             const DecorationPos origin{chunk.chunkX() * 16 + (localX & 15), y,
                                                        chunk.chunkZ() * 16 + (localZ & 15)};
                             return rootedSulfurSpringFeature::place(
                                 chunk, random, origin, context.templates, [&context](const DecorationPos& position) {
                                     return context.blockStateAt(position.x, position.y, position.z);
                                 });
                         });
        return registry;
    }();
    return built;
}

} // namespace

const placedFeatures::ContextualPlacement* placedFeatures::contextual(const wire::common::Identifier& id) {
    const auto found = contextualRegistry().find(id);
    return found == contextualRegistry().end() ? nullptr : &found->second;
}

const std::set<wire::common::Identifier>& placedFeatures::contextualIds() {
    static const std::set<wire::common::Identifier> built = [] {
        std::set<wire::common::Identifier> ids;
        for (const auto& [id, placement] : contextualRegistry()) {
            ids.insert(id);
        }
        return ids;
    }();
    return built;
}

} // namespace basalt::server::world::terrain::decoration
