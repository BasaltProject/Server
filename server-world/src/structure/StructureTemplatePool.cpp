#include <basalt/server/world/structure/StructureTemplatePool.hpp>
#include <basalt/server/world/terrain/decoration/VillageFeatures.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::server::world::structure {
namespace {

constexpr StructureBoundingBox kEmptyBox{0, 0, 0, -1, -1, -1};

constexpr std::array<StructureRotation, 4> kAllRotations{StructureRotation::None, StructureRotation::Clockwise90,
                                                         StructureRotation::Clockwise180,
                                                         StructureRotation::Counterclockwise90};

} // namespace

StructureRotation structureRotations::random(random::RandomSource& random) {
    return kAllRotations[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kAllRotations.size())))];
}

std::vector<StructureRotation> structureRotations::shuffled(random::RandomSource& random) {
    static const std::vector<StructureRotation> all{kAllRotations.begin(), kAllRotations.end()};
    return shuffledCopy(all, random);
}

StructurePoolElement StructurePoolElement::empty() { return StructurePoolElement{}; }

StructurePoolElement StructurePoolElement::legacySingle(std::string location, StructureProcessorList processors,
                                                        StructureProjection projection, bool ignoreAir) {
    StructurePoolElement element;
    element.kind_ = Kind::LegacySingle;
    element.projection_ = projection;
    element.location_ = std::move(location);
    element.processors_ = std::move(processors);
    element.ignoreAir_ = ignoreAir;
    return element;
}

StructurePoolElement StructurePoolElement::list(std::vector<StructurePoolElement> elements,
                                                StructureProjection projection) {
    StructurePoolElement element;
    element.kind_ = Kind::List;
    element.projection_ = projection;
    element.elements_ = std::make_shared<const std::vector<StructurePoolElement>>(std::move(elements));
    return element;
}

StructurePoolElement StructurePoolElement::feature(std::string feature, StructureProjection projection) {
    StructurePoolElement element;
    element.kind_ = Kind::Feature;
    element.projection_ = projection;
    element.location_ = std::move(feature);
    return element;
}

StructureBoundingBox StructurePoolElement::getBoundingBox(const PoolTemplateLoader& loader, StructurePos position,
                                                          StructureRotation rotation) const {
    switch (kind_) {
    case Kind::LegacySingle: {
        const StructureTemplate* found = loader ? loader(location_) : nullptr;
        if (found == nullptr) {
            return kEmptyBox;
        }
        return found->getBoundingBox(position, rotation, StructurePos{0, 0, 0}, StructureMirror::None);
    }
    case Kind::List: {
        std::optional<StructureBoundingBox> merged;
        for (const StructurePoolElement& element : *elements_) {
            const StructureBoundingBox box = element.getBoundingBox(loader, position, rotation);
            merged = merged.has_value()
                         ? StructureBoundingBox::fromCorners(StructurePos{merged->minX, merged->minY, merged->minZ},
                                                             StructurePos{box.maxX, box.maxY, box.maxZ})
                         : box;
        }
        return merged.value_or(kEmptyBox);
    }
    case Kind::Feature:
        return StructureBoundingBox{position.x, position.y, position.z, position.x, position.y, position.z};
    case Kind::Empty:
    default:
        return kEmptyBox;
    }
}

