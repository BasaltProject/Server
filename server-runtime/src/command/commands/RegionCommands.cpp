#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/RegionCommands.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::text::Component;

/// parity: vanilla's own cap on one `/forceload add`.
constexpr std::int64_t kMaxChunks = 256;
/// parity: vanilla's own cap on `/fillbiome`, in quarts rather than blocks.
constexpr std::int64_t kMaxQuarts = 32768;

enum class CloneMode : std::uint8_t { Normal, Force, Move };

[[nodiscard]] std::array<std::int32_t, 3> resolveBlock(const CommandContext& context, std::string_view name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0}
                                  : coordinates->resolve(context.source().position());
}

/// The world an argument named, or the one the command acts on when it named none.
[[nodiscard]] world::LoadedWorld* worldNamed(const CommandContext& context, std::string_view name,
                                             world::LoadedWorld* fallback) {
    auto* const* found = context.get<world::LoadedWorld*>(name);
    return found == nullptr || *found == nullptr ? fallback : *found;
}

[[nodiscard]] Parsed<std::int32_t> clone(const CommandContext& context, CloneMode mode, bool skipAir, bool filtered) {
    const Parsed<world::LoadedWorld*> here = context.world();
    if (!here.has_value()) {
        return std::unexpected{here.error()};
    }
    world::LoadedWorld* source = worldNamed(context, "sourceDimension", *here);
    world::LoadedWorld* target = worldNamed(context, "targetDimension", source);
    const BlockPredicateInput* filter = filtered ? context.get<BlockPredicateInput>("filter") : nullptr;
    BlockEditor editor{context.environment(), *source};
    BlockEditor destination{context.environment(), *target};

    const std::array<std::int32_t, 3> begin = resolveBlock(context, "begin");
    const std::array<std::int32_t, 3> end = resolveBlock(context, "end");
    const std::array<std::int32_t, 3> to = resolveBlock(context, "destination");
    const std::int32_t minX = std::min(begin[0], end[0]);
    const std::int32_t minY = std::min(begin[1], end[1]);
    const std::int32_t minZ = std::min(begin[2], end[2]);
    const std::int32_t maxX = std::max(begin[0], end[0]);
    const std::int32_t maxY = std::max(begin[1], end[1]);
    const std::int32_t maxZ = std::max(begin[2], end[2]);
    const std::int32_t spanX = maxX - minX + 1;
    const std::int32_t spanY = maxY - minY + 1;
    const std::int32_t spanZ = maxZ - minZ + 1;

    const std::int64_t volume = static_cast<std::int64_t>(spanX) * spanY * spanZ;
    const std::int32_t cap = source->gameRules().number(world::gameRules::kMaxBlockModifications);
    if (volume > cap) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.clone.toobig", {Component::text(std::to_string(cap)),
                                                                           Component::text(std::to_string(volume))})}};
    }

    const bool overlaps = to[0] <= maxX && to[0] + spanX > minX && to[1] <= maxY && to[1] + spanY > minY &&
                          to[2] <= maxZ && to[2] + spanZ > minZ;
    if (overlaps && target == source && mode != CloneMode::Force) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.clone.overlap")}};
    }

    for (const auto& [where, at] : std::array<std::pair<BlockEditor*, std::array<std::int32_t, 3>>, 4>{
             {{&editor, {minX, minY, minZ}},
              {&editor, {maxX, maxY, maxZ}},
              {&destination, {to[0], to[1], to[2]}},
              {&destination, {to[0] + spanX - 1, to[1] + spanY - 1, to[2] + spanZ - 1}}}}) {
        if (const std::optional<CommandSyntaxError> failure = where->require(at[0], at[1], at[2]);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
    }

    // Read first, write after: a `force` clone onto itself would otherwise copy blocks it has
    // already overwritten.
    struct Copied {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::int32_t stateId = 0;
    };

    std::vector<Copied> copied;
    for (std::int32_t x = 0; x < spanX; x++) {
        for (std::int32_t y = 0; y < spanY; y++) {
            for (std::int32_t z = 0; z < spanZ; z++) {
                const std::optional<std::int32_t> state = editor.stateAt(minX + x, minY + y, minZ + z);
                if (!state.has_value() || (skipAir && *state == reg::blocks::air().stateId)) {
                    continue;
                }
                if (filter != nullptr && !filter->matches(*state)) {
                    continue;
                }
                copied.push_back({.x = x, .y = y, .z = z, .stateId = *state});
            }
        }
    }

    if (mode == CloneMode::Move) {
        for (const Copied& one : copied) {
            static_cast<void>(editor.set(minX + one.x, minY + one.y, minZ + one.z, reg::blocks::air().stateId));
        }
    }
    std::int32_t count = 0;
    for (const Copied& one : copied) {
        if (destination.set(to[0] + one.x, to[1] + one.y, to[2] + one.z, one.stateId)) {
            count++;
        }
    }
    editor.flush();
    if (target != source) {
        destination.flush();
    }

    if (count == 0) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.clone.failed")}};
    }
    context.success(Component::translatable("commands.clone.success", {Component::text(std::to_string(count))}), true);
    return count;
}

} // namespace

