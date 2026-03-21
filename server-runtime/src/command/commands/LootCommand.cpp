#include "ItemSlots.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <cstdint>
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

/// parity: `String.hashCode`, so a table always rolls the same way for a given world seed.
[[nodiscard]] std::int32_t javaHash(std::string_view text) {
    std::int32_t hash = 0;
    for (const char c : text) {
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 31U +
                                         static_cast<std::uint32_t>(static_cast<unsigned char>(c)));
    }
    return hash;
}

[[nodiscard]] std::vector<storage::StoredItemStack>
stacksOf(const std::vector<server::world::loot::ItemStack>& rolled) {
    std::vector<storage::StoredItemStack> stacks;
    stacks.reserve(rolled.size());
    for (const server::world::loot::ItemStack& one : rolled) {
        stacks.push_back(storage::StoredItemStack{.id = one.item.toString(), .count = one.count});
    }
    return stacks;
}

[[nodiscard]] std::array<std::int32_t, 3> blockAt(const CommandContext& context, const std::string& name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

} // namespace

LootCommand::LootCommand(world::RealLootTables& lootTables, std::function<std::int64_t()> worldSeed)
    : Command("loot", permissionLevel::kGamemasters), lootTables_(&lootTables), worldSeed_(std::move(worldSeed)) {}

void LootCommand::build(const CommandNodeBuilder& root) const {
    using Stacks = std::vector<storage::StoredItemStack>;

    const auto rollTable = [this](const CommandContext& context) -> Parsed<Stacks> {
        const auto* id = context.get<Identifier>("loot_table");
        const server::world::loot::LootTable* table = id == nullptr ? nullptr : lootTables_->byId(*id);
        if (table == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "commands.drop.no_loot_table", {Component::text(id == nullptr ? std::string{} : id->toString())})}};
        }
        random::RandomSource random =
            random::RandomSource::legacy(worldSeed_() ^ static_cast<std::int64_t>(javaHash(id->toString())));
        return stacksOf(table->generate(random));
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
        random::RandomSource random =
            random::RandomSource::legacy(worldSeed_() ^ static_cast<std::int64_t>(target.entityId()));
        return stacksOf(table->generate(random));
    };

    const auto rollBlock = [this](const CommandContext& context) -> Parsed<Stacks> {
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
        random::RandomSource random = random::RandomSource::legacy(
            worldSeed_() ^ ((static_cast<std::int64_t>(at[0]) << 32) | static_cast<std::uint32_t>(at[2])));
        const server::world::loot::LootContext block{.blockState = reg::blockStateDefinitions::propertiesOf(*stateId),
                                                     .block = state->block};
        return stacksOf(table->generate(random, 0.0F, std::nullopt, block));
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
            }));
        }
        report(context, stacks);
        return static_cast<std::int32_t>(stacks.size());
    };

    const auto insert = [report](const CommandContext& context, const Stacks& stacks) -> Parsed<std::int32_t> {
        const std::array<std::int32_t, 3> at = blockAt(context, "targetPos");
        // The first free slots, so an insert fills a container rather than overwriting what is in it.
        std::vector<std::pair<std::int32_t, storage::StoredItemStack>> written;
        std::vector<std::pair<std::int32_t, storage::StoredItemStack>> held = itemSlots::containerItems(context, at);
        std::int32_t slot = 0;
        for (const storage::StoredItemStack& stack : stacks) {
            const auto taken = [&held, &written](std::int32_t candidate) {
                const auto is = [candidate](const auto& one) { return one.first == candidate; };
                return std::ranges::any_of(held, is) || std::ranges::any_of(written, is);
            };
            while (taken(slot)) {
                slot++;
            }
            written.emplace_back(slot, stack);
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

    /// The four sources, declared once per destination the way vanilla pairs them.
    using Destination = std::function<Parsed<std::int32_t>(const CommandContext&, const Stacks&)>;
    const auto sources = [this, rollTable, rollEntity, rollBlock](const CommandNodeBuilder& node,
                                                                  const Destination& run) {
        /// A source rolls first and hands the result on, so a table that cannot be rolled is
        /// reported instead of the destination reporting an empty success.
        const auto through = [run](const auto& roll) {
            return [run, roll](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<Stacks> rolled = roll(context);
                if (!rolled.has_value()) {
                    return std::unexpected{rolled.error()};
                }
                return run(context, *rolled);
            };
        };
        const CommandExecutor fromTable = through(rollTable);
        const CommandExecutor fromEntity = through(rollEntity);
        const CommandExecutor fromBlock = through(rollBlock);

        node.literal("fish", [this, fromTable](const CommandNodeBuilder& form) {
            form.argument("loot_table", lootTableArgument(), [this, fromTable](const CommandNodeBuilder& table) {
                table.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(lootTables_->ids()); });
                table.argument("pos", blockPosArgument(), [fromTable](const CommandNodeBuilder& pos) {
                    pos.executes(fromTable);
                    for (const std::string_view hand : {"mainhand", "offhand"}) {
                        pos.literal(std::string{hand},
                                    [fromTable](const CommandNodeBuilder& named) { named.executes(fromTable); });
                    }
                    // parity: the tool is parsed and ignored, since loot conditions on it are not
                    // modelled and it would change nothing.
                    pos.argument("tool", itemArgument(),
                                 [fromTable](const CommandNodeBuilder& tool) { tool.executes(fromTable); });
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
                table.executes(fromTable);
            });
        });
        node.literal("mine", [fromBlock](const CommandNodeBuilder& form) {
            form.argument("pos", blockPosArgument(), [fromBlock](const CommandNodeBuilder& pos) {
                pos.executes(fromBlock);
                for (const std::string_view hand : {"mainhand", "offhand"}) {
                    pos.literal(std::string{hand},
                                [fromBlock](const CommandNodeBuilder& named) { named.executes(fromBlock); });
                }
                pos.argument("tool", itemArgument(),
                             [fromBlock](const CommandNodeBuilder& tool) { tool.executes(fromBlock); });
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
