#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The copper golem: its weathering, the pose it shows while it works a chest, and what it carries
/// from a copper chest to a plain one.
///
/// parity: `CopperGolem`, `CopperGolemAi` and `TransportItemsBetweenContainers`.
namespace copperGolems {

/// parity: the synched `DATA_WEATHER_STATE` and `COPPER_GOLEM_STATE`, right after `Mob`'s flags.
inline constexpr std::uint8_t kWeatherDataIndex = 16;
inline constexpr std::uint8_t kStateDataIndex = 17;

/// parity: `WeatheringCopper.WeatherState`, in its own order.
enum class Weather : std::uint8_t { Unaffected, Exposed, Weathered, Oxidized };

/// parity: `CopperGolemState`.
enum class Pose : std::uint8_t { Idle, GettingItem, GettingNoItem, DroppingItem, DroppingNoItem };

/// parity: `TransportItemsBetweenContainers.TransportItemState` and `ContainerInteractionState`.
enum class Travel : std::uint8_t { Travelling, Queuing, Interacting };
enum class Interaction : std::uint8_t { PickupItem, PickupNoItem, PlaceItem, PlaceNoItem };

/// parity: `IGNORE_WEATHERING_TICK` (waxed) and `UNSET_WEATHERING_TICK`.
inline constexpr std::int64_t kWaxed = -2;
inline constexpr std::int64_t kUnset = -1;
/// parity: `WEATHERING_TICK_FROM` and `WEATHERING_TICK_TO`.
inline constexpr std::int32_t kWeatheringFrom = 504000;
inline constexpr std::int32_t kWeatheringTo = 552000;
/// parity: `TURN_TO_STATUE_CHANCE`.
inline constexpr float kStatueChance = 0.0058F;
/// parity: the constructor's first transport cooldown, and `IDLE_COOLDOWN` after a fruitless search.
inline constexpr std::int32_t kSpawnCooldownMin = 60;
inline constexpr std::int32_t kSpawnCooldownMax = 100;
inline constexpr std::int32_t kIdleCooldown = 140;
/// parity: `TransportItemsBetweenContainers`' constants.
inline constexpr std::int32_t kInteractionTicks = 60;
inline constexpr std::int32_t kOpenTick = 1;
inline constexpr std::int32_t kSoundTick = 9;
inline constexpr std::int64_t kVisitedMemoryTicks = 6000;
inline constexpr std::size_t kMaxVisited = 10;
inline constexpr std::size_t kMaxUnreachable = 50;
inline constexpr std::int32_t kMaxCarried = 16;
inline constexpr std::int32_t kHorizontalSearch = 32;
inline constexpr std::int32_t kVerticalSearch = 8;
inline constexpr double kQueueDistance = 3.0;
inline constexpr double kKeepInteractingDistance = 2.0;
/// parity: `SPEED_MULTIPLIER_WHEN_PANICKING`.
inline constexpr double kPanicSpeed = 1.5;

using Position = std::array<std::int32_t, 3>;

/// What a golem keeps beyond the entity itself.
struct GolemState {
    Weather weather = Weather::Unaffected;
    Pose pose = Pose::Idle;
    std::int64_t nextWeatheringTick = kUnset;
    /// A golem just built from a pattern, whose weathering the chest under it gives on its first turn.
    bool fromPattern = false;
    std::optional<Position> openedChest;
    /// parity: `InteractWithDoor` in the golem's core activity.
    ai::doorInteraction::State doors;
    /// The bolt that last scraped it, by entity id. parity: `lastLightningBoltUUID`.
    std::optional<std::int32_t> lastLightning;
    /// parity: the `TRANSPORT_ITEMS_COOLDOWN_TICKS` memory; zero is absent.
    std::int32_t transportCooldown = 0;
    std::optional<Position> target;
    Travel travel = Travel::Travelling;
    std::optional<Interaction> interaction;
    std::int32_t ticksSinceReaching = 0;
    /// The last square of the walk to the chest, kept once walked as `PathNavigation` keeps a
    /// finished path.
    std::optional<Position> approachEnd;
    /// parity: `VISITED_BLOCK_POSITIONS` and `UNREACHABLE_TRANSPORT_BLOCK_POSITIONS`, with expiry.
    std::vector<Position> visited;
    std::int64_t visitedUntil = 0;
    std::vector<Position> unreachable;
    std::int64_t unreachableUntil = 0;
    /// parity: `idleAnimationStartTick`, the spin every ten to twelve seconds.
    std::int32_t spinAt = 0;
    /// What the idle choice is doing: waiting this many more ticks.
    std::int32_t waitTicks = 0;
};

[[nodiscard]] bool isCopperGolem(const wire::common::Identifier& type);

/// The golem's state, made on first use from what it saved. `random` rolls the first cooldown.
[[nodiscard]] GolemState& stateOf(ServerEntity& golem, random::RandomSource& random);

/// Writes the weathering back into the saved form. parity: `addAdditionalSaveData`.
void save(ServerEntity& golem);

[[nodiscard]] std::string_view weatherName(Weather weather);

[[nodiscard]] Weather next(Weather weather);

[[nodiscard]] Weather previous(Weather weather);

/// The weathering a copper block or chest shows, waxed or not. parity: `getWeatherStateFromPattern`.
[[nodiscard]] Weather weatherOfBlock(std::string_view path);

/// parity: `CopperGolemOxidationLevels`.
[[nodiscard]] std::string_view spinSound(Weather weather);
[[nodiscard]] std::string_view hurtSound(Weather weather);
[[nodiscard]] std::string_view deathSound(Weather weather);

/// parity: `COPPER_CHESTS`, where a golem with empty hands takes from.
[[nodiscard]] bool isSourceChest(std::string_view path);

/// parity: a plain or trapped chest, where a golem with something in hand leaves it.
[[nodiscard]] bool isDestinationChest(std::string_view path);

/// parity: `ChestBlockEntity`, the block entities a golem considers at all.
[[nodiscard]] bool isChest(std::string_view path);

/// What it holds in its main hand, from its saved equipment.
[[nodiscard]] std::optional<server::world::loot::ItemStack> mainHand(const ServerEntity& golem);

/// Puts a stack in its main hand; `guaranteed` makes it always drop. parity: `setGuaranteedDrop`.
void setMainHand(ServerEntity& golem, const std::optional<server::world::loot::ItemStack>& stack, bool guaranteed);

using Slots = std::vector<std::optional<server::world::loot::ItemStack>>;

[[nodiscard]] bool isEmpty(const Slots& slots);

/// parity: `hasItemMatchingHandItem`, the item alone.
[[nodiscard]] bool holdsSameItem(const Slots& slots, const wire::common::Identifier& item);

/// parity: `pickupItemFromContainer`, up to sixteen from the first slot with anything.
[[nodiscard]] std::optional<server::world::loot::ItemStack> pickUp(Slots& slots);

/// parity: `addItemsToContainer`, answering what is left in hand. The jar takes the room left off
/// the hand rather than what it moved, so a stack that did not fit whole still comes back empty.
[[nodiscard]] std::optional<server::world::loot::ItemStack> putDown(Slots& slots, server::world::loot::ItemStack held);

} // namespace copperGolems

} // namespace basalt::server::runtime::entity
