#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <map>
#include <unordered_map>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

constexpr std::size_t kSectionBlocks = 4096;

[[nodiscard]] std::int32_t intAt(LuaArgs& lua, std::int32_t index) {
    return static_cast<std::int32_t>(lua.checkInteger(index));
}

[[nodiscard]] std::vector<std::int32_t> statesFrom(LuaArgs& lua, std::int32_t index, std::size_t expected) {
    if (!lua.isTable(index) || static_cast<std::size_t>(lua.length(index)) != expected) {
        lua.raise("expected a list of " + std::to_string(expected) + " numbers");
    }
    std::vector<std::int32_t> out(expected);
    for (std::size_t position = 0; position < expected; position++) {
        lua.pushIndex(index, static_cast<std::int64_t>(position) + 1);
        out[position] = lua.isNumber(-1) ? static_cast<std::int32_t>(lua.toInteger(-1)) : -1;
        lua.pop(1);
    }
    return out;
}

void pushInts(LuaArgs& lua, std::span<const std::int32_t> values) {
    lua.pushTable();
    for (std::size_t position = 0; position < values.size(); position++) {
        lua.pushInteger(values[position]);
        lua.setIndex(static_cast<std::int64_t>(position) + 1);
    }
}

/// A list of `{ chunk_x, chunk_z }` pairs, flattened to x, z, x, z...
[[nodiscard]] std::vector<std::int32_t> chunkPairs(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("expected a list of { chunk_x, chunk_z } pairs");
    }
    std::vector<std::int32_t> out;
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        if (!lua.isTable(-1)) {
            lua.raise("expected a list of { chunk_x, chunk_z } pairs");
        }
        for (const std::int64_t part : {1, 2}) {
            lua.pushIndex(lua.top(), part);
            out.push_back(static_cast<std::int32_t>(lua.toInteger(-1)));
            lua.pop(1);
        }
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] bool flag(LuaArgs& lua, std::int32_t options, std::string_view key, bool fallback) {
    if (!lua.isTable(options)) {
        return fallback;
    }
    lua.pushField(options, key);
    const bool value = lua.isNil(-1) ? fallback : lua.toBoolean(-1);
    lua.pop(1);
    return value;
}

[[nodiscard]] std::size_t biomeCells(const PluginWorldRef& world) {
    return static_cast<std::size_t>(std::max(world.height(), 0) / 4) * 16;
}

struct Sections {
    std::vector<std::int32_t> ys;
    std::vector<std::int32_t> states;
};

/// A table of section_y to a list of 4096 state ids, taken in ascending section order.
[[nodiscard]] Sections sectionsFrom(LuaArgs& lua, std::int32_t index) {
    if (!lua.isTable(index)) {
        lua.raise("sections is a table of section_y to a list of 4096 state ids");
    }
    std::map<std::int32_t, std::vector<std::int32_t>> ordered;
    lua.pushNil();
    while (lua.nextEntry(index)) {
        ordered[static_cast<std::int32_t>(lua.toInteger(-2))] = statesFrom(lua, lua.top(), kSectionBlocks);
        lua.pop(1);
    }
    Sections out;
    for (auto& [y, states] : ordered) {
        out.ys.push_back(y);
        out.states.insert(out.states.end(), states.begin(), states.end());
    }
    return out;
}

void pushPrevious(LuaArgs& lua, std::span<const std::int32_t> ys, std::span<const std::int32_t> previous) {
    lua.pushTable();
    for (std::size_t section = 0; section < ys.size(); section++) {
        pushInts(lua, previous.subspan(section * kSectionBlocks, kSectionBlocks));
        lua.setIndex(ys[section]);
    }
}

