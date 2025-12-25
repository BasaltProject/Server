#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

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
    };

    /// The rolls placing and breaking make, none of which is part of the world's own stream.
    struct Options {
        std::function<double()> nextDouble;
        std::function<std::int64_t()> nextSeed;
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

    /// parity: `Level.updateNeighborsAt`, which is what makes a torch fall off a broken wall.
    void notifyNeighbours(const wire::protocol::play::BlockPos& pos);

    [[nodiscard]] bool isAirLike(std::int32_t stateId) const;

    /// parity: the little upward push a thrown item leaves with.
    [[nodiscard]] double tossLift() const;

    /// Gets this player out of the bed they are in.
    void wakeUp();

    void spawnDroppedItem(const wire::common::Identifier& item, std::int32_t count, double x, double y, double z,
                          double velocityX = 0.0, double velocityY = 0.0, double velocityZ = 0.0,
                          std::int32_t pickupDelay = 40, wire::nbt::NbtCompound components = {});

    void handleUseItemOn(const wire::protocol::play::ServerboundUseItemOnPacket& useItemOn);

    void handleUseItem(const wire::protocol::play::ServerboundUseItemPacket& useItem);

    void handleSwing(std::int32_t hand);

    /// One tick of whatever the player is breaking.
    void tickDigging(player::PlayerSession& session);

private:
    ClientSession* owner_;
    Hooks hooks_;
    Options options_;
    /// What is being broken, since when, and how far along it is.
    std::optional<wire::protocol::play::BlockPos> digging_;
    double digProgress_ = 0.0;
    std::int32_t digTicks_ = 0;
};

} // namespace basalt::server::runtime
