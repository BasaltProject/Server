#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/StructureBlocks.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

namespace basalt::server::runtime::world::structureBlocks {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using Pos = std::array<std::int32_t, 3>;

constexpr std::array<std::string_view, 4> kModes{"SAVE", "LOAD", "CORNER", "DATA"};
constexpr std::array<std::string_view, 4> kModeProperties{"save", "load", "corner", "data"};
constexpr std::array<std::string_view, 4> kRotations{"NONE", "CLOCKWISE_90", "CLOCKWISE_180", "COUNTERCLOCKWISE_90"};
constexpr std::array<std::string_view, 3> kMirrors{"NONE", "LEFT_RIGHT", "FRONT_BACK"};

template <std::size_t N>
[[nodiscard]] std::optional<std::int32_t> indexOf(const std::array<std::string_view, N>& names, std::string_view name) {
    const auto found = std::ranges::find(names, name);
    return found == names.end() ? std::nullopt : std::optional{static_cast<std::int32_t>(found - names.begin())};
}

[[nodiscard]] NbtTag ints(const Pos& values) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Int};
    for (const std::int32_t value : values) {
        list.push(NbtTag::ofInt(value));
    }
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] NbtTag doubles(const std::array<double, 3>& values) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Double};
    for (const double value : values) {
        list.push(NbtTag::ofDouble(value));
    }
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] NbtTag compounds(std::vector<NbtTag> values) {
    return NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(values)});
}

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : block->path();
}

/// parity: `Block.hasDynamicShape`, set on these and the shulker boxes, whose shape is not their own.
[[nodiscard]] bool dynamicShape(std::string_view path) {
    static constexpr std::array<std::string_view, 6> kDynamic{
        "moving_piston", "bamboo", "scaffolding", "powder_snow", "pointed_dripstone", "sulfur_spike"};
    return std::ranges::find(kDynamic, path) != kDynamic.end() || path.ends_with("shulker_box");
}

/// parity: `NbtUtils.writeBlockState`.
[[nodiscard]] NbtCompound blockStateTag(std::int32_t stateId) {
    NbtCompound state;
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    state.putString("Name", block == nullptr ? "minecraft:air" : block->toString());
    const std::map<std::string, std::string> properties =
        server::registry::blockStateDefinitions::propertiesOf(stateId);
    if (!properties.empty()) {
        NbtCompound values;
        for (const auto& [key, value] : properties) {
            values.putString(key, value);
        }
        state.set("Properties", NbtTag::ofCompound(std::move(values)));
    }
    return state;
}

struct Captured {
    Pos pos{};
    std::int32_t stateId = 0;
    std::optional<NbtCompound> nbt;
};

} // namespace

Settings read(const NbtCompound& data) {
    Settings settings;
    settings.name = std::string{data.getString("name")};
    settings.author = std::string{data.getString("author")};
    settings.metadata = std::string{data.getString("metadata")};
    const auto number = [&data](std::string_view key, std::int32_t fallback) {
        return data.contains(key) ? data.getInt(key) : fallback;
    };
    settings.offset = {std::clamp(number("posX", 0), -kMaxOffset, kMaxOffset),
                       std::clamp(number("posY", 1), -kMaxOffset, kMaxOffset),
                       std::clamp(number("posZ", 0), -kMaxOffset, kMaxOffset)};
    settings.size = {std::clamp(number("sizeX", 0), 0, kMaxSize), std::clamp(number("sizeY", 0), 0, kMaxSize),
                     std::clamp(number("sizeZ", 0), 0, kMaxSize)};
    settings.rotation = indexOf(kRotations, data.getString("rotation")).value_or(0);
    settings.mirror = indexOf(kMirrors, data.getString("mirror")).value_or(0);
    settings.mode = static_cast<Mode>(indexOf(kModes, data.getString("mode")).value_or(3));
    settings.ignoreEntities = data.contains("ignoreEntities") ? data.getBoolean("ignoreEntities") : true;
    settings.strict = data.getBoolean("strict");
    settings.powered = data.getBoolean("powered");
    settings.showAir = data.getBoolean("showair");
    settings.showBoundingBox = data.contains("showboundingbox") ? data.getBoolean("showboundingbox") : true;
    settings.integrity = data.contains("integrity") ? data.getFloat("integrity") : 1.0F;
    settings.seed = data.getLong("seed");
    return settings;
}

