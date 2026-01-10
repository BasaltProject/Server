#include "ItemSlots.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

/// parity: `ItemCommands.MAX_ALLOWED_ITEMSTACKS`, which is what a slot can be asked to hold.
constexpr std::int32_t kMaxCount = 99;

[[nodiscard]] const std::vector<player::PlayerSession*>& targetsOf(const CommandContext& context,
                                                                   const std::string& name) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>(name);
    return found == nullptr ? none : *found;
}

/// The block a coordinate argument names, resolved against wherever the source stands.
[[nodiscard]] std::array<std::int32_t, 3> blockAt(const CommandContext& context, const std::string& name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0}
                                  : coordinates->resolve(context.source().position());
}

/// parity: `/item ... <modifier>` runs a loot function over the stack.
///
/// There are no item modifiers in this build, so the form is declared and refused by name rather
/// than quietly copying the stack unchanged.
[[nodiscard]] Parsed<std::int32_t> noModifiers(const CommandContext& context) {
    const auto* modifier = context.get<Identifier>("modifier");
    return std::unexpected{
        CommandSyntaxError{.component = Component::text("Item modifiers are not loaded in this build, so '" +
                                                        (modifier == nullptr ? std::string{} : modifier->toString()) +
                                                        "' cannot be applied.")}};
}

void writeInto(player::PlayerSession& target, std::int32_t slot, const storage::StoredItemStack& stack) {
    target.inventory.set(slot, stack);
    if (target.connection().sendInventoryAll) {
        target.connection().sendInventoryAll();
    }
}

void reportEntities(const CommandContext& context, const std::vector<player::PlayerSession*>& targets,
                    const std::string& id) {
    context.success(
        targets.size() == 1
            ? Component::translatable("commands.item.entity.set.success.single",
                                      {Component::text(targets.front()->name()), Component::text(id)})
            : Component::translatable("commands.item.entity.set.success.multiple",
                                      {Component::text(std::to_string(targets.size())), Component::text(id)}),
        true);
}

void reportBlock(const CommandContext& context, const std::array<std::int32_t, 3>& at, const std::string& id) {
    context.success(
        Component::translatable("commands.item.block.set.success",
                                {Component::text(std::to_string(at[0])), Component::text(std::to_string(at[1])),
                                 Component::text(std::to_string(at[2])), Component::text(id)}),
        true);
}

} // namespace

ItemCommand::ItemCommand() : Command("item", permissionLevel::kGamemasters) {}

