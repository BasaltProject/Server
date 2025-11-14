#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginEventTypes.hpp>

#include <doctest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace basalt::server::plugins;

namespace {

/// One event as one line, which is what the checksum is taken over: the whole table in one number,
/// plus a handful spelled out so a mismatch says what it should have been.
[[nodiscard]] std::string describe(const PluginEventType& type) {
    std::string fields;
    for (const std::string& field : type.fields()) {
        if (!fields.empty()) {
            fields += ',';
        }
        fields += field;
    }
    std::vector<std::string> mutables;
    for (const std::string& field : type.fields()) {
        if (type.isMutable(field)) {
            mutables.push_back(field);
        }
    }
    std::sort(mutables.begin(), mutables.end());
    std::string written;
    for (const std::string& field : mutables) {
        if (!written.empty()) {
            written += ',';
        }
        written += field;
    }
    return type.name() + "|" + (type.cancellable() ? "true" : "false") + "|" + fields + "|" + written;
}

[[nodiscard]] std::int64_t checksumOf(std::string_view text) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const char character : text) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

} // namespace

TEST_CASE("the table of events is the frozen kotlin's table") {
    const std::vector<const PluginEventType*>& all = pluginEventTypes::all();
    CHECK(all.size() == 70);

    std::set<std::string> names;
    std::string joined;
    for (const PluginEventType* type : all) {
        names.insert(type->name());
        if (!joined.empty()) {
            joined += ';';
        }
        joined += describe(*type);
    }
    // No two events answer to the same name, which is what makes a name an address.
    CHECK(names.size() == 70);
    CHECK(checksumOf(joined) == 4825059672451943845LL);

    // The ends of the list and a handful in the middle, spelled out.
    CHECK(describe(*all[0]) == "player.join|false|player_name,player_uuid,world|");
    CHECK(describe(*all[1]) == "player.quit|false|player_name,player_uuid,world|");
    CHECK(describe(*all[2]) == "player.chat|true|player_name,player_uuid,world,message|message");
    CHECK(describe(*all[17]) == "player.gamemode_change|true|player_name,player_uuid,world,gamemode,from_gamemode|");
    CHECK(describe(*all[34]) == "world.chunk_load|false|world,chunk_x,chunk_z,generated|");
    CHECK(describe(*all[51]) == "entity.shoot|true|entity_id,entity_type,world,projectile,power|");
    CHECK(describe(*all[68]) == "entity.combust|true|entity_id,entity_type,world,ticks,cause|");
    CHECK(describe(*all[69]) == "block.fade|true|world,x,y,z,block,to_block,cause|");

    CHECK(std::count_if(all.begin(), all.end(), [](const PluginEventType* type) { return type->cancellable(); }) == 50);
}

TEST_CASE("the events a handler is allowed to write back to") {
    std::vector<std::string> withMutable;
    for (const PluginEventType* type : pluginEventTypes::all()) {
        std::vector<std::string> fields;
        for (const std::string& field : type->fields()) {
            if (type->isMutable(field)) {
                fields.push_back(field);
            }
        }
        if (fields.empty()) {
            continue;
        }
        std::sort(fields.begin(), fields.end());
        std::string written = "[";
        for (const std::string& field : fields) {
            if (written.size() > 1) {
                written += ", ";
            }
            written += field;
        }
        withMutable.push_back(type->name() + "=" + written + "]");
    }
    std::string joined = "[";
    for (const std::string& entry : withMutable) {
        if (joined.size() > 1) {
            joined += ", ";
        }
        joined += entry;
    }
    joined += "]";
    CHECK(joined ==
          "[player.chat=[message], player.command=[command], player.food_change=[food], "
          "player.sign_change=[line1, line2, line3, line4], entity.damage=[amount], server.ping=[max_players, motd], "
          "server.command=[command], server.broadcast=[message], entity.heal=[amount]]");
}

TEST_CASE("the named accessors answer the same objects the list holds") {
    CHECK(&pluginEventTypes::playerJoin() == pluginEventTypes::all()[0]);
    CHECK(pluginEventTypes::playerChat().name() == "player.chat");
    CHECK(pluginEventTypes::blockBreak().cancellable());
    CHECK(pluginEventTypes::playerJoin().indexOf("player_uuid") == 1);

    // The two that are about one plugin's own life rather than something to broadcast.
    CHECK(pluginEventTypes::kPluginEnable == "plugin.enable");
    CHECK(pluginEventTypes::kPluginDisable == "plugin.disable");
    const std::vector<const PluginEventType*>& all = pluginEventTypes::all();
    CHECK(std::none_of(all.begin(), all.end(), [](const PluginEventType* type) {
        return type->name() == pluginEventTypes::kPluginEnable || type->name() == pluginEventTypes::kPluginDisable;
    }));
}