void write(const Settings& settings, NbtCompound& data) {
    data.putString("name", settings.name);
    data.putString("author", settings.author);
    data.putString("metadata", settings.metadata);
    data.putInt("posX", settings.offset[0]);
    data.putInt("posY", settings.offset[1]);
    data.putInt("posZ", settings.offset[2]);
    data.putInt("sizeX", settings.size[0]);
    data.putInt("sizeY", settings.size[1]);
    data.putInt("sizeZ", settings.size[2]);
    data.putString("rotation", std::string{kRotations[static_cast<std::size_t>(settings.rotation)]});
    data.putString("mirror", std::string{kMirrors[static_cast<std::size_t>(settings.mirror)]});
    data.putString("mode", std::string{modeName(settings.mode)});
    data.putBoolean("ignoreEntities", settings.ignoreEntities);
    data.putBoolean("strict", settings.strict);
    data.putBoolean("powered", settings.powered);
    data.putBoolean("showair", settings.showAir);
    data.putBoolean("showboundingbox", settings.showBoundingBox);
    data.putFloat("integrity", settings.integrity);
    data.putLong("seed", settings.seed);
}

std::string_view modeName(Mode mode) { return kModes[static_cast<std::size_t>(mode)]; }

std::string_view modeProperty(Mode mode) { return kModeProperties[static_cast<std::size_t>(mode)]; }

std::optional<std::string> resolvedName(std::string_view name) {
    if (name.empty()) {
        return std::nullopt;
    }
    const auto parsed = wire::common::Identifier::parse(name);
    return parsed.has_value() ? std::optional{parsed->toString()} : std::nullopt;
}

std::optional<Box> enclosingBox(WorldState& world, const Pos& self, const std::string& name) {
    std::int32_t corners = 0;
    Box box;
    for (std::int32_t chunkX = (self[0] - kCornerRange) >> 4; chunkX <= (self[0] + kCornerRange) >> 4; ++chunkX) {
        for (std::int32_t chunkZ = (self[2] - kCornerRange) >> 4; chunkZ <= (self[2] + kCornerRange) >> 4; ++chunkZ) {
            const server::world::Chunk* chunk = world.chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                const Pos at{(chunkX << 4) + entity.localX(), entity.y(), (chunkZ << 4) + entity.localZ()};
                if (entity.type().path() != "structure_block" || std::abs(at[0] - self[0]) > kCornerRange ||
                    std::abs(at[2] - self[2]) > kCornerRange) {
                    continue;
                }
                const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
                const Settings corner = read(entity.data());
                if (!state.has_value() || pathOf(*state) != "structure_block" || corner.mode != Mode::Corner ||
                    resolvedName(corner.name) != resolvedName(name)) {
                    continue;
                }
                if (corners++ == 0) {
                    box = Box{at, at};
                } else {
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        box.min[axis] = std::min(box.min[axis], at[axis]);
                        box.max[axis] = std::max(box.max[axis], at[axis]);
                    }
                }
            }
        }
    }
    if (corners == 0) {
        return std::nullopt;
    }
    if (corners == 1) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            box.min[axis] = std::min(box.min[axis], self[axis]);
            box.max[axis] = std::max(box.max[axis], self[axis]);
        }
    }
    return box;
}

