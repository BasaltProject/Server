#include "PluginMiscShared.hpp"

namespace basalt::server::plugins::pluginApi::misc {
using wire::lua::LuaArgs;

void installServerTables(LuaArgs& lua, PluginHost& host) {
    PluginHost* server = &host;
    const auto control = [server]() -> PluginServerControl& { return server->services().serverControl(); };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "tick.rate", [control](LuaArgs& args) {
        args.pushNumber(static_cast<double>(control().tickRate()));
        return 1;
    });
    lua.setField("rate");
    wire::lua::pushHostFunction(lua, "tick.set_rate", [control](LuaArgs& args) {
        control().setTickRate(static_cast<float>(args.checkNumber(1)));
        return 0;
    });
    lua.setField("set_rate");
    wire::lua::pushHostFunction(lua, "tick.frozen", [control](LuaArgs& args) {
        if (!args.isNil(1)) {
            control().setFrozen(args.toBoolean(1));
            return 0;
        }
        args.pushBoolean(control().frozen());
        return 1;
    });
    lua.setField("frozen");
    wire::lua::pushHostFunction(lua, "tick.step", [control](LuaArgs& args) {
        args.pushBoolean(control().step(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("step");
    wire::lua::pushHostFunction(lua, "tick.sprint", [control](LuaArgs& args) {
        args.pushBoolean(control().sprint(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("sprint");
    wire::lua::pushHostFunction(lua, "tick.stop_sprint", [control](LuaArgs& args) {
        args.pushBoolean(control().stopSprinting());
        return 1;
    });
    lua.setField("stop_sprint");
    wire::lua::pushHostFunction(lua, "tick.pace", [control](LuaArgs& args) {
        const std::array<double, 4> pace = control().pace();
        args.pushTable();
        for (std::size_t index = 0; index < pace.size(); ++index) {
            args.pushNumber(pace[index]);
            args.setIndex(static_cast<std::int64_t>(index) + 1);
        }
        return 1;
    });
    lua.setField("pace");
    wire::lua::pushHostFunction(lua, "tick.times", [control](LuaArgs& args) {
        const std::vector<std::int64_t> times = control().tickTimes();
        args.pushTable();
        for (std::size_t index = 0; index < times.size(); ++index) {
            args.pushInteger(times[index]);
            args.setIndex(static_cast<std::int64_t>(index) + 1);
        }
        return 1;
    });
    lua.setField("times");
    wire::lua::pushHostFunction(lua, "tick.paused", [control](LuaArgs& args) {
        args.pushBoolean(control().paused());
        return 1;
    });
    lua.setField("paused");
    lua.setGlobal("tick");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "server_links.list", [control](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginServerLink& link : control().links()) {
            args.pushTable();
            args.pushString(link.kind);
            args.setField("kind");
            args.pushString(link.label);
            args.setField("label");
            args.pushString(link.url);
            args.setField("url");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("list");
    wire::lua::pushHostFunction(lua, "server_links.set", [control](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("server_links.set takes a list of { kind = ..., url = ... }");
        }
        std::vector<PluginServerLink> links;
        const std::int64_t count = args.length(1);
        for (std::int64_t position = 1; position <= count; ++position) {
            args.pushIndex(1, position);
            links.push_back(PluginServerLink{.kind = stringField(args, -1, "kind").value_or(""),
                                             .label = stringField(args, -1, "label").value_or(""),
                                             .url = stringField(args, -1, "url").value_or("")});
            args.pop(1);
        }
        control().setLinks(std::move(links));
        return 0;
    });
    lua.setField("set");
    lua.setGlobal("server_links");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "datapacks.list", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginDatapack& pack : server->services().datapacks().packs()) {
            args.pushTable();
            args.pushString(pack.id);
            args.setField("id");
            args.pushBoolean(pack.enabled);
            args.setField("enabled");
            args.pushBoolean(pack.required);
            args.setField("required");
            args.pushString(pack.compatibility);
            args.setField("compatibility");
            args.pushString(pack.source);
            args.setField("source");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("list");
    wire::lua::pushHostFunction(lua, "datapacks.refresh", [server](LuaArgs& args) {
        static_cast<void>(args);
        server->services().datapacks().refresh();
        return 0;
    });
    lua.setField("refresh");
    wire::lua::pushHostFunction(lua, "datapacks.functions", [server](LuaArgs& args) {
        pushNames(args, server->services().datapacks().functionIds());
        return 1;
    });
    lua.setField("functions");
    wire::lua::pushHostFunction(lua, "datapacks.function_tags", [server](LuaArgs& args) {
        pushNames(args, server->services().datapacks().functionTagIds());
        return 1;
    });
    lua.setField("function_tags");
    wire::lua::pushHostFunction(lua, "datapacks.scheduled", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginScheduledFunction& each : server->services().datapacks().scheduled()) {
            args.pushTable();
            args.pushString(each.name);
            args.setField("name");
            args.pushInteger(each.triggerTime);
            args.setField("trigger_time");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("scheduled");
    wire::lua::pushHostFunction(lua, "datapacks.schedule", [server](LuaArgs& args) {
        const std::string function = args.checkString(1);
        const std::int64_t delay = args.checkInteger(2);
        const bool replace = args.booleanOr(3, true);
        args.pushBoolean(server->services().datapacks().schedule(function, delay, replace));
        return 1;
    });
    lua.setField("schedule");
    wire::lua::pushHostFunction(lua, "datapacks.unschedule", [server](LuaArgs& args) {
        args.pushInteger(server->services().datapacks().unschedule(args.checkString(1)));
        return 1;
    });
    lua.setField("unschedule");
    wire::lua::pushHostFunction(lua, "datapacks.resource", [server](LuaArgs& args) {
        const auto bytes = server->services().datapacks().resource(args.checkString(1));
        if (bytes.has_value()) {
            args.pushString(std::string_view{reinterpret_cast<const char*>(bytes->data()), bytes->size()});
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("resource");
    wire::lua::pushHostFunction(lua, "datapacks.resources", [server](LuaArgs& args) {
        pushNames(args, server->services().datapacks().resourcesUnder(args.stringOrNull(1).value_or("")));
        return 1;
    });
    lua.setField("resources");
    wire::lua::pushHostFunction(lua, "datapacks.resource_pack", [server](LuaArgs& args) {
        const std::string pack = server->services().datapacks().resourcePack(args.checkString(1));
        if (pack.empty()) {
            args.pushNil();
        } else {
            args.pushString(pack);
        }
        return 1;
    });
    lua.setField("resource_pack");
    lua.setGlobal("datapacks");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "stopwatches.list", [control](LuaArgs& args) {
        pushNames(args, control().stopwatchIds());
        return 1;
    });
    lua.setField("list");
    for (const char* operation : {"create", "restart", "remove"}) {
        const std::string name = operation;
        wire::lua::pushHostFunction(lua, std::string{"stopwatches."} + name, [control, name](LuaArgs& args) {
            args.pushBoolean(control().stopwatch(name, args.checkString(1)).has_value());
            return 1;
        });
        lua.setField(operation);
    }
    wire::lua::pushHostFunction(lua, "stopwatches.elapsed", [control](LuaArgs& args) {
        const std::optional<double> seconds = control().stopwatch("elapsed", args.checkString(1));
        if (seconds.has_value()) {
            args.pushNumber(*seconds);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("elapsed");
    lua.setGlobal("stopwatches");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "advancements.ids", [server](LuaArgs& args) {
        pushNames(args, server->services().advancements().ids());
        return 1;
    });
    lua.setField("ids");
    wire::lua::pushHostFunction(lua, "advancements.info", [server](LuaArgs& args) {
        const std::optional<PluginAdvancement> info =
            server->services().advancements().advancement(args.checkString(1));
        if (!info.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(info->id);
        args.setField("id");
        if (info->parent.has_value()) {
            args.pushString(*info->parent);
            args.setField("parent");
        }
        args.pushString(info->root);
        args.setField("root");
        pushNames(args, info->criteria);
        args.setField("criteria");
        pushNames(args, info->children);
        args.setField("children");
        args.pushString(info->frame);
        args.setField("frame");
        args.pushString(info->iconItem);
        args.setField("icon");
        args.pushString(info->json);
        args.setField("json");
        return 1;
    });
    lua.setField("info");
    for (const bool granting : {true, false}) {
        wire::lua::pushHostFunction(
            lua, granting ? "advancements.grant" : "advancements.revoke", [server, granting](LuaArgs& args) {
                const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
                if (player == nullptr) {
                    args.raise("advancements.grant(player, id, criterion) needs an online player");
                }
                PluginAdvancements& all = server->services().advancements();
                args.pushBoolean(granting ? all.grant(player->uuid(), args.checkString(2), args.checkString(3))
                                          : all.revoke(player->uuid(), args.checkString(2), args.checkString(3)));
                return 1;
            });
        lua.setField(granting ? "grant" : "revoke");
    }
    wire::lua::pushHostFunction(lua, "advancements.trigger", [server](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
        if (player == nullptr) {
            args.raise("advancements.trigger(player, trigger, facts) needs an online player");
        }
        PluginTriggerFacts facts;
        if (args.isTable(3)) {
            const auto x = args.fieldAsText(3, "x");
            const auto y = args.fieldAsText(3, "y");
            const auto z = args.fieldAsText(3, "z");
            if (x.has_value() && y.has_value() && z.has_value()) {
                facts.block = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::stol(*x)),
                                                          static_cast<std::int32_t>(std::stol(*y)),
                                                          static_cast<std::int32_t>(std::stol(*z))};
            }
            facts.item = args.fieldAsText(3, "item").value_or("");
            if (const auto count = args.fieldAsText(3, "count"); count.has_value()) {
                facts.count = static_cast<std::int32_t>(std::stol(*count));
            }
            if (const auto distance = args.fieldAsText(3, "distance"); distance.has_value()) {
                facts.distance = std::stod(*distance);
            }
        }
        args.pushBoolean(server->services().advancements().trigger(player->uuid(), args.checkString(2), facts));
        return 1;
    });
    lua.setField("trigger");
    wire::lua::pushHostFunction(lua, "advancements.progress", [server](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
        args.pushTable();
        if (player != nullptr) {
            for (const auto& [criterion, when] :
                 server->services().advancements().obtained(player->uuid(), args.checkString(2))) {
                args.pushInteger(when);
                args.setField(criterion);
            }
        }
        return 1;
    });
    lua.setField("progress");
    lua.setGlobal("advancements");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "loot.tables", [server](LuaArgs& args) {
        pushNames(args, server->services().loot().tables());
        return 1;
    });
    lua.setField("tables");
    wire::lua::pushHostFunction(lua, "loot.roll", [server](LuaArgs& args) {
        PluginLootRoll roll;
        const std::string table = args.checkString(1);
        if (!args.isTable(2)) {
            args.raise("loot.roll(table, { world = ..., x = ..., y = ..., z = ... }) takes a table");
        }
        roll.world = stringField(args, 2, "world").value_or(std::string{});
        roll.x = numberField(args, 2, "x").value_or(0.0);
        roll.y = numberField(args, 2, "y").value_or(0.0);
        roll.z = numberField(args, 2, "z").value_or(0.0);
        roll.luck = static_cast<float>(numberField(args, 2, "luck").value_or(0.0));
        roll.seed = static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0));
        roll.looter = stringField(args, 2, "looter").value_or(std::string{});
        const std::optional<std::vector<PluginLootItem>> items = server->services().loot().roll(table, roll);
        if (!items.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginLootItem& item : *items) {
            args.pushTable();
            args.pushString(item.item);
            args.setField("item");
            args.pushInteger(item.count);
            args.setField("count");
            args.pushString(item.components);
            args.setField("components");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("roll");
    wire::lua::pushHostFunction(lua, "loot.roll_json", [server](LuaArgs& args) {
        PluginLootRoll roll;
        const std::string json = args.checkString(1);
        if (!args.isTable(2)) {
            args.raise("loot.roll_json(json, { world = ..., x = ..., y = ..., z = ... }) takes a table");
        }
        roll.world = stringField(args, 2, "world").value_or(std::string{});
        roll.x = numberField(args, 2, "x").value_or(0.0);
        roll.y = numberField(args, 2, "y").value_or(0.0);
        roll.z = numberField(args, 2, "z").value_or(0.0);
        roll.luck = static_cast<float>(numberField(args, 2, "luck").value_or(0.0));
        roll.seed = static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0));
        roll.looter = stringField(args, 2, "looter").value_or(std::string{});
        const std::optional<std::vector<PluginLootItem>> items = server->services().loot().rollJson(json, roll);
        if (!items.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginLootItem& item : *items) {
            args.pushTable();
            args.pushString(item.item);
            args.setField("item");
            args.pushInteger(item.count);
            args.setField("count");
            args.pushString(item.components);
            args.setField("components");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("roll_json");
    wire::lua::pushHostFunction(lua, "loot.number", [server](LuaArgs& args) {
        const std::string json = args.checkString(1);
        const bool asFloat = args.isTable(2) && numberField(args, 2, "float").value_or(0.0) != 0.0;
        const auto seed = args.isTable(2) ? static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0)) : 0;
        const std::optional<double> value = server->services().loot().number(json, asFloat, seed);
        if (!value.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushNumber(*value);
        return 1;
    });
    lua.setField("number");
    wire::lua::pushHostFunction(lua, "loot.enchant", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("loot.enchant({ item = ..., levels = ..., world = ... }) takes a table");
        }
        PluginLootItem given;
        given.item = stringField(args, 1, "item").value_or(std::string{});
        given.count = static_cast<std::int32_t>(numberField(args, 1, "count").value_or(1.0));
        given.components = stringField(args, 1, "components").value_or(std::string{});
        PluginLootEnchant how;
        how.world = stringField(args, 1, "world").value_or(std::string{});
        how.options = stringField(args, 1, "options").value_or(how.options);
        how.levels = static_cast<std::int32_t>(numberField(args, 1, "levels").value_or(1.0));
        how.seed = static_cast<std::int64_t>(numberField(args, 1, "seed").value_or(0.0));
        const std::optional<PluginLootItem> made = server->services().loot().enchant(given, how);
        if (!made.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(made->item);
        args.setField("item");
        args.pushInteger(made->count);
        args.setField("count");
        args.pushString(made->components);
        args.setField("components");
        return 1;
    });
    lua.setField("enchant");
    lua.setGlobal("loot");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "trades.roll", [server](LuaArgs& args) {
        const std::string profession = args.checkString(1);
        PluginTradeRoll roll;
        const auto level = static_cast<std::int32_t>(args.checkInteger(2));
        if (args.isTable(3)) {
            roll.world = stringField(args, 3, "world").value_or(std::string{});
            roll.x = numberField(args, 3, "x").value_or(0.0);
            roll.y = numberField(args, 3, "y").value_or(0.0);
            roll.z = numberField(args, 3, "z").value_or(0.0);
            roll.variant = stringField(args, 3, "variant").value_or(std::string{});
            roll.amount = static_cast<std::int32_t>(numberField(args, 3, "amount").value_or(-1.0));
            roll.seed = static_cast<std::int64_t>(numberField(args, 3, "seed").value_or(0.0));
            roll.listed = namesField(args, 3, "listed");
        }
        const std::optional<std::vector<PluginTradeOffer>> offers =
            server->services().loot().trades(profession, level, roll);
        if (!offers.has_value()) {
            args.pushNil();
            return 1;
        }
        const auto pushItem = [&args](const PluginLootItem& item) {
            args.pushTable();
            args.pushString(item.item);
            args.setField("item");
            args.pushInteger(item.count);
            args.setField("count");
            args.pushString(item.components);
            args.setField("components");
        };
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginTradeOffer& offer : *offers) {
            args.pushTable();
            pushItem(offer.costA);
            args.setField("cost_a");
            if (offer.costB.has_value()) {
                pushItem(*offer.costB);
                args.setField("cost_b");
            }
            pushItem(offer.result);
            args.setField("result");
            args.pushInteger(offer.maxUses);
            args.setField("max_uses");
            args.pushInteger(offer.experience);
            args.setField("xp");
            args.pushNumber(static_cast<double>(offer.priceMultiplier));
            args.setField("price_multiplier");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("roll");
    lua.setGlobal("trades");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "maps.create", [server](LuaArgs& args) {
        args.pushInteger(server->services().maps().create(
            args.checkString(1), static_cast<std::int32_t>(args.checkInteger(2)),
            static_cast<std::int32_t>(args.checkInteger(3)), static_cast<std::int32_t>(args.integerOr(4, 0))));
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "maps.info", [server](LuaArgs& args) {
        const std::optional<PluginMap> map =
            server->services().maps().map(static_cast<std::int32_t>(args.checkInteger(1)));
        if (!map.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(map->dimension);
        args.setField("dimension");
        args.pushInteger(map->scale);
        args.setField("scale");
        args.pushInteger(map->centerX);
        args.setField("center_x");
        args.pushInteger(map->centerZ);
        args.setField("center_z");
        args.pushBoolean(map->locked);
        args.setField("locked");
        return 1;
    });
    lua.setField("info");
    wire::lua::pushHostFunction(lua, "maps.explorer", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise(
                "maps.explorer({ world = ..., x = ..., y = ..., z = ..., destination = { ... } }) takes a table");
        }
        PluginExploration request;
        request.destination = namesField(args, 1, "destination");
        request.decoration = stringField(args, 1, "decoration").value_or(request.decoration);
        request.zoom = static_cast<std::int32_t>(numberField(args, 1, "zoom").value_or(request.zoom));
        request.radius = static_cast<std::int32_t>(numberField(args, 1, "radius").value_or(request.radius));
        request.skipExisting = flagField(args, 1, "skip_existing").value_or(request.skipExisting);
        const std::optional<std::vector<std::uint8_t>> made = server->services().maps().explorerMap(
            stringField(args, 1, "world").value_or(std::string{}), numberField(args, 1, "x").value_or(0.0),
            numberField(args, 1, "y").value_or(0.0), numberField(args, 1, "z").value_or(0.0), request);
        pushSnbt(args, made);
        return 1;
    });
    lua.setField("explorer");
    lua.setGlobal("maps");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "structures.ids", [server](LuaArgs& args) {
        pushNames(args, server->services().structures().ids());
        return 1;
    });
    lua.setField("ids");
    wire::lua::pushHostFunction(lua, "structures.read", [server](LuaArgs& args) {
        pushSnbt(args, server->services().structures().read(args.checkString(1)));
        return 1;
    });
    lua.setField("read");
    wire::lua::pushHostFunction(lua, "structures.write", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> bytes = snbtBytes(args, 2);
        args.pushBoolean(bytes.has_value() &&
                         server->services().structures().store(args.checkString(1), *bytes, args.toBoolean(3)));
        return 1;
    });
    lua.setField("write");
    wire::lua::pushHostFunction(lua, "structures.remove", [server](LuaArgs& args) {
        args.pushBoolean(server->services().structures().remove(args.checkString(1), args.toBoolean(2)));
        return 1;
    });
    lua.setField("remove");
    wire::lua::pushHostFunction(lua, "structures.file", [server](LuaArgs& args) {
        args.pushString(server->services().structures().file(args.checkString(1)));
        return 1;
    });
    lua.setField("file");
    wire::lua::pushHostFunction(lua, "structures.capture", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("structures.capture({ world = ..., x = ..., y = ..., z = ..., size_x = ... }) takes a table");
        }
        const auto number = [&args](std::string_view key) {
            return static_cast<std::int32_t>(numberField(args, 1, key).value_or(0.0));
        };
        pushSnbt(args, server->services().structures().capture(stringField(args, 1, "world").value_or(std::string{}),
                                                               number("x"), number("y"), number("z"), number("size_x"),
                                                               number("size_y"), number("size_z"),
                                                               flagField(args, 1, "entities").value_or(false)));
        return 1;
    });
    lua.setField("capture");
    wire::lua::pushHostFunction(lua, "structures.place", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> bytes = snbtBytes(args, 1);
        if (!bytes.has_value() || !args.isTable(2)) {
            args.pushBoolean(false);
            return 1;
        }
        PluginStructurePlacement placement;
        placement.world = stringField(args, 2, "world").value_or(std::string{});
        placement.x = static_cast<std::int32_t>(numberField(args, 2, "x").value_or(0.0));
        placement.y = static_cast<std::int32_t>(numberField(args, 2, "y").value_or(0.0));
        placement.z = static_cast<std::int32_t>(numberField(args, 2, "z").value_or(0.0));
        placement.rotation = static_cast<std::int32_t>(numberField(args, 2, "rotation").value_or(0.0));
        placement.mirror = static_cast<std::int32_t>(numberField(args, 2, "mirror").value_or(0.0));
        placement.integrity = static_cast<float>(numberField(args, 2, "integrity").value_or(1.0));
        placement.seed = static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0));
        placement.entities = flagField(args, 2, "entities").value_or(true);
        placement.palette = static_cast<std::int32_t>(numberField(args, 2, "palette").value_or(-1.0));
        args.pushBoolean(server->services().structures().place(*bytes, placement));
        return 1;
    });
    lua.setField("place");
    lua.setGlobal("structures");
}
} // namespace basalt::server::plugins::pluginApi::misc