CloneCommand::CloneCommand() : Command("clone", permissionLevel::kGamemasters) {}

namespace {

/// The three movements every clone form ends in, plus the bare one Basalt accepts loose.
void cloneMovements(const CommandNodeBuilder& node, bool skipAir, bool filtered) {
    node.executes([skipAir, filtered](const CommandContext& context) {
        return clone(context, CloneMode::Normal, skipAir, filtered);
    });
    for (const auto& [word, mode] : std::array<std::pair<std::string_view, CloneMode>, 3>{
             {{"normal", CloneMode::Normal}, {"force", CloneMode::Force}, {"move", CloneMode::Move}}}) {
        node.literal(std::string{word}, [mode, skipAir, filtered](const CommandNodeBuilder& form) {
            form.executes([mode, skipAir, filtered](const CommandContext& context) {
                return clone(context, mode, skipAir, filtered);
            });
        });
    }
}

void cloneFilters(const CommandNodeBuilder& node) {
    node.literal("replace", [](const CommandNodeBuilder& form) { cloneMovements(form, false, false); });
    node.literal("masked", [](const CommandNodeBuilder& form) { cloneMovements(form, true, false); });
    node.literal("filtered", [](const CommandNodeBuilder& form) {
        form.argument("filter", blockPredicateArgument(),
                      [](const CommandNodeBuilder& filter) { cloneMovements(filter, false, true); });
    });
}

/// The tail every form shares: what to copy, and what to do with the source afterwards.
///
/// `strict` is declared and accepted: vanilla uses it to copy states nothing would normally place,
/// and this build's clone already writes them exactly, so it changes nothing here.
void cloneModes(const CommandNodeBuilder& node) {
    // The bare movement is Basalt's own: vanilla asks for `replace`/`masked`/`filtered` first, and
    // accepting it loose is syntax on top rather than syntax that differs.
    cloneMovements(node, false, false);
    cloneFilters(node);
    node.literal("strict", [](const CommandNodeBuilder& strict) {
        strict.executes([](const CommandContext& context) { return clone(context, CloneMode::Normal, false, false); });
        cloneFilters(strict);
    });
}

} // namespace

void CloneCommand::build(const CommandNodeBuilder& root) const {
    const auto region = [](const CommandNodeBuilder& node) {
        node.argument("begin", blockPosArgument(), [](const CommandNodeBuilder& begin) {
            begin.argument("end", blockPosArgument(), [](const CommandNodeBuilder& end) {
                end.argument("destination", blockPosArgument(), cloneModes);
                end.literal("to", [](const CommandNodeBuilder& to) {
                    to.argument("targetDimension", worldArgument(), [](const CommandNodeBuilder& target) {
                        target.argument("destination", blockPosArgument(), cloneModes);
                    });
                });
            });
        });
    };
    region(root);
    root.literal("from", [region](const CommandNodeBuilder& from) {
        from.argument("sourceDimension", worldArgument(), region);
    });
}

