#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginEventTypes.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace basalt::server::plugins;

namespace {
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

TEST_CASE("the table of events is the table of events") {
    const std::vector<const PluginEventType*>& all = pluginEventTypes::all();
    CHECK(all.size() == 165);

    std::set<std::string> names;
    std::string joined;
    for (const PluginEventType* type : all) {
        names.insert(type->name());
        if (!joined.empty()) {
            joined += ';';
        }
        joined += describe(*type);
    }
    CHECK(names.size() == 165);
    CHECK(checksumOf(joined) == -5543342650069144572LL);

    CHECK(describe(*all[0]) == "player.join|false|player_name,player_uuid,world,message,silent|message,silent");
    CHECK(describe(*all[1]) == "player.quit|false|player_name,player_uuid,world,message,silent|message,silent");
    CHECK(describe(*all[2]) == "player.chat|true|player_name,player_uuid,world,message|message");
    CHECK(describe(*all[17]) == "player.gamemode_change|true|player_name,player_uuid,world,gamemode,from_gamemode|");
    CHECK(describe(*all[34]) == "world.chunk_load|false|world,chunk_x,chunk_z,generated|");
    CHECK(describe(*all[51]) == "entity.shoot|true|entity_id,entity_type,world,projectile,power|");
    CHECK(describe(*all[68]) == "entity.combust|true|entity_id,entity_type,world,ticks,cause|");
    CHECK(describe(*all[77]) == "block.fade|true|world,x,y,z,block,to_block,cause|");

    CHECK(std::count_if(all.begin(), all.end(), [](const PluginEventType* type) { return type->cancellable(); }) == 118);
}
