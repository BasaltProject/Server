#include "ExecuteCommand.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using executeCommand::blockPosOf;
using executeCommand::conditional;
using executeCommand::storing;
using executeCommand::targetsOf;
using executeCommand::typedStore;
using executeCommand::wrapping;
using wire::common::Identifier;

/// parity: the five comparisons `/execute if score` accepts.
constexpr std::array<std::string_view, 5> kComparisons{"<", "<=", "=", ">=", ">"};

[[nodiscard]] std::optional<std::int32_t> toInt(std::string_view text) {
    std::int32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// parity: `MinMaxBounds.Ints`, whose forms are `n`, `a..b`, `a..` and `..b`.
[[nodiscard]] bool matchesRange(std::int32_t value, std::string_view range) {
    const std::size_t separator = range.find("..");
    if (separator == std::string_view::npos) {
        return toInt(range) == value;
    }
    const std::string_view low = range.substr(0, separator);
    const std::string_view high = range.substr(separator + 2);
    const std::optional<std::int32_t> min = low.empty() ? std::nullopt : toInt(low);
    const std::optional<std::int32_t> max = high.empty() ? std::nullopt : toInt(high);
    return (!min.has_value() || value >= *min) && (!max.has_value() || value <= *max);
}

/// parity: `if blocks`, whose result is how many matched, and `masked` ignores air in the source.
[[nodiscard]] std::optional<std::int32_t> compareRegions(const CommandContext& context, bool masked) {
    world::LoadedWorld* world = worldIn(context.source(), context.environment());
    if (world == nullptr) {
        return std::nullopt;
    }
    const std::array<std::int32_t, 3> start = blockPosOf(context, "start");
    const std::array<std::int32_t, 3> end = blockPosOf(context, "end");
    const std::array<std::int32_t, 3> destination = blockPosOf(context, "destination");
    const std::int32_t minX = std::min(start[0], end[0]);
    const std::int32_t minY = std::min(start[1], end[1]);
    const std::int32_t minZ = std::min(start[2], end[2]);

    std::int32_t matched = 0;
    for (std::int32_t x = minX; x <= std::max(start[0], end[0]); x++) {
        for (std::int32_t y = minY; y <= std::max(start[1], end[1]); y++) {
            for (std::int32_t z = minZ; z <= std::max(start[2], end[2]); z++) {
                const std::optional<std::int32_t> sourceState = world->state().blockState(x, y, z);
                if (!sourceState.has_value()) {
                    return std::nullopt;
                }
                if (masked && *sourceState == reg::blocks::air().stateId) {
                    continue;
                }
                const std::optional<std::int32_t> targetState = world->state().blockState(
                    destination[0] + (x - minX), destination[1] + (y - minY), destination[2] + (z - minZ));
                if (!targetState.has_value() || *sourceState != *targetState) {
                    return std::nullopt;
                }
                matched++;
            }
        }
    }
    return matched;
}

[[nodiscard]] std::optional<std::int32_t> matchesBiome(const CommandContext& context) {
    world::LoadedWorld* world = worldIn(context.source(), context.environment());
    const auto* wanted = context.get<Identifier>("biome");
    if (world == nullptr || wanted == nullptr) {
        return std::nullopt;
    }
    const std::array<std::int32_t, 3> at = blockPosOf(context, "pos");
    const server::world::Chunk* chunk = world->state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    if (chunk == nullptr) {
        return std::nullopt;
    }
    const std::int32_t localY = at[1] - chunk->verticalBounds().minY();
    if (localY < 0 || static_cast<std::size_t>(localY / 16) >= chunk->sectionCount()) {
        return std::nullopt;
    }
    const std::int32_t biomeId = chunk->biomeId((at[0] & 15) / 4, localY / 4, (at[2] & 15) / 4);
    const Identifier* name = reg::biomes::registry().nameOf(biomeId);
    return name != nullptr && *name == *wanted ? std::optional{1} : std::nullopt;
}

/// The score an argument pair names, or nothing when either half is missing.
[[nodiscard]] std::optional<std::int32_t> scoreOf(const scoreboard::Scoreboard& scoreboard,
                                                  const CommandContext& context, std::string_view holderName,
                                                  std::string_view objectiveName) {
    const auto* holders = context.get<std::vector<std::string>>(holderName);
    const auto* objective = context.get<std::string>(objectiveName);
    if (holders == nullptr || holders->empty() || objective == nullptr) {
        return std::nullopt;
    }
    return scoreboard.score(holders->front(), *objective);
}

} // namespace