FillBiomeCommand::FillBiomeCommand() : Command("fillbiome", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] std::vector<Identifier> biomeIds() {
    std::vector<Identifier> named;
    for (const std::int32_t id : reg::biomes::registry().ids()) {
        if (const Identifier* name = reg::biomes::registry().nameOf(id); name != nullptr) {
            named.push_back(*name);
        }
    }
    return named;
}

[[nodiscard]] CommandSyntaxError notABiome(const Identifier& id) {
    return CommandSyntaxError{.component = Component::translatable(
                                  "argument.resource.not_found",
                                  {Component::text(id.toString()), Component::text("minecraft:worldgen/biome")})};
}

[[nodiscard]] Parsed<std::int32_t> fillBiome(const CommandContext& context, bool filtered) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    const std::array<std::int32_t, 3> from = resolveBlock(context, "from");
    const std::array<std::int32_t, 3> to = resolveBlock(context, "to");
    const auto* biomeId = context.get<Identifier>("biome");
    const std::optional<std::int32_t> biome =
        biomeId == nullptr ? std::nullopt : reg::biomes::registry().idOf(*biomeId);
    if (!biome.has_value()) {
        return std::unexpected{notABiome(biomeId == nullptr ? Identifier::vanilla("") : *biomeId)};
    }
    std::optional<std::int32_t> filterId;
    if (filtered) {
        const auto* filter = context.get<Identifier>("filter");
        filterId = filter == nullptr ? std::nullopt : reg::biomes::registry().idOf(*filter);
        if (!filterId.has_value()) {
            return std::unexpected{notABiome(filter == nullptr ? Identifier::vanilla("") : *filter)};
        }
    }

    const server::world::ChunkHeight bounds = config::dimensionFacts((*world)->declaration().dimension).chunkHeight;
    const std::int32_t minX = std::min(from[0], to[0]);
    const std::int32_t minY = std::min(from[1], to[1]);
    const std::int32_t minZ = std::min(from[2], to[2]);
    const std::int32_t maxX = std::max(from[0], to[0]);
    const std::int32_t maxY = std::max(from[1], to[1]);
    const std::int32_t maxZ = std::max(from[2], to[2]);
    const std::int32_t minQuartX = minX >> 2;
    const std::int32_t maxQuartX = maxX >> 2;
    const std::int32_t minQuartZ = minZ >> 2;
    const std::int32_t maxQuartZ = maxZ >> 2;
    const std::int32_t minQuartY = (std::max(minY, bounds.minY()) - bounds.minY()) >> 2;
    const std::int32_t maxQuartY = (std::min(maxY, bounds.maxY()) - bounds.minY()) >> 2;

    const std::int64_t volume = static_cast<std::int64_t>(maxQuartX - minQuartX + 1) * (maxQuartY - minQuartY + 1) *
                                (maxQuartZ - minQuartZ + 1);
    if (volume > kMaxQuarts) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.fillbiome.toobig",
                                                                    {Component::text(std::to_string(kMaxQuarts)),
                                                                     Component::text(std::to_string(volume))})}};
    }

    std::int32_t changed = 0;
    for (std::int32_t quartX = minQuartX; quartX <= maxQuartX; quartX++) {
        for (std::int32_t quartZ = minQuartZ; quartZ <= maxQuartZ; quartZ++) {
            server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(quartX >> 2, quartZ >> 2);
            if (chunk == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")}};
            }
            for (std::int32_t quartY = minQuartY; quartY <= maxQuartY; quartY++) {
                if (filterId.has_value() && chunk->biomeId(quartX & 3, quartY, quartZ & 3) != *filterId) {
                    continue;
                }
                chunk->setBiomeId(quartX & 3, quartY, quartZ & 3, *biome);
                changed++;
            }
        }
    }

    context.environment().syncBiomes(**world, minX, minZ, maxX, maxZ);
    // parity: the string takes the count and both corners, seven arguments in total.
    context.success(
        Component::translatable("commands.fillbiome.success.count",
                                {Component::text(std::to_string(changed)), Component::text(std::to_string(minX)),
                                 Component::text(std::to_string(minY)), Component::text(std::to_string(minZ)),
                                 Component::text(std::to_string(maxX)), Component::text(std::to_string(maxY)),
                                 Component::text(std::to_string(maxZ))}),
        true);
    return changed;
}

} // namespace

