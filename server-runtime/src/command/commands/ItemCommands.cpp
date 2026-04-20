#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using wire::text::Component;

constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);

/// parity: `Player.drop(stack, false)`, a toss straight ahead from under the eyes.
void dropAtFeet(const CommandContext& context, const player::PlayerSession& target,
                const storage::StoredItemStack& stack) {
    const auto& worlds = context.environment().options().worlds;
    const auto found = worlds.find(target.worldName);
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(stack.id);
    if (found == worlds.end() || found->second == nullptr || !id.has_value()) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    const float sinX = wire::common::mth::sin(target.pitch * kRadians);
    const float cosX = wire::common::mth::cos(target.pitch * kRadians);
    const float sinY = wire::common::mth::sin(target.yaw * kRadians);
    const float cosY = wire::common::mth::cos(target.yaw * kRadians);
    const float angle = random.nextFloat() * (2.0F * std::numbers::pi_v<float>);
    const float spread = 0.02F * random.nextFloat();
    const float up = (-sinX * 0.3F) + 0.1F + ((random.nextFloat() - random.nextFloat()) * 0.1F);
    world::DroppedItem dropped{
        .entityId = context.environment().players().allocateEntityId(),
        .uuid = wire::common::Uuid::random(),
        .item = *id,
        .count = stack.count,
        .x = target.x,
        .y = target.y + world::fluids::eyeHeight(target.sneaking) - static_cast<double>(0.3F),
        .z = target.z,
        .velocityX = static_cast<double>((-sinY * cosX * 0.3F) + (wire::common::mth::cos(angle) * spread)),
        .velocityY = static_cast<double>(up),
        .velocityZ = static_cast<double>((cosY * cosX * 0.3F) + (wire::common::mth::sin(angle) * spread))};
    dropped.components = stack.components;
    dropped.pickupDelay = 0;
    static_cast<void>(found->second->state().spawnItem(std::move(dropped)));
}

[[nodiscard]] const std::vector<player::PlayerSession*>& targetsOf(const CommandContext& context) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>("targets");
    return found == nullptr ? none : *found;
}

} // namespace

GiveCommand::GiveCommand() : Command("give", permissionLevel::kGamemasters) {}

void GiveCommand::build(const CommandNodeBuilder& root) const {
    const auto give = [](const CommandContext& context, std::int32_t count) -> Parsed<std::int32_t> {
        const auto* parsed = context.get<ParsedItem>("item");
        if (parsed == nullptr) {
            return 0;
        }
        const reg::Item* item = reg::items::registry().byName(parsed->id);
        if (item == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("argument.item.id.invalid",
                                                                        {Component::text(parsed->id.toString())})}};
        }
        const std::vector<std::string> notDrawn = item::itemComponentCodecs::notDrawn(parsed->components);
        if (!notDrawn.empty()) {
            std::string named;
            for (const std::string& one : notDrawn) {
                named += named.empty() ? one : ", " + one;
            }
            context.source().sendSystemMessage(Component::text(
                "The item carries " + named +
                ", which no client can draw yet: it is stored and saved, but the tooltip will not show it."));
        }

        // parity: `GiveCommand.giveItem`.
        const std::int32_t maxStack = reg::itemStacking::maxStackSize(item->id, parsed->components);
        if (count > maxStack * 100) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "commands.give.failed.toomanyitems",
                    {Component::text(std::to_string(maxStack * 100)), Component::text(item->id.toString())})}};
        }
        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        for (player::PlayerSession* target : targets) {
            for (std::int32_t remaining = count; remaining > 0;) {
                const std::int32_t size = std::min(maxStack, remaining);
                remaining -= size;
                const storage::StoredItemStack stack{
                    .id = item->id.toString(), .count = size, .components = parsed->components};
                const std::int32_t leftOver = target->inventory.add(stack);
                if (leftOver > 0) {
                    dropAtFeet(
                        context, *target,
                        storage::StoredItemStack{.id = stack.id, .count = leftOver, .components = parsed->components});
                    continue;
                }
                random::RandomSource random =
                    random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
                const float pitch = (((random.nextFloat() - random.nextFloat()) * 0.7F) + 1.0F) * 2.0F;
                for (const player::PlayerSession* heard : context.environment().players().snapshot()) {
                    if (heard->worldName == target->worldName) {
                        player::playSoundFor(*heard, "entity.item.pickup", wire::protocol::play::SoundSource::Players,
                                             target->x, target->y, target->z, 0.2F, pitch,
                                             static_cast<std::int64_t>(std::random_device{}()));
                    }
                }
            }
            if (target->connection().sendInventoryAll) {
                target->connection().sendInventoryAll();
            }
        }
        context.success(
            targets.size() == 1
                ? Component::translatable("commands.give.success.single",
                                          {Component::text(std::to_string(count)), Component::text(item->id.toString()),
                                           Component::text(targets.front()->name())})
                : Component::text("Gave " + std::to_string(count) + " " + item->id.toString() + " to " +
                                  std::to_string(targets.size()) + " players."),
            true);
        return static_cast<std::int32_t>(targets.size());
    };

    root.argument("targets", playersArgument(false), [give](const CommandNodeBuilder& targets) {
        targets.argument("item", itemArgument(), [give](const CommandNodeBuilder& item) {
            item.executes([give](const CommandContext& context) { return give(context, 1); });
            item.argument("count", integerArgument(1), [give](const CommandNodeBuilder& count) {
                count.executes([give](const CommandContext& context) {
                    const auto* asked = context.get<std::int32_t>("count");
                    return give(context, asked == nullptr ? 1 : *asked);
                });
            });
        });
    });
}

