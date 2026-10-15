#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ContainerSetDataPacket;
using wire::protocol::play::OpenScreenPacket;
using wire::text::Component;

/// parity: `BeaconMenu.encodeEffect`.
[[nodiscard]] std::int16_t encodeEffect(const std::string& effect) {
    if (effect.empty()) {
        return 0;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(effect);
    if (!id.has_value()) {
        return 0;
    }
    const std::optional<std::int32_t> protocolId = world::mobEffects::protocolIdOf(*id);
    return protocolId.has_value() ? static_cast<std::int16_t>(*protocolId + 1) : std::int16_t{0};
}

[[nodiscard]] std::string nameOfEffect(const std::optional<std::int32_t>& protocolId) {
    if (!protocolId.has_value()) {
        return {};
    }
    const std::optional<Identifier> effect = world::mobEffects::byProtocolId(*protocolId);
    return effect.has_value() ? effect->toString() : std::string{};
}

} // namespace

world::beacons::Stored ContainerMenuController::beaconState() const {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!openBlock.has_value() || world == nullptr) {
        return {};
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(openBlock->x >> 4, openBlock->z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(openBlock->x & 15, openBlock->y, openBlock->z & 15);
    return entity == nullptr ? world::beacons::Stored{} : world::beacons::read(entity->data());
}

void ContainerMenuController::openBeaconMenu(PlayerSession& session, const BlockPos& pos) {
    returnCraftingGrid(session);
    openMenu = Menu::beacon(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = false;
    openEnchantment_ = false;
    openBeacon_ = true;
    beaconPayment_.reset();
    openBlockSize = 1;
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kBeacon,
                         .title = Component::translatable("container.beacon")}
            .write(buffer);
    });
    sendInventory();
    sendBeaconNumbers();
}

void ContainerMenuController::setBeaconEffects(const std::optional<std::int32_t>& primary,
                                               const std::optional<std::int32_t>& secondary) {
    // parity: `updateEffects` does nothing at all without something in the payment slot.
    if (!openBeacon_ || !beaconPayment_.has_value()) {
        return;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || !openBlock.has_value()) {
        return;
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(openBlock->x >> 4, openBlock->z >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(openBlock->x & 15, openBlock->y, openBlock->z & 15);
    if (entity == nullptr) {
        return;
    }
    world::beacons::Stored stored = world::beacons::read(entity->data());
    const std::string first = nameOfEffect(primary);
    const std::string second = nameOfEffect(secondary);
    if (!world::beacons::allows(first, second, stored.levels)) {
        return;
    }
    stored.primary = first;
    stored.secondary = second;
    world::beacons::write(stored, entity->data());
    chunk->markDirty();
    // parity: the payment goes, one item of it, and the window stays open.
    if (--beaconPayment_->count <= 0) {
        beaconPayment_.reset();
    }
    sendInventory();
    sendBeaconNumbers();
}

void ContainerMenuController::sendBeaconNumbers() {
    if (!openBeacon_) {
        return;
    }
    const world::beacons::Stored stored = beaconState();
    const auto send = [this](const std::int16_t id, const std::int16_t value) {
        owner_->sendPlayPacket(ContainerSetDataPacket::kId, [this, id, value](wire::io::ByteBuffer& buffer) {
            ContainerSetDataPacket{.containerId = openMenu.containerId(), .id = id, .value = value}.write(buffer);
        });
    };
    send(ContainerSetDataPacket::kBeaconLevels, static_cast<std::int16_t>(stored.levels));
    send(ContainerSetDataPacket::kBeaconPrimary, encodeEffect(stored.primary));
    send(ContainerSetDataPacket::kBeaconSecondary, encodeEffect(stored.secondary));
}

} // namespace basalt::server::runtime
