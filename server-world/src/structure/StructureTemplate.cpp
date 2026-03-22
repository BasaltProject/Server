#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <set>
#include <string_view>
#include <utility>

namespace basalt::server::world::structure {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] StructurePos plus(StructurePos left, StructurePos right) {
    return StructurePos{.x = left.x + right.x, .y = left.y + right.y, .z = left.z + right.z};
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// Everything a template's block entity NBT can be attached to, and the subset that carries a loot
/// seed. Both are exactly the jar's lists for the templates 26.2 ships.
const std::set<std::string, std::less<>>& dyeColors() {
    static const std::set<std::string, std::less<>> colors = {
        "white",      "orange", "magenta", "light_blue", "yellow", "lime",  "pink", "gray",
        "light_gray", "cyan",   "purple",  "blue",       "brown",  "green", "red",  "black"};
    return colors;
}

const std::set<std::string, std::less<>>& randomizableContainerBlocks() {
    static const std::set<std::string, std::less<>> blocks = [] {
        std::set<std::string, std::less<>> built = {"barrel",  "chest",  "trapped_chest", "dispenser",
                                                    "dropper", "hopper", "crafter",       "shulker_box"};
        for (const std::string& color : dyeColors()) {
            built.insert(color + "_shulker_box");
        }
        // The copper chests are chests too, which the 26.3 camps bury with a loot table.
        for (const std::string_view waxed : {"", "waxed_"}) {
            for (const std::string_view age : {"", "exposed_", "weathered_", "oxidized_"}) {
                built.insert(std::string{waxed} + std::string{age} + "copper_chest");
            }
        }
        return built;
    }();
    return blocks;
}

const std::set<std::string, std::less<>>& blockEntityBlocks() {
    static const std::set<std::string, std::less<>> blocks = [] {
        std::set<std::string, std::less<>> built = randomizableContainerBlocks();
        built.insert({"spawner", "furnace", "blast_furnace", "smoker", "brewing_stand", "beacon", "lectern", "jukebox",
                      "campfire", "soul_campfire", "beehive", "bee_nest", "decorated_pot", "chiseled_bookshelf",
                      "trial_spawner", "vault", "bell",
                      // The four the 26.2 templates carry that this list used to miss: the ancient
                      // city's comparators and sculk sensors, and the End city's ender chest.
                      "comparator", "sculk_sensor", "calibrated_sculk_sensor", "ender_chest"});
        for (const std::string& color : dyeColors()) {
            built.insert(color + "_banner");
            built.insert(color + "_wall_banner");
        }
        // Every head shares the skull block entity; leaving the rest out would be an arbitrary line.
        built.insert({"skeleton_skull", "skeleton_wall_skull", "wither_skeleton_skull", "wither_skeleton_wall_skull",
                      "zombie_head", "zombie_wall_head", "player_head", "player_wall_head", "creeper_head",
                      "creeper_wall_head", "dragon_head", "dragon_wall_head", "piglin_head", "piglin_wall_head"});
        for (const std::string_view wood :
             {"oak", "spruce", "birch", "jungle", "acacia", "cherry", "dark_oak", "pale_oak", "mangrove", "bamboo"}) {
            built.insert(std::string{wood} + "_sign");
            built.insert(std::string{wood} + "_wall_sign");
            built.insert(std::string{wood} + "_hanging_sign");
            built.insert(std::string{wood} + "_wall_hanging_sign");
        }
        return built;
    }();
    return blocks;
}

[[nodiscard]] std::int32_t intAt(const NbtList& list, std::size_t index) {
    if (index >= list.size()) {
        return 0;
    }
    const std::int32_t* value = list[index].get<std::int32_t>();
    return value == nullptr ? 0 : *value;
}

[[nodiscard]] std::optional<double> doubleAt(const NbtList& list, std::size_t index) {
    if (index >= list.size()) {
        return std::nullopt;
    }
    const double* value = list[index].get<double>();
    return value == nullptr ? std::nullopt : std::optional{*value};
}

[[nodiscard]] float floatAt(const NbtList& list, std::size_t index) {
    if (index >= list.size()) {
        return 0.0F;
    }
    const float* value = list[index].get<float>();
    return value == nullptr ? 0.0F : *value;
}

/// The state a `name[key=value,...]` string resolves to, falling back to the block's default.
[[nodiscard]] std::int32_t parsedStateId(std::string_view stateName, std::map<std::string, std::string>& properties) {
    const std::size_t open = stateName.find('[');
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsedName =
        wire::common::Identifier::parse(std::string{stateName.substr(0, open)});
    if (!parsedName.has_value()) {
        return registry::blocks::air().stateId;
    }
    const wire::common::Identifier& identifier = *parsedName;
    if (open != std::string_view::npos) {
        const std::size_t close = stateName.find(']', open);
        std::string_view body = stateName.substr(open + 1, close == std::string_view::npos ? close : close - open - 1);
        while (!body.empty()) {
            const std::size_t comma = body.find(',');
            std::string_view entry = body.substr(0, comma);
            const std::size_t equals = entry.find('=');
            if (equals != std::string_view::npos) {
                std::string key{entry.substr(0, equals)};
                std::string value{entry.substr(equals + 1)};
                const auto trim = [](std::string& text) {
                    while (!text.empty() && text.front() == ' ') {
                        text.erase(text.begin());
                    }
                    while (!text.empty() && text.back() == ' ') {
                        text.pop_back();
                    }
                };
                trim(key);
                trim(value);
                properties.emplace(std::move(key), std::move(value));
            }
            if (comma == std::string_view::npos) {
                break;
            }
            body = body.substr(comma + 1);
        }
    }
    if (const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(identifier, properties);
        resolved.has_value()) {
        return *resolved;
    }
    const registry::BlockState* block = registry::blocks::named(identifier.path());
    return block == nullptr ? registry::blocks::air().stateId : block->stateId;
}

} // namespace

