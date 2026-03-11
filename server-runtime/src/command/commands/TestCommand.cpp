#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::common::Identifier;
using wire::text::Component;

/// parity: vanilla's own cap on the level `/enchant` accepts.
constexpr std::int32_t kMaxEnchantLevel = 255;

[[nodiscard]] const std::vector<player::PlayerSession*>& targetsOf(const CommandContext& context) {
    static const std::vector<player::PlayerSession*> none;
    const auto* found = context.get<std::vector<player::PlayerSession*>>("targets");
    return found == nullptr ? none : *found;
}

[[nodiscard]] std::vector<Identifier> testIds(const world::RealTestInstances& instances) {
    std::vector<Identifier> ids;
    for (const world::TestInstance& instance : instances.all()) {
        ids.push_back(instance.id);
    }
    return ids;
}

} // namespace

EnchantCommand::EnchantCommand(const world::RealEnchantments& enchantments)
    : Command("enchant", permissionLevel::kGamemasters), enchantments_(&enchantments) {}

void EnchantCommand::build(const CommandNodeBuilder& root) const {
    const auto enchant = [this](const CommandContext& context, std::int32_t level) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("enchantment");
        if (id == nullptr) {
            return 0;
        }
        if (enchantments_->size() == 0) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::text(
                    "No enchantment registry has been sent to clients, so '" + id->toString() +
                    "' would not resolve on one. It is read from the server.jar the EULA step downloads.")}};
        }
        if (!enchantments_->contains(*id)) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable(
                                       "argument.resource.not_found",
                                       {Component::text(id->toString()), Component::text("minecraft:enchantment")})}};
        }
        const std::vector<player::PlayerSession*>& targets = targetsOf(context);
        std::int32_t enchanted = 0;
        for (player::PlayerSession* target : targets) {
            const std::int32_t slot = player::PlayerInventory::kHotbarStart + target->inventory.heldSlot();
            const std::optional<storage::StoredItemStack>& held = target->inventory.at(slot);
            if (!held.has_value()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.enchant.failed.itemless",
                                                                            {Component::text(target->name())})}};
            }
            storage::StoredItemStack updated = *held;
            const wire::nbt::NbtTag* existing = updated.components.find(player::kEnchantmentsComponent);
            wire::nbt::NbtCompound levels;
            if (const auto* compound = existing == nullptr ? nullptr : existing->get<wire::nbt::NbtCompound>();
                compound != nullptr) {
                levels = *compound;
            }
            if (level == 0) {
                static_cast<void>(levels.remove(id->toString()));
            } else {
                levels.putInt(id->toString(), level);
            }
            if (levels.names().empty()) {
                static_cast<void>(updated.components.remove(player::kEnchantmentsComponent));
            } else {
                updated.components.set(player::kEnchantmentsComponent, wire::nbt::NbtTag::ofCompound(levels));
            }
            target->inventory.set(slot, std::move(updated));
            if (target->connection().sendInventoryAll) {
                target->connection().sendInventoryAll();
            }
            enchanted++;
        }
        if (enchanted == 0) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.enchant.failed")}};
        }
        context.success(
            targets.size() == 1
                ? Component::translatable("commands.enchant.success.single",
                                          {Component::text(id->toString()), Component::text(targets.front()->name())})
                : Component::translatable(
                      "commands.enchant.success.multiple",
                      {Component::text(id->toString()), Component::text(std::to_string(targets.size()))}),
            true);
        return enchanted;
    };

    root.argument("targets", playersArgument(false), [this, enchant](const CommandNodeBuilder& targets) {
        targets.argument("enchantment", resourceArgument("minecraft:enchantment"),
                         [this, enchant](const CommandNodeBuilder& named) {
                             named.suggestsFromServer([this](const SuggestionContext& context) {
                                 return context.matchingIds(enchantments_->ids());
                             });
                             named.executes([enchant](const CommandContext& context) { return enchant(context, 1); });
                             named.argument("level", integerArgument(0, kMaxEnchantLevel),
                                            [enchant](const CommandNodeBuilder& level) {
                                                level.executes([enchant](const CommandContext& context) {
                                                    const auto* asked = context.get<std::int32_t>("level");
                                                    return enchant(context, asked == nullptr ? 1 : *asked);
                                                });
                                            });
                         });
    });
}

