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
