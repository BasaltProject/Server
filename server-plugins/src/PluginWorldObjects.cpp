#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

[[nodiscard]] std::string nameLines(LuaArgs& lua, std::int32_t index) {
    if (lua.isString(index)) {
        return lua.toText(index);
    }
    if (!lua.isTable(index)) {
        return {};
    }
    std::string out;
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        out += out.empty() ? lua.toText(-1) : "\n" + lua.toText(-1);
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] std::vector<std::string> uuidsFrom(LuaArgs& lua, std::int32_t index) {
    std::vector<std::string> out;
    if (index > lua.count() || !lua.isTable(index)) {
        return out;
    }
    const std::int64_t length = lua.length(index);
    for (std::int64_t position = 1; position <= length; position++) {
        lua.pushIndex(index, position);
        if (lua.isTable(-1)) {
            lua.pushField(-1, "uuid");
            if (lua.isString(-1)) {
                out.push_back(lua.toText(-1));
            }
            lua.pop(1);
        } else if (lua.isString(-1)) {
            out.push_back(lua.toText(-1));
        }
        lua.pop(1);
    }
    return out;
}

[[nodiscard]] std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] PluginValue audienceOf(const std::optional<std::vector<std::string>>& audience) {
    return audience.has_value() ? PluginValue{*audience} : PluginValue{};
}
} // namespace

void pushNpc(LuaArgs& lua, std::shared_ptr<PluginNpcRef> npc) {
    PluginObject object{"npc"};
    object.reads("id", [npc] { return PluginValue{static_cast<std::int64_t>(npc->id())}; })
        .reads("uuid", [npc] { return PluginValue{npc->uuid()}; })
        .reads("name", [npc] { return PluginValue{npc->name()}; })
        .reads("world", [npc] { return PluginValue{npc->world()}; })
        .reads("x", [npc] { return PluginValue{npc->x()}; })
        .reads("y", [npc] { return PluginValue{npc->y()}; })
        .reads("z", [npc] { return PluginValue{npc->z()}; })
        .reads("alive", [npc] { return PluginValue{npc->alive()}; })
        .reads("audience", [npc] { return audienceOf(npc->audience()); })
        .writes("name", [npc](LuaArgs& args, std::int32_t index) { npc->setName(args.checkString(index)); })
        .method("skin",
                [npc](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    if (args.isTable(base)) {
                        args.pushBoolean(npc->skinFrom(stringField(args, base, "name").value_or("")));
                        return 1;
                    }
                    npc->skin(args.checkString(base),
                              args.isString(base + 1) ? std::optional{args.toText(base + 1)} : std::nullopt);
                    args.pushBoolean(true);
                    return 1;
                })
        .method("skin_from",
                [npc](LuaArgs& args) {
                    args.pushBoolean(npc->skinFrom(args.checkString(firstArgument(args))));
                    return 1;
                })
        .method("looks_at_players",
                [npc](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    npc->looksAtPlayers(args.isNil(base) || args.booleanOr(base, true));
                    return 0;
                })
        .method("look",
                [npc](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    npc->look(static_cast<float>(args.checkNumber(base)),
                              static_cast<float>(args.numberOr(base + 1, 0.0)));
                    return 0;
                })
        .method("teleport",
                [npc](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::optional<PluginPoint> point = pointFrom(args, base);
                    if (point.has_value()) {
                        args.pushBoolean(npc->teleport(point->x, point->y, point->z));
                        return 1;
                    }
                    args.pushBoolean(
                        npc->teleport(args.checkNumber(base), args.checkNumber(base + 1), args.checkNumber(base + 2)));
                    return 1;
                })
        .method("show_only_to",
                [npc](LuaArgs& args) {
                    npc->showOnlyTo(uuidsFrom(args, firstArgument(args)));
                    return 0;
                })
        .method("show_to_all",
                [npc](LuaArgs& args) {
                    static_cast<void>(args);
                    npc->showToAll();
                    return 0;
                })
        .method("remove", [npc](LuaArgs& args) {
            static_cast<void>(args);
            npc->remove();
            return 0;
        });
    std::move(object).push(lua);
}