DialogCommand::DialogCommand(const world::RealDialogs& dialogs)
    : Command("dialog", permissionLevel::kGamemasters), dialogs_(&dialogs) {}

void DialogCommand::build(const CommandNodeBuilder& root) const {
    root.literal("show", [this](const CommandNodeBuilder& show) {
        show.argument("targets", playersArgument(false), [this](const CommandNodeBuilder& targets) {
            // No completions of our own: `minecraft:dialog` is a synced registry, so the client
            // completes it from the same list this server sent it.
            targets.argument("dialog", dialogArgument(), [this](const CommandNodeBuilder& named) {
                named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* id = context.get<Identifier>("dialog");
                    if (id == nullptr) {
                        return 0;
                    }
                    if (dialogs_->size() == 0) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::text(
                                "No dialog registry has been sent to clients, so '" + id->toString() +
                                "' would not resolve on one. It is read from the server.jar the EULA step "
                                "downloads.")}};
                    }
                    const std::optional<std::int32_t> dialogId = dialogs_->protocolIdOf(*id);
                    if (!dialogId.has_value()) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable(
                                "argument.resource.not_found",
                                {Component::text(id->toString()), Component::text("minecraft:dialog")})}};
                    }
                    const std::vector<player::PlayerSession*>& targets = targetsOf(context);
                    // The door that has no dialogs of its own builds a screen out of this, so the
                    // registry is read here once rather than by each connection.
                    const world::RealDialogs& registry = *dialogs_;
                    const std::optional<world::dialogs::Dialog> dialog = world::dialogs::read(
                        registry.entries()[static_cast<std::size_t>(*dialogId)].second,
                        [&registry](const Identifier& named) -> const wire::nbt::NbtCompound* {
                            const std::optional<std::int32_t> at = registry.protocolIdOf(named);
                            return at.has_value() ? &registry.entries()[static_cast<std::size_t>(*at)].second : nullptr;
                        });
                    if (!dialog.has_value()) {
                        return 0;
                    }
                    for (const player::PlayerSession* target : targets) {
                        if (target->connection().showDialog) {
                            target->connection().showDialog(*dialogId, *dialog);
                        }
                    }
                    context.success(targets.size() == 1
                                        ? Component::translatable("commands.dialog.show.single",
                                                                  {Component::text(targets.front()->name())})
                                        : Component::translatable("commands.dialog.show.multiple",
                                                                  {Component::text(std::to_string(targets.size()))}),
                                    true);
                    return static_cast<std::int32_t>(targets.size());
                });
            });
        });
    });
    root.literal("clear", [](const CommandNodeBuilder& clear) {
        clear.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
            targets.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::vector<player::PlayerSession*>& matched = targetsOf(context);
                for (const player::PlayerSession* target : matched) {
                    if (target->connection().sendPlayPacket) {
                        target->connection().sendPlayPacket(play::kClearDialogId, [](wire::io::ByteBuffer&) {});
                    }
                }
                context.success(matched.size() == 1
                                    ? Component::translatable("commands.dialog.clear.single",
                                                              {Component::text(matched.front()->name())})
                                    : Component::translatable("commands.dialog.clear.multiple",
                                                              {Component::text(std::to_string(matched.size()))}),
                                true);
                return static_cast<std::int32_t>(matched.size());
            });
        });
    });
}

TestCommand::TestCommand(const world::RealTestInstances& instances, FunctionRunner& runner)
    : Command("test", permissionLevel::kGamemasters), instances_(&instances), runner_(&runner) {}