void ItemCommand::build(const CommandNodeBuilder& root) const {
    /// The stack this form reads from, whether that is a container or another player.
    const auto readSource = [](const CommandContext& context) -> Parsed<storage::StoredItemStack> {
        const Parsed<std::int32_t> slot = itemSlots::slotOf(context, "sourceSlot");
        if (!slot.has_value()) {
            return std::unexpected{slot.error()};
        }
        const std::optional<storage::StoredItemStack> found = [&]() -> std::optional<storage::StoredItemStack> {
            if (context.get<BlockCoordinates>("source") != nullptr) {
                for (const auto& [at, stack] : itemSlots::containerItems(context, blockAt(context, "source"))) {
                    if (at == *slot) {
                        return stack;
                    }
                }
                return std::nullopt;
            }
            const std::vector<player::PlayerSession*>& players = targetsOf(context, "source");
            return players.empty() ? std::nullopt : players.front()->inventory.at(*slot);
        }();
        if (!found.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.item.source.no_such_slot")}};
        }
        return *found;
    };

    /// Copies a stack out of a block or a player and into the destination this form names.
    const auto copyInto = [readSource](const CommandContext& context, bool entityTargets) -> Parsed<std::int32_t> {
        const Parsed<storage::StoredItemStack> source = readSource(context);
        if (!source.has_value()) {
            return std::unexpected{source.error()};
        }
        const Parsed<std::int32_t> slot = itemSlots::slotOf(context, "slot");
        if (!slot.has_value()) {
            return std::unexpected{slot.error()};
        }
        if (entityTargets) {
            const std::vector<player::PlayerSession*>& targets = targetsOf(context, "targets");
            for (player::PlayerSession* target : targets) {
                writeInto(*target, *slot, *source);
            }
            reportEntities(context, targets, source->id);
            return static_cast<std::int32_t>(targets.size());
        }
        const std::array<std::int32_t, 3> at = blockAt(context, "pos");
        if (const std::optional<CommandSyntaxError> failure =
                itemSlots::writeContainer(context, at, {{*slot, *source}});
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        reportBlock(context, at, source->id);
        return 1;
    };

    const auto replace = [](const CommandContext& context, std::int32_t count) -> Parsed<std::int32_t> {
        const Parsed<std::int32_t> slot = itemSlots::slotOf(context, "slot");
        if (!slot.has_value()) {
            return std::unexpected{slot.error()};
        }
        const auto* parsed = context.get<ParsedItem>("item");
        if (parsed == nullptr) {
            return 0;
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
        const std::vector<player::PlayerSession*>& targets = targetsOf(context, "targets");
        for (player::PlayerSession* target : targets) {
            writeInto(*target, *slot,
                      storage::StoredItemStack{
                          .id = parsed->id.toString(), .count = count, .components = parsed->components});
        }
        reportEntities(context, targets, parsed->id.toString());
        return static_cast<std::int32_t>(targets.size());
    };

    const auto replaceInBlock = [](const CommandContext& context, std::int32_t count) -> Parsed<std::int32_t> {
        const Parsed<std::int32_t> slot = itemSlots::slotOf(context, "slot");
        if (!slot.has_value()) {
            return std::unexpected{slot.error()};
        }
        const auto* parsed = context.get<ParsedItem>("item");
        if (parsed == nullptr) {
            return 0;
        }
        const std::array<std::int32_t, 3> at = blockAt(context, "pos");
        const storage::StoredItemStack stack{
            .id = parsed->id.toString(), .count = count, .components = parsed->components};
        if (const std::optional<CommandSyntaxError> failure = itemSlots::writeContainer(context, at, {{*slot, stack}});
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        reportBlock(context, at, stack.id);
        return 1;
    };

    /// `from block|entity <source> <sourceSlot> [<modifier>]`, shared by both destinations.
    const auto sources = [](const CommandNodeBuilder& node, const CommandExecutor& run) {
        node.literal("from", [run](const CommandNodeBuilder& from) {
            from.literal("block", [run](const CommandNodeBuilder& form) {
                form.argument("source", blockPosArgument(), [run](const CommandNodeBuilder& source) {
                    source.argument("sourceSlot", itemSlotArgument(), [run](const CommandNodeBuilder& slot) {
                        slot.suggestsFromServer(
                            [](const SuggestionContext& context) { return context.matching(itemSlots::names()); });
                        slot.executes(run);
                        slot.argument("modifier", lootModifierArgument(),
                                      [](const CommandNodeBuilder& modifier) { modifier.executes(noModifiers); });
                    });
                });
            });
            from.literal("entity", [run](const CommandNodeBuilder& form) {
                form.argument("source", playersArgument(true), [run](const CommandNodeBuilder& source) {
                    source.argument("sourceSlot", itemSlotArgument(), [run](const CommandNodeBuilder& slot) {
                        slot.suggestsFromServer(
                            [](const SuggestionContext& context) { return context.matching(itemSlots::names()); });
                        slot.executes(run);
                        slot.argument("modifier", lootModifierArgument(),
                                      [](const CommandNodeBuilder& modifier) { modifier.executes(noModifiers); });
                    });
                });
            });
        });
    };

    /// The slot argument both `replace` and `modify` hang their forms from.
    const auto slotNode = [](const CommandNodeBuilder& node,
                             const std::function<void(const CommandNodeBuilder&)>& build) {
        node.argument("slot", itemSlotArgument(), [build](const CommandNodeBuilder& slot) {
            slot.suggestsFromServer(
                [](const SuggestionContext& context) { return context.matching(itemSlots::names()); });
            build(slot);
        });
    };

    root.literal("replace", [copyInto, replace, replaceInBlock, sources, slotNode](const CommandNodeBuilder& form) {
        form.literal("entity", [copyInto, replace, sources, slotNode](const CommandNodeBuilder& entity) {
            entity.argument(
                "targets", playersArgument(false),
                [copyInto, replace, sources, slotNode](const CommandNodeBuilder& targets) {
                    slotNode(targets, [copyInto, replace, sources](const CommandNodeBuilder& slot) {
                        slot.literal("with", [replace](const CommandNodeBuilder& with) {
                            with.argument("item", itemArgument(), [replace](const CommandNodeBuilder& item) {
                                item.executes([replace](const CommandContext& context) { return replace(context, 1); });
                                item.argument("count", integerArgument(1, kMaxCount),
                                              [replace](const CommandNodeBuilder& count) {
                                                  count.executes([replace](const CommandContext& context) {
                                                      const auto* asked = context.get<std::int32_t>("count");
                                                      return replace(context, asked == nullptr ? 1 : *asked);
                                                  });
                                              });
                            });
                        });
                        sources(slot, [copyInto](const CommandContext& context) { return copyInto(context, true); });
                    });
                });
        });
        form.literal("block", [copyInto, replaceInBlock, sources, slotNode](const CommandNodeBuilder& block) {
            block.argument(
                "pos", blockPosArgument(),
                [copyInto, replaceInBlock, sources, slotNode](const CommandNodeBuilder& pos) {
                    slotNode(pos, [copyInto, replaceInBlock, sources](const CommandNodeBuilder& slot) {
                        slot.literal("with", [replaceInBlock](const CommandNodeBuilder& with) {
                            with.argument("item", itemArgument(), [replaceInBlock](const CommandNodeBuilder& item) {
                                item.executes([replaceInBlock](const CommandContext& context) {
                                    return replaceInBlock(context, 1);
                                });
                                item.argument("count", integerArgument(1, kMaxCount),
                                              [replaceInBlock](const CommandNodeBuilder& count) {
                                                  count.executes([replaceInBlock](const CommandContext& context) {
                                                      const auto* asked = context.get<std::int32_t>("count");
                                                      return replaceInBlock(context, asked == nullptr ? 1 : *asked);
                                                  });
                                              });
                            });
                        });
                        sources(slot, [copyInto](const CommandContext& context) { return copyInto(context, false); });
                    });
                });
        });
    });

    root.literal("modify", [slotNode](const CommandNodeBuilder& form) {
        form.literal("entity", [slotNode](const CommandNodeBuilder& entity) {
            entity.argument("targets", playersArgument(false), [slotNode](const CommandNodeBuilder& targets) {
                slotNode(targets, [](const CommandNodeBuilder& slot) {
                    slot.argument("modifier", lootModifierArgument(),
                                  [](const CommandNodeBuilder& modifier) { modifier.executes(noModifiers); });
                });
            });
        });
        form.literal("block", [slotNode](const CommandNodeBuilder& block) {
            block.argument("pos", blockPosArgument(), [slotNode](const CommandNodeBuilder& pos) {
                slotNode(pos, [](const CommandNodeBuilder& slot) {
                    slot.argument("modifier", lootModifierArgument(),
                                  [](const CommandNodeBuilder& modifier) { modifier.executes(noModifiers); });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
