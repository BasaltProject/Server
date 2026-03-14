#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/commands/TagCommand.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

/// parity: vanilla gives up after this many relaxation passes and reports failure.
constexpr std::int32_t kMaxAttempts = 10000;

[[nodiscard]] const std::vector<CommandTarget>& targetsOf(const CommandContext& context) {
    static const std::vector<CommandTarget> none;
    const auto* found = context.get<std::vector<CommandTarget>>("targets");
    return found == nullptr ? none : *found;
}

/// Whether the tag was actually new. Tags live on the player session or on the entity, whichever
/// this target is; a dropped item can be tagged in vanilla but here keeps none to give back.
bool addTag(const CommandTarget& target, const std::string& tag) {
    if (player::PlayerSession* session = target.player(); session != nullptr) {
        return session->tags.insert(tag).second;
    }
    if (entity::ServerEntity* what = target.entity(); what != nullptr) {
        if (std::ranges::find(what->tags, tag) != what->tags.end()) {
            return false;
        }
        what->tags.push_back(tag);
        return true;
    }
    return false;
}

bool removeTag(const CommandTarget& target, const std::string& tag) {
    if (player::PlayerSession* session = target.player(); session != nullptr) {
        return session->tags.erase(tag) != 0;
    }
    if (entity::ServerEntity* what = target.entity(); what != nullptr) {
        const auto found = std::ranges::find(what->tags, tag);
        if (found == what->tags.end()) {
            return false;
        }
        what->tags.erase(found);
        return true;
    }
    return false;
}

/// The one shape `add` and `remove` share: count what changed, refuse when nothing did.
[[nodiscard]] Parsed<std::int32_t> changeTags(const CommandContext& context, bool adding) {
    const std::vector<CommandTarget>& targets = targetsOf(context);
    const auto* tag = context.get<std::string>("name");
    if (tag == nullptr) {
        return 0;
    }
    std::int32_t changed = 0;
    for (const CommandTarget& target : targets) {
        if (adding ? addTag(target, *tag) : removeTag(target, *tag)) {
            changed++;
        }
    }
    const std::string verb = adding ? "add" : "remove";
    const std::string many = targets.size() == 1 ? ".single" : ".multiple";
    if (changed == 0) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.tag." + verb + ".failed" + many)}};
    }
    context.success(targets.size() == 1
                        ? Component::translatable("commands.tag." + verb + ".success.single",
                                                  {Component::text(*tag), Component::text(targets.front().name())})
                        : Component::translatable("commands.tag." + verb + ".success.multiple",
                                                  {Component::text(*tag), Component::text(std::to_string(changed))}),
                    true);
    return changed;
}

} // namespace

TagCommand::TagCommand() : Command("tag", permissionLevel::kGamemasters) {}

void TagCommand::build(const CommandNodeBuilder& root) const {
    root.argument("targets", entitiesArgument(), [](const CommandNodeBuilder& targets) {
        targets.literal("add", [](const CommandNodeBuilder& add) {
            add.argument("name", wordArgument(), [](const CommandNodeBuilder& name) {
                name.executes([](const CommandContext& context) { return changeTags(context, true); });
            });
        });
        targets.literal("remove", [](const CommandNodeBuilder& remove) {
            remove.argument("name", wordArgument(), [](const CommandNodeBuilder& name) {
                // parity: only the tags the targets actually carry, which is what vanilla lists.
                name.suggestsFromServer([](const SuggestionContext& context) {
                    std::set<std::string> carried;
                    const auto* matched = context.argument<std::vector<CommandTarget>>("targets");
                    if (matched != nullptr) {
                        for (const CommandTarget& target : *matched) {
                            carried.insert_range(target.tags());
                        }
                    }
                    return context.matching(std::vector<std::string>{carried.begin(), carried.end()});
                });
                name.executes([](const CommandContext& context) { return changeTags(context, false); });
            });
        });
        targets.literal("list", [](const CommandNodeBuilder& list) {
            list.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::vector<CommandTarget>& targets = targetsOf(context);
                std::set<std::string> tags;
                for (const CommandTarget& target : targets) {
                    tags.insert_range(target.tags());
                }
                std::string joined;
                for (const std::string& tag : tags) {
                    joined += joined.empty() ? tag : ", " + tag;
                }
                const std::string many = targets.size() == 1 ? "single" : "multiple";
                const Component who = targets.size() == 1 ? Component::text(targets.front().name())
                                                          : Component::text(std::to_string(targets.size()));
                context.success(tags.empty()
                                    ? Component::translatable("commands.tag.list." + many + ".empty", {who})
                                    : Component::translatable("commands.tag.list." + many + ".success",
                                                              {who, Component::text(std::to_string(tags.size())),
                                                               Component::text(joined)}));
                return static_cast<std::int32_t>(tags.size());
            });
        });
    });
}

