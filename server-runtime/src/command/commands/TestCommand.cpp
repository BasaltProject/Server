#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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

TestCommand::TestCommand(const world::RealTestInstances& instances, FunctionRunner& runner,
                         std::function<BlockTests*()> blockTests)
    : Command("test", permissionLevel::kGamemasters), instances_(&instances), runner_(&runner),
      blockTests_(std::move(blockTests)) {}

void TestCommand::build(const CommandNodeBuilder& root) const {
    // parity: `ReportGameListener`, in green when it passed and red or yellow when it did not.
    const auto reporterFor = [this](const CommandEnvironment* environment) -> BlockTests::Reporter {
        return [this, environment](const BlockTests::Report& report) {
            if (report.passed) {
                failed_.erase(report.id);
                environment->broadcast(
                    Component::text(report.id.toString() + " passed! (" + std::to_string(report.ticks) + "gameticks)"));
                return;
            }
            failed_.insert(report.id);
            environment->broadcast(
                Component::text(std::string{report.required ? "" : "(optional) "} + report.id.toString() + " failed! ")
                    .append(report.failure.has_value() ? *report.failure : Component::text("no reason")));
        };
    };
    const auto syncAround = [](const CommandContext& context,
                               const world::LoadedWorld& placedIn) -> BlockTests::Placed {
        const CommandEnvironment* environment = &context.environment();
        const world::LoadedWorld* where = &placedIn;
        return [environment, where](const server::world::structure::StructureBoundingBox& box) {
            environment->syncChunks(*where, box.minX, box.minZ, box.maxX, box.maxZ);
        };
    };
    // parity: `RetryOptions` and the flaky tries of the test itself, as `ReportGameListener` counts them.
    using Begin = std::function<std::optional<std::string>(BlockTests::Reporter)>;
    const auto withRetries = [reporterFor](const CommandContext& context, const world::TestInstance& instance,
                                           Begin begin) -> std::optional<std::string> {
        struct Tally {
            std::int32_t made = 0;
            std::int32_t ok = 0;
        };
        const auto* times = context.get<std::int32_t>("numberOfTimes");
        const auto* until = context.get<bool>("untilFailed");
        const std::int32_t tries = times == nullptr ? 1 : *times;
        const bool halt = until != nullptr && *until;
        const bool flaky = times == nullptr && instance.maxAttempts > 1;
        const std::int32_t maxAttempts = instance.maxAttempts;
        const std::int32_t required = instance.requiredSuccesses;
        const CommandEnvironment* environment = &context.environment();
        const BlockTests::Reporter plain = reporterFor(environment);
        const auto tally = std::make_shared<Tally>();
        using Callback = std::function<void(const BlockTests::Report&)>;
        const auto self = std::make_shared<Callback>();
        const std::weak_ptr<Callback> weak = self;
        const auto again = [weak, begin]() {
            if (const std::shared_ptr<Callback> strong = weak.lock(); strong != nullptr) {
                static_cast<void>(begin([strong](const BlockTests::Report& next) { (*strong)(next); }));
            }
        };
        *self = [tally, plain, again, tries, halt, flaky, maxAttempts, required,
                 environment](const BlockTests::Report& report) {
            tally->made++;
            tally->ok += report.passed ? 1 : 0;
            if (flaky) {
                const std::string attempt = std::to_string(tally->made) + "/" + std::to_string(maxAttempts);
                if (report.passed && tally->ok >= required) {
                    plain(report);
                } else if (report.passed) {
                    environment->broadcast(Component::text("Flaky test " + report.id.toString() +
                                                           " succeeded, attempt: " + attempt +
                                                           " successes: " + std::to_string(tally->ok)));
                    again();
                } else if (maxAttempts - tally->made + tally->ok >= required) {
                    environment->broadcast(Component::text(
                        "Flaky test " + report.id.toString() + " failed, attempt: " + attempt +
                        ", successes: " + std::to_string(tally->ok) + " (" + std::to_string(required) + " required)"));
                    again();
                } else {
                    BlockTests::Report spent = report;
                    spent.failure = Component::text("Not enough successes: " + std::to_string(tally->ok) + " out of " +
                                                    std::to_string(tally->made) + " attempts, " +
                                                    std::to_string(required) + " required");
                    plain(spent);
                }
                return;
            }
            if (tries != 1) {
                std::string head = "[Run: " + std::to_string(tally->made) + ", Ok: " + std::to_string(tally->ok) +
                                   ", Fail: " + std::to_string(tally->made - tally->ok);
                if (tries != 0) {
                    head += ", Left: " + std::to_string(tries - tally->made);
                }
                environment->broadcast(Component::text(head + "]"));
            }
            plain(report);
            if (tries != 1 && (tries == 0 || tally->made < tries) && !(halt && !report.passed)) {
                again();
            }
        };
        return begin([self](const BlockTests::Report& report) { (*self)(report); });
    };
    /// The test instance blocks around the source: all within `radius`, or only the nearest one.
    const auto blocksAround = [](const CommandContext& context, BlockTests& tests, world::LoadedWorld& world,
                                 std::int32_t radius, bool onlyNearest) {
        const std::array<double, 3> here = context.source().position();
        std::vector<BlockTests::Found> found = tests.find(world, static_cast<std::int32_t>(std::floor(here[0])),
                                                          static_cast<std::int32_t>(std::floor(here[1])),
                                                          static_cast<std::int32_t>(std::floor(here[2])), radius);
        if (onlyNearest && found.size() > 1) {
            found.erase(found.begin() + 1, found.end());
        }
        return found;
    };
    const auto runAll = [this, withRetries,
                         syncAround](const CommandContext& context,
                                     const std::vector<world::TestInstance>& selected) -> Parsed<std::int32_t> {
        if (selected.empty()) {
            context.success(Component::translatable("commands.test.no_tests"));
            return 0;
        }
        std::int32_t passed = 0;
        std::int32_t skipped = 0;
        std::int32_t started = 0;
        for (const world::TestInstance& instance : selected) {
            BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr;
            if (blockTests != nullptr && instance.type == Identifier::vanilla("block_based") &&
                instance.structure.has_value()) {
                // parity: `StructureGridSpawner`, each test in its own cell beside the last.
                constexpr std::int32_t kCell = 32;
                const Parsed<world::LoadedWorld*> where = context.world();
                if (!where.has_value()) {
                    return std::unexpected{where.error()};
                }
                const std::array<double, 3> here = context.source().position();
                // parity: `TestCommand.TEST_POS_Z_OFFSET_FROM_PLAYER`.
                constexpr std::int32_t kZOffset = 3;
                const std::array<std::int32_t, 3> origin{static_cast<std::int32_t>(std::floor(here[0])) +
                                                             (started * kCell),
                                                         static_cast<std::int32_t>(std::floor(here[1])),
                                                         static_cast<std::int32_t>(std::floor(here[2])) + kZOffset};
                const std::optional<std::string> refused =
                    withRetries(context, instance,
                                [blockTests, world = *where, instance, origin,
                                 placed = syncAround(context, **where)](BlockTests::Reporter reporter) {
                                    return blockTests->start(*world, instance, origin, std::move(reporter), placed);
                                });
                if (refused.has_value()) {
                    failed_.insert(instance.id);
                    context.source().sendSystemMessage(
                        Component::text("  " + instance.id.toString() + ": cannot start, " + *refused));
                    continue;
                }
                started++;
                continue;
            }
            if (!instance.isFunctionTest() || !instance.function.has_value()) {
                skipped++;
                context.source().sendSystemMessage(
                    Component::text("  " + instance.id.toString() + ": skipped, this build runs function tests only"));
                continue;
            }
            // parity: `BuiltinTestFunctions`, whose one function, `always_pass`, succeeds at once.
            const bool builtinPass = *instance.function == Identifier::vanilla("always_pass");
            const Parsed<std::int32_t> outcome =
                builtinPass ? Parsed<std::int32_t>{1}
                            : runner_->run(context.dispatcher(), context.source(), *instance.function);
            if (outcome.has_value()) {
                passed++;
                failed_.erase(instance.id);
                context.source().sendSystemMessage(Component::text("  " + instance.id.toString() + ": passed"));
            } else {
                failed_.insert(instance.id);
                context.source().sendSystemMessage(Component::text("  " + instance.id.toString() + ": failed"));
            }
        }
        if (started > 0) {
            context.source().sendSystemMessage(
                Component::text("Started " + std::to_string(started) + " block based test(s); results follow."));
        }
        const auto ran = static_cast<std::int32_t>(selected.size()) - skipped - started;
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
    // parity: `TestFinder.Builder.allNearby` (15), `nearest` and `lookedAt` over the test instance blocks in the world.
    constexpr std::int32_t kNearby = 15;
    constexpr std::int32_t kFull = 250;
    const auto runBlocks = [this, withRetries, syncAround, blocksAround](const CommandContext& context,
                                                                         std::int32_t radius,
                                                                         bool onlyNearest) -> Parsed<std::int32_t> {
        BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr;
        const Parsed<world::LoadedWorld*> where = context.world();
        if (blockTests == nullptr || !where.has_value()) {
            if (!where.has_value()) {
                return std::unexpected{where.error()};
            }
            return 0;
        }
        const std::vector<BlockTests::Found> found = blocksAround(context, *blockTests, **where, radius, onlyNearest);
        if (found.empty()) {
            context.success(Component::translatable("commands.test.no_tests"));
            return 0;
        }
        std::int32_t started = 0;
        for (const BlockTests::Found& block : found) {
            const world::TestInstance* known = instances_->find(block.test);
            const std::optional<std::string> refused =
                known == nullptr
                    ? std::optional<std::string>{"there is no test " + block.test.toString()}
                    : withRetries(context, *known,
                                  [blockTests, world = *where, block,
                                   placed = syncAround(context, **where)](BlockTests::Reporter reporter) {
                                      return blockTests->restart(*world, block, std::move(reporter), placed);
                                  });
            if (refused.has_value()) {
                context.source().sendSystemMessage(
                    Component::text("  " + block.test.toString() + ": cannot start, " + *refused));
                continue;
            }
            started++;
        }
        context.success(
            Component::translatable("commands.test.run.running", {Component::text(std::to_string(started))}));
        return started;
    };
    const std::array<std::pair<std::string_view, std::pair<std::int32_t, bool>>, 3> kNearby3{
        {{"runthese", {kNearby, false}}, {"runthat", {kNearby, true}}, {"runclosest", {kFull, true}}}};
    for (const auto& [name, how] : kNearby3) {
        root.literal(std::string{name}, [runBlocks, how](const CommandNodeBuilder& form) {
            const CommandExecutor run = [runBlocks, how](const CommandContext& context) {
                return runBlocks(context, how.first, how.second);
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
        failed_.clear();
        // parity: `TestCommand.DEFAULT_CLEAR_RADIUS` and `MAX_CLEAR_RADIUS`.
        const auto* asked = context.get<std::int32_t>("radius");
        const std::int32_t radius = std::min(asked == nullptr ? 250 : *asked, 1024);
        std::int32_t cleared = 0;
        if (BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr; blockTests != nullptr) {
            if (const Parsed<world::LoadedWorld*> where = context.world(); where.has_value()) {
                const std::array<double, 3> here = context.source().position();
                cleared = blockTests->clearAll(**where, static_cast<std::int32_t>(std::floor(here[0])),
                                               static_cast<std::int32_t>(std::floor(here[2])), radius);
            }
        }
        if (cleared == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.test.clear.error.no_tests")}};
        }
        context.success(
            Component::translatable("commands.test.clear.success", {Component::text(std::to_string(cleared))}), true);
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

    const auto resetBlocks = [this, syncAround, blocksAround](const CommandContext& context, std::int32_t radius,
                                                              bool onlyNearest) -> Parsed<std::int32_t> {
        BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr;
        const Parsed<world::LoadedWorld*> where = context.world();
        if (blockTests == nullptr || !where.has_value()) {
            if (!where.has_value()) {
                return std::unexpected{where.error()};
            }
            return 0;
        }
        blockTests->stopAll();
        std::int32_t count = 0;
        for (const BlockTests::Found& block : blocksAround(context, *blockTests, **where, radius, onlyNearest)) {
            if (!blockTests->reset(**where, block, syncAround(context, **where)).has_value()) {
                count++;
            }
        }
        if (count == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.test.reset.error.no_tests")}};
        }
        context.success(
            Component::translatable("commands.test.reset.success", {Component::text(std::to_string(count))}), true);
        return count;
    };
    const std::array<std::pair<std::string_view, std::pair<std::int32_t, bool>>, 3> kReset{
        {{"resetclosest", {kFull, true}}, {"resetthat", {kNearby, true}}, {"resetthese", {kNearby, false}}}};
    for (const auto& [name, how] : kReset) {
        root.literal(std::string{name}, [resetBlocks, how](const CommandNodeBuilder& form) {
            form.executes([resetBlocks, how](const CommandContext& context) {
                return resetBlocks(context, how.first, how.second);
            });
        });
    }
    root.literal("locate", [this](const CommandNodeBuilder& form) {
        form.argument(
            "tests", resourceSelectorArgument("minecraft:test_instance"), [this](const CommandNodeBuilder& named) {
                named.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(testIds(*instances_)); });
                named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr;
                    const Parsed<world::LoadedWorld*> where = context.world();
                    const auto* id = context.get<Identifier>("tests");
                    if (blockTests == nullptr || !where.has_value() || id == nullptr) {
                        if (!where.has_value()) {
                            return std::unexpected{where.error()};
                        }
                        return 0;
                    }
                    context.source().sendSystemMessage(Component::translatable("commands.test.locate.started"));
                    const std::array<double, 3> here = context.source().position();
                    std::int32_t count = 0;
                    for (const BlockTests::Found& block :
                         blockTests->find(**where, static_cast<std::int32_t>(std::floor(here[0])),
                                          static_cast<std::int32_t>(std::floor(here[1])),
                                          static_cast<std::int32_t>(std::floor(here[2])), 250)) {
                        if (block.test != *id) {
                            continue;
                        }
                        const double dx = here[0] - block.at[0];
                        const double dz = here[2] - block.at[2];
                        context.success(
                            Component::translatable("commands.test.locate.found",
                                                    {Component::text("[" + std::to_string(block.at[0]) + ", " +
                                                                     std::to_string(block.at[1]) + ", " +
                                                                     std::to_string(block.at[2]) + "]"),
                                                     Component::text(std::to_string(static_cast<std::int32_t>(
                                                         std::floor(std::sqrt((dx * dx) + (dz * dz))))))}),
                            false);
                        count++;
                    }
                    if (count == 0) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.test.error.no_test_instances")}};
                    }
                    context.success(
                        Component::translatable("commands.test.locate.done", {Component::text(std::to_string(count))}),
                        true);
                    return count;
                });
            });
    });

    // parity: `TestCommand.createNewStructure`: an empty test of the asked size, five on every side by default.
    const CommandExecutor create = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("id");
        BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr;
        const Parsed<world::LoadedWorld*> where = context.world();
        if (id == nullptr || blockTests == nullptr) {
            return 0;
        }
        if (!where.has_value()) {
            return std::unexpected{where.error()};
        }
        const auto sizeOf = [&context](const char* name) {
            const auto* asked = context.get<std::int32_t>(name);
            return asked == nullptr ? 5 : *asked;
        };
        const std::array<std::int32_t, 3> size{sizeOf("width"), sizeOf("height"), sizeOf("depth")};
        if (size[0] > 48 || size[1] > 48 || size[2] > 48) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.test.error.too_large",
                                                                        {Component::text(std::to_string(48))})}};
        }
        const std::array<double, 3> here = context.source().position();
        constexpr std::int32_t kZOffset = 3;
        const std::array<std::int32_t, 3> at{static_cast<std::int32_t>(std::floor(here[0])),
                                             static_cast<std::int32_t>(std::floor(here[1])),
                                             static_cast<std::int32_t>(std::floor(here[2])) + kZOffset};
        const CommandEnvironment* environment = &context.environment();
        const world::LoadedWorld* placedIn = *where;
        const std::optional<std::string> refused = blockTests->create(
            **where, *id, at, size, [environment, placedIn](const server::world::structure::StructureBoundingBox& box) {
                environment->syncChunks(*placedIn, box.minX, box.minZ, box.maxX, box.maxZ);
            });
        if (refused.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::text(*refused)}};
        }
        context.success(Component::text("Created " + id->toString() + " at " + std::to_string(at[0]) + ", " +
                                        std::to_string(at[1]) + ", " + std::to_string(at[2])),
                        true);
        return 1;
    };
    root.literal("create", [create](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [create](const CommandNodeBuilder& id) {
            id.executes(create);
            id.argument("width", integerArgument(1), [create](const CommandNodeBuilder& width) {
                width.executes(create);
                width.argument("height", integerArgument(1), [create](const CommandNodeBuilder& height) {
                    height.executes(create);
                    height.argument("depth", integerArgument(1),
                                    [create](const CommandNodeBuilder& depth) { depth.executes(create); });
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
    root.literal("stop", [this](const CommandNodeBuilder& form) {
        form.executes([this](const CommandContext&) -> Parsed<std::int32_t> {
            if (BlockTests* blockTests = blockTests_ ? blockTests_() : nullptr; blockTests != nullptr) {
                blockTests->stopAll();
            }
            return 1;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
