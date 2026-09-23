#include "ItemSlots.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;
using Stack = std::optional<storage::StoredItemStack>;

/// parity: `ItemCommands`' `with <item> <count>`, one to ninety-nine.
constexpr std::int32_t kMaxCount = 99;

/// parity: `SlotRanges`' offsets for a player: the hotbar at 0, the rest of the inventory at 9, the
/// held hand at 98, the armour at 100 and the ender chest at 200.
constexpr std::int32_t kMainhand = 98;
constexpr std::int32_t kOffhand = 99;
constexpr std::int32_t kFeet = 100;
constexpr std::int32_t kHead = 103;
constexpr std::int32_t kEnderChest = 200;

/// One slot of whatever holds items, read and written by its vanilla id.
struct Slots {
    std::function<std::optional<Stack>(std::int32_t id)> read;
    std::function<bool(std::int32_t id, const Stack& stack)> write;
    std::function<void()> flush;
};

[[nodiscard]] bool isEmpty(const Stack& stack) { return !stack.has_value() || stack->count <= 0; }

/// parity: `Player.getSlot`, a player's slots by the ids `SlotRanges` hands out.
[[nodiscard]] Slots playerSlots(player::PlayerSession& player) {
    using player::PlayerInventory;
    const auto storageOf = [&player](std::int32_t id) -> std::optional<std::int32_t> {
        if (id >= 0 && id < 9) {
            return PlayerInventory::kHotbarStart + id;
        }
        if (id >= 9 && id < 36) {
            return id - 9;
        }
        if (id == kMainhand) {
            return PlayerInventory::kHotbarStart + player.inventory.heldSlot();
        }
        if (id == kOffhand) {
            return PlayerInventory::kOffhandSlot;
        }
        if (id >= kFeet && id <= kHead) {
            return 36 + (id - kFeet);
        }
        return std::nullopt;
    };
    const auto enderOf = [](std::int32_t id) -> std::optional<std::size_t> {
        if (id >= kEnderChest && id < kEnderChest + storage::StoredPlayer::kEnderChestSlots) {
            return static_cast<std::size_t>(id - kEnderChest);
        }
        return std::nullopt;
    };
    return Slots{.read = [&player, storageOf, enderOf](std::int32_t id) -> std::optional<Stack> {
                     if (const std::optional<std::int32_t> slot = storageOf(id); slot.has_value()) {
                         return player.inventory.at(*slot);
                     }
                     if (const std::optional<std::size_t> slot = enderOf(id); slot.has_value()) {
                         return player.enderChest[*slot];
                     }
                     return std::nullopt;
                 },
                 .write =
                     [&player, storageOf, enderOf](std::int32_t id, const Stack& stack) {
                         const Stack stored = isEmpty(stack) ? std::nullopt : stack;
                         if (const std::optional<std::int32_t> slot = storageOf(id); slot.has_value()) {
                             player.inventory.set(*slot, stored);
                             return true;
                         }
                         if (const std::optional<std::size_t> slot = enderOf(id); slot.has_value()) {
                             player.enderChest[*slot] = stored;
                             return true;
                         }
                         return false;
                     },
                 .flush =
                     [&player] {
                         if (player.connection().sendInventoryAll) {
                             player.connection().sendInventoryAll();
                         }
                     }};
}

/// A container's slots, with every write gathered and sent through `writeContainer` at the end.
struct BlockSlots {
    std::vector<std::pair<std::int32_t, storage::StoredItemStack>> held;
    std::vector<std::pair<std::int32_t, storage::StoredItemStack>> written;
    std::int32_t size = 0;
};

/// The one ItemCommands flow: where the slots are, whose they are, and what to call them.
struct Target {
    std::string name;
    std::optional<std::array<std::int32_t, 3>> block;
    player::PlayerSession* player = nullptr;
};