void pushHologram(LuaArgs& lua, std::shared_ptr<PluginHologramRef> hologram) {
    PluginObject object{"hologram"};
    object.reads("id", [hologram] { return PluginValue{static_cast<std::int64_t>(hologram->id())}; })
        .reads("type", [hologram] { return PluginValue{hologram->type()}; })
        .reads("world", [hologram] { return PluginValue{hologram->world()}; })
        .reads("x", [hologram] { return PluginValue{hologram->x()}; })
        .reads("y", [hologram] { return PluginValue{hologram->y()}; })
        .reads("z", [hologram] { return PluginValue{hologram->z()}; })
        .reads("alive", [hologram] { return PluginValue{hologram->alive()}; })
        .reads("text", [hologram] { return PluginValue{hologram->text()}; })
        .reads("audience", [hologram] { return audienceOf(hologram->audience()); })
        .writes("text", [hologram](LuaArgs& args, std::int32_t index) { hologram->setText(args.checkString(index)); })
        .method("remove",
                [hologram](LuaArgs& args) {
                    static_cast<void>(args);
                    hologram->remove();
                    return 0;
                })
        .method("teleport",
                [hologram](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::optional<PluginPoint> point = pointFrom(args, base);
                    if (point.has_value()) {
                        args.pushBoolean(hologram->teleport(point->x, point->y, point->z));
                        return 1;
                    }
                    args.pushBoolean(hologram->teleport(args.checkNumber(base), args.checkNumber(base + 1),
                                                        args.checkNumber(base + 2)));
                    return 1;
                })
        .method("show_only_to",
                [hologram](LuaArgs& args) {
                    hologram->showOnlyTo(uuidsFrom(args, firstArgument(args)));
                    return 0;
                })
        .method("show_to_all", [hologram](LuaArgs& args) {
            static_cast<void>(args);
            hologram->showToAll();
            return 0;
        });
    std::move(object).push(lua);
}

