#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

namespace basalt::server::runtime::entity {
namespace {

using wire::protocol::play::SetEntityDataPacket;
using Entry = SetEntityDataPacket::Entry;

/// parity: `VehicleEntity`'s three fields.
constexpr std::uint8_t kHurtIndex = 8;
constexpr std::uint8_t kHurtDirIndex = 9;
constexpr std::uint8_t kDamageIndex = 10;
constexpr std::uint8_t kOwnIndex = 13;

} // namespace

std::optional<VehicleData> vehicleDataOf(const ServerEntity& entity) {
    const bool cart = minecarts::isMinecart(entity.type());
    if (!cart && !boats::isBoat(entity.type())) {
        return std::nullopt;
    }
    VehicleData data{
        .hurtTime = entity.vehicleHurtTime, .hurtDir = entity.vehicleHurtDir, .damage = entity.vehicleDamage};
    const std::string_view path = entity.type().path();
    if (path == "furnace_minecart") {
        data.furnaceFuel = entity.cart != nullptr && entity.cart->fuel > 0;
    }
    if (path == "command_block_minecart") {
        data.command = entity.sourceNbt.has_value() ? std::string{entity.sourceNbt->getString("Command")} : "";
    }
    if (!cart) {
        data.bubbleTime = entity.boat != nullptr ? entity.boat->bubbleTime : 0;
    }
    return data;
}

void appendVehicleChanges(std::vector<Entry>& entries, const VehicleData& now, const VehicleData* before) {
    if (before == nullptr ? now.hurtTime != 0 : before->hurtTime != now.hurtTime) {
        entries.push_back(Entry{.index = kHurtIndex, .value = SetEntityDataPacket::IntValue{.value = now.hurtTime}});
    }
    if (before == nullptr ? now.hurtDir != 1 : before->hurtDir != now.hurtDir) {
        entries.push_back(Entry{.index = kHurtDirIndex, .value = SetEntityDataPacket::IntValue{.value = now.hurtDir}});
    }
    if (before == nullptr ? now.damage != 0.0F : before->damage != now.damage) {
        entries.push_back(Entry{.index = kDamageIndex, .value = SetEntityDataPacket::FloatValue{.value = now.damage}});
    }
    if (now.furnaceFuel.has_value() &&
        (before == nullptr ? *now.furnaceFuel : before->furnaceFuel != now.furnaceFuel)) {
        entries.push_back(
            Entry{.index = kOwnIndex, .value = SetEntityDataPacket::BoolValue{.value = *now.furnaceFuel}});
    }
    if (now.bubbleTime.has_value() &&
        (before == nullptr ? *now.bubbleTime != 0 : before->bubbleTime != now.bubbleTime)) {
        entries.push_back(Entry{.index = kOwnIndex, .value = SetEntityDataPacket::IntValue{.value = *now.bubbleTime}});
    }
    if (now.command.has_value() && (before == nullptr ? !now.command->empty() : before->command != now.command)) {
        entries.push_back(Entry{.index = kOwnIndex, .value = SetEntityDataPacket::StringValue{.value = *now.command}});
    }
}

} // namespace basalt::server::runtime::entity
