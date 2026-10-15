#include "ItemSlots.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::text::Component;

/// parity: `ItemCommands.MAX_ALLOWED_ITEMSTACKS`, the same cap `/item` uses.
constexpr std::int32_t kMaxCount = 99;

[[nodiscard]] std::vector<storage::StoredItemStack>
stacksOf(const std::vector<server::world::loot::ItemStack>& rolled) {
    std::vector<storage::StoredItemStack> stacks;
    stacks.reserve(rolled.size());
    for (const server::world::loot::ItemStack& one : rolled) {
        stacks.push_back(
            storage::StoredItemStack{.id = one.item.toString(), .count = one.count, .components = one.components});
    }
    return stacks;
}

[[nodiscard]] std::optional<server::world::loot::EntityFacts> playerFacts(const CommandContext& context) {
    const player::PlayerSession* player = context.source().player();
    if (player == nullptr) {
        return std::nullopt;
    }
    server::world::loot::EntityFacts facts{.type = Identifier::vanilla("player"), .scoreName = player->name()};
    if (const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem(); held.has_value()) {
        facts.mainhand = Identifier::parse(held->id).value_or(Identifier::vanilla("air"));
    }
    return facts;
}

/// parity: `LootTable.getRandomItems(LootParams)`.
[[nodiscard]] std::vector<storage::StoredItemStack> roll(const CommandContext& context, world::RealLootTables& tables,
                                                         const server::world::loot::LootTable& table,
                                                         server::world::loot::LootContext loot,
                                                         const std::array<double, 3>& origin) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (world.has_value()) {
        tables.prepareAt(loot, **world, origin[0], origin[1], origin[2]);
    } else {
        tables.prepare(loot);
    }
    if (world.has_value() && table.randomSequence.has_value()) {
        return stacksOf(
            table.generate((*world)->randomSequences().of(*table.randomSequence), loot.luck, std::nullopt, loot));
    }
    random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
    return stacksOf(table.generate(random, loot.luck, std::nullopt, loot));
}

[[nodiscard]] std::int32_t containerSize(const CommandContext& context, const std::array<std::int32_t, 3>& at) {
    const Parsed<world::LoadedWorld*> world = context.world();
    const server::world::Chunk* chunk =
        world.has_value() ? (*world)->state().chunkIfLoaded(at[0] >> 4, at[2] >> 4) : nullptr;
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    return entity == nullptr ? 0 : world::containers::sizeOf(std::string{entity->type().path()}).value_or(0);
}

[[nodiscard]] std::array<std::int32_t, 3> blockAt(const CommandContext& context, const std::string& name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

enum class ToolFrom : std::uint8_t { Nothing, Argument, MainHand, OffHand };

/// parity: `LootCommand.getSourceHandItem` and the `tool` argument.
[[nodiscard]] Parsed<std::optional<storage::StoredItemStack>> toolOf(const CommandContext& context, ToolFrom from) {
    if (from == ToolFrom::Nothing) {
        return std::optional<storage::StoredItemStack>{};
    }
    if (from == ToolFrom::Argument) {
        const auto* parsed = context.get<ParsedItem>("tool");
        if (parsed == nullptr) {
            return std::optional<storage::StoredItemStack>{};
        }
        return std::optional{
            storage::StoredItemStack{.id = parsed->id.toString(), .count = 1, .components = parsed->components}};
    }
    const player::PlayerSession* player = context.source().player();
    if (player == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.drop.no_held_items", {context.source().displayName()})}};
    }
    return from == ToolFrom::MainHand ? player->inventory.heldItem()
                                      : player->inventory.at(player::PlayerInventory::kOffhandSlot);
}

/// parity: `LootContextParams.TOOL`.
void useTool(server::world::loot::LootContext& loot, const std::optional<storage::StoredItemStack>& tool) {
    if (!tool.has_value() || tool->count <= 0) {
        loot.tool = Identifier::vanilla("air");
        return;
    }
    loot.tool = Identifier::parse(tool->id).value_or(Identifier::vanilla("air"));
    for (const auto& [enchantment, level] : player::enchantmentLevels(*tool)) {
        loot.toolEnchantments.emplace(enchantment.toString(), level);
    }
}

} // namespace

LootCommand::LootCommand(world::RealLootTables& lootTables)
    : Command("loot", permissionLevel::kGamemasters), lootTables_(&lootTables) {}

