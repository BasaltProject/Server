#include <basalt/server/world/region/ChunkNbtParts.hpp>

#include <format>
#include <optional>
#include <span>
#include <string>

namespace basalt::server::world::region::chunkNbt {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

namespace {

[[nodiscard]] std::vector<std::int32_t> boxToArray(const structure::StructureBoundingBox& box) {
    return {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
}

[[nodiscard]] std::optional<structure::StructureBoundingBox> arrayToBox(std::span<const std::int32_t> values) {
    if (values.size() != 6) {
        return std::nullopt;
    }
    return structure::StructureBoundingBox{values[0], values[1], values[2], values[3], values[4], values[5]};
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

} // namespace

NbtTag encodeTicks(const Chunk& chunk, std::span<const ChunkScheduledTick> ticks) {
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

NbtTag encodeGeneratedEntity(const ChunkGeneratedEntity& entity) {
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

NbtCompound encodeStructures(const structure::ChunkStructures& structures) {
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

std::expected<ChunkGeneratedEntity, ChunkNbtError> decodeGeneratedEntity(const NbtCompound& tag) {
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

} // namespace basalt::server::world::region::chunkNbt
