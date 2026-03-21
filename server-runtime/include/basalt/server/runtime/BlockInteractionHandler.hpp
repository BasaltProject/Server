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

/// Breaking, placing, using and the block state that follows from each: the digging clock, the
/// updates everyone watching is sent, and what a broken block leaves on the ground.
class BlockInteractionHandler {
public:
    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId)>
            breakAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item)>
            placeAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item)>
            interactAllowed;
        std::function<bool(const player::PlayerSession& player, const std::string& item)> dropAllowed;
        /// Whether the dig may start at all, which is where a protection plugin says no first.
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)>
            digAllowed;
        /// Whether the held item may take this much wear, and whether that would finish it.
        std::function<bool(const player::PlayerSession& player, const std::string& item, std::int32_t amount,
                           bool broken)>
            itemDamageAllowed;
        /// Whether the sign may be written, and what it really says: `lines` is the plugin's to edit.
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<std::string>& lines)>
            signChange;
        /// Whether a bucket may be filled or emptied here.
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& item, bool filling)>
            bucketAllowed;
        std::function<bool(const player::PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)>
            bedAllowed;
        /// Whether a stack may be put into the world, and why it is going there.
        std::function<bool(std::string_view worldName, double x, double y, double z, const std::string& item,
                           std::int32_t count, std::string_view cause)>
            itemSpawnAllowed;
    };

    /// The rolls placing and breaking make, none of which is part of the world's own stream.
    struct Options {
        std::function<double()> nextDouble;
        std::function<std::int64_t()> nextSeed;
        std::function<bool()> nextBool;
    };

    explicit BlockInteractionHandler(ClientSession& owner, Hooks hooks = {}, Options options = {});

    /// parity: `ServerboundPlayerActionPacket`, which is starting, cancelling and finishing a dig,
    /// dropping what is held, and swapping hands.
    void handlePlayerAction(const wire::protocol::play::ServerboundPlayerActionPacket& action);

    /// One block, its new state, to everyone who can see it.
    void broadcastBlockUpdate(const wire::protocol::play::BlockPos& pos, std::int32_t stateId);

    /// The sounds and particles a block change owes whoever is watching.
    void broadcastEffects(const wire::protocol::play::BlockPos& pos, std::int32_t stateId,
                          const std::vector<world::BlockEffect>& effects);

    void writeSign(const wire::protocol::play::SignUpdatePacket& update);

    /// parity: `ComposterBlock.useItemOn` and `useWithoutItem`: one compostable item in, or the bone
    /// meal out of a composter that is ready. Answers whether the click was the composter's.
    bool useComposter(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos);

    /// parity: `handleSetCommandBlock`, the screen a command block opens. Refused outright for anyone
    /// who is not an operator in creative, which is what `canUseGameMasterBlocks` asks.
    void programCommandBlock(const wire::protocol::play::SetCommandBlockPacket& packet);

    /// parity: `Level.updateNeighborsAt`, which is what makes a torch fall off a broken wall.
    void notifyNeighbours(const wire::protocol::play::BlockPos& pos);

    /// The same, for a square whose old block is known to have gone, so the cascade can run the half
    /// that belongs to the block that left.
    void notifyNeighboursAfterRemoval(const wire::protocol::play::BlockPos& pos, std::int32_t removedStateId);

    [[nodiscard]] bool isAirLike(std::int32_t stateId) const;

    /// parity: the little upward push a thrown item leaves with.
    [[nodiscard]] double tossLift() const;

    /// Gets this player out of the bed they are in.
    /// What sleeping needs of the session: the spawn point, the plugin's say, and the bang a bed
    /// makes where it will not have one.
    [[nodiscard]] player::SleepContext sleepContext();

    void wakeUp();

    void spawnDroppedItem(const wire::common::Identifier& item, std::int32_t count, double x, double y, double z,
                          double velocityX = 0.0, double velocityY = 0.0, double velocityZ = 0.0,
                          std::int32_t pickupDelay = 40, wire::nbt::NbtCompound components = {});

    void handleUseItemOn(const wire::protocol::play::ServerboundUseItemOnPacket& useItemOn);

    /// parity: `ITEM_USED_ON_BLOCK`, raised by the items whose use changes the block clicked.
    void awardItemUsedOn(const player::PlayerSession& session, const std::optional<std::int32_t>& clickedState,
                         const wire::common::Identifier& item) const;

    void handleUseItem(const wire::protocol::play::ServerboundUseItemPacket& useItem);

    /// parity: `handlePunch`, an arm swung at the air or a block.
    void handlePunch();

    /// One tick of whatever the player is breaking.
    void tickDigging(player::PlayerSession& session);

    /// What breaking a block needs of the server, built from what this connection knows. Public
    /// because everything that has to reach a whole world takes one of these, not only breaking.
    [[nodiscard]] player::BreakContext breakContext();

    /// Wears the held tool by `cost`, as using it on a mob does.
    void wearHeldItem(std::int32_t cost);

