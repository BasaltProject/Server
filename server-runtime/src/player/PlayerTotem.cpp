#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerTotem.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <array>
#include <expected>
#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;
using wire::common::Identifier;
using wire::nbt::NbtCompound;

constexpr std::string_view kDeathProtection = "minecraft:death_protection";

/// parity: `EntityEvent.PROTECTED_FROM_DEATH`, which the client plays as the totem.
constexpr std::int8_t kProtectedFromDeath = 35;

/// parity: `DeathProtection.TOTEM_OF_UNDYING`.
[[nodiscard]] std::vector<reg::ConsumeEffect> totemEffects() {
    return {reg::ConsumeEffect{reg::ConsumeEffect::ClearAll{}},
            reg::ConsumeEffect{reg::ConsumeEffect::Apply{Identifier::vanilla("regeneration"), 900, 1, 1.0F}},
            reg::ConsumeEffect{reg::ConsumeEffect::Apply{Identifier::vanilla("absorption"), 100, 1, 1.0F}},
            reg::ConsumeEffect{reg::ConsumeEffect::Apply{Identifier::vanilla("fire_resistance"), 800, 0, 1.0F}}};
}

[[nodiscard]] std::optional<Identifier> idOf(std::string_view text) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(text);
    return id.has_value() ? std::optional{*id} : std::nullopt;
}

/// parity: `ConsumeEffect.CODEC` over the kinds a death can carry.
[[nodiscard]] std::vector<reg::ConsumeEffect> effectsOf(const NbtCompound& protection) {
    std::vector<reg::ConsumeEffect> effects;
    const wire::nbt::NbtList* list = protection.getList("death_effects");
    if (list == nullptr) {
        return effects;
    }
    for (const wire::nbt::NbtTag& entry : *list) {
        const NbtCompound* effect = entry.get<NbtCompound>();
        if (effect == nullptr) {
            continue;
        }
        const std::string_view type = effect->getString("type");
        if (type == "minecraft:clear_all_effects") {
            effects.emplace_back(reg::ConsumeEffect::ClearAll{});
        } else if (type == "minecraft:apply_effects") {
            const float probability = effect->getFloat("probability", 1.0F);
            if (const wire::nbt::NbtList* applied = effect->getList("effects"); applied != nullptr) {
                for (const wire::nbt::NbtTag& one : *applied) {
                    const NbtCompound* instance = one.get<NbtCompound>();
                    const std::optional<Identifier> id =
                        instance == nullptr ? std::nullopt : idOf(instance->getString("id"));
                    if (id.has_value()) {
                        effects.emplace_back(reg::ConsumeEffect::Apply{*id, instance->getInt("duration", 0),
                                                                       instance->getByte("amplifier", 0), probability});
                    }
                }
            }
        } else if (type == "minecraft:remove_effects") {
            if (const std::optional<Identifier> id = idOf(effect->getString("effects")); id.has_value()) {
                effects.emplace_back(reg::ConsumeEffect::Remove{*id});
            }
        } else if (type == "minecraft:teleport_randomly") {
            effects.emplace_back(reg::ConsumeEffect::TeleportRandomly{effect->getFloat("diameter", 16.0F)});
        }
    }
    return effects;
}

} // namespace

std::optional<std::vector<reg::ConsumeEffect>> deathProtectionOf(const storage::StoredItemStack& stack) {
    if (stack.count <= 0) {
        return std::nullopt;
    }
    if (const NbtCompound* explicitly = stack.components.getCompound(kDeathProtection); explicitly != nullptr) {
        return effectsOf(*explicitly);
    }
    // parity: a patch can take the default away, written `!minecraft:death_protection`.
    if (stack.components.find("!" + std::string{kDeathProtection}) != nullptr) {
        return std::nullopt;
    }
    if (stack.id == "minecraft:totem_of_undying") {
        return totemEffects();
    }
    return std::nullopt;
}

bool checkTotemDeathProtection(PlayerSession& player, const TotemContext& context) {
    if (context.bypassesInvulnerability) {
        return false;
    }
    std::vector<std::int32_t> slots{PlayerInventory::kHotbarStart + player.inventory.heldSlot(),
                                    PlayerInventory::kOffhandSlot};
    if (context.worksAnywhereInInventory) {
        for (std::int32_t slot = 0; slot < PlayerInventory::kMainSlots; ++slot) {
            if (slot != slots.front()) {
                slots.push_back(slot);
            }
        }
    }
    for (const std::int32_t slot : slots) {
        const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
        const std::optional<std::vector<reg::ConsumeEffect>> effects =
            held.has_value() ? deathProtectionOf(*held) : std::nullopt;
        if (!effects.has_value()) {
            continue;
        }
        if (context.resurrectAllowed && !context.resurrectAllowed(player, slot == PlayerInventory::kOffhandSlot)) {
            return false;
        }
        storage::StoredItemStack left = *held;
        left.count -= 1;
        player.inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
        if (context.slotChanged) {
            context.slotChanged(player, slot);
        }
        statAwards::used(player, held->id);
        if (context.usedTotem) {
            context.usedTotem(player, *held);
        }
        if (context.interactFinished) {
            context.interactFinished(player);
        }
        player.setHealth(1.0F);
        applyConsumeEffectsFor(player, *effects, context.nextFloat);
        if (player.connection().syncHealth) {
            player.connection().syncHealth();
        }
        if (context.registry != nullptr) {
            const wire::protocol::play::EntityEventPacket packet{.entityId = player.entityId(),
                                                                 .eventId = kProtectedFromDeath};
            for (const PlayerSession* watcher : context.registry->snapshot()) {
                if (watcher->worldName == player.worldName && watcher->connection().sendPlayPacket) {
                    watcher->connection().sendPlayPacket(
                        wire::protocol::play::EntityEventPacket::kId,
                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                }
            }
        }
        return true;
    }
    return false;
}

} // namespace basalt::server::runtime::player
