#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::region::chunkNbt {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr int kBlockMinIndirectBits = 4;
constexpr int kBlockMaxIndirectBits = 8;
constexpr int kBiomeMinIndirectBits = 1;
constexpr int kBiomeMaxIndirectBits = 3;
constexpr std::size_t kSectionBlockCount = 16 * 16 * 16;
constexpr std::size_t kSectionBiomeCount = 4 * 4 * 4;

constexpr std::array<std::string_view, 5> kBlockEntityMetadata = {"id", "x", "y", "z", "keepPacked"};

[[nodiscard]] int bitsFor(std::size_t distinctCount) {
    int bits = 0;
    while ((std::size_t{1} << bits) < distinctCount) {
        bits++;
    }
    return bits;
}

[[nodiscard]] int blockDirectBits() {
    static const int bits = bitsFor(static_cast<std::size_t>(registry::blocks::stateCount()));
    return bits;
}

[[nodiscard]] int biomeDirectBits() {
    static const int bits = bitsFor(registry::biomes::registry().size());
    return bits;
}

[[nodiscard]] std::vector<std::int32_t> boxToArray(const structure::StructureBoundingBox& box) {
    return {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
}

[[nodiscard]] std::optional<structure::StructureBoundingBox> arrayToBox(std::span<const std::int32_t> values) {
    if (values.size() != 6) {
        return std::nullopt;
    }
    return structure::StructureBoundingBox{values[0], values[1], values[2], values[3], values[4], values[5]};
}

/// Reused across a chunk's sections and owned by one write.
struct PaletteScratch {
    std::vector<std::int32_t> blockSlots;
    std::vector<std::int32_t> biomeSlots;
    std::vector<std::int32_t> paletteValues;
    std::vector<std::int32_t> blockValues;
    std::vector<std::int32_t> biomeValues;

    PaletteScratch()
        : blockSlots(static_cast<std::size_t>(registry::blocks::stateCount()), 0),
          biomeSlots(registry::biomes::registry().size(), 0), paletteValues(kSectionBlockCount, 0),
          blockValues(kSectionBlockCount, 0), biomeValues(kSectionBiomeCount, 0) {}
};

/// The palette is derived in first-appearance order through plain arrays.
template <typename ToNbt>
[[nodiscard]] NbtCompound encodePalette(std::span<const std::int32_t> values, int minIndirectBits, int maxIndirectBits,
                                        int directBits, NbtType elementType, std::span<std::int32_t> slotOfValue,
                                        std::span<std::int32_t> paletteValues, ToNbt&& elementToNbt) {
    std::size_t paletteSize = 0;
    for (const std::int32_t value : values) {
        if (slotOfValue[static_cast<std::size_t>(value)] != 0) {
            continue;
        }
        paletteValues[paletteSize] = value;
        paletteSize++;
        slotOfValue[static_cast<std::size_t>(value)] = static_cast<std::int32_t>(paletteSize);
    }

    std::vector<NbtTag> entries;
    entries.reserve(paletteSize);
    for (std::size_t i = 0; i < paletteSize; ++i) {
        entries.push_back(elementToNbt(paletteValues[i]));
    }

    NbtCompound tag;
    tag.set("palette", NbtTag::ofList(NbtList{elementType, std::move(entries)}));
    if (paletteSize > 1) {
        const int neededBits = bitsFor(paletteSize);
        const bool indirect = neededBits <= maxIndirectBits;
        const int bitsPerEntry = indirect ? std::max(neededBits, minIndirectBits) : directBits;
        wire::protocol::play::BitStorage storage{bitsPerEntry, values.size()};
        for (std::size_t i = 0; i < values.size(); ++i) {
            storage.set(i, indirect ? slotOfValue[static_cast<std::size_t>(values[i])] - 1 : values[i]);
        }
        std::vector<std::int64_t> words;
        words.reserve(storage.words().size());
        for (const std::uint64_t word : storage.words()) {
            words.push_back(static_cast<std::int64_t>(word));
        }
        tag.putLongArray("data", std::move(words));
    }

    for (std::size_t i = 0; i < paletteSize; ++i) {
        slotOfValue[static_cast<std::size_t>(paletteValues[i])] = 0;
    }
    return tag;
}

[[nodiscard]] std::vector<std::int32_t> decodePalette(std::span<const std::int32_t> paletteIds,
                                                      std::span<const std::int64_t> data, std::size_t entryCount,
                                                      int minIndirectBits, int maxIndirectBits, int directBits) {
    if (paletteIds.size() == 1) {
        return std::vector<std::int32_t>(entryCount, paletteIds[0]);
    }
    const int neededBits = bitsFor(paletteIds.size());
    const bool indirect = neededBits <= maxIndirectBits;
    const int bitsPerEntry = indirect ? std::max(neededBits, minIndirectBits) : directBits;
    std::vector<std::uint64_t> words;
    words.reserve(data.size());
    for (const std::int64_t word : data) {
        words.push_back(static_cast<std::uint64_t>(word));
    }
    const wire::protocol::play::BitStorage storage{bitsPerEntry, entryCount, std::move(words)};
    std::vector<std::int32_t> values;
    values.reserve(entryCount);
    for (std::size_t i = 0; i < entryCount; ++i) {
        const std::int32_t raw = storage.get(i);
        if (!indirect) {
            values.push_back(raw);
        } else if (raw >= 0 && static_cast<std::size_t>(raw) < paletteIds.size()) {
            values.push_back(paletteIds[static_cast<std::size_t>(raw)]);
        } else {
            values.push_back(paletteIds[0]);
        }
    }
    return values;
}

[[nodiscard]] NbtCompound encodeBlockStates(const ChunkSection& section, PaletteScratch& scratch) {
    section.copyBlockStateIdsInto(scratch.blockValues);
    return encodePalette(scratch.blockValues, kBlockMinIndirectBits, kBlockMaxIndirectBits, blockDirectBits(),
                         NbtType::Compound, scratch.blockSlots, scratch.paletteValues, [](std::int32_t stateId) {
                             // Every state, not only the ones some caller happened to register.
                             NbtCompound entry;
                             const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
                             entry.putString("id", block == nullptr ? "minecraft:air" : block->toString());
                             const std::map<std::string, std::string> properties =
                                 registry::blockStateDefinitions::propertiesOf(stateId);
                             if (!properties.empty()) {
                                 NbtCompound propertiesTag;
                                 for (const auto& [property, value] : properties) {
                                     propertiesTag.putString(property, value);
                                 }
                                 entry.set("properties", NbtTag::ofCompound(std::move(propertiesTag)));
                             }
                             return NbtTag::ofCompound(std::move(entry));
                         });
}

[[nodiscard]] NbtCompound encodeBiomes(const ChunkSection& section, PaletteScratch& scratch) {
    section.copyBiomeIdsInto(scratch.biomeValues);
    return encodePalette(scratch.biomeValues, kBiomeMinIndirectBits, kBiomeMaxIndirectBits, biomeDirectBits(),
                         NbtType::String, scratch.biomeSlots, scratch.paletteValues, [](std::int32_t biomeId) {
                             const Identifier* name = registry::biomes::registry().nameOf(biomeId);
                             return NbtTag::ofString(name == nullptr ? "minecraft:plains" : name->toString());
                         });
}

[[nodiscard]] NbtTag encodeTicks(const Chunk& chunk, std::span<const ChunkScheduledTick> ticks) {
    std::vector<NbtTag> entries;
    entries.reserve(ticks.size());
    for (const ChunkScheduledTick& tick : ticks) {
        NbtCompound tag;
        tag.putString("i", tick.type.toString());
        tag.putInt("x", chunk.chunkX() * 16 + tick.localX);
        tag.putInt("y", tick.y);
        tag.putInt("z", chunk.chunkZ() * 16 + tick.localZ);
        tag.putInt("t", tick.delay);
        tag.putInt("p", tick.priority);
        entries.push_back(NbtTag::ofCompound(std::move(tag)));
    }
    const NbtType type = entries.empty() ? NbtType::End : NbtType::Compound;
    return NbtTag::ofList(NbtList{type, std::move(entries)});
}

[[nodiscard]] NbtTag encodeGeneratedEntity(const ChunkGeneratedEntity& entity) {
    NbtCompound tag;
    tag.putString("id", entity.type.toString());
    NbtList pos{NbtType::Double, {}};
    pos.push(NbtTag::ofDouble(entity.x));
    pos.push(NbtTag::ofDouble(entity.y));
    pos.push(NbtTag::ofDouble(entity.z));
    tag.set("Pos", NbtTag::ofList(std::move(pos)));
    NbtList rotation{NbtType::Float, {}};
    rotation.push(NbtTag::ofFloat(entity.yaw));
    rotation.push(NbtTag::ofFloat(entity.pitch));
    tag.set("Rotation", NbtTag::ofList(std::move(rotation)));
    tag.set("BasaltData", NbtTag::ofCompound(entity.data));
    if (!entity.passengers.empty()) {
        std::vector<NbtTag> passengers;
        passengers.reserve(entity.passengers.size());
        for (const ChunkGeneratedEntity& passenger : entity.passengers) {
            passengers.push_back(encodeGeneratedEntity(passenger));
        }
        tag.set("Passengers", NbtTag::ofList(NbtList{NbtType::Compound, std::move(passengers)}));
    }
    return NbtTag::ofCompound(std::move(tag));
}

[[nodiscard]] NbtCompound encodeStructures(const structure::ChunkStructures& structures) {
    std::vector<NbtTag> starts;
    for (const structure::StructureStartRecord& start : structures.starts()) {
        NbtCompound startTag;
        startTag.putString("id", Identifier::vanilla(start.structure).toString());
        startTag.putInt("ChunkX", start.chunkX);
        startTag.putInt("ChunkZ", start.chunkZ);
        startTag.putIntArray("BB", boxToArray(start.fullBounds));
        std::vector<NbtTag> children;
        children.reserve(start.pieces.size());
        for (const structure::StructureBoundingBox& piece : start.pieces) {
            NbtCompound child;
            child.putIntArray("BB", boxToArray(piece));
            children.push_back(NbtTag::ofCompound(std::move(child)));
        }
        const NbtType childType = children.empty() ? NbtType::End : NbtType::Compound;
        startTag.set("Children", NbtTag::ofList(NbtList{childType, std::move(children)}));
        starts.push_back(NbtTag::ofCompound(std::move(startTag)));
    }
    NbtCompound out;
    const NbtType type = starts.empty() ? NbtType::End : NbtType::Compound;
    out.set("starts", NbtTag::ofList(NbtList{type, std::move(starts)}));
    return out;
}

} // namespace