[[nodiscard]] std::optional<std::string> snbtOf(const std::optional<std::vector<std::uint8_t>>& bytes) {
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    if (!read.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::snbt::stringify(read->tag);
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(LuaArgs& lua, std::int32_t index) {
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(lua.checkString(index));
    if (!parsed.has_value()) {
        lua.raise("that is not valid NBT text");
    }
    return wire::nbt::nbtIO::write(*parsed);
}

void pushText(LuaArgs& lua, const std::optional<std::string>& text) {
    if (text.has_value()) {
        lua.pushString(*text);
    } else {
        lua.pushNil();
    }
}

struct Spot {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t next = 0;
};

[[nodiscard]] Spot spotAt(LuaArgs& lua, std::int32_t base) {
    const std::optional<PluginPoint> point = pointFrom(lua, base);
    if (point.has_value()) {
        return {static_cast<std::int32_t>(std::floor(point->x)), static_cast<std::int32_t>(std::floor(point->y)),
                static_cast<std::int32_t>(std::floor(point->z)), base + 1};
    }
    return {intAt(lua, base), intAt(lua, base + 1), intAt(lua, base + 2), base + 3};
}
} // namespace

void addBulkWorldMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world,
                         const std::shared_ptr<WorldHooks>& hooks) {
    object.reads("block_version", [world] { return PluginValue{static_cast<std::int64_t>(world->blockVersion())}; })
        .reads("folder", [world] { return PluginValue{world->folder()}; })
        .method("section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> out(kSectionBlocks);
                    if (!world->readSection(intAt(args, base), intAt(args, base + 1), intAt(args, base + 2),
                                            args.isNil(base + 3) || args.booleanOr(base + 3, true), out)) {
                        args.pushNil();
                        return 1;
                    }
                    pushInts(args, out);
                    return 1;
                })
        .method("set_section",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> states = statesFrom(args, base + 3, kSectionBlocks);
                    args.pushBoolean(world->writeSection(intAt(args, base), intAt(args, base + 1),
                                                         intAt(args, base + 2), states,
                                                         args.isNil(base + 4) || args.booleanOr(base + 4, true)));
                    return 1;
                })
        .method("apply_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const Sections sections = sectionsFrom(args, base + 2);
                    const bool resend = flag(args, base + 3, "resend", true);
                    const bool wantPrevious = flag(args, base + 3, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? sections.states.size() : 0);
                    const std::int32_t changed = world->applySections(intAt(args, base), intAt(args, base + 1),
                                                                      sections.ys, sections.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    pushPrevious(args, sections.ys, previous);
                    return 2;
                })
        .method("apply_chunks",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (!args.isTable(base)) {
                        args.raise("apply_chunks takes a list of { x, z, sections }");
                    }
                    std::vector<std::int32_t> chunks;
                    Sections all;
                    const std::int64_t length = args.length(base);
                    for (std::int64_t position = 1; position <= length; position++) {
                        args.pushIndex(base, position);
                        const std::int32_t entry = args.top();
                        args.pushField(entry, "x");
                        const std::int32_t chunkX = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "z");
                        const std::int32_t chunkZ = intAt(args, -1);
                        args.pop(1);
                        args.pushField(entry, "sections");
                        const Sections sections = sectionsFrom(args, args.top());
                        args.pop(2);
                        chunks.insert(chunks.end(), {chunkX, chunkZ, static_cast<std::int32_t>(sections.ys.size())});
                        all.ys.insert(all.ys.end(), sections.ys.begin(), sections.ys.end());
                        all.states.insert(all.states.end(), sections.states.begin(), sections.states.end());
                    }
                    const bool resend = flag(args, base + 1, "resend", true);
                    const bool wantPrevious = flag(args, base + 1, "previous", false);
                    std::vector<std::int32_t> previous(wantPrevious ? all.states.size() : 0);
                    const std::int32_t changed = world->applyChunks(chunks, all.ys, all.states, resend, previous);
                    if (changed < 0) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushInteger(changed);
                    if (!wantPrevious) {
                        return 1;
                    }
                    args.pushTable();
                    std::size_t section = 0;
                    for (std::size_t chunk = 0; chunk < chunks.size() / 3; chunk++) {
                        const auto count = static_cast<std::size_t>(chunks[(chunk * 3) + 2]);
                        pushPrevious(args, std::span<const std::int32_t>{all.ys}.subspan(section, count),
                                     std::span<const std::int32_t>{previous}.subspan(section * kSectionBlocks,
                                                                                     count * kSectionBlocks));
                        args.setIndex(static_cast<std::int64_t>(chunk) + 1);
                        section += count;
                    }
                    return 2;
                })
        .method("resend_chunks",
                [world](LuaArgs& args) {
                    world->resendChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("relight_chunks",
                [world](LuaArgs& args) {
                    world->relightChunks(chunkPairs(args, firstArgument(args)));
                    return 0;
                })
        .method("loaded_chunks",
                [world](LuaArgs& args) {
                    const std::vector<std::int32_t> flat = world->loadedChunks();
                    args.pushTable();
                    for (std::size_t pair = 0; pair + 1 < flat.size(); pair += 2) {
                        args.pushTable();
                        args.pushInteger(flat[pair]);
                        args.setIndex(1);
                        args.pushInteger(flat[pair + 1]);
                        args.setIndex(2);
                        args.setIndex(static_cast<std::int64_t>(pair / 2) + 1);
                    }
                    return 1;
                })
        .method("block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    pushText(args, snbtOf(world->blockEntityNbt(spot.x, spot.y, spot.z)));
                    return 1;
                })
        .method("set_block_entity",
                [world](LuaArgs& args) {
                    const Spot spot = spotAt(args, firstArgument(args));
                    args.pushBoolean(world->setBlockEntityNbt(spot.x, spot.y, spot.z, bytesOf(args, spot.next)));
                    return 1;
                })
        .method("block_entities",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::vector<std::int32_t> flat =
                        world->blockEntityPositions(intAt(args, base), intAt(args, base + 1));
                    args.pushTable();
                    for (std::size_t triple = 0; triple + 2 < flat.size(); triple += 3) {
                        args.pushTable();
                        for (std::size_t part = 0; part < 3; part++) {
                            args.pushInteger(flat[triple + part]);
                            args.setIndex(static_cast<std::int64_t>(part) + 1);
                        }
                        args.setIndex(static_cast<std::int64_t>(triple / 3) + 1);
                    }
                    return 1;
                })
        .method("entity_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::int32_t id = 0;
                    if (args.isTable(base)) {
                        args.pushField(base, "id");
                        id = static_cast<std::int32_t>(args.toInteger(-1));
                        args.pop(1);
                    } else {
                        id = intAt(args, base);
                    }
                    pushText(args, snbtOf(world->entityNbt(id)));
                    return 1;
                })
        .method("spawn_nbt",
                [world](LuaArgs& args) {
                    const std::int32_t id = world->spawnEntityNbt(bytesOf(args, firstArgument(args)));
                    const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
                    if (made == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, made);
                    }
                    return 1;
                })
        .method("entity",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::shared_ptr<PluginEntityRef> found = args.isString(base) && !args.isNumber(base)
                                                                       ? world->entityByUuid(args.toText(base))
                                                                       : world->entityById(intAt(args, base));
                    if (found == nullptr) {
                        args.pushNil();
                    } else {
                        pushEntity(args, found);
                    }
                    return 1;
                })
        .method("entities_in",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const std::shared_ptr<PluginEntityRef>& entity :
                         world->entitiesIn(intAt(args, base), intAt(args, base + 1))) {
                        pushEntity(args, entity);
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("biomes",
                [world, hooks](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    std::vector<std::int32_t> ids(biomeCells(*world));
                    if (!world->readBiomes(intAt(args, base), intAt(args, base + 1), ids)) {
                        args.pushNil();
                        return 1;
                    }
                    const std::vector<std::string> names =
                        hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
                    args.pushTable();
                    for (std::size_t cell = 0; cell < ids.size(); cell++) {
                        const auto id = static_cast<std::size_t>(ids[cell]);
                        args.pushString(id < names.size() ? names[id] : "");
                        args.setIndex(static_cast<std::int64_t>(cell) + 1);
                    }
                    return 1;
                })
        .method("set_biomes", [world, hooks](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            if (!args.isTable(base + 2)) {
                args.raise("set_biomes(chunk_x, chunk_z, biomes) takes one biome name per cell");
            }
            std::unordered_map<std::string, std::int32_t> byName;
            const std::vector<std::string> names = hooks->biomeIds ? hooks->biomeIds() : std::vector<std::string>{};
            for (std::size_t id = 0; id < names.size(); id++) {
                byName.emplace(names[id], static_cast<std::int32_t>(id));
            }
            std::vector<std::int32_t> ids(biomeCells(*world), -1);
            for (std::size_t cell = 0; cell < ids.size(); cell++) {
                args.pushIndex(base + 2, static_cast<std::int64_t>(cell) + 1);
                if (args.isString(-1)) {
                    const auto found = byName.find(args.toText(-1));
                    if (found == byName.end()) {
                        args.raise("'" + args.toText(-1) + "' is not a biome");
                    }
                    ids[cell] = found->second;
                }
                args.pop(1);
            }
            args.pushBoolean(world->writeBiomes(intAt(args, base), intAt(args, base + 1), ids));
            return 1;
        });
}
} // namespace basalt::server::plugins
