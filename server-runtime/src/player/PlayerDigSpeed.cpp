#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::player {

namespace {

std::optional<wire::common::Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
        wire::common::Identifier::parse(id);
    return parsed.has_value() ? std::optional<wire::common::Identifier>{*parsed} : std::nullopt;
}

} // namespace

float destroyProgressPerTickFor(const PlayerSession& session, const wire::common::Identifier& block,
                                const registry::BlockHardness* hardness,
                                const world::realBlockTags::TagMembers* blockTags, const bool underwater) {
    if (hardness == nullptr || hardness->isUnbreakable()) {
        return 0.0F;
    }
    const std::optional<storage::StoredItemStack> held = session.inventory.heldItem();
    const std::optional<wire::common::Identifier> heldItem =
        held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const auto amplifierOf = [&session](const wire::common::Identifier& effect) {
        const auto found = session.effects.find(effect);
        return found == session.effects.end() ? -1 : found->second.amplifier;
    };
    const registry::itemTools::DigConditions conditions{
        .efficiencyLevel =
            held.has_value() ? enchantmentLevel(*held, wire::common::Identifier::vanilla("efficiency")) : 0,
        .hasteAmplifier = amplifierOf(world::mobEffects::haste()),
        .miningFatigueAmplifier = amplifierOf(world::mobEffects::miningFatigue()),
        .underwater = underwater,
        .inAir = !session.onGround};
    return registry::itemTools::destroyProgressPerTick(
        heldItem.has_value() ? &*heldItem : nullptr, block, hardness->destroyTime, hardness->requiresCorrectTool,
        conditions, [blockTags](const std::string_view tag, const wire::common::Identifier& tagged) {
            return blockTags != nullptr && world::realBlockTags::contains(*blockTags, tag, tagged);
        });
}

} // namespace basalt::server::runtime::player