namespace {

/// parity: the `ChunkStatus` names.
[[nodiscard]] std::string_view statusName(ChunkStage stage) {
    switch (stage) {
    case ChunkStage::Terrain:
        return "minecraft:terrain";
    case ChunkStage::Decorated:
        return "minecraft:features";
    case ChunkStage::Full:
    default:
        return "minecraft:full";
    }
}

[[nodiscard]] std::optional<ChunkStage> stageOf(std::string_view status) {
    if (status.empty() || status == "minecraft:full" || status == "full") {
        return ChunkStage::Full;
    }
    if (status.starts_with("minecraft:")) {
        status.remove_prefix(10);
    }
    if (status == "terrain") {
        return ChunkStage::Terrain;
    }
    if (status == "features" || status == "initialize_light" || status == "light" || status == "spawn") {
        return ChunkStage::Decorated;
    }
    return std::nullopt;
}

} // namespace

NbtCompound write(const Chunk& chunk) {
    NbtCompound tag;
    tag.putInt("DataVersion", kDataVersion);
    tag.putInt("xPos", chunk.chunkX());
    tag.putInt("yPos", chunk.verticalBounds().minSection());
    tag.putInt("zPos", chunk.chunkZ());
    tag.putLong("LastUpdate", 0);
    tag.putLong("InhabitedTime", chunk.inhabitedTime());
    tag.putString("Status", std::string{statusName(chunk.stage())});

    PaletteScratch scratch;
    std::vector<NbtTag> sections;
    sections.reserve(chunk.sectionCount());
    for (std::size_t i = 0; i < chunk.sectionCount(); ++i) {
        NbtCompound sectionTag;
        sectionTag.putByte(
            "Y", static_cast<std::int8_t>(chunk.verticalBounds().minSection() + static_cast<std::int32_t>(i)));
        sectionTag.set("block_states", NbtTag::ofCompound(encodeBlockStates(chunk.section(i), scratch)));
        sectionTag.set("biomes", NbtTag::ofCompound(encodeBiomes(chunk.section(i), scratch)));
        sections.push_back(NbtTag::ofCompound(std::move(sectionTag)));
    }
    tag.set("sections", NbtTag::ofList(NbtList{NbtType::Compound, std::move(sections)}));

    std::vector<NbtTag> blockEntities;
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        NbtCompound entityTag;
        for (std::size_t i = 0; i < entity.data().size(); ++i) {
            entityTag.set(entity.data().names()[i], entity.data().values()[i]);
        }
        entityTag.putString("id", entity.type().toString());
        entityTag.putInt("x", chunk.chunkX() * 16 + entity.localX());
        entityTag.putInt("y", entity.y());
        entityTag.putInt("z", chunk.chunkZ() * 16 + entity.localZ());
        blockEntities.push_back(NbtTag::ofCompound(std::move(entityTag)));
    }
    const NbtType blockEntityType = blockEntities.empty() ? NbtType::End : NbtType::Compound;
    tag.set("block_entities", NbtTag::ofList(NbtList{blockEntityType, std::move(blockEntities)}));
    tag.set("block_ticks", encodeTicks(chunk, chunk.scheduledBlockTicks()));
    tag.set("fluid_ticks", encodeTicks(chunk, chunk.scheduledFluidTicks()));

    if (chunk.stage() != ChunkStage::Full) {
        std::vector<std::vector<NbtTag>> perSection(chunk.sectionCount());
        for (const std::span<const ChunkShapeUpdate> marks : {chunk.fluidUpdates(), chunk.shapeUpdates()}) {
            for (const ChunkShapeUpdate& mark : marks) {
                const std::int32_t section = (mark.y >> 4) - chunk.verticalBounds().minSection();
                if ((mark.x >> 4) != chunk.chunkX() || (mark.z >> 4) != chunk.chunkZ() || section < 0 ||
                    static_cast<std::size_t>(section) >= perSection.size()) {
                    continue;
                }
                perSection[static_cast<std::size_t>(section)].push_back(NbtTag::ofShort(
                    static_cast<std::int16_t>((mark.x & 15) | ((mark.z & 15) << 4) | ((mark.y & 15) << 8))));
            }
        }
        std::vector<NbtTag> post;
        post.reserve(perSection.size());
        for (std::vector<NbtTag>& offsets : perSection) {
            const NbtType offsetType = offsets.empty() ? NbtType::End : NbtType::Short;
            post.push_back(NbtTag::ofList(NbtList{offsetType, std::move(offsets)}));
        }
        tag.set("PostProcessing", NbtTag::ofList(NbtList{NbtType::List, std::move(post)}));
    }

    std::vector<NbtTag> generated;
    generated.reserve(chunk.generatedEntities().size());
    for (const ChunkGeneratedEntity& entity : chunk.generatedEntities()) {
        generated.push_back(encodeGeneratedEntity(entity));
    }
    const NbtType generatedType = generated.empty() ? NbtType::End : NbtType::Compound;
    tag.set(kGeneratedEntitiesKey, NbtTag::ofList(NbtList{generatedType, std::move(generated)}));

    std::vector<NbtTag> entities;
    entities.reserve(chunk.savedEntities().size());
    for (const NbtCompound& entity : chunk.savedEntities()) {
        entities.push_back(NbtTag::ofCompound(entity));
    }
    const NbtType entityType = entities.empty() ? NbtType::End : NbtType::Compound;
    tag.set(kEntitiesKey, NbtTag::ofList(NbtList{entityType, std::move(entities)}));

    if (!chunk.structures().empty()) {
        tag.set("structures", NbtTag::ofCompound(encodeStructures(chunk.structures())));
    }
    return tag;
}