std::vector<PlacedJigsaw> StructurePoolElement::getShuffledJigsawBlocks(const PoolTemplateLoader& loader,
                                                                        StructurePos position,
                                                                        StructureRotation rotation,
                                                                        random::RandomSource& random) const {
    switch (kind_) {
    case Kind::LegacySingle: {
        const StructureTemplate* found = loader ? loader(location_) : nullptr;
        if (found == nullptr) {
            return {};
        }
        std::vector<PlacedJigsaw> jigsaws =
            shuffledCopy(found->jigsawBlocks(position, rotation, StructureMirror::None, StructurePos{0, 0, 0}), random);
        std::stable_sort(jigsaws.begin(), jigsaws.end(), [](const PlacedJigsaw& first, const PlacedJigsaw& second) {
            return first.selectionPriority > second.selectionPriority;
        });
        return jigsaws;
    }
    case Kind::List:
        return elements_->empty() ? std::vector<PlacedJigsaw>{}
                                  : elements_->front().getShuffledJigsawBlocks(loader, position, rotation, random);
    case Kind::Feature:
        // parity: `FeaturePoolElement.getShuffledJigsawBlocks`. The synthetic connector is DOWN_SOUTH
        // at the requested position, ignores rotation and, since 26.3, has no name.
        return {PlacedJigsaw{.pos = position,
                             .front = StructureDirection::Down,
                             .top = StructureDirection::South,
                             .rollable = true,
                             .name = "",
                             .pool = "minecraft:empty",
                             .target = "minecraft:empty",
                             .finalStateName = "minecraft:air",
                             .placementPriority = 0,
                             .selectionPriority = 0}};
    case Kind::Empty:
    default:
        return {};
    }
}

void StructurePoolElement::place(const PoolTemplateLoader& loader, Chunk& chunk, StructurePos position,
                                 StructureRotation rotation, StructureBoundingBox clipTo, random::RandomSource& random,
                                 std::optional<std::int64_t> worldSeed) const {
    switch (kind_) {
    case Kind::LegacySingle: {
        const StructureTemplate* found = loader ? loader(location_) : nullptr;
        if (found == nullptr) {
            return;
        }
        found->placeInto(chunk, TemplatePlacement{.position = position,
                                                  .rotation = rotation,
                                                  .mirror = StructureMirror::None,
                                                  .pivot = StructurePos{0, 0, 0},
                                                  .clipTo = clipTo,
                                                  .processors = &processors_,
                                                  .random = &random,
                                                  .worldSeed = worldSeed,
                                                  .ignoreStructureBlocks = true,
                                                  .ignoreAir = ignoreAir_,
                                                  .gravityOffset = projection_ == StructureProjection::TerrainMatching
                                                                       ? std::optional<std::int32_t>{-1}
                                                                       : std::nullopt});
        return;
    }
    case Kind::List:
        for (const StructurePoolElement& element : *elements_) {
            element.place(loader, chunk, position, rotation, clipTo, random, worldSeed);
        }
        return;
    case Kind::Feature:
        terrain::decoration::villageFeatures::place(location_, chunk, position.x, position.y, position.z, random);
        return;
    case Kind::Empty:
    default:
        return;
    }
}

StructureTemplatePool::StructureTemplatePool(std::string fallback,
                                             const std::vector<std::pair<StructurePoolElement, std::int32_t>>& weighted)
    : fallback_(std::move(fallback)) {
    for (const auto& [element, weight] : weighted) {
        for (std::int32_t repeat = 0; repeat < weight; ++repeat) {
            templates_.push_back(element);
        }
    }
}

const StructurePoolElement& StructureTemplatePool::getRandomTemplate(random::RandomSource& random) const {
    static const StructurePoolElement empty = StructurePoolElement::empty();
    if (templates_.empty()) {
        return empty;
    }
    return templates_[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(templates_.size())))];
}

std::vector<StructurePoolElement> StructureTemplatePool::getShuffledTemplates(random::RandomSource& random) const {
    return shuffledCopy(templates_, random);
}

std::int32_t StructureTemplatePool::maxHeight(const PoolTemplateLoader& loader) const {
    std::int32_t tallest = 0;
    for (const StructurePoolElement& element : templates_) {
        if (element.kind() == StructurePoolElement::Kind::Empty) {
            continue;
        }
        const StructureBoundingBox box = element.getBoundingBox(loader, StructurePos{0, 0, 0}, StructureRotation::None);
        tallest = std::max(tallest, box.maxY - box.minY + 1);
    }
    return tallest;
}

} // namespace basalt::server::world::structure
