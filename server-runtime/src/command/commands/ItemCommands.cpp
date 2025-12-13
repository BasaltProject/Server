#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <limits>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using wire::text::Component;

/// Vanilla's own cap on `/give`, which exists so a typo cannot ask for two billion items.
constexpr std::int32_t kMaxCount = 6400;

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

        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        for (player::PlayerSession* target : targets) {
            const std::int32_t leftOver = target->inventory.add(
                storage::StoredItemStack{.id = item->id.toString(), .count = count, .components = parsed->components});
            // Nothing is dropped on the floor, because there are no item entities yet. Saying so
            // beats a silent disappearance the operator only notices later.
            if (leftOver > 0) {
                context.source().sendSystemMessage(Component::text(
                    target->name() + "'s inventory was full: " + std::to_string(leftOver) + " of " +
                    std::to_string(count) + " did not fit and were lost, since dropped items do not exist yet."));
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
            item.argument("count", integerArgument(1, kMaxCount), [give](const CommandNodeBuilder& count) {
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
                // maxCount 0 is vanilla's "count only" form: nothing is taken, but everything that
                // would have been is still counted.
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