NbtCompound capture(WorldState& world, const Pos& origin, const Pos& size, bool withEntities) {
    NbtCompound tag;
    std::vector<Captured> full;
    std::vector<Captured> other;
    std::vector<Captured> withData;
    const Pos corner{origin[0] + size[0] - 1, origin[1] + size[1] - 1, origin[2] + size[2] - 1};
    const Pos min{std::min(origin[0], corner[0]), std::min(origin[1], corner[1]), std::min(origin[2], corner[2])};
    const Pos max{std::max(origin[0], corner[0]), std::max(origin[1], corner[1]), std::max(origin[2], corner[2])};
    if (size[0] >= 1 && size[1] >= 1 && size[2] >= 1) {
        for (std::int32_t y = min[1]; y <= max[1]; ++y) {
            for (std::int32_t x = min[0]; x <= max[0]; ++x) {
                for (std::int32_t z = min[2]; z <= max[2]; ++z) {
                    const std::optional<std::int32_t> state = world.blockState(x, y, z);
                    if (!state.has_value() || pathOf(*state) == "structure_void") {
                        continue;
                    }
                    Captured info{.pos = {x - min[0], y - min[1], z - min[2]}, .stateId = *state};
                    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
                    if (const server::world::ChunkBlockEntity* entity =
                            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
                        entity != nullptr) {
                        // parity: `BlockEntity.saveWithId`: its own fields and its id, no position.
                        NbtCompound saved = entity->data();
                        static_cast<void>(saved.remove("x"));
                        static_cast<void>(saved.remove("y"));
                        static_cast<void>(saved.remove("z"));
                        saved.putString("id", entity->type().toString());
                        info.nbt = std::move(saved);
                        withData.push_back(std::move(info));
                    } else if (!dynamicShape(pathOf(*state)) && server::registry::blockShapes::isFullCube(*state)) {
                        full.push_back(std::move(info));
                    } else {
                        other.push_back(std::move(info));
                    }
                }
            }
        }
    }
    // The walk is y, x, z already, which is the order each list is sorted in.
    std::vector<Captured> blocks = std::move(full);
    blocks.insert(blocks.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    blocks.insert(blocks.end(), std::make_move_iterator(withData.begin()), std::make_move_iterator(withData.end()));
    // parity: `SimplePalette.idFor`, numbered by first appearance.
    std::vector<std::int32_t> palette;
    std::vector<NbtTag> blockList;
    for (const Captured& info : blocks) {
        auto found = std::ranges::find(palette, info.stateId);
        if (found == palette.end()) {
            palette.push_back(info.stateId);
            found = palette.end() - 1;
        }
        NbtCompound block;
        block.set("pos", ints(info.pos));
        block.putInt("state", static_cast<std::int32_t>(found - palette.begin()));
        if (info.nbt.has_value()) {
            block.set("nbt", NbtTag::ofCompound(*info.nbt));
        }
        blockList.push_back(NbtTag::ofCompound(std::move(block)));
    }
    tag.set("blocks", compounds(std::move(blockList)));
    std::vector<NbtTag> paletteList;
    for (const std::int32_t stateId : palette) {
        paletteList.push_back(NbtTag::ofCompound(blockStateTag(stateId)));
    }
    tag.set("palette", compounds(std::move(paletteList)));
    std::vector<NbtTag> entityList;
    if (withEntities && size[0] >= 1 && size[1] >= 1 && size[2] >= 1) {
        // parity: `fillEntityList` over the box of whole blocks, players left out.
        const auto inside = [&](double x, double y, double z, double width, double height) {
            const double half = width / 2.0;
            return x + half > min[0] && x - half < max[0] + 1 && y + height > min[1] && y < max[1] + 1 &&
                   z + half > min[2] && z - half < max[2] + 1;
        };
        const auto add = [&](double x, double y, double z, NbtCompound saved) {
            NbtCompound entry;
            const std::array<double, 3> relative{x - min[0], y - min[1], z - min[2]};
            entry.set("pos", doubles(relative));
            entry.set("blockPos", ints({static_cast<std::int32_t>(std::floor(relative[0])),
                                        static_cast<std::int32_t>(std::floor(relative[1])),
                                        static_cast<std::int32_t>(std::floor(relative[2]))}));
            entry.set("nbt", NbtTag::ofCompound(std::move(saved)));
            entityList.push_back(NbtTag::ofCompound(std::move(entry)));
        };
        for (const entity::ServerEntity* entity : world.entities()) {
            if (!entity->removed && inside(entity->x, entity->y, entity->z, entity->width(), entity->height())) {
                add(entity->x, entity->y, entity->z, entity::entityNbt::encode(*entity));
            }
        }
        for (const DroppedItem& item : world.items()) {
            if (inside(item.x, item.y, item.z, 0.25, 0.25)) {
                add(item.x, item.y, item.z, droppedItemNbt::encode(item));
            }
        }
    }
    tag.set("entities", compounds(std::move(entityList)));
    tag.set("size", ints(size));
    tag.putInt("DataVersion", server::world::region::chunkNbt::kDataVersion);
    return tag;
}

} // namespace basalt::server::runtime::world::structureBlocks