[[nodiscard]] std::array<std::int32_t, 3> blockAt(const CommandContext& context, const std::string& name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

/// parity: `BlockItemAccessor.getContainer`: a block entity that is a container, or the error.
[[nodiscard]] Parsed<std::int32_t> containerSize(const CommandContext& context, const std::array<std::int32_t, 3>& at,
                                                 std::string_view error) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    const std::optional<std::int32_t> state = (*world)->state().blockState(at[0], at[1], at[2]);
    const Identifier* block = state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    const std::optional<std::int32_t> size = block == nullptr ? std::nullopt : world::containers::sizeOf(block->path());
    if (!size.has_value()) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable(std::string{error}, {Component::text(std::to_string(at[0])),
                                                                      Component::text(std::to_string(at[1])),
                                                                      Component::text(std::to_string(at[2]))})}};
    }
    return *size;
}

/// The targets or sources a `block <pos>` or `entity <targets>` argument names.
[[nodiscard]] Parsed<std::vector<Target>> targetsOf(const CommandContext& context, const std::string& name,
                                                    bool asSource) {
    if (context.get<BlockCoordinates>(name) != nullptr) {
        const std::array<std::int32_t, 3> at = blockAt(context, name);
        const Parsed<std::int32_t> size = containerSize(
            context, at, asSource ? "commands.item.source.not_a_container" : "commands.item.target.not_a_container");
        if (!size.has_value()) {
            return std::unexpected{size.error()};
        }
        return std::vector<Target>{Target{.block = at}};
    }
    std::vector<Target> targets;
    if (const auto* entities = context.get<std::vector<CommandTarget>>(name); entities != nullptr) {
        for (const CommandTarget& one : *entities) {
            // Only a player has slots here: every other entity reads as one with none.
            targets.push_back(Target{.name = one.name(), .player = one.player()});
        }
    }
    return targets;
}

/// Runs `body` over one target's slots, and writes what changed back.
template <typename Body>
void withSlots(const CommandContext& context, const Target& target, world::RealRecipes* recipes, Body&& body) {
    if (target.player != nullptr) {
        Slots slots = playerSlots(*target.player);
        body(slots);
        slots.flush();
        return;
    }
    if (!target.block.has_value()) {
        Slots none{.read = [](std::int32_t) -> std::optional<Stack> { return std::nullopt; },
                   .write = [](std::int32_t, const Stack&) { return false; },
                   .flush = [] {}};
        body(none);
        return;
    }
    BlockSlots block{.held = itemSlots::containerItems(context, *target.block),
                     .size = containerSize(context, *target.block, "").value_or(0)};
    Slots slots{.read = [&block](std::int32_t id) -> std::optional<Stack> {
                    if (id < 0 || id >= block.size) {
                        return std::nullopt;
                    }
                    for (auto it = block.written.rbegin(); it != block.written.rend(); ++it) {
                        if (it->first == id) {
                            return isEmpty(it->second) ? Stack{} : Stack{it->second};
                        }
                    }
                    for (const auto& [slot, stack] : block.held) {
                        if (slot == id) {
                            return Stack{stack};
                        }
                    }
                    return Stack{};
                },
                .write =
                    [&block](std::int32_t id, const Stack& stack) {
                        if (id < 0 || id >= block.size) {
                            return false;
                        }
                        std::erase_if(block.written, [id](const auto& one) { return one.first == id; });
                        block.written.emplace_back(id, stack.value_or(storage::StoredItemStack{.count = 0}));
                        return true;
                    },
                .flush = [] {}};
    body(slots);
    if (!block.written.empty()) {
        static_cast<void>(itemSlots::writeContainer(context, *target.block, block.written, recipes));
    }
}

/// parity: `ItemProvider`: `of` stops when the items run out, `cycle` starts over, and `override`
/// empties every slot after the last one.
enum class Distribution : std::uint8_t { Replace, Fill, Override };