namespace {

/// One relaxation pass. Answers true once nothing had to move, which is when the spread is done.
bool spreadApart(std::vector<std::pair<double, double>>& positions, double spreadDistance) {
    bool settled = true;
    for (std::size_t i = 0; i < positions.size(); i++) {
        double pushX = 0.0;
        double pushZ = 0.0;
        std::int32_t pushes = 0;
        for (std::size_t j = 0; j < positions.size(); j++) {
            if (i == j) {
                continue;
            }
            const double dx = positions[i].first - positions[j].first;
            const double dz = positions[i].second - positions[j].second;
            const double distance = std::sqrt((dx * dx) + (dz * dz));
            if (distance >= spreadDistance) {
                continue;
            }
            settled = false;
            pushes++;
            if (distance == 0.0) {
                pushX += 1.0;
            } else {
                pushX += dx / distance;
                pushZ += dz / distance;
            }
        }
        if (pushes == 0) {
            continue;
        }
        positions[i] = {positions[i].first + (pushX / pushes * spreadDistance),
                        positions[i].second + (pushZ / pushes * spreadDistance)};
    }
    return settled;
}

/// parity: the mean distance between every pair, which is the number vanilla reports.
[[nodiscard]] std::string averageDistance(const std::vector<std::pair<double, double>>& positions) {
    if (positions.size() < 2) {
        return "0.00";
    }
    double total = 0.0;
    std::int32_t pairs = 0;
    for (std::size_t i = 0; i < positions.size(); i++) {
        for (std::size_t j = i + 1; j < positions.size(); j++) {
            const double dx = positions[i].first - positions[j].first;
            const double dz = positions[i].second - positions[j].second;
            total += std::sqrt((dx * dx) + (dz * dz));
            pairs++;
        }
    }
    const auto scaled = static_cast<std::int64_t>(std::llround(total / pairs * 100.0));
    const std::string fraction = std::to_string(scaled % 100);
    return std::to_string(scaled / 100) + "." + (fraction.size() < 2 ? "0" + fraction : fraction);
}

} // namespace

SpreadPlayersCommand::SpreadPlayersCommand(std::function<double()> nextDouble)
    : Command("spreadplayers", permissionLevel::kGamemasters), nextDouble_(std::move(nextDouble)) {}

void SpreadPlayersCommand::build(const CommandNodeBuilder& root) const {
    const auto spread = [this](const CommandContext& context, const std::int32_t* maxHeight) -> Parsed<std::int32_t> {
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const auto* matched = context.get<std::vector<player::PlayerSession*>>("targets");
        const auto* distance = context.get<float>("spreadDistance");
        const auto* range = context.get<float>("maxRange");
        const auto* center = context.get<Coordinates>("center");
        if (matched == nullptr || distance == nullptr || range == nullptr || center == nullptr) {
            return 0;
        }
        const auto spreadDistance = static_cast<double>(*distance);
        const auto maxRange = static_cast<double>(*range);
        if (spreadDistance > maxRange) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::text("The spread distance must not be larger than the maximum range.")}};
        }
        const std::array<double, 3> at = center->resolve(context.source());
        std::vector<std::pair<double, double>> positions;
        positions.reserve(matched->size());
        for (std::size_t index = 0; index < matched->size(); index++) {
            positions.emplace_back(at[0] - maxRange + (nextDouble_() * maxRange * 2),
                                   at[2] - maxRange + (nextDouble_() * maxRange * 2));
        }

        std::int32_t attempts = 0;
        while (attempts < kMaxAttempts && !spreadApart(positions, spreadDistance)) {
            attempts++;
        }
        if (attempts >= kMaxAttempts) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "commands.spreadplayers.failed.entities",
                    {Component::text(std::to_string(matched->size())), Component::text(std::to_string(at[0])),
                     Component::text(std::to_string(at[2])), Component::text(std::to_string(spreadDistance))})}};
        }

        for (std::size_t index = 0; index < matched->size(); index++) {
            player::PlayerSession* target = (*matched)[index];
            const auto [x, z] = positions[index];
            const auto blockX = static_cast<std::int32_t>(x);
            const auto blockZ = static_cast<std::int32_t>(z);
            const server::world::Chunk* column = (*world)->state().chunkIfLoaded(blockX >> 4, blockZ >> 4);
            const double surface =
                column == nullptr ? target->y
                                  : static_cast<double>(column->heightAt(blockX & 15, blockZ & 15,
                                                                         server::world::HeightmapType::MotionBlocking));
            const double y = maxHeight == nullptr ? surface : std::min(surface, static_cast<double>(*maxHeight));
            if (target->connection().teleportTo) {
                target->connection().teleportTo(x, y, z, target->yaw, target->pitch);
            }
        }

        context.success(Component::translatable(
                            "commands.spreadplayers.success.entities",
                            {Component::text(std::to_string(matched->size())), Component::text(std::to_string(at[0])),
                             Component::text(std::to_string(at[2])), Component::text(averageDistance(positions))}),
                        true);
        return static_cast<std::int32_t>(matched->size());
    };

    root.argument("center", vec2Argument(), [spread](const CommandNodeBuilder& center) {
        center.argument("spreadDistance", floatArgument(0.0F), [spread](const CommandNodeBuilder& distance) {
            distance.argument("maxRange", floatArgument(1.0F), [spread](const CommandNodeBuilder& range) {
                range.argument("respectTeams", boolArgument(), [spread](const CommandNodeBuilder& teams) {
                    teams.argument("targets", playersArgument(false), [spread](const CommandNodeBuilder& targets) {
                        targets.executes([spread](const CommandContext& context) { return spread(context, nullptr); });
                    });
                });
                range.literal("under", [spread](const CommandNodeBuilder& under) {
                    under.argument("maxHeight", integerArgument(), [spread](const CommandNodeBuilder& height) {
                        height.argument("respectTeams", boolArgument(), [spread](const CommandNodeBuilder& teams) {
                            teams.argument("targets", playersArgument(false),
                                           [spread](const CommandNodeBuilder& targets) {
                                               targets.executes([spread](const CommandContext& context) {
                                                   return spread(context, context.get<std::int32_t>("maxHeight"));
                                               });
                                           });
                        });
                    });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