void TestCommand::build(const CommandNodeBuilder& root) const {
    const auto runAll = [this](const CommandContext& context,
                               const std::vector<world::TestInstance>& selected) -> Parsed<std::int32_t> {
        if (selected.empty()) {
            context.success(Component::translatable("commands.test.no_tests"));
            return 0;
        }
        std::int32_t passed = 0;
        std::int32_t skipped = 0;
        for (const world::TestInstance& instance : selected) {
            if (!instance.isFunctionTest() || !instance.function.has_value()) {
                skipped++;
                context.source().sendSystemMessage(
                    Component::text("  " + instance.id.toString() + ": skipped, this build runs function tests only"));
                continue;
            }
            const Parsed<std::int32_t> outcome =
                runner_->run(context.dispatcher(), context.source(), *instance.function);
            if (outcome.has_value()) {
                passed++;
                failed_.erase(instance.id);
                context.source().sendSystemMessage(Component::text("  " + instance.id.toString() + ": passed"));
            } else {
                failed_.insert(instance.id);
                context.source().sendSystemMessage(Component::text("  " + instance.id.toString() + ": failed"));
            }
        }
        const auto ran = static_cast<std::int32_t>(selected.size()) - skipped;
        context.success(Component::text("Ran " + std::to_string(ran) + " test(s): " + std::to_string(passed) +
                                        " passed, " + std::to_string(ran - passed) + " failed, " +
                                        std::to_string(skipped) + " skipped."),
                        true);
        return passed;
    };

    const auto runNamed = [this, runAll](const CommandContext& context) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("tests");
        const world::TestInstance* instance = id == nullptr ? nullptr : instances_->find(*id);
        if (instance == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("commands.test.error.test_instance_not_found")}};
        }
        return runAll(context, {*instance});
    };
    const auto runFailed = [this, runAll](const CommandContext& context) {
        std::vector<world::TestInstance> selected;
        for (const world::TestInstance& instance : instances_->all()) {
            if (failed_.contains(instance.id)) {
                selected.push_back(instance);
            }
        }
        return runAll(context, selected);
    };

    /// The `<numberOfTimes> [<untilFailed>] [<rotationSteps>] [<testsPerRow>]` tail several forms
    /// share. The arguments that lay tests out in the world are read and ignored: there are no test
    /// structures in this build, only function tests.
    const auto repeats = [](const CommandNodeBuilder& node, const CommandExecutor& run) {
        node.argument("numberOfTimes", integerArgument(0), [run](const CommandNodeBuilder& times) {
            times.executes(run);
            times.argument("untilFailed", boolArgument(), [run](const CommandNodeBuilder& until) {
                until.executes(run);
                until.argument("rotationSteps", integerArgument(0), [run](const CommandNodeBuilder& steps) {
                    steps.executes(run);
                    steps.argument("testsPerRow", integerArgument(1),
                                   [run](const CommandNodeBuilder& row) { row.executes(run); });
                });
            });
        });
    };

    // parity: the whole tree, so a datapack that drives tests parses here too.
    root.literal("runall", [this, runAll](const CommandNodeBuilder& form) {
        form.executes([this, runAll](const CommandContext& context) { return runAll(context, instances_->all()); });
    });
    for (const std::string_view name : {"runthese", "runthat", "runclosest"}) {
        root.literal(std::string{name}, [this, runAll](const CommandNodeBuilder& form) {
            const CommandExecutor run = [this, runAll](const CommandContext& context) {
                return runAll(context, instances_->all());
            };
            form.executes(run);
            form.argument("numberOfTimes", integerArgument(0), [run](const CommandNodeBuilder& times) {
                times.executes(run);
                times.argument("untilFailed", boolArgument(),
                               [run](const CommandNodeBuilder& until) { until.executes(run); });
            });
        });
    }
    root.literal("runfailed", [runFailed, repeats](const CommandNodeBuilder& form) {
        form.executes(runFailed);
        form.argument("onlyRequiredTests", boolArgument(), [runFailed, repeats](const CommandNodeBuilder& only) {
            only.executes(runFailed);
            repeats(only, runFailed);
        });
        repeats(form, runFailed);
    });
    root.literal("run", [this, runNamed, repeats](const CommandNodeBuilder& form) {
        form.argument("tests", resourceSelectorArgument("minecraft:test_instance"),
                      [this, runNamed, repeats](const CommandNodeBuilder& named) {
                          named.suggestsFromServer([this](const SuggestionContext& context) {
                              return context.matchingIds(testIds(*instances_));
                          });
                          named.executes(runNamed);
                          repeats(named, runNamed);
                      });
    });
    root.literal("runmultiple", [this, runNamed](const CommandNodeBuilder& form) {
        form.argument("tests", resourceSelectorArgument("minecraft:test_instance"),
                      [this, runNamed](const CommandNodeBuilder& named) {
                          named.suggestsFromServer([this](const SuggestionContext& context) {
                              return context.matchingIds(testIds(*instances_));
                          });
                          named.executes(runNamed);
                          named.argument("amount", integerArgument(0),
                                         [runNamed](const CommandNodeBuilder& amount) { amount.executes(runNamed); });
                      });
    });
    root.literal("verify", [this, runNamed](const CommandNodeBuilder& form) {
        form.argument("tests", resourceSelectorArgument("minecraft:test_instance"),
                      [this, runNamed](const CommandNodeBuilder& named) {
                          named.suggestsFromServer([this](const SuggestionContext& context) {
                              return context.matchingIds(testIds(*instances_));
                          });
                          named.executes(runNamed);
                      });
    });

    const CommandExecutor clear = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        const auto cleared = static_cast<std::int32_t>(failed_.size());
        failed_.clear();
        context.success(
            Component::translatable("commands.test.clear.success", {Component::text(std::to_string(cleared))}));
        return cleared;
    };
    root.literal("clearall", [clear](const CommandNodeBuilder& form) {
        form.executes(clear);
        form.argument("radius", integerArgument(0),
                      [clear](const CommandNodeBuilder& radius) { radius.executes(clear); });
    });
    for (const std::string_view name : {"clearthat", "clearthese"}) {
        root.literal(std::string{name}, [clear](const CommandNodeBuilder& form) { form.executes(clear); });
    }

    /// Nothing is laid out in the world, so there is nothing to reset, locate or clear there.
    const CommandExecutor nothingPlaced = [](const CommandContext& context) -> Parsed<std::int32_t> {
        context.success(Component::translatable("commands.test.error.no_test_instances"));
        return 0;
    };
    for (const std::string_view name : {"resetclosest", "resetthat", "resetthese"}) {
        root.literal(std::string{name},
                     [nothingPlaced](const CommandNodeBuilder& form) { form.executes(nothingPlaced); });
    }
    root.literal("locate", [this, nothingPlaced](const CommandNodeBuilder& form) {
        form.argument("tests", resourceSelectorArgument("minecraft:test_instance"),
                      [this, nothingPlaced](const CommandNodeBuilder& named) {
                          named.suggestsFromServer([this](const SuggestionContext& context) {
                              return context.matchingIds(testIds(*instances_));
                          });
                          named.executes(nothingPlaced);
                      });
    });

    const CommandExecutor noStructures = [](const CommandContext&) -> Parsed<std::int32_t> {
        return std::unexpected{CommandSyntaxError{
            .component = Component::text("Test structures are not modelled in this build: only function tests run.")}};
    };
    root.literal("create", [noStructures](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [noStructures](const CommandNodeBuilder& id) {
            id.executes(noStructures);
            id.argument("width", integerArgument(0), [noStructures](const CommandNodeBuilder& width) {
                width.executes(noStructures);
                width.argument("height", integerArgument(0), [noStructures](const CommandNodeBuilder& height) {
                    height.argument("depth", integerArgument(0),
                                    [noStructures](const CommandNodeBuilder& depth) { depth.executes(noStructures); });
                });
            });
        });
    });

    /// parity: `/test pos`, which prints the position as a line to paste into a test.
    const auto reportPosition = [](const CommandContext& context, const std::string& variable) -> Parsed<std::int32_t> {
        const std::array<double, 3> at = context.source().position();
        context.success(Component::text("final BlockPos " + variable + " = new BlockPos(" +
                                        std::to_string(static_cast<std::int32_t>(at[0])) + ", " +
                                        std::to_string(static_cast<std::int32_t>(at[1])) + ", " +
                                        std::to_string(static_cast<std::int32_t>(at[2])) + ");"));
        return 1;
    };
    root.literal("pos", [reportPosition](const CommandNodeBuilder& form) {
        form.executes([reportPosition](const CommandContext& context) { return reportPosition(context, "pos"); });
        form.argument("var", wordArgument(), [reportPosition](const CommandNodeBuilder& named) {
            named.executes([reportPosition](const CommandContext& context) {
                const auto* variable = context.get<std::string>("var");
                return reportPosition(context, variable == nullptr ? "pos" : *variable);
            });
        });
    });
    root.literal("stop", [](const CommandNodeBuilder& form) {
        form.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            context.success(Component::translatable("commands.test.no_tests"));
            return 0;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