/// The modifier the command names, as the functions to run.
[[nodiscard]] Parsed<std::vector<server::world::loot::LootFunction>> modifierOf(const CommandContext& context,
                                                                                world::RealLootTables* tables) {
    const auto* named = context.get<ResourceOrInline>("modifier");
    if (named == nullptr) {
        return std::vector<server::world::loot::LootFunction>{};
    }
    server::world::loot::LootReferences* references = tables == nullptr ? nullptr : &tables->references();
    const wire::common::JsonValue value = named->inlineValue.value_or(
        wire::common::JsonValue::of(named->id.has_value() ? named->id->toString() : std::string{}));
    auto parsed = server::world::loot::lootTableJson::parseModifier(value, references);
    if (!parsed.has_value()) {
        return std::unexpected{CommandSyntaxError{.component = Component::text(parsed.error().message)}};
    }
    return std::move(*parsed);
}

/// parity: `ItemCommands.applyModifier`: the functions in a command's loot context, the result cut
/// to what the item stacks to.
[[nodiscard]] Stack modify(const CommandContext& context, world::RealLootTables* tables,
                           const std::vector<server::world::loot::LootFunction>& functions, const Stack& stack) {
    if (functions.empty() || isEmpty(stack)) {
        return stack;
    }
    const auto id = Identifier::parse(stack->id);
    if (!id.has_value()) {
        return stack;
    }
    server::world::loot::LootContext loot;
    const std::array<double, 3> at = context.source().position();
    if (const Parsed<world::LoadedWorld*> world = context.world(); world.has_value() && tables != nullptr) {
        tables->prepareAt(loot, **world, at[0], at[1], at[2]);
    }
    loot.hasEntity = context.source().player() != nullptr;
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    server::world::loot::ItemStack item{.item = *id, .count = stack->count, .components = stack->components};
    for (const server::world::loot::LootFunction& function : functions) {
        item = function.apply(std::move(item), random, std::nullopt, loot);
    }
    if (item.count <= 0 || item.item.path() == "air") {
        return std::nullopt;
    }
    item.count = std::min(item.count, server::registry::itemStacking::maxStackSize(item.item));
    return storage::StoredItemStack{.id = item.item.toString(), .count = item.count, .components = item.components};
}

/// parity: `CommandResponseTracker.sendFeedback`: fail when nothing changed, name the one target
/// that did, count them otherwise; the answer is every slot changed.
[[nodiscard]] Parsed<std::int32_t> feedback(const CommandContext& context, const std::vector<Target>& targets,
                                            const std::vector<std::int32_t>& counts, std::string_view action,
                                            const std::optional<std::string>& knownItem) {
    std::int32_t total = 0;
    std::int32_t changed = 0;
    const Target* only = nullptr;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        total += counts[index];
        if (counts[index] != 0) {
            ++changed;
            only = &targets[index];
        }
    }
    if (changed == 0) {
        return std::unexpected{CommandSyntaxError{
            .component = knownItem.has_value() ? Component::translatable("commands.item.target.failed.known_item",
                                                                         {Component::text(*knownItem)})
                                               : Component::translatable("commands.item.target.failed")}};
    }
    const std::string suffix = knownItem.has_value() ? ".known_item" : "";
    std::vector<Component> arguments{Component::text(std::to_string(total))};
    std::string key;
    if (only != nullptr && changed == 1 && only->block.has_value()) {
        key = "commands.item.block." + std::string{action} + ".success" + suffix;
        for (const std::int32_t axis : *only->block) {
            arguments.push_back(Component::text(std::to_string(axis)));
        }
    } else if (changed == 1) {
        key = "commands.item.entity." + std::string{action} + ".success.single" + suffix;
        arguments.push_back(Component::text(only->name));
    } else {
        key = "commands.item.entity." + std::string{action} + ".success.multiple" + suffix;
        arguments = {Component::text(std::to_string(changed))};
    }
    if (knownItem.has_value()) {
        arguments.push_back(Component::text(*knownItem));
    }
    context.success(Component::translatable(key, arguments), true);
    return total;
}

[[nodiscard]] CommandSyntaxError noSuchSlot(std::string_view direction, const SlotSourceValue* slots) {
    return CommandSyntaxError{
        .component = Component::translatable("commands.item." + std::string{direction} + ".no_such_slot",
                                             {Component::text(slots == nullptr ? std::string{} : slots->name)})};
}

} // namespace

