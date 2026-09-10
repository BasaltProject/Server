#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
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
#include <cstdint>
#include <map>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::world::containers {
namespace {

/// parity: `Container.getMaxStackSize()`, whose default nothing that holds items overrides, so what
/// decides the divisor is the item and only the item.
constexpr std::int32_t kContainerMaxStack = 99;

/// parity: `CakeBlock`'s seven slices, which is what `getOutputSignal` counts down from.
constexpr std::int32_t kFullCake = 7;
constexpr std::int32_t kLavaCauldronSignal = 3;
constexpr std::int32_t kMaxCharges = 4;

/// parity: `CopperGolemStatueBlock.Pose`, in its declared order, which is what the signal counts.
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
            // parity: `BrewingStandBlock.getAnalogOutputSignal`, its five slots read like any container.
            {"brewing_stand", brewingStands::kSize},
            // A jukebox holds one disc, and what a comparator reads off it is the song, not the slot.
            {"jukebox", 1},
            // A lectern holds one book, and what a comparator reads off it is the page.
            {"lectern", 1},
            // parity: `ContainerSingleItem`, one stack and no screen.
            {"decorated_pot", decoratedPots::kSize},
            // parity: `ChiseledBookShelfBlockEntity`, six slots read by which was touched last.
            {"chiseled_bookshelf", chiseledBookshelves::kSize},
            // The two container minecarts. They are not blocks, but a comparator beside a detector
            // rail reads one the same way it reads a chest, so the size lives in the same table.
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

/// The blocks a comparator reads straight off their own state, with no contents involved.
///
/// parity: each block's `getAnalogOutputSignal`. They are one line each in the jar and they are one
/// line each here; what they have in common is that the number is already in the state.
[[nodiscard]] std::optional<std::int32_t> stateSignal(std::string_view path,
                                                      const std::map<std::string, std::string>& properties) {
    // parity: `CakeBlock.getOutputSignal`, two per slice left, so a whole cake reads fourteen.
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
    // parity: `CreakingHeartBlock.hasAnalogOutputSignal` is true and its number comes from the
    // Creaking bound to it: fifteen next to it, down to nothing at thirty-two blocks, and zero with
    // none. There is no Creaking in this build, so the number is always zero; what matters is that
    // the heart **answers**, because that is what stops a comparator looking past it for a container.
    if (path == "creaking_heart") {
        return 0;
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
    // parity: `ShelfBlockEntity implements ListBackedContainer`, three slots and no door of its own,
    // so a hopper fills a shelf and another one empties it. It is by suffix rather than by name
    // because there is one shelf per wood.
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

std::int32_t maxStackFor(const std::string_view path, const wire::common::Identifier& item) {
    const std::int32_t inHand = registry::itemStacking::maxStackSize(item);
    // parity: `ChiseledBookShelfBlockEntity.getMaxStackSize` and `JukeboxBlockEntity`'s, both one.
    // The lectern's is one too and is not here: a lectern is no container to a hopper.
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

/// parity: `ChiseledBookShelfBlockEntity` takes and gives through `setItem`/`removeItem`, both of
/// which run `updateState` and light the slot on the block. A hopper reaches it through its own door
/// for that reason, the way it reaches a jukebox and a composter, so the plain container path has to
/// keep its hands off.
[[nodiscard]] bool hasItsOwnDoor(const std::string_view path) { return path == "chiseled_bookshelf"; }

bool acceptsThroughFace(const std::string_view path, const std::int32_t slot, const wire::common::Identifier& item,
                        const bool slotIsEmpty) {
    if (hasItsOwnDoor(path)) {
        return false;
    }
    // parity: `AbstractFurnaceBlockEntity.canPlaceItem`: nothing goes into the result slot, and the
    // fuel slot takes fuel or **one** empty bucket, which is what a lava bucket leaves behind.
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
            return registry::brewing::isIngredient(item);
        }
        if (slot == brewingStands::kFuelSlot) {
            return item == registry::brewing::fuel();
        }
        static constexpr std::array<std::string_view, 4> kBottles{"potion", "splash_potion", "lingering_potion",
                                                                  "glass_bottle"};
        return slotIsEmpty && std::ranges::find(kBottles, item.path()) != kBottles.end();
    }
    // parity: `ShulkerBoxBlockEntity.canPlaceItemThroughFace`, the one thing a box will not hold.
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
    // parity: `AbstractFurnaceBlockEntity.canTakeItemThroughFace`: what comes out of the fuel slot
    // downwards is the bucket the lava came in, and never the fuel itself.
    if (furnaces::cookerFor(path).has_value() && from == registry::BlockFace::Down && slot == furnaces::kFuelSlot) {
        return item.path() == "bucket" || item.path() == "water_bucket";
    }
    // parity: `BrewingStandBlockEntity.canTakeItemThroughFace`: the ingredient slot only gives back
    // the glass bottle a brew leaves in it.
    if (path == "brewing_stand" && slot == brewingStands::kIngredientSlot) {
        return item.path() == "glass_bottle";
    }
    return true;
}

bool setItemTellsComparators(const std::string_view path, const std::int32_t slot, const bool sameItem) {
    // parity: `HopperBlockEntity.setItem` and `LecternBlockEntity`'s, neither of which calls
    // `setChanged`: a hopper filled by command leaves the comparator beside it reading nothing.
    if (path == "hopper" || path == "lectern") {
        return false;
    }
    // parity: `ChiseledBookShelfBlockEntity.setItem` goes through `updateState`, so what the
    // comparators hear is the block changing state, not this.
    if (path == "chiseled_bookshelf") {
        return false;
    }
    // parity: `AbstractFurnaceBlockEntity.setItem`, which only speaks for the slot whose contents
    // decide the cook time.
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
    // parity: `ShelfBlock.getAnalogOutputSignal`, three bits and only from behind.
    if (readers.shelf) {
        if (const std::optional<std::int32_t> shelf = readers.shelf(x, y, z, *stateId, from); shelf.has_value()) {
            return shelf;
        }
    }
    // parity: `SculkSensorBlock.getAnalogOutputSignal`, the **frequency** of the last thing it heard
    // and only while it is awake. Not the state's `power`, which says how close the noise was:
    // measured in `sensormuerte`, a pig dying leaves the state at ten and the comparator at fifteen.
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
        if (textOf(properties, "sculk_sensor_phase") != "active") {
            return 0;
        }
        const server::world::Chunk* awake = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* heard =
            awake == nullptr ? nullptr : awake->blockEntityAt(x & 15, y, z & 15);
        return heard == nullptr ? 0 : heard->data().getInt(std::string{vibrations::kFrequencyKey}, 0);
    }
    // parity: `CommandBlock.getAnalogOutputSignal`, which is the `SuccessCount` of the last command it
    // ran and nothing else. Like the rail it answers zero rather than nothing.
    if (commandBlocks::modeOf(path).has_value()) {
        const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        return entity == nullptr ? 0 : commandBlocks::comparatorSignal(entity->data()).value_or(0);
    }
    // parity: `DetectorRailBlock.getAnalogOutputSignal`, which is a reading like any other even
    // though what it reads is not in the block: it answers zero, never nothing.
    if (path == "detector_rail") {
        return readers.cart ? readers.cart(x, y, z) : 0;
    }
    const std::optional<std::int32_t> size = sizeOf(path);
    if (!size.has_value()) {
        return std::nullopt;
    }
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
    // parity: `LecternBlock.getAnalogOutputSignal`, which is nothing at all without a book, and
    // otherwise how far through it the reader is.
    if (path == "lectern") {
        return textOf(properties, "has_book") == "true" ? lecterns::comparatorSignal(entity->data()) : 0;
    }
    // parity: `ChestBlock.getAnalogOutputSignal` reads the **pair**, so the same four stacks give
    // three in a single chest and two in a double one.
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
        // parity: `getRedstoneSignalFromContainer` divides each stack by
        // `container.getMaxStackSize(stack)`, so a chest with one bucket in it reads far higher than
        // a chest with one stone. Reading 64 for everything was a full fifteen out on a shulker box.
        const std::int32_t cap =
            slot.has_value() ? std::min(kContainerMaxStack, registry::itemStacking::maxStackSize(slot->item)) : 1;
        counts.emplace_back(slot.has_value() ? slot->count : 0, cap);
    }
    return redstone::containerSignal(counts, seen);
}

} // namespace basalt::server::runtime::world::containers