void pushWorld(LuaArgs& lua, std::shared_ptr<PluginWorldRef> world, WorldHooks hooks) {
    const std::shared_ptr<WorldHooks> shared = std::make_shared<WorldHooks>(std::move(hooks));
    const auto resolve = [shared](const std::string& text) {
        return shared->resolveText ? shared->resolveText(text) : text;
    };
    PluginObject object{"world"};
    object.reads("name", [world] { return PluginValue{world->name()}; })
        .reads("time", [world] { return PluginValue{world->time()}; })
        .reads("raining", [world] { return PluginValue{world->raining()}; })
        .reads("thundering", [world] { return PluginValue{world->thundering()}; })
        .reads("difficulty", [world] { return PluginValue{world->difficulty()}; })
        .reads("rules", [world] { return PluginValue{world->gameRules()}; })
        .reads("dimension", [world] { return PluginValue{world->dimension()}; })
        .reads("seed", [world] { return PluginValue{world->seed()}; })
        .reads("min_height", [world] { return PluginValue{static_cast<std::int64_t>(world->minHeight())}; })
        .reads("height", [world] { return PluginValue{static_cast<std::int64_t>(world->height())}; })
        .reads("day_time", [world] { return PluginValue{world->dayTime()}; })
        .method("biome",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::optional<std::string> biome =
                        world->biomeAt(static_cast<std::int32_t>(args.checkInteger(base)),
                                       static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                       static_cast<std::int32_t>(args.checkInteger(base + 2)));
                    if (biome.has_value()) {
                        args.pushString(*biome);
                    } else {
                        args.pushNil();
                    }
                    return 1;
                })
        .method("set_biome",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->setBiomeAt(static_cast<std::int32_t>(args.checkInteger(base)),
                                                       static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                       static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                       args.checkString(base + 3)));
                    return 1;
                })
        .method("highest",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushInteger(world->highest(static_cast<std::int32_t>(args.checkInteger(base)),
                                                    static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                    args.isString(base + 2) ? args.toText(base + 2) : "motion_blocking",
                                                    true));
                    return 1;
                })
        .method("chunk_loaded",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->chunkLoaded(static_cast<std::int32_t>(args.checkInteger(base)),
                                                        static_cast<std::int32_t>(args.checkInteger(base + 1))));
                    return 1;
                })
        .method("load_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->loadChunk(static_cast<std::int32_t>(args.checkInteger(base)),
                                                      static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                      args.isNil(base + 2) || args.booleanOr(base + 2, true)));
                    return 1;
                })
        .method("unload_chunk",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    args.pushBoolean(world->unloadChunk(static_cast<std::int32_t>(args.checkInteger(base)),
                                                        static_cast<std::int32_t>(args.checkInteger(base + 1))));
                    return 1;
                })
        .method("set_time",
                [world](LuaArgs& args) {
                    world->setDayTime(args.checkInteger(firstArgument(args)));
                    return 0;
                })
        .method("set_weather",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    world->setWeather(args.booleanOr(base, false), args.booleanOr(base + 1, false));
                    return 0;
                })
        .method("set_spawn",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    world->setSpawn(static_cast<std::int32_t>(args.checkInteger(base)),
                                    static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                    static_cast<std::int32_t>(args.checkInteger(base + 2)));
                    return 0;
                })
        .method("block",
                [world](LuaArgs& args) {
                    const std::optional<PluginPoint> point = pointFrom(args, args.count() >= 2 ? 2 : 1);
                    std::optional<std::string> block;
                    if (point.has_value()) {
                        block = world->block(static_cast<std::int32_t>(std::floor(point->x)),
                                             static_cast<std::int32_t>(std::floor(point->y)),
                                             static_cast<std::int32_t>(std::floor(point->z)));
                    } else {
                        const std::int32_t base = args.count() >= 4 ? 2 : 1;
                        block = world->block(static_cast<std::int32_t>(args.checkInteger(base)),
                                             static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                             static_cast<std::int32_t>(args.checkInteger(base + 2)));
                    }
                    if (block.has_value()) {
                        args.pushString(*block);
                    } else {
                        args.pushNil();
                    }
                    return 1;
                })
        .method("block_state",
                [world](LuaArgs& args) {
                    const std::optional<PluginPoint> point = pointFrom(args, args.count() >= 2 ? 2 : 1);
                    const std::int32_t base = args.count() >= 4 ? 2 : 1;
                    const std::optional<std::string> state =
                        point.has_value() ? world->blockState(static_cast<std::int32_t>(std::floor(point->x)),
                                                              static_cast<std::int32_t>(std::floor(point->y)),
                                                              static_cast<std::int32_t>(std::floor(point->z)))
                                          : world->blockState(static_cast<std::int32_t>(args.checkInteger(base)),
                                                              static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                              static_cast<std::int32_t>(args.checkInteger(base + 2)));
                    if (state.has_value()) {
                        args.pushString(*state);
                    } else {
                        args.pushNil();
                    }
                    return 1;
                })
        .method("set_block",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::optional<PluginPoint> point = pointFrom(args, base);
                    if (point.has_value()) {
                        args.pushBoolean(world->setBlock(static_cast<std::int32_t>(std::floor(point->x)),
                                                         static_cast<std::int32_t>(std::floor(point->y)),
                                                         static_cast<std::int32_t>(std::floor(point->z)),
                                                         args.checkString(base + 1)));
                        return 1;
                    }
                    args.pushBoolean(world->setBlock(static_cast<std::int32_t>(args.checkInteger(base)),
                                                     static_cast<std::int32_t>(args.checkInteger(base + 1)),
                                                     static_cast<std::int32_t>(args.checkInteger(base + 2)),
                                                     args.checkString(base + 3)));
                    return 1;
                })
        .method("rule",
                [world](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string name = args.checkString(base);
                    if (args.isNil(base + 1)) {
                        const std::optional<std::string> value = world->gameRule(name);
                        if (value.has_value()) {
                            args.pushString(*value);
                        } else {
                            args.pushNil();
                        }
                        return 1;
                    }
                    args.pushBoolean(world->setGameRule(name, args.toText(base + 1)));
                    return 1;
                })
        .method("entities",
                [world](LuaArgs& args) {
                    args.pushTable();
                    std::int64_t position = 1;
                    for (const std::shared_ptr<PluginEntityRef>& entity : world->entities()) {
                        pushEntity(args, entity);
                        args.setIndex(position++);
                    }
                    return 1;
                })
        .method("npc",
                [world, shared, resolve](LuaArgs& args) {
                    const std::int32_t options = firstArgument(args);
                    if (!args.isTable(options)) {
                        args.raise("world:npc{ name = ..., pos = ... } takes a table");
                    }
                    args.pushField(options, "pos");
                    const std::optional<PluginPoint> point = pointFrom(args, args.top());
                    args.pop(1);
                    if (!point.has_value()) {
                        args.raise("an npc needs a pos: worlds.default():npc{ pos = p.pos }");
                    }
                    args.pushField(options, "on_click");
                    std::function<void(const std::string&)> onClick;
                    if (args.isFunction(-1) && shared->onNpcClick) {
                        wire::lua::LuaRef handler = wire::lua::LuaRef::of(args.state(), -1);
                        const std::shared_ptr<wire::lua::LuaRef> held =
                            std::make_shared<wire::lua::LuaRef>(std::move(handler));
                        const WorldHooks::NpcClick call = shared->onNpcClick;
                        onClick = [held, call](const std::string& uuid) { call(*held, uuid); };
                    }
                    args.pop(1);
                    args.pushField(options, "name");
                    const std::string name = resolve(nameLines(args, args.top()));
                    args.pop(1);
                    const std::shared_ptr<PluginNpcRef> made =
                        world->npc(name, point->x, point->y, point->z, std::move(onClick));
                    if (made == nullptr) {
                        args.pushNil();
                        return 1;
                    }
                    args.pushField(options, "skin");
                    if (args.isTable(-1)) {
                        const std::int32_t skin = args.top();
                        const std::optional<std::string> texture = stringField(args, skin, "texture");
                        if (texture.has_value()) {
                            made->skin(*texture, stringField(args, skin, "signature"));
                        }
                    }
                    args.pop(1);
                    pushNpc(args, made);
                    return 1;
                })
        .method("hologram",
                [world, resolve](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::string text = resolve(args.checkString(base));
                    const std::optional<PluginPoint> point = pointFrom(args, base + 1);
                    const std::shared_ptr<PluginHologramRef> made =
                        point.has_value() ? world->hologram(text, point->x, point->y, point->z)
                                          : world->hologram(text, args.checkNumber(base + 1),
                                                            args.checkNumber(base + 2), args.checkNumber(base + 3));
                    if (made == nullptr) {
                        args.pushNil();
                    } else {
                        pushHologram(args, made);
                    }
                    return 1;
                })
        .method("spawn", [world](LuaArgs& args) {
            const std::int32_t base = firstArgument(args);
            const std::string type = args.checkString(base);
            const std::optional<PluginPoint> point = pointFrom(args, base + 1);
            const std::shared_ptr<PluginEntityRef> made =
                point.has_value() ? world->spawn(type, point->x, point->y, point->z)
                                  : world->spawn(type, args.checkNumber(base + 1), args.checkNumber(base + 2),
                                                 args.checkNumber(base + 3));
            if (made == nullptr) {
                args.pushNil();
            } else {
                pushEntity(args, made);
            }
            return 1;
        });
    object.method("spawn_as", [world](LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        const std::shared_ptr<PluginEntityRef> made =
            world->spawnAs(args.checkString(base), args.checkNumber(base + 1), args.checkNumber(base + 2),
                           args.checkNumber(base + 3), static_cast<float>(args.numberOr(base + 4, 0.0)),
                           args.isString(base + 5) ? args.checkString(base + 5) : std::string{"plugin"});
        if (made == nullptr) {
            args.pushNil();
        } else {
            pushEntity(args, made);
        }
        return 1;
    });
    object.method("explode", [world](LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        args.pushBoolean(world->explode(args.checkNumber(base), args.checkNumber(base + 1), args.checkNumber(base + 2),
                                        static_cast<float>(args.checkNumber(base + 3)), args.booleanOr(base + 4, true),
                                        static_cast<std::int32_t>(args.integerOr(base + 5, -1)),
                                        args.booleanOr(base + 6, false)));
        return 1;
    });
    object.method("drop_item", [world](LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        const std::optional<PluginItem> item = itemFrom(args, base);
        if (!item.has_value()) {
            args.pushNil();
            return 1;
        }
        wire::nbt::NbtCompound stack;
        stack.putString("id", item->id);
        stack.putInt("count", item->count);
        const std::vector<std::uint8_t> bytes =
            wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(stack)));
        const std::int32_t id =
            world->dropItem(bytes, args.checkNumber(base + 1), args.checkNumber(base + 2), args.checkNumber(base + 3),
                            args.numberOr(base + 4, 0.0), args.numberOr(base + 5, 0.0), args.numberOr(base + 6, 0.0));
        const std::shared_ptr<PluginEntityRef> made = id < 0 ? nullptr : world->entityById(id);
        if (made == nullptr) {
            args.pushNil();
        } else {
            pushEntity(args, made);
        }
        return 1;
    });
    addBulkWorldMethods(object, world, shared);
    addEnvWorldMethods(object, world);
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