ItemCommand::ItemCommand(world::RealRecipes* recipes, world::RealLootTables* lootTables)
    : Command("item", permissionLevel::kGamemasters), recipes_(recipes), lootTables_(lootTables) {}

void ItemCommand::build(const CommandNodeBuilder& root) const {
    world::RealRecipes* recipes = recipes_;
    world::RealLootTables* tables = lootTables_;

    /// parity: `ItemCommands.getItems`: every slot the source names, empty ones included, through
    /// the modifier when there is one.
    const auto sourceItems = [recipes, tables](const CommandContext& context) -> Parsed<std::vector<Stack>> {
        const Parsed<std::vector<Target>> sources = targetsOf(context, "source", true);
        if (!sources.has_value()) {
            return std::unexpected{sources.error()};
        }
        const Parsed<std::vector<server::world::loot::LootFunction>> functions = modifierOf(context, tables);
        if (!functions.has_value()) {
            return std::unexpected{functions.error()};
        }
        const auto* slots = context.get<SlotSourceValue>("sourceSlots");
        std::vector<Stack> items;
        for (const Target& source : *sources) {
            withSlots(context, source, recipes, [&](Slots& access) {
                for (const std::int32_t id : slots == nullptr ? std::vector<std::int32_t>{} : slots->slots) {
                    if (const std::optional<Stack> held = access.read(id); held.has_value()) {
                        items.push_back(modify(context, tables, *functions, *held));
                    }
                }
            });
        }
        if (items.empty()) {
            return std::unexpected{noSuchSlot("source", slots)};
        }
        return items;
    };

    /// parity: `ItemCommands.setItems`: each target gets the items from the start, one per slot.
    const auto setItems = [recipes](const CommandContext& context, Distribution distribution,
                                    const std::vector<Stack>& items,
                                    const std::optional<std::string>& knownItem) -> Parsed<std::int32_t> {
        const Parsed<std::vector<Target>> targets = targetsOf(context, "target", false);
        if (!targets.has_value()) {
            return std::unexpected{targets.error()};
        }
        const auto* slots = context.get<SlotSourceValue>("slots");
        std::int32_t selected = 0;
        std::vector<std::int32_t> counts;
        for (const Target& target : *targets) {
            std::int32_t count = 0;
            withSlots(context, target, recipes, [&](Slots& access) {
                std::size_t next = 0;
                for (const std::int32_t id : slots == nullptr ? std::vector<std::int32_t>{} : slots->slots) {
                    if (!access.read(id).has_value()) {
                        continue;
                    }
                    Stack item;
                    if (distribution == Distribution::Fill) {
                        if (items.empty()) {
                            break;
                        }
                        item = items[next++ % items.size()];
                    } else if (next < items.size()) {
                        item = items[next++];
                    } else if (distribution == Distribution::Override) {
                        item = std::nullopt;
                    } else {
                        break;
                    }
                    ++selected;
                    if (access.write(id, item)) {
                        ++count;
                    }
                }
            });
            counts.push_back(count);
        }
        if (selected == 0) {
            return std::unexpected{noSuchSlot("target", slots)};
        }
        return feedback(context, *targets, counts, "replace", knownItem);
    };

    /// parity: `ItemCommands.modifyItems`, over the slots that hold something.
    const auto modifyItems = [recipes, tables](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<std::vector<Target>> targets = targetsOf(context, "target", false);
        if (!targets.has_value()) {
            return std::unexpected{targets.error()};
        }
        const Parsed<std::vector<server::world::loot::LootFunction>> functions = modifierOf(context, tables);
        if (!functions.has_value()) {
            return std::unexpected{functions.error()};
        }
        const auto* slots = context.get<SlotSourceValue>("slots");
        std::int32_t selected = 0;
        std::vector<std::int32_t> counts;
        for (const Target& target : *targets) {
            std::int32_t count = 0;
            withSlots(context, target, recipes, [&](Slots& access) {
                for (const std::int32_t id : slots == nullptr ? std::vector<std::int32_t>{} : slots->slots) {
                    const std::optional<Stack> held = access.read(id);
                    if (!held.has_value() || isEmpty(*held)) {
                        continue;
                    }
                    ++selected;
                    if (access.write(id, modify(context, tables, *functions, *held))) {
                        ++count;
                    }
                }
            });
            counts.push_back(count);
        }
        if (selected == 0) {
            return std::unexpected{noSuchSlot("target", slots)};
        }
        return feedback(context, *targets, counts, "modify", std::nullopt);
    };

    /// `block <pos>` or `entity <targets>`, each leading on to the same `then`.
    const auto provider = [](const CommandNodeBuilder& node, const std::string& name,
                             const std::function<void(const CommandNodeBuilder&)>& then) {
        node.literal("block", [&](const CommandNodeBuilder& block) { block.argument(name, blockPosArgument(), then); });
        node.literal("entity",
                     [&](const CommandNodeBuilder& entity) { entity.argument(name, entitiesArgument(false), then); });
    };

    for (const auto& [literal, distribution] :
         {std::pair{"replace", Distribution::Replace}, std::pair{"fill", Distribution::Fill},
          std::pair{"override", Distribution::Override}}) {
        root.literal(literal, [&](const CommandNodeBuilder& form) {
            provider(form, "target", [&](const CommandNodeBuilder& target) {
                target.argument("slots", slotSourceArgument(), [&](const CommandNodeBuilder& slots) {
                    slots.literal("from", [&](const CommandNodeBuilder& from) {
                        provider(from, "source", [&](const CommandNodeBuilder& source) {
                            source.argument("sourceSlots", slotSourceArgument(), [&](const CommandNodeBuilder& in) {
                                const auto copy = [sourceItems, setItems, distribution](
                                                      const CommandContext& context) -> Parsed<std::int32_t> {
                                    const Parsed<std::vector<Stack>> items = sourceItems(context);
                                    if (!items.has_value()) {
                                        return std::unexpected{items.error()};
                                    }
                                    return setItems(context, distribution, *items, std::nullopt);
                                };
                                in.executes(copy);
                                in.argument("modifier", lootModifierOrInlineArgument(),
                                            [copy](const CommandNodeBuilder& modifier) { modifier.executes(copy); });
                            });
                        });
                    });
                    slots.literal("with", [&](const CommandNodeBuilder& with) {
                        with.argument("item", itemArgument(), [&](const CommandNodeBuilder& item) {
                            const auto put = [setItems, distribution](const CommandContext& context,
                                                                      std::int32_t count) -> Parsed<std::int32_t> {
                                const auto* parsed = context.get<ParsedItem>("item");
                                if (parsed == nullptr) {
                                    return 0;
                                }
                                // parity: `ItemInput.createItemStack` refuses more than one stack.
                                const std::int32_t most = server::registry::itemStacking::maxStackSize(parsed->id);
                                if (most > 0 && count > most) {
                                    return std::unexpected{CommandSyntaxError{
                                        .component = Component::translatable("arguments.item.overstacked",
                                                                             {Component::text(parsed->id.toString()),
                                                                              Component::text(std::to_string(most))})}};
                                }
                                const storage::StoredItemStack stack{
                                    .id = parsed->id.toString(), .count = count, .components = parsed->components};
                                return setItems(context, distribution, {stack}, stack.id);
                            };
                            item.executes([put](const CommandContext& context) { return put(context, 1); });
                            item.argument("count", integerArgument(1, kMaxCount),
                                          [put](const CommandNodeBuilder& count) {
                                              count.executes([put](const CommandContext& context) {
                                                  const auto* amount = context.get<std::int32_t>("count");
                                                  return put(context, amount == nullptr ? 1 : *amount);
                                              });
                                          });
                        });
                    });
                });
            });
        });
    }
    root.literal("modify", [&](const CommandNodeBuilder& form) {
        provider(form, "target", [&](const CommandNodeBuilder& target) {
            target.argument("slots", slotSourceArgument(), [&](const CommandNodeBuilder& slots) {
                slots.argument("modifier", lootModifierOrInlineArgument(),
                               [modifyItems](const CommandNodeBuilder& modifier) { modifier.executes(modifyItems); });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
