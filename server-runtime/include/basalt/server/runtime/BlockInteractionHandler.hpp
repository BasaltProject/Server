#pragma once

#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::registry {
struct BlockHardness;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
enum class BedProblem : std::uint8_t;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class ClientSession;

class BlockInteractionHandler {
public:
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId, bool& dropItems)>
            breakAllowed;
        std::function<void(const player::PlayerSession* player, const std::string& world, std::int32_t x,
                           std::int32_t y, std::int32_t z, std::int32_t stateId,
                           std::vector<server::world::loot::ItemStack>& drops)>
            blockDrops;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item)>
            placeAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item, std::string_view face, bool mainHand)>
            interactAllowed;
        std::function<bool(const player::PlayerSession& player, const std::string& item, bool mainHand)> airUseAllowed;
        std::function<bool(const player::PlayerSession& player, const std::string& item, std::int32_t count)>
            dropAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view face, bool instaBreak)>
            digAllowed;
        std::function<std::optional<std::int32_t>(const player::PlayerSession& player, const std::string& item,
                                                  std::int32_t now, std::int32_t before)>
            foodEaten;
        std::function<bool(const player::PlayerSession& player, const std::string& item, std::int32_t amount,
                           bool broken)>
            itemDamageAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<std::string>& lines)>
            signChange;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item, bool filling, std::string_view face)>
            bucketAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)>
            bedAllowed;
        std::function<void(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)>
            bedLeft;
        std::function<bool(const player::PlayerSession& player, std::string_view state, std::int32_t hookId,
                           std::int32_t& exp)>
            fishing;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId)>
            fertilizeAllowed;
        std::function<bool(const player::PlayerSession& player, std::string_view type, std::int32_t x, std::int32_t y,
                           std::int32_t z, std::string_view face)>
            hangingAllowed;
    };

    struct Options {
        std::function<double()> nextDouble;
        std::function<std::int64_t()> nextSeed;
        std::function<bool()> nextBool;
    };

    explicit BlockInteractionHandler(ClientSession& owner, Hooks hooks = {}, Options options = {});

    /// parity: `ServerboundPlayerActionPacket`.
    void handlePlayerAction(const wire::protocol::play::ServerboundPlayerActionPacket& action);

    void broadcastBlockUpdate(const wire::protocol::play::BlockPos& pos, std::int32_t stateId);

    void broadcastEffects(const wire::protocol::play::BlockPos& pos, std::int32_t stateId,
                          const std::vector<world::BlockEffect>& effects);

    void writeSign(const wire::protocol::play::SignUpdatePacket& update);

    /// parity: `ComposterBlock.useItemOn` and `useWithoutItem`.
    bool useComposter(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `handleSetCommandBlock`, the screen a command block opens.
    void programCommandBlock(const wire::protocol::play::SetCommandBlockPacket& packet);
    /// parity: `handleSetCommandMinecart`, a command block cart's line typed in.
    void programCommandMinecart(const wire::protocol::play::ServerboundSetCommandMinecartPacket& packet);

    /// parity: `handleSetStructureBlock`, for a player allowed game master blocks.
    void setStructureBlock(const wire::protocol::play::SetStructureBlockPacket& packet);
    void setTestBlock(const wire::protocol::play::SetTestBlockPacket& packet);
    void testInstanceBlockAction(const wire::protocol::play::TestInstanceBlockActionPacket& packet);

    /// parity: `Level.updateNeighborsAt`, which is what makes a torch fall off a broken wall.
    void notifyNeighbours(const wire::protocol::play::BlockPos& pos);

    void notifyNeighboursAfterRemoval(const wire::protocol::play::BlockPos& pos, std::int32_t removedStateId);

    [[nodiscard]] bool isAirLike(std::int32_t stateId) const;

    /// parity: the little upward push a thrown item leaves with.
    [[nodiscard]] double tossLift() const;

    [[nodiscard]] player::SleepContext sleepContext();

    void wakeUp();

    void spawnDroppedItem(const wire::common::Identifier& item, std::int32_t count, double x, double y, double z,
                          double velocityX = 0.0, double velocityY = 0.0, double velocityZ = 0.0,
                          std::int32_t pickupDelay = 40, wire::nbt::NbtCompound components = {},
                          std::optional<wire::common::Uuid> thrower = std::nullopt);

    void handleUseItemOn(const wire::protocol::play::ServerboundUseItemOnPacket& useItemOn);

    /// parity: `ITEM_USED_ON_BLOCK`, raised by the items whose use changes the block clicked.
    void awardItemUsedOn(const player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                         const std::optional<std::int32_t>& clickedState,
                         const std::optional<storage::StoredItemStack>& tool) const;

    /// parity: `DEFAULT_BLOCK_USE`.
    void awardBlockUse(const player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                       const std::optional<std::int32_t>& clickedState,
                       const std::optional<storage::StoredItemStack>& tool, bool withoutItem) const;

    void handleUseItem(const wire::protocol::play::ServerboundUseItemPacket& useItem);

    /// parity: `handlePunch`, an arm swung at the air or a block.
    void handlePunch();

    void tickDigging(player::PlayerSession& session);

    [[nodiscard]] player::BreakContext breakContext();

    void wearHeldItem(std::int32_t cost);

private:
    [[nodiscard]] std::int64_t nextSeed() const;

    [[nodiscard]] double nextDouble() const;

    [[nodiscard]] bool breaksInOneTick(const wire::common::Identifier* block,
                                       const registry::BlockHardness* hardness) const;

    [[nodiscard]] float destroyProgressPerTick(const wire::common::Identifier& block,
                                               const registry::BlockHardness* hardness) const;

    /// parity: `Player.getDestroySpeed` asks whether the eyes are under water, not the feet.
    [[nodiscard]] bool eyesInWater() const;

    void wearHeldItemOnBreak(const wire::common::Identifier& block);

    void igniteAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                  const storage::StoredItemStack& held);

    [[nodiscard]] bool useBoneMeal(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                   std::int32_t heldSlot, const storage::StoredItemStack& held);

    /// parity: `ItemFrameItem.useOn`.
    [[nodiscard]] bool hangFrame(player::PlayerSession& session, const wire::protocol::play::BlockHitResult& hit,
                                 std::int32_t heldSlot, const storage::StoredItemStack& held,
                                 const wire::common::Identifier& type);

    [[nodiscard]] bool chargeAnchor(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                    std::int32_t heldSlot, const storage::StoredItemStack& held);

    [[nodiscard]] bool useHoneycomb(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                    std::int32_t heldSlot, const storage::StoredItemStack& held);

    [[nodiscard]] bool useAxe(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                              std::int32_t heldSlot, const storage::StoredItemStack& held);

    void placeEnderEye(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                       const storage::StoredItemStack& held);

    /// parity: `BellBlock.onHit` with `isProperHit`, which is what a bell can swing towards.
    [[nodiscard]] bool ringBell(const wire::protocol::play::BlockHitResult& hit);

    void dropHeldItem(bool all);

    void openNewSign(const wire::protocol::play::BlockPos& pos, std::int32_t stateId);

    [[nodiscard]] bool editSign(const wire::protocol::play::BlockPos& pos);

    void heardBySculk(std::string_view event, const wire::protocol::play::BlockPos& pos);

    void dropLootFor(const wire::common::Identifier& block, std::int32_t stateId,
                     const wire::protocol::play::BlockPos& pos, const wire::nbt::NbtCompound* carried = nullptr);

    /// parity: `Block.popResource`'s own `Mth.nextDouble(random, -0.25, 0.25)`.
    [[nodiscard]] double popJitter() const;

    [[nodiscard]] bool useLectern(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    void openLectern(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    [[nodiscard]] bool useJukebox(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    [[nodiscard]] bool placeMinecart(player::PlayerSession& session, const wire::common::Identifier& item,
                                     std::int32_t heldSlot, const storage::StoredItemStack& held,
                                     const wire::protocol::play::BlockHitResult& hit);

    [[nodiscard]] bool placeBoat(player::PlayerSession& session, const wire::common::Identifier& item,
                                 std::int32_t heldSlot, const storage::StoredItemStack& held,
                                 const wire::protocol::play::BlockHitResult& hit);

    [[nodiscard]] bool useSpawnEgg(player::PlayerSession& session, const wire::common::Identifier& egg,
                                   std::int32_t heldSlot, const storage::StoredItemStack& held,
                                   const wire::protocol::play::BlockHitResult& hit);

    void useBookshelf(player::PlayerSession& session, const wire::protocol::play::BlockHitResult& hit);

    void useShelf(player::PlayerSession& session, const wire::protocol::play::BlockHitResult& hit);

    void usePot(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    [[nodiscard]] bool placeOnCampfire(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `SnowyBlock.updateShape`.
    void refreshSnowUnder(const wire::protocol::play::BlockPos& pos);

    void refreshSnowAt(const wire::protocol::play::BlockPos& pos);

    void startDigging(const wire::protocol::play::BlockPos& pos, std::int32_t direction);
    [[nodiscard]] bool digPermitted(const wire::protocol::play::BlockPos& pos, std::int32_t direction);

    void stopDigging();

    void broadcastDestruction(const wire::protocol::play::BlockPos& pos, std::int32_t stage, float perTick);

    void useBucketOn(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot,
                     const storage::StoredItemStack& held, const wire::protocol::play::BlockHitResult& hit);

    [[nodiscard]] std::vector<std::array<double, 6>> buildersInTheWay() const;

    void primeTntAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                    const storage::StoredItemStack& held, bool wears);

    void toggleBlock(const wire::protocol::play::BlockPos& pos, float playerYaw);

    void cycleBlock(const wire::protocol::play::BlockPos& pos);

    void useBed(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t state);

    void setSpawnPointAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& bed);

    void broadcastSound(const wire::protocol::play::BlockPos& pos, const std::string& sound, float volume, float pitch);

    ClientSession* owner_;
    Hooks hooks_;
    [[nodiscard]] bool mayUseGameMasterBlocks(player::PlayerSession& session) const;

    /// parity: `updateNeighbourForOutputSignal`.
    void notifyComparators(const wire::protocol::play::BlockPos& pos);

    void sendBlockEntity(const wire::protocol::play::BlockPos& pos);

    Options options_;
    item::ItemDamageEffects damageEffects_;
    std::optional<wire::protocol::play::BlockPos> diggingAt_;
    std::int32_t diggingTicks_ = 0;
    std::int32_t diggingStage_ = -1;
    /// parity: 26.3 `ServerPlayerGameMode.destroyDirection`.
    std::int32_t diggingDirection_ = 0;
};

} // namespace basalt::server::runtime
