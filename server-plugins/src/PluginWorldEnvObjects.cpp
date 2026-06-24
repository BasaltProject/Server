#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

constexpr std::array<std::string_view, 6> kFaceNames{"down", "up", "north", "south", "west", "east"};

[[nodiscard]] std::int32_t faceAt(LuaArgs& args, std::int32_t index) {
    if (args.isNumber(index)) {
        return static_cast<std::int32_t>(args.toInteger(index));
    }
    const std::string name = args.checkString(index);
    for (std::size_t face = 0; face < kFaceNames.size(); ++face) {
        if (kFaceNames[face] == name) {
            return static_cast<std::int32_t>(face);
        }
    }
    args.raise("'" + name + "' is not a face: down, up, north, south, west or east");
}

[[nodiscard]] std::optional<double> numberField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<double> value;
    if (lua.isNumber(-1)) {
        value = lua.toNumber(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::optional<std::string> textField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> enchantmentsAt(LuaArgs& lua, std::int32_t index) {
    std::vector<std::pair<std::string, std::int32_t>> out;
    if (index > lua.count() || !lua.isTable(index)) {
        return out;
    }
    lua.pushNil();
    while (lua.nextEntry(index)) {
        if (lua.isString(-2) && lua.isNumber(-1)) {
            out.emplace_back(lua.toText(-2), static_cast<std::int32_t>(lua.toInteger(-1)));
        }
        lua.pop(1);
    }
    return out;
}

void pushDrops(LuaArgs& args, const std::vector<PluginDrop>& drops) {
    args.pushTable();
    std::int64_t position = 1;
    for (const PluginDrop& drop : drops) {
        args.pushTable();
        args.pushString(drop.item);
        args.setField("item");
        args.pushInteger(drop.count);
        args.setField("count");
        if (!drop.components.empty()) {
            args.pushString(drop.components);
            args.setField("components");
        }
        args.setIndex(position++);
    }
}

constexpr std::array<std::string_view, 8> kCategories{
    "monster",        "creature",      "ambient", "axolotls", "underground_water_creature",
    "water_creature", "water_ambient", "misc"};

void pushBox(LuaArgs& args, const std::array<std::int32_t, 6>& box) {
    args.pushTable();
    for (std::size_t at = 0; at < box.size(); ++at) {
        args.pushInteger(box[at]);
        args.setIndex(static_cast<std::int64_t>(at) + 1);
    }
}

void pushStrings(LuaArgs& args, const std::vector<std::string>& values) {
    args.pushTable();
    std::int64_t position = 1;
    for (const std::string& value : values) {
        args.pushString(value);
        args.setIndex(position++);
    }
}

void pushBorder(LuaArgs& args, const PluginBorder& border) {
    args.pushTable();
    args.pushNumber(border.centerX);
    args.setField("center_x");
    args.pushNumber(border.centerZ);
    args.setField("center_z");
    args.pushNumber(border.size);
    args.setField("size");
    args.pushNumber(border.targetSize);
    args.setField("target_size");
    args.pushInteger(border.lerpTicks);
    args.setField("lerp_ticks");
    args.pushNumber(border.damagePerBlock);
    args.setField("damage_amount");
    args.pushNumber(border.damageBuffer);
    args.setField("damage_buffer");
    args.pushInteger(border.warningBlocks);
    args.setField("warning_distance");
    args.pushInteger(border.warningSeconds);
    args.setField("warning_time");
}
} // namespace

void addEnvWorldMethods(PluginObject& object, const std::shared_ptr<PluginWorldRef>& world) {
    object
        .reads("player_count",
               [world] {
                   PluginWorldEnv* env = world->env();
                   return PluginValue{static_cast<std::int64_t>(env == nullptr ? 0 : env->playerCount())};
               })
        .reads("block_entity_count",
               [world] {
                   PluginWorldEnv* env = world->env();
                   return PluginValue{static_cast<std::int64_t>(env == nullptr ? 0 : env->blockEntityCount())};
               })
        .pushes("border",
                [world](LuaArgs& args) {
                    PluginWorldEnv* env = world->env();
                    pushBorder(args, env == nullptr ? PluginBorder{} : env->border());
                })
        .reads("view_distance",
               [world] {
                   PluginWorldEnv* env = world->env();
                   return PluginValue{static_cast<std::int64_t>(env == nullptr ? 10 : env->viewDistance())};
               })
        .writes("view_distance",
                [world](LuaArgs& args, std::int32_t index) {
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->setViewDistance(static_cast<std::int32_t>(args.checkInteger(index)));
                    }
                })
        .reads("sea_level",
               [world] {
                   PluginWorldEnv* env = world->env();
                   return PluginValue{static_cast<std::int64_t>(env == nullptr ? 63 : env->seaLevel())};
               })
        .pushes("forced_chunks",
                [world](LuaArgs& args) {
                    PluginWorldEnv* env = world->env();
                    const std::vector<std::int32_t> flat =
                        env == nullptr ? std::vector<std::int32_t>{} : env->forcedChunks();
                    args.pushTable();
                    for (std::size_t at = 0; at + 1 < flat.size(); at += 2) {
                        args.pushTable();
                        args.pushInteger(flat[at]);
                        args.setField("x");
                        args.pushInteger(flat[at + 1]);
                        args.setField("z");
                        args.setIndex(static_cast<std::int64_t>(at / 2) + 1);
                    }
                })
        .method("force_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->setChunkForced(static_cast<std::int32_t>(args.checkInteger(base)),
                                            static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                            args.booleanOr(base + 2, true));
                    }
                    return 0;
                })
        .pushes("weather_times",
                [world](LuaArgs& args) {
                    PluginWorldEnv* env = world->env();
                    const std::array<std::int32_t, 3> times =
                        env == nullptr ? std::array<std::int32_t, 3>{} : env->weatherTimes();
                    args.pushTable();
                    args.pushInteger(times[0]);
                    args.setField("rain");
                    args.pushInteger(times[1]);
                    args.setField("thunder");
                    args.pushInteger(times[2]);
                    args.setField("clear");
                })
        .method("set_weather_times",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    if (env == nullptr || !args.isTable(base)) {
                        args.raise("set_weather_times takes a table: rain, thunder, clear");
                    }
                    std::array<std::int32_t, 3> times = env->weatherTimes();
                    times[0] = static_cast<std::int32_t>(numberField(args, base, "rain").value_or(times[0]));
                    times[1] = static_cast<std::int32_t>(numberField(args, base, "thunder").value_or(times[1]));
                    times[2] = static_cast<std::int32_t>(numberField(args, base, "clear").value_or(times[2]));
                    env->setWeatherTimes(times[0], times[1], times[2]);
                    return 0;
                })
        .method("inhabited_time",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const auto chunkX = static_cast<std::int32_t>(args.checkInteger(base));
                    const auto chunkZ = static_cast<std::int32_t>(args.checkInteger(base + 1));
                    if (env != nullptr && args.isNumber(base + 2)) {
                        env->setInhabitedTime(chunkX, chunkZ, args.toInteger(base + 2));
                    }
                    args.pushInteger(env == nullptr ? 0 : env->inhabitedTime(chunkX, chunkZ));
                    return 1;
                })
        .method("structures",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const std::vector<PluginStructureStart> starts =
                        env == nullptr ? std::vector<PluginStructureStart>{}
                                       : env->structureStarts(static_cast<std::int32_t>(args.checkInteger(base)),
                                                              static_cast<std::int32_t>(args.checkInteger(base + 1)));
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const PluginStructureStart& start : starts) {
                        args.pushTable();
                        args.pushString(start.structure);
                        args.setField("structure");
                        args.pushInteger(start.chunkX);
                        args.setField("chunk_x");
                        args.pushInteger(start.chunkZ);
                        args.setField("chunk_z");
                        pushBox(args, start.bounds);
                        args.setField("bounds");
                        args.pushTable();
                        std::int64_t piece = 1;
                        for (const std::array<std::int32_t, 6>& box : start.pieces) {
                            pushBox(args, box);
                            args.setIndex(piece++);
                        }
                        args.setField("pieces");
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("raids",
                [world](LuaArgs& args) {
                    PluginWorldEnv* env = world->env();
                    const std::vector<PluginRaid> raids = env == nullptr ? std::vector<PluginRaid>{} : env->raids();
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const PluginRaid& raid : raids) {
                        args.pushTable();
                        args.pushInteger(raid.id);
                        args.setField("id");
                        args.pushBoolean(raid.started);
                        args.setField("started");
                        args.pushInteger(raid.ticksActive);
                        args.setField("ticks_active");
                        args.pushInteger(raid.omenLevel);
                        args.setField("omen_level");
                        args.pushString(raid.status);
                        args.setField("status");
                        args.pushInteger(raid.groupsSpawned);
                        args.setField("groups_spawned");
                        args.pushInteger(raid.totalGroups);
                        args.setField("total_groups");
                        args.pushNumber(raid.totalHealth);
                        args.setField("total_health");
                        args.pushTable();
                        for (std::size_t at = 0; at < raid.center.size(); ++at) {
                            args.pushInteger(raid.center[at]);
                            args.setIndex(static_cast<std::int64_t>(at) + 1);
                        }
                        args.setField("center");
                        pushStrings(args, raid.heroes);
                        args.setField("heroes");
                        args.pushTable();
                        std::int64_t raider = 1;
                        for (const std::int32_t id : raid.raiders) {
                            args.pushInteger(id);
                            args.setIndex(raider++);
                        }
                        args.setField("raiders");
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("set_raid",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    bool changed = false;
                    if (env != nullptr && args.isTable(base + 1)) {
                        const auto id = static_cast<std::int32_t>(args.checkInteger(base));
                        for (const PluginRaid& raid : env->raids()) {
                            if (raid.id == id) {
                                changed = env->setRaid(
                                    id,
                                    static_cast<std::int32_t>(
                                        numberField(args, base + 1, "omen_level").value_or(raid.omenLevel)),
                                    static_cast<std::int32_t>(
                                        numberField(args, base + 1, "total_groups").value_or(raid.totalGroups)));
                            }
                        }
                    }
                    args.pushBoolean(changed);
                    return 1;
                })
        .method("fluid",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const PluginFluid fluid =
                        env == nullptr ? PluginFluid{}
                                       : env->fluidAt(static_cast<std::int32_t>(args.checkInteger(base)),
                                                      static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                      static_cast<std::int32_t>(args.checkInteger(base + 2)));
                    args.pushTable();
                    args.pushString(fluid.fluid);
                    args.setField("fluid");
                    args.pushBoolean(fluid.source);
                    args.setField("source");
                    args.pushInteger(fluid.level);
                    args.setField("level");
                    args.pushBoolean(fluid.falling);
                    args.setField("falling");
                    return 1;
                })
        .method("lightning_target",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const std::optional<std::array<std::int32_t, 3>> at =
                        env == nullptr ? std::nullopt
                                       : env->lightningTarget(static_cast<std::int32_t>(args.checkInteger(base)),
                                                              static_cast<std::int32_t>(args.checkInteger(base + 1)));
                    if (!at.has_value()) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushTable();
                    for (std::size_t index = 0; index < at->size(); ++index) {
                        args.pushInteger((*at)[index]);
                        args.setIndex(static_cast<std::int64_t>(index) + 1);
                    }
                    return 1;
                })
        .method("game_event",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->gameEvent(args.checkString(base), args.checkNumber(base + 1), args.checkNumber(base + 2),
                                       args.checkNumber(base + 3),
                                       args.isString(base + 4) ? args.checkString(base + 4) : std::string{});
                    }
                    return 0;
                })
        .method("grow_tree",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushBoolean(env != nullptr &&
                                     env->growTree(static_cast<std::int32_t>(args.checkInteger(base)),
                                                   static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                   static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                   args.checkString(base + 3)));
                    return 1;
                })
        .method("pois",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    std::vector<std::string> types;
                    if (args.isTable(base + 4)) {
                        const std::int64_t length = args.length(base + 4);
                        for (std::int64_t position = 1; position <= length; ++position) {
                            args.pushIndex(base + 4, position);
                            types.push_back(args.toText(-1));
                            args.pop(1);
                        }
                    }
                    const std::string kind = args.isString(base + 5) ? args.checkString(base + 5) : std::string{"any"};
                    const std::vector<PluginPoi> found =
                        env == nullptr
                            ? std::vector<PluginPoi>{}
                            : env->pointsOfInterest(types, static_cast<std::int32_t>(args.checkInteger(base)),
                                                    static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                    static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                    static_cast<std::int32_t>(args.checkInteger(base + 3)),
                                                    kind == "free"    ? 0
                                                    : kind == "taken" ? 1
                                                                      : 2);
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const PluginPoi& poi : found) {
                        args.pushTable();
                        args.pushString(poi.type);
                        args.setField("type");
                        args.pushInteger(poi.x);
                        args.setField("x");
                        args.pushInteger(poi.y);
                        args.setField("y");
                        args.pushInteger(poi.z);
                        args.setField("z");
                        args.pushInteger(poi.freeTickets);
                        args.setField("free_tickets");
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .pushes("spawn_rules",
                [world](LuaArgs& args) {
                    PluginWorldEnv* env = world->env();
                    const PluginSpawnTunables rules = env == nullptr ? PluginSpawnTunables{} : env->spawnTunables();
                    args.pushTable();
                    args.pushTable();
                    for (std::size_t at = 0; at < kCategories.size(); ++at) {
                        if (rules.limits[at] >= 0) {
                            args.pushInteger(rules.limits[at]);
                            args.setField(kCategories[at]);
                        }
                    }
                    args.setField("limits");
                    args.pushTable();
                    for (std::size_t at = 0; at < kCategories.size(); ++at) {
                        if (rules.ticksPer[at] >= 0) {
                            args.pushInteger(rules.ticksPer[at]);
                            args.setField(kCategories[at]);
                        }
                    }
                    args.setField("ticks_per");
                    args.pushBoolean(rules.allowMonsters);
                    args.setField("allow_monsters");
                    args.pushBoolean(rules.allowAnimals);
                    args.setField("allow_animals");
                })
        .reads("tickable_block_entity_count",
               [world] {
                   PluginWorldEnv* env = world->env();
                   return PluginValue{static_cast<std::int64_t>(env == nullptr ? 0 : env->tickableBlockEntityCount())};
               })
        .method("random_tick",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushBoolean(env != nullptr &&
                                     env->randomTick(static_cast<std::int32_t>(args.checkInteger(base)),
                                                     static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                     static_cast<std::int32_t>(args.checkInteger(base + 2))));
                    return 1;
                })
        .method("place_tree",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const std::string tree = args.checkString(base);
                    const bool apply = args.isNil(base + 4) || args.toBoolean(base + 4);
                    const auto placed =
                        env == nullptr ? std::nullopt
                                       : env->placeTree(tree, static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                        static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                        static_cast<std::int32_t>(args.checkInteger(base + 3)), apply);
                    if (!placed.has_value()) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const PluginPlacedBlock& block : *placed) {
                        args.pushTable();
                        args.pushInteger(block.x);
                        args.setField("x");
                        args.pushInteger(block.y);
                        args.setField("y");
                        args.pushInteger(block.z);
                        args.setField("z");
                        args.pushInteger(block.before);
                        args.setField("before");
                        args.pushInteger(block.after);
                        args.setField("after");
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("tunable",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const std::optional<double> value =
                        env == nullptr ? std::nullopt : env->tunable(args.checkString(base));
                    if (!value.has_value()) {
                        args.pushNil();
                    } else {
                        args.pushNumber(*value);
                    }
                    return 1;
                })
        .method("set_tunable",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushBoolean(env != nullptr &&
                                     env->setTunable(args.checkString(base), args.checkNumber(base + 1)));
                    return 1;
                })
        .method("set_spawn_rules",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    if (env == nullptr || !args.isTable(base)) {
                        args.raise("set_spawn_rules takes a table: limits, ticks_per, allow_monsters, allow_animals");
                    }
                    PluginSpawnTunables rules = env->spawnTunables();
                    for (const char* group : {"limits", "ticks_per"}) {
                        args.pushField(base, group);
                        if (args.isTable(-1)) {
                            for (std::size_t at = 0; at < kCategories.size(); ++at) {
                                args.pushField(-1, kCategories[at]);
                                if (args.isNumber(-1)) {
                                    (std::string_view{group} == "limits" ? rules.limits : rules.ticksPer)[at] =
                                        static_cast<std::int32_t>(args.toInteger(-1));
                                }
                                args.pop(1);
                            }
                        }
                        args.pop(1);
                    }
                    for (const char* flag : {"allow_monsters", "allow_animals"}) {
                        args.pushField(base, flag);
                        if (args.isBoolean(-1)) {
                            (std::string_view{flag} == "allow_monsters" ? rules.allowMonsters : rules.allowAnimals) =
                                args.toBoolean(-1);
                        }
                        args.pop(1);
                    }
                    env->setSpawnTunables(rules);
                    return 0;
                })
        .method("set_border",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    if (env == nullptr || !args.isTable(base)) {
                        args.raise("set_border takes a table of border settings");
                    }
                    PluginBorder border = env->border();
                    border.centerX = numberField(args, base, "center_x").value_or(border.centerX);
                    border.centerZ = numberField(args, base, "center_z").value_or(border.centerZ);
                    border.size = numberField(args, base, "size").value_or(border.size);
                    border.damagePerBlock = numberField(args, base, "damage_amount").value_or(border.damagePerBlock);
                    border.damageBuffer = numberField(args, base, "damage_buffer").value_or(border.damageBuffer);
                    border.warningBlocks = static_cast<std::int32_t>(
                        numberField(args, base, "warning_distance").value_or(border.warningBlocks));
                    border.warningSeconds = static_cast<std::int32_t>(
                        numberField(args, base, "warning_time").value_or(border.warningSeconds));
                    env->setBorder(border, numberField(args, base, "seconds").value_or(0.0));
                    return 0;
                })
        .method("sound",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->sound(args.checkString(base),
                                   args.isString(base + 6) ? args.checkString(base + 6) : "master",
                                   args.checkNumber(base + 1), args.checkNumber(base + 2), args.checkNumber(base + 3),
                                   static_cast<float>(args.numberOr(base + 4, 1.0)),
                                   static_cast<float>(args.numberOr(base + 5, 1.0)), args.integerOr(base + 7, 0));
                    }
                    return 0;
                })
        .method("particle",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    if (env == nullptr || !args.isTable(base)) {
                        args.raise("particle takes a table: type, x, y, z, count, offset, speed, data");
                    }
                    PluginParticle particle;
                    particle.type = textField(args, base, "type").value_or("minecraft:flame");
                    particle.x = numberField(args, base, "x").value_or(0.0);
                    particle.y = numberField(args, base, "y").value_or(0.0);
                    particle.z = numberField(args, base, "z").value_or(0.0);
                    particle.count = static_cast<std::int32_t>(numberField(args, base, "count").value_or(1.0));
                    particle.offsetX = static_cast<float>(numberField(args, base, "offset_x").value_or(0.0));
                    particle.offsetY = static_cast<float>(numberField(args, base, "offset_y").value_or(0.0));
                    particle.offsetZ = static_cast<float>(numberField(args, base, "offset_z").value_or(0.0));
                    const auto speed = static_cast<float>(numberField(args, base, "speed").value_or(0.0));
                    particle.speed = {static_cast<float>(numberField(args, base, "speed_x").value_or(speed)),
                                      static_cast<float>(numberField(args, base, "speed_y").value_or(speed)),
                                      static_cast<float>(numberField(args, base, "speed_z").value_or(speed))};
                    particle.randomization =
                        static_cast<std::int32_t>(numberField(args, base, "randomization").value_or(0.0));
                    args.pushField(base, "force");
                    particle.force = args.isBoolean(-1) && args.toBoolean(-1);
                    args.pop(1);
                    particle.item = textField(args, base, "item").value_or("");
                    particle.receiver = textField(args, base, "player").value_or("");
                    args.pushField(base, "data");
                    if (args.isTable(-1)) {
                        const std::int64_t length = args.length(-1);
                        for (std::int64_t position = 1; position <= length; ++position) {
                            args.pushIndex(-1, position);
                            particle.data.push_back(args.isNumber(-1) ? args.toNumber(-1) : 0.0);
                            args.pop(1);
                        }
                    }
                    args.pop(1);
                    env->particle(particle);
                    return 0;
                })
        .method("level_event",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->levelEvent(static_cast<std::int32_t>(args.checkInteger(base)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 3)),
                                        static_cast<std::int32_t>(args.integerOr(base + 4, 0)),
                                        args.booleanOr(base + 5, false));
                    }
                    return 0;
                })
        .method("block_event",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (PluginWorldEnv* env = world->env(); env != nullptr) {
                        env->blockEvent(static_cast<std::int32_t>(args.checkInteger(base)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 3)),
                                        static_cast<std::int32_t>(args.integerOr(base + 4, 0)));
                    }
                    return 0;
                })
        .method("drops",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    const std::optional<std::vector<PluginDrop>> drops =
                        env == nullptr
                            ? std::nullopt
                            : env->blockDrops(static_cast<std::int32_t>(args.checkInteger(base)),
                                              static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                              static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                              args.isString(base + 3) ? args.checkString(base + 3) : std::string{},
                                              enchantmentsAt(args, base + 4), true);
                    if (!drops.has_value()) {
                        args.pushNil();
                    } else {
                        pushDrops(args, *drops);
                    }
                    return 1;
                })
        .method("break_block",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushBoolean(
                        env != nullptr &&
                        env->breakBlock(static_cast<std::int32_t>(args.checkInteger(base)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                        static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                        args.isString(base + 3) ? args.checkString(base + 3) : std::string{},
                                        enchantmentsAt(args, base + 4), args.booleanOr(base + 5, true)));
                    return 1;
                })
        .method("bone_meal",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushBoolean(env != nullptr &&
                                     env->boneMeal(static_cast<std::int32_t>(args.checkInteger(base)),
                                                   static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                   static_cast<std::int32_t>(args.checkInteger(base + 2))));
                    return 1;
                })
        .method("signal",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    PluginWorldEnv* env = world->env();
                    args.pushInteger(env == nullptr
                                         ? 0
                                         : env->signalFrom(static_cast<std::int32_t>(args.checkInteger(base)),
                                                           static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                           static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                           faceAt(args, base + 3), args.booleanOr(base + 4, false)));
                    return 1;
                })
        .method("survives", [world](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            PluginWorldEnv* env = world->env();
            args.pushBoolean(env == nullptr || env->survives(static_cast<std::int32_t>(args.checkInteger(base)),
                                                             static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                             static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                             static_cast<std::int32_t>(args.checkInteger(base + 3))));
            return 1;
        });
}
} // namespace basalt::server::plugins