void FillBiomeCommand::build(const CommandNodeBuilder& root) const {
    root.argument("from", blockPosArgument(), [](const CommandNodeBuilder& from) {
        from.argument("to", blockPosArgument(), [](const CommandNodeBuilder& to) {
            to.argument("biome", resourceArgument("minecraft:worldgen/biome"), [](const CommandNodeBuilder& biome) {
                biome.suggestsFromServer(
                    [](const SuggestionContext& context) { return context.matchingIds(biomeIds()); });
                biome.executes([](const CommandContext& context) { return fillBiome(context, false); });
                biome.literal("replace", [](const CommandNodeBuilder& replace) {
                    replace.argument(
                        "filter", resourceOrTagArgument("minecraft:worldgen/biome"),
                        [](const CommandNodeBuilder& filter) {
                            filter.suggestsFromServer(
                                [](const SuggestionContext& context) { return context.matchingIds(biomeIds()); });
                            filter.executes([](const CommandContext& context) { return fillBiome(context, true); });
                        });
                });
            });
        });
    });
}

ForceLoadCommand::ForceLoadCommand() : Command("forceload", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] std::string chunkName(std::int32_t x, std::int32_t z) {
    return std::to_string(x) + ", " + std::to_string(z);
}

[[nodiscard]] std::expected<std::vector<std::pair<std::int32_t, std::int32_t>>, CommandSyntaxError>
chunkRange(const CommandContext& context, bool single) {
    const std::array<double, 3> origin = context.source().position();
    const auto* fromArgument = context.get<ColumnCoordinates>("from");
    if (fromArgument == nullptr) {
        return std::vector<std::pair<std::int32_t, std::int32_t>>{};
    }
    const std::array<std::int32_t, 2> from = fromArgument->resolve(origin);
    const auto* toArgument = single ? nullptr : context.get<ColumnCoordinates>("to");
    const std::array<std::int32_t, 2> to = toArgument == nullptr ? from : toArgument->resolve(origin);
    const std::int32_t minX = std::min(from[0], to[0]) >> 4;
    const std::int32_t maxX = std::max(from[0], to[0]) >> 4;
    const std::int32_t minZ = std::min(from[1], to[1]) >> 4;
    const std::int32_t maxZ = std::max(from[1], to[1]) >> 4;
    const std::int64_t count = static_cast<std::int64_t>(maxX - minX + 1) * (maxZ - minZ + 1);
    if (count > kMaxChunks) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.forceload.toobig",
                                                                    {Component::text(std::to_string(kMaxChunks)),
                                                                     Component::text(std::to_string(count))})}};
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> chunks;
    for (std::int32_t x = minX; x <= maxX; x++) {
        for (std::int32_t z = minZ; z <= maxZ; z++) {
            chunks.emplace_back(x, z);
        }
    }
    return chunks;
}

