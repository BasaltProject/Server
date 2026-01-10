#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::world::containers {
namespace {

/// parity: `Item.getMaxStackSize`, which this build does not read yet: everything stacks to 64.
constexpr std::int32_t kDefaultMaxStack = 64;

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
    // parity: `CopperGolemStatueBlock`, one per pose in the order the enum declares them.
    if (path == "copper_golem_statue" || path.ends_with("_copper_golem_statue")) {
        const auto found = std::ranges::find(kGolemPoses, textOf(properties, "copper_golem_pose"));
        return found == kGolemPoses.end() ? 0 : static_cast<std::int32_t>(found - kGolemPoses.begin()) + 1;
    }
    return std::nullopt;
}

} // namespace

std::optional<std::int32_t> sizeOf(std::string_view path) {
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
        return jukeboxes::comparatorOutputAt(entity->data());
    }
    // parity: `LecternBlock.getAnalogOutputSignal`, which is nothing at all without a book, and
    // otherwise how far through it the reader is.
    if (path == "lectern") {
        return textOf(properties, "has_book") == "true" ? lecterns::comparatorSignal(entity->data()) : 0;
    }
    const blockInventory::Slots slots = blockInventory::read(entity->data(), *size);
    std::vector<std::pair<std::int32_t, std::int32_t>> counts;
    counts.reserve(slots.size());
    for (const std::optional<server::world::loot::ItemStack>& slot : slots) {
        counts.emplace_back(slot.has_value() ? slot->count : 0, kDefaultMaxStack);
    }
    return redstone::containerSignal(counts, *size);
}

} // namespace basalt::server::runtime::world::containers