private:
    /// The seed a loot roll, a fire's delay or a sound's own random starts from.
    [[nodiscard]] std::int64_t nextSeed() const;

    [[nodiscard]] double nextDouble() const;

    /// Whether the block in front of this player comes apart in a single tick.
    [[nodiscard]] bool breaksInOneTick(const wire::common::Identifier* block,
                                       const registry::BlockHardness* hardness) const;

    /// How much of a block this player takes off per tick, tool, enchantments and effects included.
    [[nodiscard]] float destroyProgressPerTick(const wire::common::Identifier& block,
                                               const registry::BlockHardness* hardness) const;

    /// parity: `Player.getDestroySpeed` asks whether the eyes are under water, not the feet.
    [[nodiscard]] bool eyesInWater() const;

    void wearHeldItemOnBreak(const wire::common::Identifier& block);

    /// Sets fire where the flint and steel points, and lights a Nether portal if it lands in a frame.
    void igniteAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                  const storage::StoredItemStack& held);

    /// Grows what was clicked, when bone meal does anything to it. Answers whether it did.
    [[nodiscard]] bool useBoneMeal(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                   std::int32_t heldSlot, const storage::StoredItemStack& held);

    /// parity: `ItemFrameItem.useOn`, which hangs the frame in the square next to the face clicked
    /// and only when it has something to hang on. Answers whether it went up.
    [[nodiscard]] bool hangFrame(player::PlayerSession& session, const wire::protocol::play::BlockHitResult& hit,
                                 std::int32_t heldSlot, const storage::StoredItemStack& held,
                                 const wire::common::Identifier& type);

    /// Puts one piece of glowstone into a respawn anchor. Answers whether it took it.
    [[nodiscard]] bool chargeAnchor(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                    std::int32_t heldSlot, const storage::StoredItemStack& held);

    /// Waxes what was clicked. Answers whether the block took the wax.
    [[nodiscard]] bool useHoneycomb(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                                    std::int32_t heldSlot, const storage::StoredItemStack& held);

    /// Strips a log, scrapes a step of oxidation, or takes the wax off. Answers whether it did one.
    [[nodiscard]] bool useAxe(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos,
                              std::int32_t heldSlot, const storage::StoredItemStack& held);

    /// Arms an end portal frame, and opens the portal when that was the twelfth eye.
    void placeEnderEye(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                       const storage::StoredItemStack& held);

    /// parity: `BellBlock.onHit` with `isProperHit`, which is what a bell can swing towards.
    [[nodiscard]] bool ringBell(const wire::protocol::play::BlockHitResult& hit);

    void dropHeldItem(bool all);

    /// A newly placed sign: its block entity, then the editor for whoever put it there.
    void openNewSign(const wire::protocol::play::BlockPos& pos, std::int32_t stateId);

    /// Reopens an existing sign, unless it has been waxed. Answers whether it did.
    [[nodiscard]] bool editSign(const wire::protocol::play::BlockPos& pos);

    /// Tells the sculk sensors around `pos` that something happened there.
    void heardBySculk(std::string_view event, const wire::protocol::play::BlockPos& pos);

    /// Rolls `block`'s loot table and drops each resulting stack where it stood.
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

    /// Takes the snow off the block under `pos`, or puts it on. parity: `SnowyBlock.updateShape`.
    void refreshSnowUnder(const wire::protocol::play::BlockPos& pos);

    void refreshSnowAt(const wire::protocol::play::BlockPos& pos);

    void startDigging(const wire::protocol::play::BlockPos& pos);

    void stopDigging();

    void broadcastDestruction(const wire::protocol::play::BlockPos& pos, std::int32_t stage, float perTick);

    void useBucketOn(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot,
                     const storage::StoredItemStack& held, const wire::protocol::play::BlockHitResult& hit);

    /// The boxes of everything in this world that stops a block being placed inside it.
    [[nodiscard]] std::vector<std::array<double, 6>> buildersInTheWay() const;

    /// Lighting a tnt block by hand, which takes the block away and leaves the entity.
    void primeTntAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t heldSlot,
                    const storage::StoredItemStack& held, bool wears);

    void toggleBlock(const wire::protocol::play::BlockPos& pos, float playerYaw);

    void cycleBlock(const wire::protocol::play::BlockPos& pos);

    void useBed(player::PlayerSession& session, const wire::protocol::play::BlockPos& pos, std::int32_t state);

    void setSpawnPointAt(player::PlayerSession& session, const wire::protocol::play::BlockPos& bed);

    /// A sound this player and everyone around them hears, which is what a lever and a diode make.
    void broadcastSound(const wire::protocol::play::BlockPos& pos, const std::string& sound, float volume, float pitch);

    ClientSession* owner_;
    Hooks hooks_;
    [[nodiscard]] bool mayUseGameMasterBlocks(player::PlayerSession& session) const;

    /// parity: `updateNeighbourForOutputSignal`, the half of a block change that a comparator hears.
    void notifyComparators(const wire::protocol::play::BlockPos& pos);

    /// One block entity's data to everyone watching, which is how a client is told what to put in
    /// the screen it is about to open.
    void sendBlockEntity(const wire::protocol::play::BlockPos& pos);

    Options options_;
    /// What an enchantment takes off a durability cost, read from the definitions themselves.
    item::ItemDamageEffects damageEffects_;
    /// Where this player is digging, for how long, and which crack stage everyone else has seen.
    std::optional<wire::protocol::play::BlockPos> diggingAt_;
    std::int32_t diggingTicks_ = 0;
    std::int32_t diggingStage_ = -1;
};

} // namespace basalt::server::runtime