[[nodiscard]] Parsed<std::int32_t> forceLoad(const CommandContext& context, bool single, bool adding) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    const auto chunks = chunkRange(context, single);
    if (!chunks.has_value()) {
        return std::unexpected{chunks.error()};
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> moved;
    for (const auto& [x, z] : *chunks) {
        const std::int64_t key = world::ChunkGenerationQueue::pack(x, z);
        const bool changed =
            adding ? (*world)->forcedChunks().insert(key).second : (*world)->forcedChunks().erase(key) != 0;
        if (changed) {
            moved.emplace_back(x, z);
        }
    }
    const std::string verb = adding ? "added" : "removed";
    if (moved.empty()) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.forceload." + verb + ".failure")}};
    }
    if (adding) {
        // Generated now, not on next visit: the point of forcing a chunk is that it is there.
        for (const auto& [x, z] : moved) {
            (*world)->generationQueue().request(x, z, 0);
        }
    }
    context.success(
        moved.size() == 1
            ? Component::translatable("commands.forceload." + verb + ".single",
                                      {Component::text(chunkName(moved.front().first, moved.front().second)),
                                       Component::text((*world)->name())})
            : Component::translatable("commands.forceload." + verb + ".multiple",
                                      {Component::text(std::to_string(moved.size())), Component::text((*world)->name()),
                                       Component::text(chunkName(moved.front().first, moved.front().second)),
                                       Component::text(chunkName(moved.back().first, moved.back().second))}),
        true);
    return static_cast<std::int32_t>(moved.size());
}

} // namespace

void ForceLoadCommand::build(const CommandNodeBuilder& root) const {
    for (const bool adding : {true, false}) {
        root.literal(adding ? "add" : "remove", [adding](const CommandNodeBuilder& form) {
            if (!adding) {
                form.literal("all", [](const CommandNodeBuilder& all) {
                    all.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                        const Parsed<world::LoadedWorld*> world = context.world();
                        if (!world.has_value()) {
                            return std::unexpected{world.error()};
                        }
                        (*world)->forcedChunks().clear();
                        context.success(Component::translatable("commands.forceload.removed.all",
                                                                {Component::text((*world)->name())}),
                                        true);
                        return 0;
                    });
                });
            }
            form.argument("from", columnPosArgument(), [adding](const CommandNodeBuilder& from) {
                from.executes([adding](const CommandContext& context) { return forceLoad(context, true, adding); });
                from.argument("to", columnPosArgument(), [adding](const CommandNodeBuilder& to) {
                    to.executes([adding](const CommandContext& context) { return forceLoad(context, false, adding); });
                });
            });
        });
    }
    root.literal("query", [](const CommandNodeBuilder& query) {
        query.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            const std::set<std::int64_t>& forced = (*world)->forcedChunks();
            if (forced.empty()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.forceload.added.none",
                                                                            {Component::text((*world)->name())})}};
            }
            std::string joined;
            for (const std::int64_t key : forced) {
                const std::string one =
                    chunkName(world::ChunkGenerationQueue::unpackX(key), world::ChunkGenerationQueue::unpackZ(key));
                joined += joined.empty() ? one : ", " + one;
            }
            context.success(Component::translatable("commands.forceload.list.multiple",
                                                    {Component::text(std::to_string(forced.size())),
                                                     Component::text((*world)->name()), Component::text(joined)}));
            return static_cast<std::int32_t>(forced.size());
        });
        query.argument("pos", columnPosArgument(), [](const CommandNodeBuilder& pos) {
            pos.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<world::LoadedWorld*> world = context.world();
                if (!world.has_value()) {
                    return std::unexpected{world.error()};
                }
                const auto* coordinates = context.get<ColumnCoordinates>("pos");
                if (coordinates == nullptr) {
                    return 0;
                }
                const std::array<std::int32_t, 2> at = coordinates->resolve(context.source().position());
                const std::int32_t chunkX = at[0] >> 4;
                const std::int32_t chunkZ = at[1] >> 4;
                const bool forced =
                    (*world)->forcedChunks().contains(world::ChunkGenerationQueue::pack(chunkX, chunkZ));
                const Component where = Component::text(chunkName(chunkX, chunkZ));
                if (!forced) {
                    return std::unexpected{CommandSyntaxError{
                        .component = Component::translatable("commands.forceload.query.failure",
                                                             {where, Component::text((*world)->name())})}};
                }
                context.success(Component::translatable("commands.forceload.query.success",
                                                        {where, Component::text((*world)->name())}));
                return 1;
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
