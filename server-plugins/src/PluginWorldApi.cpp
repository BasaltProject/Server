#include "PluginApiShared.hpp"

namespace basalt::server::plugins::pluginApi::internal {
void installWorld(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;

    const auto pushOne = [server, target](LuaArgs& args, const std::shared_ptr<PluginPlayerRef>& player) {
        if (player == nullptr) {
            args.pushNil();
        } else {
            pushPlayer(args, player, &server->services().storage(), target->manifest.id, &target->text);
        }
    };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "players.online", [server, pushOne](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::shared_ptr<PluginPlayerRef>& player : server->services().players().online()) {
            pushOne(args, player);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("online");
    wire::lua::pushHostFunction(lua, "players.by_name", [server, pushOne](LuaArgs& args) {
        pushOne(args, server->services().players().byName(args.checkString(1)));
        return 1;
    });
    lua.setField("by_name");
    wire::lua::pushHostFunction(lua, "players.by_uuid", [server, pushOne](LuaArgs& args) {
        pushOne(args, server->services().players().byUuid(args.checkString(1)));
        return 1;
    });
    lua.setField("by_uuid");
    wire::lua::pushHostFunction(lua, "players.count", [server](LuaArgs& args) {
        args.pushInteger(static_cast<std::int64_t>(server->services().players().online().size()));
        return 1;
    });
    lua.setField("count");
    lua.setGlobal("players");

    const auto hooks = [server, target] {
        WorldHooks built;
        built.onNpcClick = [server, target](const LuaRef& handler, const std::string& uuid) {
            server->npcClicked(*target, handler, uuid);
        };
        built.resolveText = [target](const std::string& written) { return target->text.resolve(written, nullptr, 0); };
        built.biomeIds = [server] { return server->services().worlds().biomeIds(); };
        return built;
    };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "worlds.create", [server, hooks](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("worlds.create{ name = ..., dimension = ... } takes a table");
        }
        const std::optional<std::string> name = stringField(args, 1, "name");
        if (!name.has_value()) {
            args.raise("a world needs a name");
        }
        args.pushField(1, "seed");
        const std::optional<std::int64_t> seed = args.isNumber(-1) ? std::optional{args.toInteger(-1)} : std::nullopt;
        args.pop(1);
        const std::shared_ptr<PluginWorldRef> made =
            server->services().worlds().create(*name, stringField(args, 1, "dimension").value_or("overworld"),
                                               stringField(args, 1, "type").value_or("normal"), seed);
        if (made == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, made, hooks());
        }
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "worlds.unload", [server](LuaArgs& args) {
        std::string name;
        if (args.isTable(1)) {
            args.pushField(1, "name");
            name = args.toText(-1);
            args.pop(1);
        } else {
            name = args.checkString(1);
        }
        args.pushBoolean(server->services().worlds().unload(name, args.booleanOr(2, true)));
        return 1;
    });
    lua.setField("unload");
    wire::lua::pushHostFunction(lua, "worlds.delete", [server](LuaArgs& args) {
        std::string name;
        if (args.isTable(1)) {
            args.pushField(1, "name");
            name = args.checkString(-1);
            args.pop(1);
        } else {
            name = args.checkString(1);
        }
        args.pushBoolean(server->services().worlds().erase(name));
        return 1;
    });
    lua.setField("delete");
    wire::lua::pushHostFunction(lua, "worlds.all", [server, hooks](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::shared_ptr<PluginWorldRef>& world : server->services().worlds().all()) {
            pushWorld(args, world, hooks());
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("all");
    wire::lua::pushHostFunction(lua, "worlds.by_name", [server, hooks](LuaArgs& args) {
        const std::shared_ptr<PluginWorldRef> world = server->services().worlds().byName(args.checkString(1));
        if (world == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, world, hooks());
        }
        return 1;
    });
    lua.setField("by_name");
    wire::lua::pushHostFunction(lua, "worlds.default", [server, hooks](LuaArgs& args) {
        const std::shared_ptr<PluginWorldRef> world = server->services().worlds().defaultWorld();
        if (world == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, world, hooks());
        }
        return 1;
    });
    lua.setField("default");
    wire::lua::pushHostFunction(lua, "worlds.state_id", [server](LuaArgs& args) {
        args.pushInteger(server->services().worlds().stateId(args.checkString(1)));
        return 1;
    });
    lua.setField("state_id");
    wire::lua::pushHostFunction(lua, "worlds.state_text", [server](LuaArgs& args) {
        args.pushString(server->services().worlds().stateText(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("state_text");
    wire::lua::pushHostFunction(lua, "worlds.biomes", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& biome : server->services().worlds().biomeIds()) {
            args.pushString(biome);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("biomes");
    wire::lua::pushHostFunction(lua, "worlds.blocks", [server](LuaArgs& args) {
        const std::vector<std::string> flat = server->services().worlds().blockTable();
        args.pushTable();
        for (std::size_t at = 0; at + 2 < flat.size(); at += 3) {
            args.pushTable();
            args.pushString(flat[at]);
            args.setField("name");
            args.pushInteger(std::stoll(flat[at + 1]));
            args.setField("first_state");
            args.pushInteger(std::stoll(flat[at + 2]));
            args.setField("state_count");
            args.setIndex(static_cast<std::int64_t>(at / 3) + 1);
        }
        return 1;
    });
    lua.setField("blocks");
    wire::lua::pushHostFunction(lua, "worlds.block_properties", [server](LuaArgs& args) {
        const std::vector<std::string> flat = server->services().worlds().blockProperties(args.checkString(1));
        args.pushTable();
        for (std::size_t at = 0; at + 1 < flat.size(); at += 2) {
            args.pushTable();
            args.pushString(flat[at]);
            args.setField("name");
            args.pushTable();
            std::int64_t position = 1;
            for (std::size_t from = 0; from <= flat[at + 1].size();) {
                const std::size_t bar = flat[at + 1].find('|', from);
                const std::size_t end = bar == std::string::npos ? flat[at + 1].size() : bar;
                args.pushString(flat[at + 1].substr(from, end - from));
                args.setIndex(position++);
                from = end + 1;
            }
            args.setField("values");
            args.setIndex(static_cast<std::int64_t>(at / 2) + 1);
        }
        return 1;
    });
    lua.setField("block_properties");
    wire::lua::pushHostFunction(lua, "worlds.state_flags", [server](LuaArgs& args) {
        const std::int32_t bits =
            server->services().worlds().stateFlags(static_cast<std::int32_t>(args.checkInteger(1)));
        args.pushTable();
        const auto flag = [&args, bits](const char* name, std::int32_t mask) {
            args.pushBoolean((bits & mask) != 0);
            args.setField(name);
        };
        flag("blocks_motion", 1);
        flag("occludes", 2);
        flag("full_cube", 4);
        flag("replaceable", 8);
        flag("has_fluid", 16);
        flag("has_collision", 32);
        flag("conducts_redstone", 64);
        flag("ticks_randomly", 128);
        flag("air", 256);
        flag("has_block_entity", 1 << 18);
        args.pushInteger((bits >> 9) & 15);
        args.setField("light_emission");
        args.pushInteger((bits >> 13) & 31);
        args.setField("light_opacity");
        return 1;
    });
    lua.setField("state_flags");
    wire::lua::pushHostFunction(lua, "worlds.block_hardness", [server](LuaArgs& args) {
        const std::vector<double> facts = server->services().worlds().blockHardness(args.checkString(1));
        if (facts.size() < 3) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushNumber(facts[0]);
        args.setField("destroy_time");
        args.pushNumber(facts[1]);
        args.setField("explosion_resistance");
        args.pushBoolean(facts[2] != 0.0);
        args.setField("needs_tool");
        return 1;
    });
    lua.setField("block_hardness");
    wire::lua::pushHostFunction(lua, "worlds.block_info", [server](LuaArgs& args) {
        const std::vector<double> facts =
            server->services().worlds().blockInfo(static_cast<std::int32_t>(args.checkInteger(1)));
        if (facts.size() < 7) {
            args.pushNil();
            return 1;
        }
        static constexpr std::array<const char*, 5> kPush{"normal", "destroy", "block", "ignore", "push_only"};
        args.pushTable();
        args.pushNumber(facts[0]);
        args.setField("friction");
        args.pushNumber(facts[1]);
        args.setField("speed_factor");
        args.pushNumber(facts[2]);
        args.setField("jump_factor");
        args.pushString(kPush[static_cast<std::size_t>(std::clamp(facts[3], 0.0, 4.0))]);
        args.setField("push_reaction");
        args.pushInteger(static_cast<std::int64_t>(facts[4]));
        args.setField("map_color");
        args.pushNumber(facts[5]);
        args.setField("ignite_odds");
        args.pushNumber(facts[6]);
        args.setField("burn_odds");
        return 1;
    });
    lua.setField("block_info");
    wire::lua::pushHostFunction(lua, "worlds.block_instrument", [server](LuaArgs& args) {
        const std::string name =
            server->services().worlds().blockInstrument(static_cast<std::int32_t>(args.checkInteger(1)));
        if (name.empty()) {
            args.pushNil();
        } else {
            args.pushString(name);
        }
        return 1;
    });
    lua.setField("block_instrument");
    wire::lua::pushHostFunction(lua, "worlds.block_sounds", [server](LuaArgs& args) {
        const std::vector<std::string> facts =
            server->services().worlds().blockSounds(static_cast<std::int32_t>(args.checkInteger(1)));
        if (facts.size() < 7) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushNumber(std::stod(facts[0]));
        args.setField("volume");
        args.pushNumber(std::stod(facts[1]));
        args.setField("pitch");
        for (std::size_t at = 2; at < 7; ++at) {
            args.pushString(facts[at]);
            args.setField(std::array<const char*, 5>{"break", "step", "place", "hit", "fall"}[at - 2]);
        }
        return 1;
    });
    lua.setField("block_sounds");
    wire::lua::pushHostFunction(lua, "worlds.tool_facts", [server](LuaArgs& args) {
        const std::vector<double> facts = server->services().worlds().toolFacts(
            args.isString(1) ? args.checkString(1) : std::string{}, static_cast<std::int32_t>(args.checkInteger(2)));
        if (facts.size() < 2) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushBoolean(facts[0] != 0.0);
        args.setField("correct_for_drops");
        args.pushNumber(facts[1]);
        args.setField("mining_speed");
        return 1;
    });
    lua.setField("tool_facts");
    wire::lua::pushHostFunction(lua, "worlds.game_rules", [server](LuaArgs& args) {
        const std::vector<std::string> specs = server->services().worlds().gameRuleSpecs();
        args.pushTable();
        for (std::size_t at = 0; at + 2 < specs.size(); at += 3) {
            args.pushTable();
            args.pushString(specs[at + 1]);
            args.setField("type");
            args.pushString(specs[at + 2]);
            args.setField("default");
            args.setField(specs[at]);
        }
        return 1;
    });
    lua.setField("game_rules");
    wire::lua::pushHostFunction(lua, "worlds.transform_state", [server](LuaArgs& args) {
        args.pushInteger(server->services().worlds().transformState(static_cast<std::int32_t>(args.checkInteger(1)),
                                                                    static_cast<std::int32_t>(args.integerOr(2, 0)),
                                                                    static_cast<std::int32_t>(args.integerOr(3, 0))));
        return 1;
    });
    lua.setField("transform_state");
    wire::lua::pushHostFunction(lua, "worlds.block_entity_types", [server](LuaArgs& args) {
        const std::vector<std::string> pairs = server->services().worlds().blockEntityTypes();
        args.pushTable();
        for (std::size_t at = 0; at + 1 < pairs.size(); at += 2) {
            args.pushString(pairs[at + 1]);
            args.setField(pairs[at]);
        }
        return 1;
    });
    lua.setField("block_entity_types");
    wire::lua::pushHostFunction(lua, "worlds.data_version", [server](LuaArgs& args) {
        args.pushInteger(server->services().worlds().dataVersion());
        return 1;
    });
    lua.setField("data_version");
    wire::lua::pushHostFunction(lua, "worlds.fix_nbt", [server](LuaArgs& args) {
        const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed =
            wire::nbt::snbt::parse(args.checkString(2));
        if (!parsed.has_value()) {
            args.raise("that is not valid NBT text");
        }
        const std::vector<std::uint8_t> bytes = wire::nbt::nbtIO::write(*parsed);
        const std::optional<std::vector<std::uint8_t>> fixed = server->services().worlds().fixNbt(
            args.checkString(1), bytes, static_cast<std::int32_t>(args.checkInteger(3)));
        if (fixed.has_value()) {
            const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*fixed);
            if (read.has_value()) {
                args.pushString(wire::nbt::snbt::stringify(read->tag));
                return 1;
            }
        }
        args.pushNil();
        return 1;
    });
    lua.setField("fix_nbt");
    wire::lua::pushHostFunction(lua, "worlds.fix_name", [server](LuaArgs& args) {
        const std::optional<std::string> fixed = server->services().worlds().fixName(
            args.checkString(1), args.checkString(2), static_cast<std::int32_t>(args.checkInteger(3)));
        if (fixed.has_value()) {
            args.pushString(*fixed);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("fix_name");
    wire::lua::pushHostFunction(lua, "worlds.items", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& item : server->services().worlds().itemIds()) {
            args.pushString(item);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("items");
    wire::lua::pushHostFunction(lua, "worlds.entity_types", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& type : server->services().worlds().entityTypeIds()) {
            args.pushString(type);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("entity_types");
    wire::lua::pushHostFunction(lua, "worlds.registry", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& id : server->services().worlds().registryIds(args.checkString(1))) {
            args.pushString(id);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("registry");
    wire::lua::pushHostFunction(lua, "worlds.registry_data", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> data =
            server->services().worlds().registryData(args.checkString(1), args.checkString(2));
        const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read =
            data.has_value() ? wire::nbt::nbtIO::readAnonymous(*data) : std::unexpected(wire::nbt::NbtError{});
        if (read.has_value()) {
            args.pushString(wire::nbt::snbt::stringify(read->tag));
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("registry_data");
    wire::lua::pushHostFunction(lua, "worlds.tags", [server](LuaArgs& args) {
        const std::vector<std::string> flat = server->services().worlds().tags(args.checkString(1));
        args.pushTable();
        for (std::size_t at = 0; at + 1 < flat.size(); at += 2) {
            args.pushTable();
            std::int64_t position = 1;
            for (std::size_t from = 0; from <= flat[at + 1].size() && !flat[at + 1].empty();) {
                const std::size_t bar = flat[at + 1].find('|', from);
                const std::size_t end = bar == std::string::npos ? flat[at + 1].size() : bar;
                args.pushString(flat[at + 1].substr(from, end - from));
                args.setIndex(position++);
                from = end + 1;
            }
            args.setField(flat[at]);
        }
        return 1;
    });
    lua.setField("tags");
    lua.setGlobal("worlds");
    installRecipes(lua, host);

    wire::lua::installWaitingFunction(
        *instance.sandbox, "request_chunk", [server, target, lane](LuaArgs& args, wire::lua::LuaCall::Resume resume) {
            const std::shared_ptr<PluginWorldRef> world = server->services().worlds().byName(args.toText(1));
            if (lane->deciderDepth > 0 || world == nullptr) {
                resume();
                return;
            }
            bool generate = true;
            bool urgent = false;
            if (args.isTable(4)) {
                args.pushField(4, "generate");
                generate = args.isNil(-1) ? true : args.toBoolean(-1);
                args.pop(1);
                args.pushField(4, "urgent");
                urgent = !args.isNil(-1) && args.toBoolean(-1);
                args.pop(1);
            }
            const auto immediate = std::make_shared<bool>(true);
            world->requestChunk(static_cast<std::int32_t>(args.toInteger(2)),
                                static_cast<std::int32_t>(args.toInteger(3)), generate, urgent ? 0 : 100,
                                [server, target, lane, resume, immediate](bool) {
                                    if (*immediate) {
                                        resume();
                                        return;
                                    }
                                    lane->sandbox->refill();
                                    server->resuming(*target, resume);
                                });
            *immediate = false;
        });

    wire::lua::installWaitingFunction(
        *instance.sandbox, "at", [server, target, lane](wire::lua::LuaCall::Resume resume) {
            if (lane->deciderDepth > 0) {
                resume();
                return;
            }
            LuaArgs args{lane->sandbox->state()};
            const std::optional<PluginPoint> point = pointFrom(args, 1);
            const std::string world = point.has_value() && point->world.has_value() ? *point->world : args.toText(1);
            const double x = point.has_value() ? point->x : args.toNumber(2);
            const double z = point.has_value() ? point->z : args.toNumber(3);
            const bool arranged = server->services().at(world, x, z, [server, target, lane, resume] {
                lane->sandbox->refill();
                server->resuming(*target, resume);
            });
            if (!arranged) {
                resume();
            }
        });

    wire::lua::pushHostFunction(lua, "pos", [](LuaArgs& args) {
        pushPosition(args, args.checkString(1), args.checkNumber(2), args.checkNumber(3), args.checkNumber(4));
        return 1;
    });
    lua.setGlobal("pos");
}

} // namespace basalt::server::plugins::pluginApi::internal
