#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <map>
#include <span>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::beehives {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] const NbtList* beesOf(const NbtCompound& data) { return data.getList("bees"); }

[[nodiscard]] NbtList* beesOf(NbtCompound& data) {
    NbtTag* bees = data.find("bees");
    return bees == nullptr ? nullptr : bees->get<NbtList>();
}

[[nodiscard]] bool litCampfire(std::int32_t state) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(state);
    if (block == nullptr || (block->path() != "campfire" && block->path() != "soul_campfire")) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(state);
    const auto lit = properties.find("lit");
    return lit != properties.end() && lit->second == "true";
}

} // namespace

bool isHive(std::string_view blockPath) { return blockPath == "beehive" || blockPath == "bee_nest"; }

std::size_t occupantCount(const NbtCompound& data) {
    const NbtList* bees = beesOf(data);
    return bees == nullptr ? 0 : bees->size();
}

std::size_t maxOccupants(const NbtCompound& data, const std::size_t fallback) {
    return data.contains("MaxBees") ? static_cast<std::size_t>(std::max(data.getInt("MaxBees"), 0)) : fallback;
}

bool isFull(const NbtCompound& data, const std::size_t fallback) {
    return occupantCount(data) >= maxOccupants(data, fallback);
}

void addOccupant(NbtCompound& data, NbtCompound bee) {
    // parity: `Occupant.of`, whose minimum stay is read from the saved nectar.
    const bool nectar = bee.getBoolean("HasNectar");
    NbtCompound occupant;
    occupant.set("entity_data", NbtTag::ofCompound(std::move(bee)));
    occupant.putInt("ticks_in_hive", 0);
    occupant.putInt("min_ticks_in_hive", nectar ? kMinTicksNectar : kMinTicksNectarless);
    NbtList* bees = beesOf(data);
    if (bees == nullptr || bees->elementType() != NbtType::Compound) {
        std::vector<NbtTag> only;
        only.push_back(NbtTag::ofCompound(std::move(occupant)));
        data.set("bees", NbtTag::ofList(NbtList{NbtType::Compound, std::move(only)}));
        return;
    }
    std::vector<NbtTag> all = bees->elements();
    all.push_back(NbtTag::ofCompound(std::move(occupant)));
    data.set("bees", NbtTag::ofList(NbtList{NbtType::Compound, std::move(all)}));
}

void tickOccupants(NbtCompound& data, std::vector<std::size_t>& due) {
    due.clear();
    NbtList* bees = beesOf(data);
    if (bees == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < bees->size(); ++index) {
        NbtCompound* occupant = (*bees)[index].get<NbtCompound>();
        if (occupant == nullptr) {
            continue;
        }
        // parity: `ticksInHive++ > minTicksInHive`, compared before the count moves on.
        const std::int32_t ticks = occupant->getInt("ticks_in_hive");
        occupant->putInt("ticks_in_hive", ticks + 1);
        if (ticks > occupant->getInt("min_ticks_in_hive")) {
            due.push_back(index);
        }
    }
}

const NbtCompound* occupant(const NbtCompound& data, std::size_t index) {
    const NbtList* bees = beesOf(data);
    return bees == nullptr || index >= bees->size() ? nullptr : (*bees)[index].get<NbtCompound>();
}

void removeOccupants(NbtCompound& data, const std::vector<std::size_t>& indices) {
    NbtList* bees = beesOf(data);
    if (bees == nullptr || indices.empty()) {
        return;
    }
    std::vector<NbtTag> kept;
    std::size_t next = 0;
    for (std::size_t index = 0; index < bees->size(); ++index) {
        if (next < indices.size() && indices[next] == index) {
            ++next;
            continue;
        }
        kept.push_back((*bees)[index]);
    }
    data.set("bees", NbtTag::ofList(NbtList{NbtType::Compound, std::move(kept)}));
}

std::optional<Pos> flowerOf(const NbtCompound& data) {
    const std::span<const std::int32_t> parts = data.getIntArray("flower_pos");
    return parts.size() == 3 ? std::optional{Pos{parts[0], parts[1], parts[2]}} : std::nullopt;
}

void setFlower(NbtCompound& data, const Pos& flower) {
    data.putIntArray("flower_pos", {flower[0], flower[1], flower[2]});
}

bool isFireNearby(const WorldState& world, const Pos& hive) {
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                const std::optional<std::int32_t> state = world.blockState(hive[0] + dx, hive[1] + dy, hive[2] + dz);
                const wire::common::Identifier* block =
                    state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
                // parity: `instanceof FireBlock`, which soul fire is not.
                if (block != nullptr && block->path() == "fire") {
                    return true;
                }
            }
        }
    }
    return false;
}

bool isSmokey(const WorldState& world, const Pos& at) {
    constexpr std::int32_t kReach = 5;
    constexpr double kPostMin = 6.0 / 16.0;
    constexpr double kPostMax = 10.0 / 16.0;
    constexpr std::size_t kBoxStride = 6;
    for (std::int32_t down = 1; down <= kReach; ++down) {
        const std::int32_t state = world.blockState(at[0], at[1] - down, at[2]).value_or(0);
        if (litCampfire(state)) {
            return true;
        }
        // parity: the smoke stops at a collision crossing the middle four pixels of the column.
        const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(state);
        for (std::size_t box = 0; box + kBoxStride <= boxes.size(); box += kBoxStride) {
            const std::span<const std::int32_t> one = boxes.subspan(box, kBoxStride);
            const auto unit = [&one](std::size_t index) {
                return static_cast<double>(one[index]) / registry::blockShapes::kUnits;
            };
            if (unit(0) < kPostMax && unit(3) > kPostMin && unit(2) < kPostMax && unit(5) > kPostMin &&
                unit(4) > unit(1)) {
                return litCampfire(world.blockState(at[0], at[1] - down - 1, at[2]).value_or(0));
            }
        }
    }
    return false;
}

} // namespace basalt::server::runtime::world::beehives
