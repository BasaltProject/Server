#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::world::containers {
namespace {

/// parity: `CakeBlock`'s seven slices, which is what `getOutputSignal` counts down from.
constexpr std::int32_t kFullCake = 7;
constexpr std::int32_t kLavaCauldronSignal = 3;
constexpr std::int32_t kMaxCharges = 4;

/// parity: `CopperGolemStatueBlock.Pose`.
constexpr std::array<std::string_view, 4> kGolemPoses{"standing", "sitting", "running", "star"};

[[nodiscard]] const std::unordered_map<std::string_view, std::int32_t>& sizes() {
    static const std::unordered_map<std::string_view, std::int32_t> table = [] {
        std::unordered_map<std::string_view, std::int32_t> built{
            {"furnace", 3},
            {"blast_furnace", 3},
            {"smoker", 3},
            {"barrel", kChestSize},
            {"hopper", 5},
            {"dispenser", 9},
            {"dropper", 9},
            {"crafter", 9},
            // parity: `BrewingStandBlock.getAnalogOutputSignal`.
            {"brewing_stand", brewingStands::kSize},
            // A jukebox holds one disc, and what a comparator reads off it is the song, not the slot.
            {"jukebox", 1},
            // A lectern holds one book, and what a comparator reads off it is the page.
            {"lectern", 1},
            // parity: `ContainerSingleItem`, one stack and no screen.
            {"decorated_pot", decoratedPots::kSize},
            // parity: `ChiseledBookShelfBlockEntity`, six slots read by which was touched last.
            {"chiseled_bookshelf", chiseledBookshelves::kSize},
            {"chest_minecart", kChestSize},
            {"hopper_minecart", 5},
        };
        for (const std::string_view chest : reshapes::chestBlocks()) {
            built.emplace(chest, kChestSize);
        }
        for (const std::string_view box : shulkers::blocks()) {
            built.emplace(box, shulkers::kSize);
        }
        return built;
    }();
    return table;
}

[[nodiscard]] std::optional<std::int32_t> numberOf(const std::map<std::string, std::string>& properties,
                                                   const std::string& name) {
    const auto found = properties.find(name);
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const std::from_chars_result result =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    if (result.ec != std::errc{} || result.ptr != found->second.data() + found->second.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string textOf(const std::map<std::string, std::string>& properties, const std::string& name) {
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: each block's `getAnalogOutputSignal`.
[[nodiscard]] std::optional<std::int32_t> stateSignal(std::string_view path,
                                                      const std::map<std::string, std::string>& properties) {
    // parity: `CakeBlock.getOutputSignal`.
    if (path == "cake") {
        return (kFullCake - numberOf(properties, "bites").value_or(0)) * 2;
    }
    // parity: `CandleCakeBlock`, always a full cake: a cake with a candle has no bites.
    if (path.ends_with("candle_cake")) {
        return kFullCake * 2;
    }
    if (path == "composter" || path == "water_cauldron" || path == "powder_snow_cauldron") {
        return numberOf(properties, "level").value_or(0);
    }
    // parity: `LavaCauldronBlock`, which is always three because lava has no levels.
    if (path == "lava_cauldron") {
        return kLavaCauldronSignal;
    }
    // parity: `RespawnAnchorBlock.getScaledChargeLevel`, four charges spread over fifteen.
    if (path == "respawn_anchor") {
        const float charges = static_cast<float>(numberOf(properties, "charges").value_or(0));
        return static_cast<std::int32_t>(charges / static_cast<float>(kMaxCharges) *
                                         static_cast<float>(redstone::kMaxSignal));
    }
    if (path == "end_portal_frame") {
        return textOf(properties, "eye") == "true" ? redstone::kMaxSignal : 0;
    }
    if (path == "beehive" || path == "bee_nest") {
        return numberOf(properties, "honey_level").value_or(0);
    }
    if (path.ends_with("copper_bulb")) {
        return textOf(properties, "lit") == "true" ? redstone::kMaxSignal : 0;
    }
    // parity: `CopperGolemStatueBlock`, one per pose in the order the enum declares them.
    if (path == "copper_golem_statue" || path.ends_with("_copper_golem_statue")) {
        const auto found = std::ranges::find(kGolemPoses, textOf(properties, "copper_golem_pose"));
        return found == kGolemPoses.end() ? 0 : static_cast<std::int32_t>(found - kGolemPoses.begin()) + 1;
    }
    return std::nullopt;
}

} // namespace

std::optional<std::int32_t> sizeOf(std::string_view path) {
    // parity: `ShelfBlockEntity implements ListBackedContainer`.
    if (shelves::isShelf(path)) {
        return shelves::kSize;
    }
    const auto found = sizes().find(path);
    return found == sizes().end() ? std::nullopt : std::optional{found->second};
}

bool isPlain(std::string_view path) {
    if (path == "barrel" || path == "hopper" || path == "dispenser" || path == "dropper") {
        return true;
    }
    return std::ranges::find(reshapes::chestBlocks(), path) != reshapes::chestBlocks().end() ||
           shulkers::isShulkerBox(path);
}

std::int32_t maxStackFor(const std::string_view path, const server::world::loot::ItemStack& stack) {
    const std::int32_t inHand = registry::itemStacking::maxStackSize(stack.item, stack.components);
    // parity: `ChiseledBookShelfBlockEntity.getMaxStackSize` and `JukeboxBlockEntity`'s.
    if (path == "chiseled_bookshelf" || path == "jukebox") {
        return 1;
    }
    return inHand;
}

std::vector<std::int32_t> slotsForFace(const std::string_view path, const std::int32_t size,
                                       const registry::BlockFace from) {
    // parity: `AbstractFurnaceBlockEntity`'s three arrays, and `BrewingStandBlockEntity`'s.
    if (furnaces::cookerFor(path).has_value()) {
        if (from == registry::BlockFace::Down) {
            return {furnaces::kResultSlot, furnaces::kFuelSlot};
        }
        return from == registry::BlockFace::Up ? std::vector{furnaces::kInputSlot} : std::vector{furnaces::kFuelSlot};
    }
    if (path == "brewing_stand") {
        if (from == registry::BlockFace::Up) {
            return {brewingStands::kIngredientSlot};
        }
        if (from == registry::BlockFace::Down) {
            return {0, 1, 2, brewingStands::kIngredientSlot};
        }
        return {0, 1, 2, brewingStands::kFuelSlot};
    }
    std::vector<std::int32_t> every(static_cast<std::size_t>(std::max(size, 0)));
    std::iota(every.begin(), every.end(), 0);
    return every;
}

/// parity: `ChiseledBookShelfBlockEntity` takes and gives through `setItem`/`removeItem`.
[[nodiscard]] bool hasItsOwnDoor(const std::string_view path) { return path == "chiseled_bookshelf"; }

bool acceptsThroughFace(const std::string_view path, const std::int32_t slot, const wire::common::Identifier& item,
                        const bool slotIsEmpty) {
    if (hasItsOwnDoor(path)) {
        return false;
    }
    // parity: `AbstractFurnaceBlockEntity.canPlaceItem`.
    if (furnaces::cookerFor(path).has_value()) {
        if (slot == furnaces::kResultSlot) {
            return false;
        }
        if (slot != furnaces::kFuelSlot) {
            return true;
        }
        return registry::fuels::isFuel(item) || (item.path() == "bucket" && slotIsEmpty);
    }
    // parity: `BrewingStandBlockEntity.canPlaceItem`, slot by slot.
    if (path == "brewing_stand") {
        if (slot == brewingStands::kIngredientSlot) {
            return brewingStands::isReagent(item);
        }
        if (slot == brewingStands::kFuelSlot) {
            return item == registry::brewing::fuel();
        }
        return slotIsEmpty && brewingStands::isPotionInput(item);
    }
    // parity: `ShulkerBoxBlockEntity.canPlaceItemThroughFace`.
    if (shulkers::isShulkerBox(path)) {
        return !shulkers::isShulkerBox(item.path());
    }
    return true;
}

bool givesThroughFace(const std::string_view path, const std::int32_t slot, const wire::common::Identifier& item,
                      const registry::BlockFace from) {
    if (hasItsOwnDoor(path)) {
        return false;
    }
    // parity: `AbstractFurnaceBlockEntity.canTakeItemThroughFace`.
    if (furnaces::cookerFor(path).has_value() && from == registry::BlockFace::Down && slot == furnaces::kFuelSlot) {
        return item.path() == "bucket" || item.path() == "water_bucket";
    }
    // parity: `BrewingStandBlockEntity.canTakeItemThroughFace`.
    if (path == "brewing_stand" && slot == brewingStands::kIngredientSlot) {
        return item.path() == "glass_bottle";
    }
    return true;
}

bool setItemTellsComparators(const std::string_view path, const std::int32_t slot, const bool sameItem) {
    // parity: `HopperBlockEntity.setItem` and `LecternBlockEntity`'s, neither of which calls.
    if (path == "hopper" || path == "lectern") {
        return false;
    }
    // parity: `ChiseledBookShelfBlockEntity.setItem` goes through `updateState`, so what the.
    if (path == "chiseled_bookshelf") {
        return false;
    }
    // parity: `AbstractFurnaceBlockEntity.setItem`.
    if (furnaces::cookerFor(path).has_value()) {
        return slot == furnaces::kInputSlot && !sameItem;
    }
    return true;
}

std::optional<std::int32_t> analogSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         std::string_view from, const Readers& readers) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (const std::optional<std::int32_t> plain = stateSignal(path, properties); plain.has_value()) {
        return plain;
    }
    // parity: `CreakingHeartBlock.getAnalogOutputSignal`.
    if (path == "creaking_heart") {
        if (textOf(properties, "creaking_heart_state") == "uprooted") {
            return 0;
        }
        const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* heart =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        const std::optional<wire::common::Uuid> bound =
            heart == nullptr ? std::nullopt : entity::creakings::heartCreaking(heart->data());
        if (!bound.has_value()) {
            return 0;
        }
        for (const entity::ServerEntity* creaking : world.entities()) {
            if (!creaking->removed && creaking->uuid() == *bound) {
                const double dx = creaking->x - (x + 0.5);
                const double dy = creaking->y - y;
                const double dz = creaking->z - (z + 0.5);
                return entity::creakings::signalFor(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
            }
        }
        return 0;
    }
    // parity: `ShelfBlock.getAnalogOutputSignal`, three bits and only from behind.
    if (readers.shelf) {
        if (const std::optional<std::int32_t> shelf = readers.shelf(x, y, z, *stateId, from); shelf.has_value()) {
            return shelf;
        }
    }
    // parity: `SculkSensorBlock.getAnalogOutputSignal`.
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
        if (textOf(properties, "sculk_sensor_phase") != "active") {
            return 0;
        }
        const server::world::Chunk* awake = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* heard =
            awake == nullptr ? nullptr : awake->blockEntityAt(x & 15, y, z & 15);
        return heard == nullptr ? 0 : heard->data().getInt(std::string{vibrations::kFrequencyKey}, 0);
    }
    // parity: `CommandBlock.getAnalogOutputSignal`.
    if (commandBlocks::modeOf(path).has_value()) {
        const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        return entity == nullptr ? 0 : commandBlocks::comparatorSignal(entity->data()).value_or(0);
    }
    // parity: `DetectorRailBlock.getAnalogOutputSignal`.
    if (path == "detector_rail") {
        return readers.cart ? readers.cart(x, y, z) : 0;
    }
    const std::optional<std::int32_t> size = sizeOf(path);
    if (!size.has_value()) {
        return std::nullopt;
    }
    // parity: `getRedstoneSignalFromContainer` reads every slot.
    world.unpackLoot(x, y, z);
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return 0;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return 0;
    }
    // parity: `ChiseledBookShelfBlock.getAnalogOutputSignal`, the last slot touched plus one.
    if (path == "chiseled_bookshelf") {
        return chiseledBookshelves::comparatorSignal(entity->data());
    }
    // parity: `CrafterBlockEntity.getRedstoneSignal` counts slots, not how full they are.
    if (path == "crafter") {
        return readers.crafter ? readers.crafter(entity->data()) : 0;
    }
    // parity: `JukeboxBlock.getAnalogOutputSignal`, the number of the song that is playing.
    if (path == "jukebox") {
        return jukeboxes::comparatorOutputAt(world, entity->data());
    }
    // parity: `LecternBlock.getAnalogOutputSignal`.
    if (path == "lectern") {
        return textOf(properties, "has_book") == "true" ? lecterns::comparatorSignal(entity->data()) : 0;
    }
    // parity: `ChestBlock.getAnalogOutputSignal` reads the **pair**.
    std::int32_t seen = *size;
    blockInventory::Slots slots = blockInventory::readFor(path, entity->data(), *size);
    if (const std::optional<std::pair<doubleChests::Half, doubleChests::Half>> pair =
            doubleChests::halves(world, x, y, z);
        pair.has_value()) {
        const doubleChests::Half& other = pair->first == doubleChests::Half{x, y, z} ? pair->second : pair->first;
        const server::world::Chunk* half = world.chunkIfLoaded(other[0] >> 4, other[2] >> 4);
        const server::world::ChunkBlockEntity* beyond =
            half == nullptr ? nullptr : half->blockEntityAt(other[0] & 15, other[1], other[2] & 15);
        if (beyond != nullptr) {
            const blockInventory::Slots rest = blockInventory::read(beyond->data(), *size);
            slots.insert(slots.end(), rest.begin(), rest.end());
            seen += *size;
        }
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> counts;
    counts.reserve(slots.size());
    for (const std::optional<server::world::loot::ItemStack>& slot : slots) {
        // parity: `getRedstoneSignalFromContainer` divides each stack by.
        const std::int32_t cap =
            slot.has_value() ? std::min(kSlotLimit, registry::itemStacking::maxStackSize(slot->item, slot->components))
                             : 1;
        counts.emplace_back(slot.has_value() ? slot->count : 0, cap);
    }
    return redstone::containerSignal(counts, seen);
}

} // namespace basalt::server::runtime::world::containers