void LootCommand::build(const CommandNodeBuilder& root) const {
    using Stacks = std::vector<storage::StoredItemStack>;

    const auto rollTable = [this](const CommandContext& context, ToolFrom from) -> Parsed<Stacks> {
        const auto* id = context.get<Identifier>("loot_table");
        const server::world::loot::LootTable* table = id == nullptr ? nullptr : lootTables_->byId(*id);
        if (table == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "commands.drop.no_loot_table", {Component::text(id == nullptr ? std::string{} : id->toString())})}};
        }
        // parity: `dropChestLoot` and `dropFishingLoot`.
        server::world::loot::LootContext loot{.hasEntity = context.source().player() != nullptr};
        loot.thisEntity = playerFacts(context);
        if (from != ToolFrom::Nothing || context.get<BlockCoordinates>("pos") != nullptr) {
            const Parsed<std::optional<storage::StoredItemStack>> tool = toolOf(context, from);
            if (!tool.has_value()) {
                return std::unexpected{tool.error()};
            }
            useTool(loot, *tool);
        }
        std::array<double, 3> origin = context.source().position();
        if (context.get<BlockCoordinates>("pos") != nullptr) {
            const std::array<std::int32_t, 3> at = blockAt(context, "pos");
            origin = {at[0] + 0.5, at[1] + 0.5, at[2] + 0.5};
        }
        return roll(context, *lootTables_, *table, std::move(loot), origin);
    };

    const auto rollEntity = [this](const CommandContext& context) -> Parsed<Stacks> {
        const auto* targets = context.get<std::vector<CommandTarget>>("target");
        if (targets == nullptr || targets->empty()) {
            return Stacks{};
        }
        const CommandTarget& target = targets->front();
        const server::world::loot::LootTable* table = lootTables_->forEntity(target.type());
        if (table == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.drop.no_loot_table.entity",
                                                                        {Component::text(target.name())})}};
        }
        // parity: `dropKillLoot`.
        server::world::loot::LootContext loot{.hasEntity = true};
        loot.thisEntity = server::world::loot::EntityFacts{.type = target.type(), .scoreName = target.uuid()};
        loot.attacker = playerFacts(context);
        loot.directAttacker = loot.attacker;
        loot.attackingPlayer = loot.attacker;
        loot.killedByPlayer = loot.attacker.has_value();
        loot.damageType = Identifier::vanilla("magic");
        return roll(context, *lootTables_, *table, std::move(loot), context.source().position());
    };

    const auto rollBlock = [this](const CommandContext& context, ToolFrom from) -> Parsed<Stacks> {
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const std::array<std::int32_t, 3> at = blockAt(context, "pos");
        const std::optional<std::int32_t> stateId = (*world)->state().blockState(at[0], at[1], at[2]);
        if (!stateId.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")}};
        }
        const std::optional<reg::BlockState> state = reg::blocks::byStateId(*stateId);
        if (!state.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::text("Nothing is at that position.")}};
        }
        const server::world::loot::LootTable* table = lootTables_->forBlock(state->block);
        if (table == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::text(state->block.toString() +
                                             " has no loot table this build can roll, so it would drop nothing.")}};
        }
        // parity: `dropBlockLoot`, the block's own drops with the source behind them.
        server::world::loot::LootContext loot{.blockState = reg::blockStateDefinitions::propertiesOf(*stateId),
                                              .block = state->block,
                                              .hasEntity = context.source().player() != nullptr};
        loot.thisEntity = playerFacts(context);
        const Parsed<std::optional<storage::StoredItemStack>> tool = toolOf(context, from);
        if (!tool.has_value()) {
            return std::unexpected{tool.error()};
        }
        useTool(loot, *tool);
        return roll(context, *lootTables_, *table, std::move(loot), {at[0] + 0.5, at[1] + 0.5, at[2] + 0.5});
    };

    const auto report = [](const CommandContext& context, const Stacks& stacks) {
        if (stacks.empty()) {
            context.success(Component::translatable("commands.drop.no_loot_table.block", {Component::text("")}));
            return;
        }
        std::int32_t total = 0;
        for (const storage::StoredItemStack& stack : stacks) {
            total += stack.count;
        }
        context.success(
            Component::translatable("commands.drop.success.multiple", {Component::text(std::to_string(total))}), true);
    };

    const auto give = [report](const CommandContext& context, const Stacks& stacks) -> Parsed<std::int32_t> {
        const auto* players = context.get<std::vector<player::PlayerSession*>>("players");
        if (players == nullptr) {
            return 0;
        }
        for (player::PlayerSession* player : *players) {
            for (const storage::StoredItemStack& stack : stacks) {
                static_cast<void>(player->inventory.add(stack));
            }
            if (player->connection().sendInventoryAll) {
                player->connection().sendInventoryAll();
            }
        }
        report(context, stacks);
        return static_cast<std::int32_t>(stacks.size() * players->size());
    };

    const auto spawn = [report](const CommandContext& context, const Stacks& stacks) -> Parsed<std::int32_t> {
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const auto* coordinates = context.get<Coordinates>("targetPos");
        const std::array<double, 3> at =
            coordinates == nullptr ? context.source().position() : coordinates->resolve(context.source());
        for (const storage::StoredItemStack& stack : stacks) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(stack.id);
            if (!id.has_value()) {
                continue;
            }
            static_cast<void>((*world)->state().spawnItem(world::DroppedItem{
                .entityId = context.environment().players().allocateEntityId(),
                .uuid = wire::common::Uuid::random(),
                .item = *id,
                .count = stack.count,
                .x = at[0],
                .y = at[1],
                .z = at[2],
                .components = stack.components,
            }));
        }
        report(context, stacks);
        return static_cast<std::int32_t>(stacks.size());
    };

    const auto insert = [report](const CommandContext& context, const Stacks& stacks) -> Parsed<std::int32_t> {
        const std::array<std::int32_t, 3> at = blockAt(context, "targetPos");
        // parity: `LootCommand.distributeToContainer`.
        std::map<std::int32_t, storage::StoredItemStack> slots;
        for (auto& [slot, stack] : itemSlots::containerItems(context, at)) {
            slots.insert_or_assign(slot, std::move(stack));
        }
        const std::int32_t size = containerSize(context, at);
        std::set<std::int32_t> touched;
        for (storage::StoredItemStack stack : stacks) {
            const std::int32_t most =
                reg::itemStacking::maxStackSize(Identifier::parse(stack.id).value_or(Identifier::vanilla("air")));
            for (std::int32_t slot = 0; slot < size && stack.count > 0; ++slot) {
                const auto found = slots.find(slot);
                if (found == slots.end()) {
                    slots.emplace(slot, stack);
                    touched.insert(slot);
                    break;
                }
                storage::StoredItemStack& current = found->second;
                if (current.id == stack.id && current.components == stack.components && current.count < most) {
                    const std::int32_t moved = std::min(stack.count, most - current.count);
                    current.count += moved;
                    stack.count -= moved;
                    touched.insert(slot);
                }
            }
        }
        std::vector<std::pair<std::int32_t, storage::StoredItemStack>> written;
        for (const std::int32_t slot : touched) {
            written.emplace_back(slot, slots.at(slot));
        }
        if (const std::optional<CommandSyntaxError> failure = itemSlots::writeContainer(context, at, written);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        report(context, stacks);
        return static_cast<std::int32_t>(stacks.size());
    };

    const auto replaceInto = [report](const CommandContext& context, const Stacks& stacks,
                                      bool entities) -> Parsed<std::int32_t> {
        const Parsed<std::int32_t> slot = itemSlots::slotOf(context, "slot", !entities);
        if (!slot.has_value()) {
            return std::unexpected{slot.error()};
        }
        if (stacks.empty()) {
            return 0;
        }
        const storage::StoredItemStack& first = stacks.front();
        if (entities) {
            const auto* targets = context.get<std::vector<player::PlayerSession*>>("entities");
            if (targets == nullptr) {
                return 0;
            }
            for (player::PlayerSession* target : *targets) {
                target->inventory.set(*slot, first);
                if (target->connection().sendInventoryAll) {
                    target->connection().sendInventoryAll();
                }
            }
            report(context, stacks);
            return static_cast<std::int32_t>(targets->size());
        }
        const std::array<std::int32_t, 3> at = blockAt(context, "targetPos");
        if (const std::optional<CommandSyntaxError> failure = itemSlots::writeContainer(context, at, {{*slot, first}});
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        report(context, stacks);
        return 1;
    };

    using Destination = std::function<Parsed<std::int32_t>(const CommandContext&, const Stacks&)>;
    const auto sources = [this, rollTable, rollEntity, rollBlock](const CommandNodeBuilder& node,
                                                                  const Destination& run) {
        const auto through = [run](const auto& roll) {
            return [run, roll](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<Stacks> rolled = roll(context);
                if (!rolled.has_value()) {
                    return std::unexpected{rolled.error()};
                }
                return run(context, *rolled);
            };
        };
        const auto fromTable = [through, rollTable](ToolFrom from) -> CommandExecutor {
            return through([rollTable, from](const CommandContext& context) { return rollTable(context, from); });
        };
        const auto fromBlock = [through, rollBlock](ToolFrom from) -> CommandExecutor {
            return through([rollBlock, from](const CommandContext& context) { return rollBlock(context, from); });
        };
        const CommandExecutor fromEntity = through(rollEntity);

        node.literal("fish", [this, fromTable](const CommandNodeBuilder& form) {
            form.argument("loot_table", lootTableArgument(), [this, fromTable](const CommandNodeBuilder& table) {
                table.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(lootTables_->ids()); });
                table.argument("pos", blockPosArgument(), [fromTable](const CommandNodeBuilder& pos) {
                    pos.executes(fromTable(ToolFrom::Nothing));
                    pos.literal("mainhand", [fromTable](const CommandNodeBuilder& named) {
                        named.executes(fromTable(ToolFrom::MainHand));
                    });
                    pos.literal("offhand", [fromTable](const CommandNodeBuilder& named) {
                        named.executes(fromTable(ToolFrom::OffHand));
                    });
                    pos.argument("tool", itemArgument(), [fromTable](const CommandNodeBuilder& tool) {
                        tool.executes(fromTable(ToolFrom::Argument));
                    });
                });
            });
        });
        node.literal("kill", [fromEntity](const CommandNodeBuilder& form) {
            form.argument("target", entitiesArgument(true),
                          [fromEntity](const CommandNodeBuilder& target) { target.executes(fromEntity); });
        });
        node.literal("loot", [this, fromTable](const CommandNodeBuilder& form) {
            form.argument("loot_table", lootTableArgument(), [this, fromTable](const CommandNodeBuilder& table) {
                table.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(lootTables_->ids()); });
                table.executes(fromTable(ToolFrom::Nothing));
            });
        });
        node.literal("mine", [fromBlock](const CommandNodeBuilder& form) {
            form.argument("pos", blockPosArgument(), [fromBlock](const CommandNodeBuilder& pos) {
                pos.executes(fromBlock(ToolFrom::Nothing));
                pos.literal("mainhand", [fromBlock](const CommandNodeBuilder& named) {
                    named.executes(fromBlock(ToolFrom::MainHand));
                });
                pos.literal("offhand", [fromBlock](const CommandNodeBuilder& named) {
                    named.executes(fromBlock(ToolFrom::OffHand));
                });
                pos.argument("tool", itemArgument(), [fromBlock](const CommandNodeBuilder& tool) {
                    tool.executes(fromBlock(ToolFrom::Argument));
                });
            });
        });
    };

    root.literal("give", [give, sources](const CommandNodeBuilder& form) {
        form.argument("players", playersArgument(false),
                      [give, sources](const CommandNodeBuilder& players) { sources(players, give); });
    });
    root.literal("insert", [insert, sources](const CommandNodeBuilder& form) {
        form.argument("targetPos", blockPosArgument(),
                      [insert, sources](const CommandNodeBuilder& pos) { sources(pos, insert); });
    });
    root.literal("spawn", [spawn, sources](const CommandNodeBuilder& form) {
        form.argument("targetPos", vec3Argument(),
                      [spawn, sources](const CommandNodeBuilder& pos) { sources(pos, spawn); });
    });
    root.literal("replace", [replaceInto, sources](const CommandNodeBuilder& form) {
        const auto slotForms = [replaceInto, sources](const CommandNodeBuilder& node, bool entities) {
            node.argument("slot", itemSlotArgument(), [replaceInto, sources, entities](const CommandNodeBuilder& slot) {
                slot.suggestsFromServer(
                    [](const SuggestionContext& context) { return context.matching(itemSlots::names()); });
                const Destination run = [replaceInto, entities](const CommandContext& context, const Stacks& stacks) {
                    return replaceInto(context, stacks, entities);
                };
                sources(slot, run);
                slot.argument("count", integerArgument(0, kMaxCount),
                              [sources, run](const CommandNodeBuilder& count) { sources(count, run); });
            });
        };
        form.literal("block", [slotForms](const CommandNodeBuilder& block) {
            block.argument("targetPos", blockPosArgument(),
                           [slotForms](const CommandNodeBuilder& pos) { slotForms(pos, false); });
        });
        form.literal("entity", [slotForms](const CommandNodeBuilder& entity) {
            entity.argument("entities", playersArgument(false),
                            [slotForms](const CommandNodeBuilder& targets) { slotForms(targets, true); });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