const std::vector<StructureBlockInfo>& StructureTemplate::paletteBlocks(const std::size_t index) const {
    return index < palettes_.size() ? palettes_[index] : blocks_;
}

namespace {

/// One palette's states, resolved once and shared by every block that points at it.
struct Palette {
    std::vector<std::map<std::string, std::string>> properties;
    std::vector<std::int32_t> stateIds;
    std::vector<std::optional<StructureDirection>> frontFacing;
    std::vector<std::optional<StructureDirection>> topFacing;
};

[[nodiscard]] Palette readPaletteEntries(const NbtList& paletteList) {
    Palette palette;
    palette.properties.reserve(paletteList.size());
    palette.stateIds.reserve(paletteList.size());
    palette.frontFacing.reserve(paletteList.size());
    palette.topFacing.reserve(paletteList.size());
    for (std::size_t index = 0; index < paletteList.size(); ++index) {
        // parity: `BlockState.CODEC` since 26.3: `{id, properties}`, a bare id (wrapped as `{"": id}`
        // in a mixed list), or 26.2's `{Name, Properties}`.
        const NbtCompound* entry = paletteList[index].get<NbtCompound>();
        const std::string* bare = paletteList[index].get<std::string>();
        if (entry != nullptr && entry->size() == 1 && entry->names()[0].empty()) {
            bare = entry->values()[0].get<std::string>();
            entry = nullptr;
        }
        std::map<std::string, std::string> properties;
        std::string name = bare != nullptr ? *bare : std::string{};
        if (entry != nullptr) {
            const bool legacy = entry->contains("Name");
            name = std::string{entry->getString(legacy ? "Name" : "id")};
            if (const NbtCompound* propertyTag = entry->getCompound(legacy ? "Properties" : "properties");
                propertyTag != nullptr) {
                for (std::size_t at = 0; at < propertyTag->size(); ++at) {
                    const std::string* value = propertyTag->values()[at].get<std::string>();
                    if (value != nullptr) {
                        properties.emplace(propertyTag->names()[at], *value);
                    }
                }
            }
        }
        std::int32_t stateId = registry::blocks::air().stateId;
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> identifier =
            wire::common::Identifier::parse(name);
        if (identifier.has_value()) {
            if (const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(*identifier, properties);
                resolved.has_value()) {
                stateId = *resolved;
            } else if (const registry::BlockState* block = registry::blocks::named(identifier->path());
                       block != nullptr) {
                stateId = block->stateId;
            }
        }
        // parity: the jigsaw block's `orientation` property (`front_top`, front first) is the one
        // block property this project reads, since jigsaw assembly cannot work without it.
        std::optional<StructureDirection> front;
        std::optional<StructureDirection> top;
        if (name == "minecraft:jigsaw") {
            const auto orientation = properties.find("orientation");
            if (orientation != properties.end()) {
                const std::size_t split = orientation->second.find('_');
                if (split != std::string::npos) {
                    front = directionFromName(std::string_view{orientation->second}.substr(0, split));
                    top = directionFromName(std::string_view{orientation->second}.substr(split + 1));
                }
            }
        }
        palette.properties.push_back(std::move(properties));
        palette.stateIds.push_back(stateId);
        palette.frontFacing.push_back(front);
        palette.topFacing.push_back(top);
    }
    return palette;
}

/// Resolves one palette's states against the shared block positions.
[[nodiscard]] std::vector<StructureBlockInfo> readPalette(const NbtList& paletteList, const NbtList& blockList) {
    const Palette palette = readPaletteEntries(paletteList);
    std::vector<StructureBlockInfo> target;
    target.reserve(blockList.size());
    for (std::size_t index = 0; index < blockList.size(); ++index) {
        const NbtCompound* blockTag = blockList[index].get<NbtCompound>();
        if (blockTag == nullptr) {
            continue;
        }
        const NbtList* posList = blockTag->getList("pos");
        if (posList == nullptr || posList->size() < 3) {
            continue;
        }
        const auto paletteIndex = static_cast<std::size_t>(blockTag->getInt("state"));
        StructureBlockInfo info;
        info.pos = StructurePos{intAt(*posList, 0), intAt(*posList, 1), intAt(*posList, 2)};
        info.stateId =
            paletteIndex < palette.stateIds.size() ? palette.stateIds[paletteIndex] : registry::blocks::air().stateId;
        if (const NbtCompound* blockEntity = blockTag->getCompound("nbt"); blockEntity != nullptr) {
            info.blockEntityNbt = *blockEntity;
        }
        if (paletteIndex < palette.frontFacing.size()) {
            info.frontFacing = palette.frontFacing[paletteIndex];
            info.topFacing = palette.topFacing[paletteIndex];
            info.properties = palette.properties[paletteIndex];
        }
        target.push_back(std::move(info));
    }
    return target;
}

/// parity: `StructureTemplate.transform` for an entity's fractional position.
[[nodiscard]] std::array<double, 3> transformEntityPosition(double sourceX, double sourceY, double sourceZ,
                                                            StructureMirror mirror, StructureRotation rotation,
                                                            StructurePos pivot) {
    double x = sourceX;
    double z = sourceZ;
    if (mirror == StructureMirror::LeftRight) {
        z = 1.0 - z;
    } else if (mirror == StructureMirror::FrontBack) {
        x = 1.0 - x;
    }
    switch (rotation) {
    case StructureRotation::Counterclockwise90:
        return {pivot.x - pivot.z + z, sourceY, pivot.x + pivot.z + 1.0 - x};
    case StructureRotation::Clockwise90:
        return {pivot.x + pivot.z + 1.0 - z, sourceY, pivot.z - pivot.x + x};
    case StructureRotation::Clockwise180:
        return {pivot.x + pivot.x + 1.0 - x, sourceY, pivot.z + pivot.z + 1.0 - z};
    case StructureRotation::None:
    default:
        return {x, sourceY, z};
    }
}

[[nodiscard]] float transformEntityYaw(float sourceYaw, StructureMirror mirror, StructureRotation rotation) {
    float mirroredYaw = sourceYaw;
    if (mirror == StructureMirror::FrontBack) {
        mirroredYaw = -sourceYaw;
    } else if (mirror == StructureMirror::LeftRight) {
        mirroredYaw = 180.0F - sourceYaw;
    }
    const float mirrorOffset = mirroredYaw - sourceYaw;
    float rotationOffset = 0.0F;
    if (rotation == StructureRotation::Clockwise90) {
        rotationOffset = 90.0F;
    } else if (rotation == StructureRotation::Clockwise180) {
        rotationOffset = 180.0F;
    } else if (rotation == StructureRotation::Counterclockwise90) {
        rotationOffset = 270.0F;
    }
    return sourceYaw + rotationOffset + mirrorOffset;
}

/// parity: the jigsaw replacement's `final_state`, parsed and then transformed like any state.
[[nodiscard]] std::int32_t parsedAndTransformedStateId(std::string_view stateName, StructureMirror mirror,
                                                       StructureRotation rotation) {
    std::map<std::string, std::string> properties;
    const std::int32_t stateId = parsedStateId(stateName, properties);
    return StructureTemplate::transformedStateId(stateId, properties, mirror, rotation);
}

/// One block that survived the pre-write pass: where the processors read, and where it lands.
struct ProcessedBlock {
    const StructureBlockInfo* block;
    StructurePos processorPos;
    StructurePos placedPos;
};

/// parity: `AppendLoot`, the block entity modifier a processor rule carries. It builds the tag even
/// when the template block had none, and takes the seed from the same positional random the rule was
/// tested with.
void placeAppendedLoot(Chunk& chunk, std::int32_t stateId, StructurePos worldPos, const std::string& table,
                       const std::optional<NbtCompound>& existing = std::nullopt) {
    static const std::map<std::string, std::string, std::less<>> types{{"suspicious_sand", "brushable_block"},
                                                                       {"suspicious_gravel", "brushable_block"}};
    const auto type = types.find(pathOf(stateId));
    if (type == types.end()) {
        return;
    }
    random::RandomSource lootRandom = random::RandomSource::worldgen(
        random::RandomSource::legacy(random::positionalSeed(worldPos.x, worldPos.y, worldPos.z)));
    NbtCompound data = existing.value_or(NbtCompound{});
    data.remove("id");
    data.set("LootTable", NbtTag::ofString(table));
    data.set("LootTableSeed", NbtTag::ofLong(lootRandom.nextLong()));
    chunk.setDecorationBlockEntity(
        worldPos.x, worldPos.y, worldPos.z, stateId,
        wire::common::Identifier::parse(type->second).value_or(wire::common::Identifier::vanilla("brushable_block")),
        std::move(data));
}

void placeBlockEntityInto(Chunk& chunk, const StructureBlockInfo& blockInfo, std::int32_t stateId,
                          StructurePos worldPos, random::RandomSource* random, bool decoration) {
    if (!blockInfo.blockEntityNbt.has_value()) {
        return;
    }
    const std::string finalPath = pathOf(stateId);
    if (finalPath.empty() || !blockEntityBlocks().contains(finalPath)) {
        return;
    }
    const std::string typeName{blockInfo.blockEntityNbt->getString("id")};
    if (typeName.empty() || typeName == "minecraft:jigsaw") {
        return;
    }
    NbtCompound data = *blockInfo.blockEntityNbt;
    if (!data.contains("components")) {
        data.set("components", NbtTag::ofCompound(NbtCompound{}));
    }
    if (random != nullptr && randomizableContainerBlocks().contains(finalPath)) {
        data.set("LootTableSeed", NbtTag::ofLong(random->nextLong()));
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> type =
        wire::common::Identifier::parse(typeName);
    if (!type.has_value()) {
        return;
    }
    if (decoration) {
        chunk.setDecorationBlockEntity(worldPos.x, worldPos.y, worldPos.z, stateId, *type, std::move(data));
        return;
    }
    chunk.setBlockEntity(ChunkBlockEntity{worldPos.x & 15, worldPos.y, worldPos.z & 15, *type, std::move(data)});
}

} // namespace

void StructureTemplate::load(const NbtCompound& nbt) {
    blocks_.clear();
    palettes_.clear();
    entities_.clear();
    if (const NbtList* sizeList = nbt.getList("size"); sizeList != nullptr) {
        size_ = StructurePos{intAt(*sizeList, 0), intAt(*sizeList, 1), intAt(*sizeList, 2)};
    } else {
        size_ = StructurePos{};
    }

    // parity: a template carries either one `palette` or several `palettes`; the shipwreck and the
    // ocean ruins carry eight. The positions in `blocks` are shared and only the state index
    // resolves differently, which is why the palette is drawn at placement time.
    const NbtList* blockList = nbt.getList("blocks");
    if (blockList == nullptr) {
        return;
    }
    std::vector<const NbtList*> paletteLists;
    if (const NbtList* palettes = nbt.getList("palettes"); palettes != nullptr) {
        for (const NbtTag& entry : *palettes) {
            if (const NbtList* palette = entry.get<NbtList>(); palette != nullptr) {
                paletteLists.push_back(palette);
            }
        }
    } else if (const NbtList* palette = nbt.getList("palette"); palette != nullptr) {
        paletteLists.push_back(palette);
    }
    if (paletteLists.empty()) {
        return;
    }
    palettes_.reserve(paletteLists.size());
    for (const NbtList* paletteList : paletteLists) {
        palettes_.push_back(readPalette(*paletteList, *blockList));
    }
    blocks_ = palettes_.front();

    const NbtList* entityList = nbt.getList("entities");
    if (entityList == nullptr) {
        return;
    }
    for (const NbtTag& entry : *entityList) {
        const NbtCompound* entityTag = entry.get<NbtCompound>();
        if (entityTag == nullptr) {
            continue;
        }
        const NbtList* pos = entityTag->getList("pos");
        const NbtList* blockPos = entityTag->getList("blockPos");
        const NbtCompound* data = entityTag->getCompound("nbt");
        if (pos == nullptr || blockPos == nullptr || data == nullptr || pos->size() < 3 || blockPos->size() < 3) {
            continue;
        }
        const std::optional<double> x = doubleAt(*pos, 0);
        const std::optional<double> y = doubleAt(*pos, 1);
        const std::optional<double> z = doubleAt(*pos, 2);
        if (!x.has_value() || !y.has_value() || !z.has_value()) {
            continue;
        }
        entities_.push_back(
            StructureEntityInfo{.x = *x,
                                .y = *y,
                                .z = *z,
                                .blockPos = StructurePos{intAt(*blockPos, 0), intAt(*blockPos, 1), intAt(*blockPos, 2)},
                                .data = *data});
    }
}

std::vector<PlacedJigsaw> StructureTemplate::jigsawBlocks(const StructurePos position, const StructureRotation rotation,
                                                          const StructureMirror mirror,
                                                          const StructurePos pivot) const {
    std::vector<PlacedJigsaw> placed;
    for (const StructureBlockInfo& blockInfo : blocks_) {
        if (!blockInfo.frontFacing.has_value() || !blockInfo.topFacing.has_value() ||
            !blockInfo.blockEntityNbt.has_value()) {
            continue;
        }
        const StructurePos worldPos = plus(transform(blockInfo.pos, mirror, rotation, pivot), position);
        const NbtCompound& nbt = *blockInfo.blockEntityNbt;
        const std::string_view joint = nbt.getString("joint");
        const StructureDirection front = rotated(mirrored(*blockInfo.frontFacing, mirror), rotation);
        placed.push_back(
            PlacedJigsaw{.pos = worldPos,
                         .front = front,
                         .top = rotated(mirrored(*blockInfo.topFacing, mirror), rotation),
                         .rollable = joint == "rollable" || (joint.empty() && !isHorizontal(*blockInfo.frontFacing)),
                         .name = std::string{nbt.getString("name", "minecraft:empty")},
                         .pool = std::string{nbt.getString("pool", "minecraft:empty")},
                         .target = std::string{nbt.getString("target", "minecraft:empty")},
                         .finalStateName = std::string{nbt.getString("final_state", "minecraft:air")},
                         .placementPriority = nbt.getInt("placement_priority", 0),
                         .selectionPriority = nbt.getInt("selection_priority", 0)});
    }
    return placed;
}

std::vector<StructureDataMarker> StructureTemplate::dataMarkers(const StructurePos position,
                                                                const StructureRotation rotation,
                                                                const StructureMirror mirror,
                                                                const StructurePos pivot) const {
    std::vector<StructureDataMarker> markers;
    for (const StructureBlockInfo& blockInfo : blocks_) {
        if (pathOf(blockInfo.stateId) != "structure_block" || !blockInfo.blockEntityNbt.has_value()) {
            continue;
        }
        std::string mode{blockInfo.blockEntityNbt->getString("mode")};
        for (char& character : mode) {
            if (character >= 'a' && character <= 'z') {
                character = static_cast<char>(character - 32);
            }
        }
        if (mode != "DATA") {
            continue;
        }
        markers.push_back(
            StructureDataMarker{.pos = plus(transform(blockInfo.pos, mirror, rotation, pivot), position),
                                .metadata = std::string{blockInfo.blockEntityNbt->getString("metadata")}});
    }
    return markers;
}

StructureBoundingBox StructureTemplate::getBoundingBox(const StructurePos position, const StructureRotation rotation,
                                                       const StructurePos pivot, const StructureMirror mirror) const {
    const StructurePos delta{size_.x - 1, size_.y - 1, size_.z - 1};
    const StructurePos corner1 = transform(StructurePos{0, 0, 0}, mirror, rotation, pivot);
    const StructurePos corner2 = transform(delta, mirror, rotation, pivot);
    return StructureBoundingBox::fromCorners(corner1, corner2).move(position);
}

void StructureTemplate::placeEntities(Chunk& chunk, const StructurePos position, const StructureRotation rotation,
                                      const StructureMirror mirror, const StructurePos pivot,
                                      const StructureBoundingBox* clipTo) const {
    for (const StructureEntityInfo& entityInfo : entities_) {
        const StructurePos ownerPos = plus(transform(entityInfo.blockPos, mirror, rotation, pivot), position);
        if (clipTo != nullptr && !clipTo->isInside(ownerPos)) {
            continue;
        }
        const std::array<double, 3> moved =
            transformEntityPosition(entityInfo.x, entityInfo.y, entityInfo.z, mirror, rotation, pivot);
        const double worldX = moved[0] + position.x;
        const double worldY = moved[1] + position.y;
        const double worldZ = moved[2] + position.z;
        NbtCompound data = entityInfo.data;
        data.remove("UUID");
        data.set("Pos",
                 NbtTag::ofList(NbtList{
                     NbtType::Double, {NbtTag::ofDouble(worldX), NbtTag::ofDouble(worldY), NbtTag::ofDouble(worldZ)}}));
        float sourceYaw = 0.0F;
        float pitch = 0.0F;
        if (const NbtList* sourceRotation = data.getList("Rotation"); sourceRotation != nullptr) {
            sourceYaw = floatAt(*sourceRotation, 0);
            pitch = floatAt(*sourceRotation, 1);
        }
        const float yaw = transformEntityYaw(sourceYaw, mirror, rotation);
        data.set("Rotation", NbtTag::ofList(NbtList{NbtType::Float, {NbtTag::ofFloat(yaw), NbtTag::ofFloat(pitch)}}));
        const std::string typeName{data.getString("id")};
        if (typeName.empty()) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> type =
            wire::common::Identifier::parse(typeName);
        if (!type.has_value()) {
            continue;
        }
        chunk.addGeneratedEntity(ChunkGeneratedEntity{
            .type = *type, .x = worldX, .y = worldY, .z = worldZ, .yaw = yaw, .pitch = pitch, .data = std::move(data)});
    }
}

void StructureTemplate::placeInto(Chunk& chunk, const TemplatePlacement& placement) const {
    static const StructureProcessorList kNoProcessors;
    const StructureProcessorList& processors = placement.processors == nullptr ? kNoProcessors : *placement.processors;
    // parity: `getRandomPalette` draws whether there is one palette or eight, and the draw comes out
    // of the placement's own stream when it has one.
    std::size_t paletteIndex = 0;
    if (paletteCount() <= 1) {
        if (placement.settingsRandom != nullptr) {
            static_cast<void>(placement.settingsRandom->nextInt(1));
        }
    } else if (placement.settingsRandom != nullptr) {
        paletteIndex =
            static_cast<std::size_t>(placement.settingsRandom->nextInt(static_cast<std::int32_t>(paletteCount())));
    } else {
        random::RandomSource chooser = random::RandomSource::worldgen(random::RandomSource::legacy(
            random::positionalSeed(placement.position.x, placement.position.y, placement.position.z)));
        paletteIndex = static_cast<std::size_t>(chooser.nextInt(static_cast<std::int32_t>(paletteCount())));
    }

    const std::vector<StructureBlockInfo>& paletteBlockList = paletteBlocks(paletteIndex);
    const bool wholePiece = placement.wholePiece || !processors.capped().empty();
    const ChunkHeight bounds = chunk.verticalBounds();
    std::vector<ProcessedBlock> processedBlocks;
    processedBlocks.reserve(paletteBlockList.size());
    for (const StructureBlockInfo& blockInfo : paletteBlockList) {
        if (placement.ignoreAir && registry::blocks::isAir(blockInfo.stateId)) {
            continue;
        }
        if (placement.ignoreStructureBlocks && pathOf(blockInfo.stateId) == "structure_block") {
            continue;
        }
        const StructurePos originalWorldPos =
            plus(transform(blockInfo.pos, placement.mirror, placement.rotation, placement.pivot), placement.position);
        // parity: `processBlockInfos` rejects blocks outside the chunk box before any processor,
        // *unless* one of them evaluates the entire piece state. Only a capped one does, and it
        // needs the whole piece to choose from; the write below still clips.
        if (!wholePiece && !placement.clipTo.isInside(originalWorldPos)) {
            continue;
        }
        const std::int32_t localX = originalWorldPos.x & 15;
        const std::int32_t localZ = originalWorldPos.z & 15;
        StructurePos worldPos = originalWorldPos;
        if (placement.gravityOffset.has_value()) {
            // parity: terrain matching appends a gravity processor over `WORLD_SURFACE_WG`. It uses
            // the unrotated template Y, not the piece origin or the transformed Y, and it reads the
            // **frozen** map: during generation the level is not a server level, so the processor
            // never swaps in the live heightmap. Reading the live one let an earlier piece of the
            // same village raise the ground the next one lands on.
            worldPos.y = chunk.worldgenHeightAt(localX, localZ, HeightmapType::WorldSurface) +
                         *placement.gravityOffset + blockInfo.pos.y;
        }
        const bool inHeight = worldPos.y >= bounds.minY() && worldPos.y <= bounds.maxY();
        if (!inHeight && !placement.wholePiece) {
            continue;
        }
        if (!wholePiece && !placement.clipTo.isInside(worldPos)) {
            continue;
        }
        processedBlocks.push_back(
            ProcessedBlock{.block = &blockInfo, .processorPos = originalWorldPos, .placedPos = worldPos});
    }

    // `processBlockInfos` completes the whole list before the first block is written, so one
    // projected block cannot raise the height sampled by another block of the same template. The
    // capped pass also has to see every final state before it picks.
    struct Resolved {
        const StructureBlockInfo* block;
        StructurePos worldPos;
        std::int32_t stateId;
    };

    std::vector<Resolved> resolved;
    resolved.reserve(processedBlocks.size());
    std::map<std::size_t, std::string> ruleLoot;
    for (const ProcessedBlock& entry : processedBlocks) {
        const StructureBlockInfo& blockInfo = *entry.block;
        const std::int32_t processorLocalX = entry.processorPos.x & 15;
        const std::int32_t processorLocalZ = entry.processorPos.z & 15;
        std::int32_t stateId = 0;
        // parity: the jigsaw replacement, applied to every jigsaw block unconditionally.
        if (blockInfo.frontFacing.has_value()) {
            const std::string finalStateName =
                blockInfo.blockEntityNbt.has_value()
                    ? std::string{blockInfo.blockEntityNbt->getString("final_state", "minecraft:air")}
                    : std::string{"minecraft:air"};
            const std::size_t open = finalStateName.find('[');
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> finalName =
                wire::common::Identifier::parse(finalStateName.substr(0, open));
            if (finalName.has_value() && finalName->path() == "structure_void") {
                continue;
            }
            stateId = parsedAndTransformedStateId(finalStateName, placement.mirror, placement.rotation);
        } else if (!processors.empty()) {
            const std::int32_t worldStateId =
                entry.processorPos.y >= bounds.minY() && entry.processorPos.y <= bounds.maxY()
                    ? chunk.blockStateId(processorLocalX, entry.processorPos.y, processorLocalZ)
                    : 0;
            const std::optional<std::int32_t> processedStateId =
                processors.processAt(blockInfo.stateId, worldStateId, entry.processorPos, placement.settingsRandom);
            if (!processedStateId.has_value()) {
                continue;
            }
            const std::map<std::string, std::string> properties =
                *processedStateId == blockInfo.stateId
                    ? blockInfo.properties
                    : registry::blockStateDefinitions::propertiesOf(*processedStateId);
            stateId = transformedStateId(*processedStateId, properties, placement.mirror, placement.rotation);
            // parity: a plain `RuleProcessor` rule hands its `append_loot` to the block it outputs.
            if (std::optional<std::string> loot =
                    processors.appendedLootAt(blockInfo.stateId, worldStateId, entry.processorPos);
                loot.has_value()) {
                ruleLoot[resolved.size()] = std::move(*loot);
            }
        } else {
            stateId = transformedStateId(blockInfo.stateId, blockInfo.properties, placement.mirror, placement.rotation);
        }
        if (placement.blockProcessor) {
            const std::int32_t worldStateId =
                chunk.blockStateId(processorLocalX, entry.processorPos.y, processorLocalZ);
            const std::optional<std::int32_t> processed =
                placement.blockProcessor(stateId, worldStateId, entry.processorPos);
            if (!processed.has_value()) {
                continue;
            }
            stateId = *processed;
        }
        resolved.push_back(Resolved{.block = &blockInfo, .worldPos = entry.placedPos, .stateId = stateId});
    }

    // Each capped pass sees what the previous one left, which is why the states are carried forward
    // rather than every pass reading the same original list.
    std::vector<std::int32_t> cappedStates;
    cappedStates.reserve(resolved.size());
    for (const Resolved& entry : resolved) {
        cappedStates.push_back(entry.stateId);
    }
    // The loot a capped rule hands its output, kept by index so the entity goes down with the block.
    std::map<std::size_t, std::string> cappedLoot = std::move(ruleLoot);
    if (placement.worldSeed.has_value()) {
        for (const CappedRules& capped : processors.capped()) {
            for (const auto& [index, rule] : capped.choose(cappedStates, *placement.worldSeed, placement.position)) {
                cappedStates[static_cast<std::size_t>(index)] = rule.outputStateId;
                if (rule.appendLootTable.has_value()) {
                    cappedLoot[static_cast<std::size_t>(index)] = *rule.appendLootTable;
                }
            }
        }
    }

    for (std::size_t index = 0; index < resolved.size(); ++index) {
        const Resolved& entry = resolved[index];
        const std::int32_t finalStateId = cappedStates[index];
        if (wholePiece && !placement.clipTo.isInside(entry.worldPos)) {
            continue;
        }
        if (entry.worldPos.y < bounds.minY() || entry.worldPos.y > bounds.maxY()) {
            continue;
        }
        if ((entry.worldPos.x >> 4) == chunk.chunkX() && (entry.worldPos.z >> 4) == chunk.chunkZ()) {
            chunk.setBlockStateId(entry.worldPos.x & 15, entry.worldPos.y, entry.worldPos.z & 15, finalStateId);
            // parity: `placeInWorld` ends by running `updateFromNeighbourShapes` over every block it
            // put down. The templates store a stair's `shape` as the modeller left it, so without
            // this a hull's whole flight keeps `outer_right` where the jar reads `straight`.
            if (registry::blockConnections::connects(finalStateId) ||
                registry::blockConnections::isStairs(finalStateId)) {
                chunk.markShapeUpdate(entry.worldPos.x, entry.worldPos.y, entry.worldPos.z);
            }
        } else {
            // A clip wider than the chunk is deliberate for the features that have one, so what
            // falls outside belongs to a neighbour and has to be deferred. Masking it back in
            // wrapped the far end of a fossil onto its own chunk's west edge.
            chunk.setDecorationBlockStateId(entry.worldPos.x, entry.worldPos.y, entry.worldPos.z, finalStateId,
                                            std::nullopt, true);
        }
        placeBlockEntityInto(chunk, *entry.block, finalStateId, entry.worldPos, placement.random, false);
        if (const auto loot = cappedLoot.find(index); loot != cappedLoot.end()) {
            placeAppendedLoot(chunk, finalStateId, entry.worldPos, loot->second, entry.block->blockEntityNbt);
        }
    }
    placeEntities(chunk, placement.position, placement.rotation, placement.mirror, placement.pivot, &placement.clipTo);
}

bool StructureTemplate::placeDecorationInto(Chunk& chunk, const StructurePos position, const StructureRotation rotation,
                                            random::RandomSource& random) const {
    if (size_.x < 1 || size_.y < 1 || size_.z < 1 || (blocks_.empty() && entities_.empty())) {
        return false;
    }
    // The palette draw happens even for vanilla's single retained palette.
    static_cast<void>(random.nextInt(1));
    const StructureMirror mirror = StructureMirror::None;
    const StructurePos pivot{0, 0, 0};
    const ChunkHeight bounds = chunk.verticalBounds();
    for (const StructureBlockInfo& blockInfo : blocks_) {
        const StructurePos worldPos = plus(transform(blockInfo.pos, mirror, rotation, pivot), position);
        if (worldPos.y < bounds.minY() || worldPos.y > bounds.maxY()) {
            continue;
        }
        const std::int32_t stateId = transformedStateId(blockInfo.stateId, blockInfo.properties, mirror, rotation);
        chunk.setDecorationBlockStateId(worldPos.x, worldPos.y, worldPos.z, stateId, std::nullopt, true);
        placeBlockEntityInto(chunk, blockInfo, stateId, worldPos, &random, true);
    }
    placeEntities(chunk, position, rotation, mirror, pivot, nullptr);
    return true;
}

} // namespace basalt::server::world::structure