void ExecuteCommand::buildConditions(const CommandNodeBuilder& builder, CommandNode& execute, bool positive) const {
    builder.literal("block", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("pos", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& pos) {
            pos.argument("block", blockPredicateArgument(), [&execute, positive](const CommandNodeBuilder& block) {
                conditional(block, execute, positive, [](const CommandContext& context) {
                    world::LoadedWorld* world = worldIn(context.source(), context.environment());
                    const auto* predicate = context.get<BlockPredicateInput>("block");
                    const std::array<std::int32_t, 3> at = blockPosOf(context, "pos");
                    const std::optional<std::int32_t> state =
                        world == nullptr ? std::nullopt : world->state().blockState(at[0], at[1], at[2]);
                    return state.has_value() && predicate != nullptr && predicate->matches(*state) ? std::optional{1}
                                                                                                   : std::nullopt;
                });
            });
        });
    });

    builder.literal("blocks", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("start", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& start) {
            start.argument("end", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& end) {
                end.argument(
                    "destination", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& destination) {
                        for (const bool masked : {false, true}) {
                            destination.literal(
                                masked ? "masked" : "all", [&execute, positive, masked](const CommandNodeBuilder& one) {
                                    conditional(one, execute, positive, [masked](const CommandContext& context) {
                                        return compareRegions(context, masked);
                                    });
                                });
                        }
                    });
            });
        });
    });

    builder.literal("data", [this, &execute, positive](const CommandNodeBuilder& form) {
        const auto existing = [this](const CommandContext& context,
                                     const DataHolderRef& holder) -> std::optional<std::int32_t> {
            const auto* path = context.get<std::string>("path");
            return hooks_.data != nullptr && path != nullptr && hooks_.data->pathExists(context, holder, *path)
                       ? std::optional{1}
                       : std::nullopt;
        };
        form.literal("block", [&execute, positive, existing](const CommandNodeBuilder& one) {
            one.argument(
                "sourcePos", blockPosArgument(), [&execute, positive, existing](const CommandNodeBuilder& pos) {
                    pos.argument("path", nbtPathArgument(),
                                 [&execute, positive, existing](const CommandNodeBuilder& path) {
                                     conditional(path, execute, positive, [existing](const CommandContext& context) {
                                         return existing(context, DataHolderRef{DataHolderRef::Block{
                                                                      .pos = blockPosOf(context, "sourcePos")}});
                                     });
                                 });
                });
        });
        form.literal("entity", [&execute, positive, existing](const CommandNodeBuilder& one) {
            one.argument(
                "source", entitiesArgument(true), [&execute, positive, existing](const CommandNodeBuilder& source) {
                    source.argument(
                        "path", nbtPathArgument(), [&execute, positive, existing](const CommandNodeBuilder& path) {
                            conditional(path, execute, positive,
                                        [existing](const CommandContext& context) -> std::optional<std::int32_t> {
                                            const std::vector<CommandTarget>& matched = targetsOf(context, "source");
                                            const std::optional<DataHolderRef> holder =
                                                matched.empty() ? std::nullopt : entityHolderOf(matched.front());
                                            if (!holder.has_value()) {
                                                return std::nullopt;
                                            }
                                            return existing(context, *holder);
                                        });
                        });
                });
        });
        form.literal("storage", [&execute, positive, existing](const CommandNodeBuilder& one) {
            one.argument(
                "source", resourceLocationArgument(), [&execute, positive, existing](const CommandNodeBuilder& source) {
                    source.argument(
                        "path", nbtPathArgument(), [&execute, positive, existing](const CommandNodeBuilder& path) {
                            conditional(path, execute, positive,
                                        [existing](const CommandContext& context) -> std::optional<std::int32_t> {
                                            const auto* id = context.get<Identifier>("source");
                                            if (id == nullptr) {
                                                return std::nullopt;
                                            }
                                            return existing(context, DataHolderRef{DataHolderRef::Storage{.id = *id}});
                                        });
                        });
                });
        });
    });

    builder.literal("dimension", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("dimension", worldArgument(), [&execute, positive](const CommandNodeBuilder& named) {
            conditional(named, execute, positive, [](const CommandContext& context) -> std::optional<std::int32_t> {
                auto* const* wanted = context.get<world::LoadedWorld*>("dimension");
                world::LoadedWorld* here = worldIn(context.source(), context.environment());
                return wanted != nullptr && here != nullptr && (*wanted)->name() == here->name() ? std::optional{1}
                                                                                                 : std::nullopt;
            });
        });
    });

    builder.literal("entity", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("entities", optionalEntitiesArgument(), [&execute, positive](const CommandNodeBuilder& entities) {
            conditional(entities, execute, positive, [](const CommandContext& context) -> std::optional<std::int32_t> {
                const std::size_t count = targetsOf(context, "entities").size();
                return count > 0 ? std::optional{static_cast<std::int32_t>(count)} : std::nullopt;
            });
        });
    });

    builder.literal("loaded", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("pos", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& pos) {
            conditional(pos, execute, positive, [](const CommandContext& context) -> std::optional<std::int32_t> {
                world::LoadedWorld* world = worldIn(context.source(), context.environment());
                const std::array<std::int32_t, 3> at = blockPosOf(context, "pos");
                return world != nullptr && world->state().isLoaded(at[0] >> 4, at[2] >> 4) ? std::optional{1}
                                                                                           : std::nullopt;
            });
        });
    });

    builder.literal("biome", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument("pos", blockPosArgument(), [&execute, positive](const CommandNodeBuilder& pos) {
            pos.argument("biome", resourceOrTagArgument("minecraft:worldgen/biome"),
                         [&execute, positive](const CommandNodeBuilder& biome) {
                             conditional(biome, execute, positive, matchesBiome);
                         });
        });
    });

    // parity: `ExecuteIfFunctionCustomModifier`: the first function to return decides, `if` passing
    // on a value other than 0 and `unless` on 0; a source whose functions never return goes nowhere.
    builder.literal("function", [this, &execute, positive](const CommandNodeBuilder& form) {
        form.argument("name", functionArgument(), [this, &execute, positive](const CommandNodeBuilder& named) {
            named.redirectTo(execute, true, [this, positive](const CommandContext& context) {
                std::vector<std::shared_ptr<const CommandSource>> onwards;
                const auto* name = context.get<FunctionName>("name");
                FunctionRunner* runner = hooks_.functions;
                if (runner == nullptr || name == nullptr) {
                    return onwards;
                }
                std::vector<const function::ServerFunction*> functions;
                if (name->isTag) {
                    functions = runner->library().tag(name->id);
                } else if (const function::ServerFunction* one = runner->library().get(name->id); one != nullptr) {
                    functions.push_back(one);
                }
                const ExecutionSource running{
                    context.source(),
                    ExecutionSource::Changes{
                        .permission = std::max(context.source().permissionLevel(), permissionLevel::kGamemasters),
                        .silent = true}};
                if (runner->insideFunction() && context.tailNode() != nullptr) {
                    // parity: `scheduleFunctionConditionsAndTest`: the functions are queued in an
                    // isolated call, and the rest of the line resumes for this source afterwards.
                    const CommandDispatcher* dispatcher = &context.dispatcher();
                    const CommandNode* tail = context.tailNode();
                    const std::int32_t cursor = context.tailCursor();
                    const std::uint64_t generation = dispatcher->generation();
                    std::shared_ptr<const CommandSource> resumed = runner->keep(context.source());
                    runner->deferUntilReturn(
                        *dispatcher, runner->keep(running), std::move(functions),
                        [dispatcher, tail, cursor, generation, resumed, input = context.input(),
                         positive](const std::optional<FunctionRunner::Returned>& returned) {
                            if (returned.has_value() && (returned->value != 0) == positive) {
                                static_cast<void>(dispatcher->continueTail(*tail, cursor, input, *resumed, generation));
                            }
                        });
                    return onwards;
                }
                for (const function::ServerFunction* function : functions) {
                    const Parsed<std::optional<FunctionRunner::Returned>> ran =
                        runner->call(context.dispatcher(), running, *function);
                    if (!ran.has_value() || !ran->has_value()) {
                        continue;
                    }
                    if (((*ran)->value != 0) == positive) {
                        onwards.push_back(wrapping(context.source(), {}));
                    }
                    break;
                }
                return onwards;
            });
        });
    });

    builder.literal("score", [this, &execute, positive](const CommandNodeBuilder& form) {
        form.argument("target", scoreHolderArgument(), [this, &execute, positive](const CommandNodeBuilder& target) {
            target.argument(
                "targetObjective", objectiveArgument(),
                [this, &execute, positive](const CommandNodeBuilder& objective) {
                    for (const std::string_view comparison : kComparisons) {
                        objective.literal(std::string{comparison}, [this, &execute, positive,
                                                                    comparison](const CommandNodeBuilder& one) {
                            one.argument(
                                "source", scoreHolderArgument(),
                                [this, &execute, positive, comparison](const CommandNodeBuilder& source) {
                                    source.argument(
                                        "sourceObjective", objectiveArgument(),
                                        [this, &execute, positive, comparison](const CommandNodeBuilder& other) {
                                            conditional(
                                                other, execute, positive,
                                                [this, comparison](
                                                    const CommandContext& context) -> std::optional<std::int32_t> {
                                                    const std::optional<std::int32_t> value =
                                                        scoreOf(*scoreboard_, context, "target",
                                                                "targetObjecti"
                                                                "ve");
                                                    const std::optional<std::int32_t> against =
                                                        scoreOf(*scoreboard_, context, "source", "sourceObjective");
                                                    if (!value.has_value() || !against.has_value()) {
                                                        return std::nullopt;
                                                    }
                                                    const bool holds = comparison == "<"    ? *value < *against
                                                                       : comparison == "<=" ? *value <= *against
                                                                       : comparison == "="  ? *value == *against
                                                                       : comparison == ">=" ? *value >= *against
                                                                                            : *value > *against;
                                                    return holds ? std::optional{1} : std::nullopt;
                                                });
                                        });
                                });
                        });
                    }
                    objective.literal("matches", [this, &execute, positive](const CommandNodeBuilder& matches) {
                        matches.argument(
                            "range", intRangeArgument(), [this, &execute, positive](const CommandNodeBuilder& range) {
                                conditional(range, execute, positive,
                                            [this](const CommandContext& context) -> std::optional<std::int32_t> {
                                                const std::optional<std::int32_t> value =
                                                    scoreOf(*scoreboard_, context, "target", "targetObjective");
                                                const auto* wanted = context.get<std::string>("range");
                                                if (!value.has_value() || wanted == nullptr) {
                                                    return std::nullopt;
                                                }
                                                return matchesRange(*value, *wanted) ? std::optional{1} : std::nullopt;
                                            });
                            });
                    });
                });
        });
    });

    // Declared so the tree matches vanilla's; each needs a system this build has not got.
    const auto never = [](const CommandContext&) { return std::optional<std::int32_t>{}; };
    // parity: `ExecuteCommand.checkCustomPredicate`, this entity if the source has one, at its spot.
    builder.literal("predicate", [&execute, positive](const CommandNodeBuilder& form) {
        form.argument(
            "predicate", lootPredicateOrInlineArgument(), [&execute, positive](const CommandNodeBuilder& one) {
                conditional(one, execute, positive, [](const CommandContext& context) -> std::optional<std::int32_t> {
                    const auto* named = context.get<ResourceOrInline>("predicate");
                    const auto& test = context.environment().options().predicate;
                    if (named == nullptr || !test) {
                        return std::nullopt;
                    }
                    const wire::common::JsonValue value = named->inlineValue.value_or(
                        wire::common::JsonValue::of(named->id.has_value() ? named->id->toString() : std::string{}));
                    const ExecutionSource* execution = context.source().asExecution();
                    std::optional<CommandTarget> self;
                    if (execution != nullptr && execution->entity().has_value()) {
                        self = execution->entity();
                    } else if (context.source().player() != nullptr) {
                        self = CommandTarget{*context.source().player()};
                    }
                    const std::optional<bool> passed =
                        test(value, self.has_value() ? &*self : nullptr,
                             worldIn(context.source(), context.environment()), context.source().position());
                    return passed.value_or(false) ? std::optional{1} : std::nullopt;
                });
            });
    });
    builder.literal("items", [&execute, positive, never](const CommandNodeBuilder& form) {
        const auto tail = [&execute, positive, never](const CommandNodeBuilder& node) {
            node.argument("slots", itemSlotsArgument(), [&execute, positive, never](const CommandNodeBuilder& slots) {
                slots.argument("item_predicate", itemPredicateArgument(),
                               [&execute, positive, never](const CommandNodeBuilder& one) {
                                   conditional(one, execute, positive, never);
                               });
            });
        };
        form.literal("block", [tail](const CommandNodeBuilder& one) { one.argument("pos", blockPosArgument(), tail); });
        form.literal("entity",
                     [tail](const CommandNodeBuilder& one) { one.argument("entities", entitiesArgument(), tail); });
    });
    builder.literal("stopwatch", [&execute, positive, never](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [&execute, positive, never](const CommandNodeBuilder& id) {
            id.argument("range", floatRangeArgument(), [&execute, positive, never](const CommandNodeBuilder& one) {
                conditional(one, execute, positive, never);
            });
        });
    });
}