namespace {

[[nodiscard]] std::expected<std::vector<std::int32_t>, ChunkNbtError> decodeBlockStates(const NbtCompound& tag) {
    const NbtList* paletteList = tag.getList("palette");
    if (paletteList == nullptr) {
        return std::unexpected(ChunkNbtError{"missing block_states palette"});
    }
    std::vector<std::int32_t> paletteIds;
    paletteIds.reserve(paletteList->size());
    for (const NbtTag& entry : *paletteList) {
        // parity: `BlockState.CODEC`.
        const NbtCompound* entryTag = entry.get<NbtCompound>();
        const std::string* bare = entry.get<std::string>();
        if (entryTag != nullptr && entryTag->size() == 1 && entryTag->names()[0].empty()) {
            bare = entryTag->values()[0].get<std::string>();
            entryTag = nullptr;
        }
        std::string name = bare != nullptr ? *bare : std::string{};
        const NbtCompound* propertiesTag = nullptr;
        if (entryTag != nullptr) {
            const bool legacy = entryTag->contains("Name");
            name = std::string{entryTag->getString(legacy ? "Name" : "id")};
            propertiesTag = entryTag->getCompound(legacy ? "Properties" : "properties");
        }
        if (name.empty()) {
            return std::unexpected(ChunkNbtError{"invalid block palette entry"});
        }
        std::map<std::string, std::string> properties;
        if (propertiesTag != nullptr) {
            for (std::size_t i = 0; i < propertiesTag->size(); ++i) {
                const auto* value = propertiesTag->values()[i].get<std::string>();
                if (value == nullptr) {
                    return std::unexpected(ChunkNbtError{
                        std::format("invalid property {} for block {}", propertiesTag->names()[i], name)});
                }
                properties.emplace(propertiesTag->names()[i], *value);
            }
        }
        const std::expected<Identifier, wire::common::IdentifierError> block = Identifier::parse(name);
        if (!block.has_value()) {
            return std::unexpected(ChunkNbtError{std::format("invalid block name {}", name)});
        }
        // parity: `BlockState.CODEC`, where whatever the entry leaves out keeps the block's default.
        std::optional<std::int32_t> stateId =
            registry::blockStateDefinitions::stateIdOf(*block, properties, registry::blocks::defaultStateIdOf(*block));
        if (!stateId.has_value()) {
            stateId = registry::blocks::stateIdOf(*block, properties);
        }
        if (!stateId.has_value()) {
            return std::unexpected(ChunkNbtError{std::format("unknown block state {}", name)});
        }
        paletteIds.push_back(*stateId);
    }
    if (paletteIds.empty()) {
        return std::unexpected(ChunkNbtError{"empty block_states palette"});
    }
    return decodePalette(paletteIds, tag.getLongArray("data"), kSectionBlockCount, kBlockMinIndirectBits,
                         kBlockMaxIndirectBits, blockDirectBits());
}

[[nodiscard]] std::expected<std::vector<std::int32_t>, ChunkNbtError> decodeBiomes(const NbtCompound& tag) {
    const NbtList* paletteList = tag.getList("palette");
    if (paletteList == nullptr) {
        return std::unexpected(ChunkNbtError{"missing biomes palette"});
    }
    std::vector<std::int32_t> paletteIds;
    paletteIds.reserve(paletteList->size());
    for (const NbtTag& entry : *paletteList) {
        const auto* name = entry.get<std::string>();
        if (name == nullptr) {
            return std::unexpected(ChunkNbtError{"invalid biome palette entry"});
        }
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
        if (!id.has_value()) {
            return std::unexpected(ChunkNbtError{std::format("invalid biome name {}", *name)});
        }
        const std::optional<std::int32_t> biomeId = registry::biomes::registry().idOf(*id);
        if (!biomeId.has_value()) {
            return std::unexpected(ChunkNbtError{std::format("unknown biome {}", *name)});
        }
        paletteIds.push_back(*biomeId);
    }
    if (paletteIds.empty()) {
        return std::unexpected(ChunkNbtError{"empty biomes palette"});
    }
    return decodePalette(paletteIds, tag.getLongArray("data"), kSectionBiomeCount, kBiomeMinIndirectBits,
                         kBiomeMaxIndirectBits, biomeDirectBits());
}

[[nodiscard]] std::expected<ChunkGeneratedEntity, ChunkNbtError> decodeGeneratedEntity(const NbtCompound& tag) {
    const std::string_view type = tag.getString("id");
    if (type.empty()) {
        return std::unexpected(ChunkNbtError{"generated entity has no id"});
    }
    const NbtList* pos = tag.getList("Pos");
    const NbtList* rotation = tag.getList("Rotation");
    if (pos == nullptr || rotation == nullptr || pos->size() != 3 || rotation->size() != 2) {
        return std::unexpected(ChunkNbtError{std::format("generated entity {} has invalid vectors", type)});
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(type);
    if (!id.has_value()) {
        return std::unexpected(ChunkNbtError{std::format("invalid generated entity id {}", type)});
    }
    ChunkGeneratedEntity entity;
    entity.type = *id;
    const auto* x = (*pos)[0].get<double>();
    const auto* y = (*pos)[1].get<double>();
    const auto* z = (*pos)[2].get<double>();
    const auto* yaw = (*rotation)[0].get<float>();
    const auto* pitch = (*rotation)[1].get<float>();
    if (x == nullptr || y == nullptr || z == nullptr || yaw == nullptr || pitch == nullptr) {
        return std::unexpected(ChunkNbtError{std::format("generated entity {} has invalid vectors", type)});
    }
    entity.x = *x;
    entity.y = *y;
    entity.z = *z;
    entity.yaw = *yaw;
    entity.pitch = *pitch;
    if (const NbtCompound* data = tag.getCompound("BasaltData")) {
        entity.data = *data;
    }
    if (const NbtList* passengers = tag.getList("Passengers")) {
        for (const NbtTag& passenger : *passengers) {
            const NbtCompound* passengerTag = passenger.get<NbtCompound>();
            if (passengerTag == nullptr) {
                return std::unexpected(ChunkNbtError{std::format("generated passenger of {} is not a compound", type)});
            }
            const std::expected<ChunkGeneratedEntity, ChunkNbtError> decoded = decodeGeneratedEntity(*passengerTag);
            if (!decoded.has_value()) {
                return std::unexpected(decoded.error());
            }
            entity.passengers.push_back(*decoded);
        }
    }
    return entity;
}

void decodeStructureStart(const NbtTag& entry, std::vector<structure::StructureStartRecord>& into) {
    const NbtCompound* startTag = entry.get<NbtCompound>();
    if (startTag == nullptr) {
        return;
    }
    std::vector<structure::StructureBoundingBox> pieces;
    if (const NbtList* children = startTag->getList("Children")) {
        for (const NbtTag& child : *children) {
            const NbtCompound* childTag = child.get<NbtCompound>();
            if (childTag == nullptr) {
                continue;
            }
            if (const std::optional<structure::StructureBoundingBox> piece = arrayToBox(childTag->getIntArray("BB"))) {
                pieces.push_back(*piece);
            }
        }
    }
    std::optional<structure::StructureBoundingBox> bounds = arrayToBox(startTag->getIntArray("BB"));
    if (!bounds.has_value()) {
        // parity: `StructureStart.createTag` never writes the start's own box.
        for (const structure::StructureBoundingBox& piece : pieces) {
            bounds = bounds.has_value() ? bounds->encapsulating(piece) : piece;
        }
    }
    if (!bounds.has_value()) {
        return;
    }
    // parity: a start that found no place writes itself as `INVALID` rather than being left out.
    if (startTag->getString("id") == "INVALID") {
        return;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(startTag->getString("id"));
    if (!id.has_value()) {
        return;
    }
    into.push_back({id->path(), startTag->getInt("ChunkX"), startTag->getInt("ChunkZ"), *bounds, std::move(pieces)});
}

void decodeStructures(const NbtCompound* tag, structure::ChunkStructures& into) {
    if (tag == nullptr) {
        return;
    }
    std::vector<structure::StructureStartRecord> records;
    if (const NbtList* starts = tag->getList("starts"); starts != nullptr) {
        for (const NbtTag& entry : *starts) {
            decodeStructureStart(entry, records);
        }
    } else if (const NbtCompound* byName = tag->getCompound("starts"); byName != nullptr) {
        for (const NbtTag& entry : byName->values()) {
            decodeStructureStart(entry, records);
        }
    }
    into.restore(std::move(records));
}

} // namespace

std::expected<Chunk, ChunkNbtError> read(const NbtCompound& tag, ChunkHeight bounds) {
    const std::int32_t saved = tag.getInt("DataVersion");
    if (saved > kDataVersion) {
        return std::unexpected(ChunkNbtError{std::format(
            "saved by a newer version of the game (data version {}, this build reads up to {})", saved, kDataVersion)});
    }
    if (saved > 0 && saved < kDataVersion) {
        NbtCompound upgraded = tag;
        server::datafix::upgrade(server::datafix::Domain::Chunk, upgraded, saved, {.overworld = bounds.minY() == -64});
        upgraded.putInt("DataVersion", kDataVersion);
        return read(upgraded, bounds);
    }
    const std::optional<ChunkStage> stage = stageOf(tag.getString("Status"));
    if (!stage.has_value()) {
        return std::unexpected(ChunkNbtError{"saved before it had terrain", false});
    }
    Chunk chunk{tag.getInt("xPos"), tag.getInt("zPos"), bounds};
    chunk.setStage(*stage);
    if (*stage == ChunkStage::Full) {
        chunk.markLoadedComplete();
    }
    chunk.restoreInhabitedTime(tag.getLong("InhabitedTime"));

    const NbtList* sections = tag.getList("sections");
    if (sections == nullptr) {
        return std::unexpected(ChunkNbtError{"missing sections list"});
    }
    for (const NbtTag& entry : *sections) {
        const NbtCompound* sectionTag = entry.get<NbtCompound>();
        if (sectionTag == nullptr) {
            return std::unexpected(ChunkNbtError{"section entry is not a compound"});
        }
        const std::int32_t y = sectionTag->getByte("Y");
        const std::int32_t index = y - bounds.minSection();
        if (index < 0 || static_cast<std::size_t>(index) >= chunk.sectionCount()) {
            continue;
        }
        const NbtCompound* blockStates = sectionTag->getCompound("block_states");
        if (blockStates != nullptr) {
            const std::expected<std::vector<std::int32_t>, ChunkNbtError> ids = decodeBlockStates(*blockStates);
            if (!ids.has_value()) {
                return std::unexpected(ids.error());
            }
            chunk.section(static_cast<std::size_t>(index)).setBlockStateIds(*ids);
        }
        const NbtCompound* biomes = sectionTag->getCompound("biomes");
        if (biomes != nullptr) {
            const std::expected<std::vector<std::int32_t>, ChunkNbtError> ids = decodeBiomes(*biomes);
            if (!ids.has_value()) {
                return std::unexpected(ids.error());
            }
            chunk.section(static_cast<std::size_t>(index)).setBiomeIds(*ids);
        }
    }
    // The sections were filled behind the chunk's back, so its heightmaps mean nothing yet.
    chunk.invalidateHeightmaps();

    if (const NbtList* blockEntities = tag.getList("block_entities")) {
        for (const NbtTag& entry : *blockEntities) {
            const NbtCompound* entityTag = entry.get<NbtCompound>();
            if (entityTag == nullptr) {
                return std::unexpected(ChunkNbtError{"block entity entry is not a compound"});
            }
            const std::string_view id = entityTag->getString("id");
            if (id.empty()) {
                return std::unexpected(ChunkNbtError{"block entity has no id"});
            }
            const std::int32_t worldX = entityTag->getInt("x");
            const std::int32_t y = entityTag->getInt("y");
            const std::int32_t worldZ = entityTag->getInt("z");
            const std::int32_t localX = worldX - chunk.chunkX() * 16;
            const std::int32_t localZ = worldZ - chunk.chunkZ() * 16;
            if (!isInsideChunk(localX, y, localZ, bounds)) {
                return std::unexpected(ChunkNbtError{
                    std::format("block entity {} at ({},{},{}) is outside its chunk", id, worldX, y, worldZ)});
            }
            const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(id);
            if (!type.has_value()) {
                return std::unexpected(ChunkNbtError{std::format("invalid block entity id {}", id)});
            }
            NbtCompound data;
            for (std::size_t i = 0; i < entityTag->size(); ++i) {
                const std::string& name = entityTag->names()[i];
                if (std::ranges::find(kBlockEntityMetadata, name) == kBlockEntityMetadata.end()) {
                    data.set(name, entityTag->values()[i]);
                }
            }
            chunk.setBlockEntity(ChunkBlockEntity{localX, y, localZ, *type, std::move(data)});
        }
    }

    for (const auto& [key, isFluid] :
         {std::pair{std::string_view{"block_ticks"}, false}, std::pair{std::string_view{"fluid_ticks"}, true}}) {
        const NbtList* ticks = tag.getList(key);
        if (ticks == nullptr) {
            continue;
        }
        for (const NbtTag& entry : *ticks) {
            const NbtCompound* tickTag = entry.get<NbtCompound>();
            if (tickTag == nullptr) {
                return std::unexpected(ChunkNbtError{"scheduled tick entry is not a compound"});
            }
            const std::string_view type = tickTag->getString("i");
            if (type.empty()) {
                return std::unexpected(ChunkNbtError{"scheduled tick has no id"});
            }
            const std::int32_t worldX = tickTag->getInt("x");
            const std::int32_t y = tickTag->getInt("y");
            const std::int32_t worldZ = tickTag->getInt("z");
            const std::int32_t localX = worldX - chunk.chunkX() * 16;
            const std::int32_t localZ = worldZ - chunk.chunkZ() * 16;
            if (!isInsideChunk(localX, y, localZ, bounds)) {
                return std::unexpected(ChunkNbtError{
                    std::format("scheduled tick {} at ({},{},{}) is outside its chunk", type, worldX, y, worldZ)});
            }
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(type);
            if (!id.has_value()) {
                return std::unexpected(ChunkNbtError{std::format("invalid scheduled tick id {}", type)});
            }
            const ChunkScheduledTick tick{
                *id, localX, y, localZ, tickTag->getInt("t"), std::clamp(tickTag->getInt("p"), -3, 3)};
            if (isFluid) {
                chunk.scheduleFluidTick(tick);
            } else {
                chunk.scheduleBlockTick(tick);
            }
        }
    }

    if (const NbtList* generated = tag.getList(kGeneratedEntitiesKey)) {
        for (const NbtTag& entry : *generated) {
            const NbtCompound* entityTag = entry.get<NbtCompound>();
            if (entityTag == nullptr) {
                return std::unexpected(ChunkNbtError{"generated entity entry is not a compound"});
            }
            const std::expected<ChunkGeneratedEntity, ChunkNbtError> entity = decodeGeneratedEntity(*entityTag);
            if (!entity.has_value()) {
                return std::unexpected(entity.error());
            }
            chunk.addGeneratedEntity(*entity);
        }
    }

    std::vector<NbtCompound> entities;
    if (const NbtList* saved = tag.getList(kEntitiesKey)) {
        for (const NbtTag& entry : *saved) {
            if (const NbtCompound* entity = entry.get<NbtCompound>()) {
                entities.push_back(*entity);
            }
        }
    }
    chunk.setSavedEntities(std::move(entities));
    decodeStructures(tag.getCompound("structures"), chunk.structures());
    // parity: `PostProcessing`.
    if (const NbtList* post = tag.getList("PostProcessing"); post != nullptr) {
        for (std::size_t index = 0; index < post->size(); ++index) {
            const NbtList* offsets = (*post)[index].get<NbtList>();
            if (offsets == nullptr) {
                continue;
            }
            const std::int32_t baseY = (bounds.minSection() + static_cast<std::int32_t>(index)) * 16;
            for (const NbtTag& entry : *offsets) {
                const std::int16_t* packed = entry.get<std::int16_t>();
                if (packed == nullptr) {
                    continue;
                }
                const auto offset = static_cast<std::uint16_t>(*packed);
                chunk.markShapeUpdate((chunk.chunkX() * 16) + (offset & 15), baseY + ((offset >> 8) & 15),
                                      (chunk.chunkZ() * 16) + ((offset >> 4) & 15));
            }
        }
    }
    // Straight off disk, so nothing here is newer than the file it came from.
    chunk.markClean();
    return chunk;
}

} // namespace basalt::server::world::region::chunkNbt