ClearCommand::ClearCommand() : Command("clear", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] Parsed<std::int32_t> clear(const CommandContext& context,
                                         const std::vector<player::PlayerSession*>& targets,
                                         const ItemPredicateInput* predicate, std::int32_t maxCount) {
    std::int32_t cleared = 0;
    for (player::PlayerSession* target : targets) {
        std::int32_t remaining = maxCount < 0 ? std::numeric_limits<std::int32_t>::max() : maxCount;
        for (std::int32_t slot = 0; slot < storage::StoredInventory::kPlayerSlots; slot++) {
            const std::optional<storage::StoredItemStack>& stack = target->inventory.at(slot);
            if (!stack.has_value() || (predicate != nullptr && !predicate->matches(stack->id))) {
                continue;
            }
            if (remaining == 0) {
                cleared += stack->count;
                continue;
            }
            const std::int32_t taken = std::min(stack->count, remaining);
            cleared += taken;
            remaining -= taken;
            if (taken >= stack->count) {
                target->inventory.set(slot, std::nullopt);
            } else {
                storage::StoredItemStack left = *stack;
                left.count = stack->count - taken;
                target->inventory.set(slot, std::move(left));
            }
        }
        if (maxCount != 0 && target->connection().sendInventoryAll) {
            target->connection().sendInventoryAll();
        }
    }

    const Component who = targets.size() == 1 ? Component::text(targets.front()->name())
                                              : Component::text(std::to_string(targets.size()));
    if (cleared == 0) {
        return std::unexpected{CommandSyntaxError{
            .component =
                Component::translatable(targets.size() == 1 ? "clear.failed.single" : "clear.failed.multiple", {who})}};
    }
    const std::string kind = maxCount == 0 ? "test" : "success";
    const std::string many = targets.size() == 1 ? ".single" : ".multiple";
    context.success(
        Component::translatable("commands.clear." + kind + many, {Component::text(std::to_string(cleared)), who}),
        true);
    return cleared;
}

} // namespace

void ClearCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<player::PlayerSession*> self = context.requirePlayer();
        if (!self.has_value()) {
            return std::unexpected{self.error()};
        }
        return clear(context, {*self}, nullptr, -1);
    });
    root.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
        targets.executes([](const CommandContext& context) { return clear(context, targetsOf(context), nullptr, -1); });
        targets.argument("item", itemPredicateArgument(), [](const CommandNodeBuilder& item) {
            item.executes([](const CommandContext& context) {
                return clear(context, targetsOf(context), context.get<ItemPredicateInput>("item"), -1);
            });
            item.argument("maxCount", integerArgument(0), [](const CommandNodeBuilder& maxCount) {
                maxCount.executes([](const CommandContext& context) {
                    const auto* cap = context.get<std::int32_t>("maxCount");
                    return clear(context, targetsOf(context), context.get<ItemPredicateInput>("item"),
                                 cap == nullptr ? -1 : *cap);
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