void ExecuteCommand::buildStores(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess) const {
    builder.literal("score", [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
        form.argument(
            "targets", scoreHolderArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& targets) {
                targets.argument("objective", objectiveArgument(),
                                 [this, &execute, fromSuccess](const CommandNodeBuilder& objective) {
                                     storing(objective, execute, fromSuccess, [this](const CommandContext& context) {
                                         const auto* named = context.get<std::string>("objective");
                                         const auto* found = context.get<std::vector<std::string>>("targets");
                                         const std::string objectiveName = named == nullptr ? std::string{} : *named;
                                         const std::vector<std::string> holders =
                                             found == nullptr ? std::vector<std::string>{} : *found;
                                         return [this, objectiveName, holders](std::int32_t value) {
                                             for (const std::string& holder : holders) {
                                                 scoreboard_->setScore(holder, objectiveName, value);
                                             }
                                         };
                                     });
                                 });
            });
    });

    builder.literal("bossbar", [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
        form.argument("id", resourceLocationArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& id) {
            for (const bool max : {false, true}) {
                id.literal(max ? "max" : "value", [this, &execute, fromSuccess, max](const CommandNodeBuilder& one) {
                    storing(one, execute, fromSuccess, [this, max](const CommandContext& context) {
                        const auto* named = context.get<Identifier>("id");
                        CustomBossBar* bar =
                            hooks_.bossBars == nullptr || named == nullptr ? nullptr : hooks_.bossBars->get(*named);
                        return [bar, max](std::int32_t value) {
                            if (bar == nullptr) {
                                return;
                            }
                            if (max) {
                                bar->max = std::max(1, value);
                            } else {
                                bar->value = value;
                            }
                        };
                    });
                });
            }
        });
    });

    builder.literal("storage", [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
        form.argument("target", resourceLocationArgument(),
                      [this, &execute, fromSuccess](const CommandNodeBuilder& target) {
                          target.argument(
                              "path", nbtPathArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& path) {
                                  typedStore(path, execute, fromSuccess,
                                             [this](const CommandContext& context, std::string_view type) {
                                                 const auto* id = context.get<Identifier>("target");
                                                 const auto* named = context.get<std::string>("path");
                                                 const std::optional<Identifier> target =
                                                     id == nullptr ? std::nullopt : std::optional{*id};
                                                 const std::string where = named == nullptr ? std::string{} : *named;
                                                 const std::string kind{type};
                                                 return [this, target, where, kind](double value) {
                                                     if (hooks_.data != nullptr && target.has_value()) {
                                                         hooks_.data->store(*target, where, value, kind);
                                                     }
                                                 };
                                             });
                              });
                      });
    });

    builder.literal("block", [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
        form.argument("targetPos", blockPosArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& pos) {
            pos.argument("path", nbtPathArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& path) {
                // The block store needs a context of its own to reach the world, so the position is
                // captured and the write is done against the source that reported the result.
                typedStore(path, execute, fromSuccess, [this](const CommandContext& context, std::string_view type) {
                    const auto* named = context.get<std::string>("path");
                    const std::string where = named == nullptr ? std::string{} : *named;
                    const std::string kind{type};
                    const std::array<std::int32_t, 3> at = blockPosOf(context, "targetPos");
                    const CommandContext held = context;
                    return [this, held, at, where, kind](double value) {
                        if (hooks_.data != nullptr) {
                            hooks_.data->storeInBlock(held, at, where, value, kind);
                        }
                    };
                });
            });
        });
    });

    builder.literal("entity", [this, &execute, fromSuccess](const CommandNodeBuilder& form) {
        form.argument(
            "target", entitiesArgument(true), [this, &execute, fromSuccess](const CommandNodeBuilder& target) {
                target.argument(
                    "path", nbtPathArgument(), [this, &execute, fromSuccess](const CommandNodeBuilder& path) {
                        typedStore(path, execute, fromSuccess,
                                   [this](const CommandContext& context, std::string_view type) {
                                       const auto* named = context.get<std::string>("path");
                                       const std::string where = named == nullptr ? std::string{} : *named;
                                       const std::string kind{type};
                                       const std::vector<CommandTarget>& matched = targetsOf(context, "target");
                                       const std::optional<DataHolderRef> holder =
                                           matched.size() == 1 ? entityHolderOf(matched.front()) : std::nullopt;
                                       return [this, holder, where, kind](double value) {
                                           const auto* entity =
                                               holder.has_value() ? holder->as<DataHolderRef::Entity>() : nullptr;
                                           if (hooks_.data != nullptr && entity != nullptr) {
                                               hooks_.data->storeInEntity(*entity, where, value, kind);
                                           }
                                       };
                                   });
                    });
            });
    });
}

} // namespace basalt::server::runtime::command::commands
