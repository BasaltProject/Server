#include "WasmWorlds.hpp"

#include <algorithm>
#include <array>
#include <bit>

namespace basalt::server::plugins::wasm::worlds {
namespace {
constexpr std::array<std::string_view, 6> kFaces{"down", "up", "north", "south", "west", "east"};
constexpr std::array<std::string_view, 9> kSequenceKinds{"int",    "bound",    "long",  "bool", "float",
                                                         "double", "gaussian", "reset", "clear"};

struct Ctx {
    State& state;
    PluginWorldRef& world;
    PluginWorldEnv* env;
    const Value& args;
    Value out;
};

using Run = std::int32_t (*)(Ctx&);

struct Op {
    std::string_view name;
    bool needsEnv;
    Run run;
};

[[nodiscard]] std::int32_t at(const Ctx& c, std::string_view key) {
    return static_cast<std::int32_t>(intField(c.args, key));
}

[[nodiscard]] std::int32_t faceOf(const Value& args, std::string_view key) {
    const Value* found = args.find(key);
    if (found == nullptr) {
        return -1;
    }
    if (const std::string* name = found->asString()) {
        const auto face = std::ranges::find(kFaces, std::string_view{*name});
        return face == kFaces.end() ? -1 : static_cast<std::int32_t>(face - kFaces.begin());
    }
    const std::int64_t whole = intField(args, key, -1);
    return whole >= 0 && whole < 6 ? static_cast<std::int32_t>(whole) : -1;
}

[[nodiscard]] std::vector<std::pair<std::string, std::int32_t>> enchantments(const Value& args) {
    std::vector<std::pair<std::string, std::int32_t>> out;
    const Value* found = args.find("enchantments");
    if (found == nullptr || found->asMap() == nullptr) {
        return out;
    }
    for (const MapEntry& entry : *found->asMap()) {
        out.emplace_back(entry.key, static_cast<std::int32_t>(intField(*found, entry.key)));
    }
    return out;
}

[[nodiscard]] Value placed(const std::vector<PluginPlacedBlock>& blocks) {
    Value::List list;
    for (const PluginPlacedBlock& block : blocks) {
        Value::Map row;
        put(row, "x", Value{block.x});
        put(row, "y", Value{block.y});
        put(row, "z", Value{block.z});
        put(row, "before", Value{block.before});
        put(row, "after", Value{block.after});
        list.emplace_back(std::move(row));
    }
    return Value{std::move(list)};
}

[[nodiscard]] std::int32_t finish(Ctx& c, Value value) {
    c.out = std::move(value);
    return abi::kOk;
}

[[nodiscard]] std::int32_t finishBool(Ctx& c, bool value) { return finish(c, Value{value}); }

std::int32_t save(Ctx& c) {
    c.world.save();
    return abi::kOk;
}

std::int32_t useItem(Ctx& c) {
    const std::int32_t face = faceOf(c.args, "face");
    if (face < 0) {
        return abi::kErrInvalid;
    }
    return finish(c, Value{c.world.useItemOn(at(c, "x"), at(c, "y"), at(c, "z"), face, textField(c.args, "item"),
                                             static_cast<float>(realField(c.args, "yaw")),
                                             static_cast<float>(realField(c.args, "pitch")))});
}

std::int32_t survives(Ctx& c) {
    return finishBool(c, c.env->survives(at(c, "x"), at(c, "y"), at(c, "z"), at(c, "state")));
}

std::int32_t boneMeal(Ctx& c) { return finishBool(c, c.env->boneMeal(at(c, "x"), at(c, "y"), at(c, "z"))); }

std::int32_t breakBlock(Ctx& c) {
    return finishBool(c, c.env->breakBlock(at(c, "x"), at(c, "y"), at(c, "z"), textField(c.args, "tool"),
                                           enchantments(c.args), boolField(c.args, "drops", true)));
}

std::int32_t drops(Ctx& c) {
    const std::optional<std::vector<PluginDrop>> found =
        c.env->blockDrops(at(c, "x"), at(c, "y"), at(c, "z"), textField(c.args, "tool"), enchantments(c.args),
                          boolField(c.args, "by_entity", true));
    if (!found) {
        return abi::kErrAbsent;
    }
    Value::List list;
    for (const PluginDrop& drop : *found) {
        Value::Map row;
        put(row, "item", Value{drop.item});
        put(row, "count", Value{drop.count});
        put(row, "components", Value{drop.components});
        list.emplace_back(std::move(row));
    }
    return finish(c, Value{std::move(list)});
}

std::int32_t signal(Ctx& c) {
    const std::int32_t face = faceOf(c.args, "face");
    return face < 0 ? abi::kErrInvalid
                    : finish(c, Value{c.env->signalFrom(at(c, "x"), at(c, "y"), at(c, "z"), face,
                                                        boolField(c.args, "direct"))});
}

std::int32_t ownSignal(Ctx& c) {
    const std::int32_t face = faceOf(c.args, "face");
    return face < 0 ? abi::kErrInvalid
                    : finish(c, Value{c.env->ownSignalFrom(at(c, "x"), at(c, "y"), at(c, "z"), face)});
}

std::int32_t analogSignal(Ctx& c) {
    const std::int32_t face = faceOf(c.args, "face");
    return face < 0 ? abi::kErrInvalid
                    : finish(c, Value{c.env->analogSignal(at(c, "x"), at(c, "y"), at(c, "z"), face)});
}

std::int32_t fluid(Ctx& c) {
    const PluginFluid found = c.env->fluidAt(at(c, "x"), at(c, "y"), at(c, "z"));
    Value::Map row;
    put(row, "fluid", Value{found.fluid});
    put(row, "source", Value{found.source});
    put(row, "level", Value{found.level});
    put(row, "falling", Value{found.falling});
    return finish(c, Value{std::move(row)});
}

std::int32_t fluidFlow(Ctx& c) {
    const std::array<double, 3> flow = c.env->fluidFlow(at(c, "x"), at(c, "y"), at(c, "z"));
    return finish(c, Value{Value::List{Value{flow[0]}, Value{flow[1]}, Value{flow[2]}}});
}

std::int32_t highest(Ctx& c) {
    return finish(c, Value{c.world.highest(at(c, "x"), at(c, "z"), textField(c.args, "kind", "surface"),
                                           boolField(c.args, "load"))});
}

std::int32_t randomTick(Ctx& c) { return finishBool(c, c.env->randomTick(at(c, "x"), at(c, "y"), at(c, "z"))); }

std::int32_t growTree(Ctx& c) {
    return finishBool(c, c.env->growTree(at(c, "x"), at(c, "y"), at(c, "z"), textField(c.args, "sapling")));
}

std::int32_t placeTree(Ctx& c) {
    const auto blocks = c.env->placeTree(textField(c.args, "tree"), at(c, "x"), at(c, "y"), at(c, "z"),
                                         boolField(c.args, "apply", true));
    return blocks ? finish(c, placed(*blocks)) : abi::kErrAbsent;
}

std::int32_t placeFeature(Ctx& c) {
    const auto blocks = c.env->placeFeature(textField(c.args, "feature"), at(c, "x"), at(c, "y"), at(c, "z"),
                                            intField(c.args, "seed"), boolField(c.args, "apply", true));
    return blocks ? finish(c, placed(*blocks)) : abi::kErrAbsent;
}

std::int32_t featureIds(Ctx& c) { return finish(c, textList(c.env->featureIds())); }

std::int32_t pois(Ctx& c) {
    std::vector<std::string> types;
    if (const Value* found = c.args.find("types"); found != nullptr && found->asList() != nullptr) {
        for (const Value& item : *found->asList()) {
            if (const std::string* text = item.asString()) {
                types.push_back(*text);
            }
        }
    }
    const std::vector<PluginPoi> found = c.env->pointsOfInterest(
        types, at(c, "x"), at(c, "y"), at(c, "z"), static_cast<std::int32_t>(intField(c.args, "radius", 16)),
        static_cast<std::int32_t>(intField(c.args, "occupancy", 2)));
    Value::List list;
    for (const PluginPoi& poi : found) {
        Value::Map row;
        put(row, "type", Value{poi.type});
        put(row, "x", Value{poi.x});
        put(row, "y", Value{poi.y});
        put(row, "z", Value{poi.z});
        put(row, "free_tickets", Value{poi.freeTickets});
        list.emplace_back(std::move(row));
    }
    return finish(c, Value{std::move(list)});
}

std::int32_t climate(Ctx& c) {
    const auto found = c.env->climateAtQuart(at(c, "x"), at(c, "y"), at(c, "z"));
    if (!found) {
        return abi::kErrAbsent;
    }
    Value::List list;
    for (const std::int64_t parameter : *found) {
        list.emplace_back(parameter);
    }
    return finish(c, Value{std::move(list)});
}

std::int32_t humidity(Ctx& c) { return finish(c, Value{c.env->humidity(at(c, "x"), at(c, "y"), at(c, "z"))}); }

std::int32_t temperature(Ctx& c) { return finish(c, Value{c.env->temperature(at(c, "x"), at(c, "y"), at(c, "z"))}); }

std::int32_t beaconScan(Ctx& c) {
    const PluginBeacon beacon = c.env->beaconScan(at(c, "x"), at(c, "y"), at(c, "z"));
    Value::List sections;
    for (const std::array<std::int32_t, 2>& section : beacon.sections) {
        sections.emplace_back(intList(section));
    }
    Value::Map row;
    put(row, "levels", Value{beacon.levels});
    put(row, "sections", Value{std::move(sections)});
    return finish(c, Value{std::move(row)});
}

std::int32_t lightningTarget(Ctx& c) {
    const auto found = c.env->lightningTarget(at(c, "x"), at(c, "z"));
    return found ? finish(c, intList(*found)) : abi::kErrAbsent;
}

[[nodiscard]] Value raidValue(const PluginRaid& raid) {
    Value::List raiders;
    for (const std::int32_t id : raid.raiders) {
        raiders.emplace_back(id);
    }
    Value::List leaders;
    for (const auto& [wave, leader] : raid.leaders) {
        leaders.emplace_back(Value::List{Value{wave}, Value{leader}});
    }
    Value::Map row;
    put(row, "id", Value{raid.id});
    put(row, "started", Value{raid.started});
    put(row, "ticks_active", Value{raid.ticksActive});
    put(row, "omen_level", Value{raid.omenLevel});
    put(row, "status", Value{raid.status});
    put(row, "groups_spawned", Value{raid.groupsSpawned});
    put(row, "total_groups", Value{raid.totalGroups});
    put(row, "total_health", Value{static_cast<double>(raid.totalHealth)});
    put(row, "center", intList(raid.center));
    put(row, "heroes", textList(raid.heroes));
    put(row, "raiders", Value{std::move(raiders)});
    put(row, "leaders", Value{std::move(leaders)});
    put(row, "bar_title", Value{raid.barTitle});
    put(row, "bar_color", Value{raid.barColor});
    put(row, "bar_style", Value{raid.barStyle});
    put(row, "bar_progress", Value{static_cast<double>(raid.barProgress)});
    return Value{std::move(row)};
}

std::int32_t raids(Ctx& c) {
    Value::List list;
    for (const PluginRaid& raid : c.env->raids()) {
        list.push_back(raidValue(raid));
    }
    return finish(c, Value{std::move(list)});
}

std::int32_t raidControl(Ctx& c) {
    std::array<std::int32_t, 4> numbers{};
    if (const Value* found = c.args.find("arguments"); found != nullptr && found->asList() != nullptr) {
        for (std::size_t index = 0; index < numbers.size() && index < found->asList()->size(); ++index) {
            if (const std::int64_t* whole = (*found->asList())[index].asInt()) {
                numbers[index] = static_cast<std::int32_t>(*whole);
            }
        }
    }
    return finish(c, Value{c.env->raidControl(at(c, "id"), textField(c.args, "action"), numbers, at(c, "value"))});
}

std::int32_t raidBar(Ctx& c) {
    return finishBool(
        c, c.env->raidBar(at(c, "id"), textField(c.args, "action"), textField(c.args, "text"), at(c, "value")));
}

std::int32_t setRaid(Ctx& c) {
    return finishBool(c, c.env->setRaid(at(c, "id"), at(c, "omen_level"), at(c, "total_groups")));
}

std::int32_t sculk(Ctx& c) {
    return finishBool(c,
                      c.env->sculk(textField(c.args, "action"), at(c, "x"), at(c, "y"), at(c, "z"), at(c, "source_x"),
                                   at(c, "source_y"), at(c, "source_z"), at(c, "value"), textField(c.args, "player")));
}

std::int32_t hive(Ctx& c) {
    return finish(c, Value{c.env->hive(textField(c.args, "action"), at(c, "x"), at(c, "y"), at(c, "z"),
                                       at(c, "entity_id"), textField(c.args, "player"))});
}

std::int32_t vault(Ctx& c) {
    return finish(c, Value{c.env->vault(textField(c.args, "action"), at(c, "x"), at(c, "y"), at(c, "z"),
                                        textField(c.args, "player"))});
}

std::int32_t creakingHeart(Ctx& c) {
    const std::string answer = c.env->creakingHeart(textField(c.args, "action"), at(c, "x"), at(c, "y"), at(c, "z"));
    return finish(c, answer.empty() ? Value{} : Value{answer});
}

std::int32_t dragonFight(Ctx& c) {
    const auto facts = c.world.dragonFight();
    if (!facts) {
        return abi::kErrAbsent;
    }
    Value::Map row;
    put(row, "previously_killed", Value{facts->previouslyKilled});
    put(row, "portal", facts->portal ? intList(*facts->portal) : Value{});
    put(row, "respawn_stage", Value{facts->respawnStage});
    put(row, "gateways_left", Value{facts->gatewaysLeft});
    put(row, "dragon_uuid", Value{facts->dragonUuid});
    put(row, "bar_progress", Value{static_cast<double>(facts->barProgress)});
    put(row, "bar_visible", Value{facts->barVisible});
    put(row, "bar_title", Value{facts->barTitle});
    put(row, "bar_color", Value{facts->barColor});
    put(row, "bar_style", Value{facts->barStyle});
    put(row, "respawn_crystals", textList(facts->respawnCrystals));
    put(row, "healing_crystals", textList(facts->healingCrystals));
    return finish(c, Value{std::move(row)});
}

std::int32_t dragonFightAction(Ctx& c) {
    return finishBool(c, c.world.dragonFightAction(textField(c.args, "action"), at(c, "value")));
}

std::int32_t dragonFightBar(Ctx& c) {
    return finishBool(c,
                      c.world.dragonFightBar(textField(c.args, "action"), textField(c.args, "text"), at(c, "value")));
}

std::int32_t placeEndGateway(Ctx& c) {
    return finishBool(c, c.world.placeEndGateway(at(c, "x"), at(c, "y"), at(c, "z")));
}

std::int32_t randomSequence(Ctx& c) {
    const std::string kind = textField(c.args, "kind", "long");
    const auto found = std::ranges::find(kSequenceKinds, std::string_view{kind});
    if (found == kSequenceKinds.end()) {
        return abi::kErrInvalid;
    }
    const auto operation = static_cast<std::int32_t>(found - kSequenceKinds.begin());
    const std::int64_t value = c.env->randomSequence(textField(c.args, "id"), operation, intField(c.args, "argument"));
    switch (operation) {
    case 3:
        return finishBool(c, value != 0);
    case 4:
        return finish(c, Value{static_cast<double>(std::bit_cast<float>(static_cast<std::int32_t>(value)))});
    case 5:
    case 6:
        return finish(c, Value{std::bit_cast<double>(value)});
    default:
        return finish(c, Value{value});
    }
}

std::int32_t inhabitedTime(Ctx& c) {
    if (hasField(c.args, "value")) {
        c.env->setInhabitedTime(at(c, "chunk_x"), at(c, "chunk_z"), intField(c.args, "value"));
    }
    return finish(c, Value{c.env->inhabitedTime(at(c, "chunk_x"), at(c, "chunk_z"))});
}

std::int32_t forceChunk(Ctx& c) {
    c.env->setChunkForced(at(c, "chunk_x"), at(c, "chunk_z"), boolField(c.args, "forced", true));
    return abi::kOk;
}

std::int32_t moveToMarker(Ctx& c) { return finish(c, Value{c.env->moveToTimeMarker(textField(c.args, "marker"))}); }

std::int32_t chunkHolders(Ctx& c) {
    return finish(c, textList(c.world.chunkHolders(at(c, "chunk_x"), at(c, "chunk_z"), boolField(c.args, "all"))));
}

std::int32_t breakSpeed(Ctx& c) {
    return finish(c, Value{c.env->breakSpeed(textField(c.args, "player"), at(c, "x"), at(c, "y"), at(c, "z"))});
}

[[nodiscard]] Value box(const std::array<std::int32_t, 6>& bounds) { return intList(bounds); }

std::int32_t structures(Ctx& c) {
    Value::List list;
    for (const PluginStructureStart& start : c.env->structureStarts(at(c, "chunk_x"), at(c, "chunk_z"))) {
        Value::List pieces;
        for (const std::array<std::int32_t, 6>& piece : start.pieces) {
            pieces.push_back(box(piece));
        }
        Value::Map row;
        put(row, "structure", Value{start.structure});
        put(row, "chunk_x", Value{start.chunkX});
        put(row, "chunk_z", Value{start.chunkZ});
        put(row, "bounds", box(start.bounds));
        put(row, "pieces", Value{std::move(pieces)});
        list.emplace_back(std::move(row));
    }
    return finish(c, Value{std::move(list)});
}

std::int32_t locateStructure(Ctx& c) {
    const auto found = c.world.locateStructure(textField(c.args, "structure"), at(c, "x"), at(c, "z"),
                                               static_cast<std::int32_t>(intField(c.args, "cells", 100)),
                                               boolField(c.args, "skip_existing"));
    return found ? finish(c, intList(*found)) : abi::kErrAbsent;
}

std::int32_t locateBiome(Ctx& c) {
    std::vector<std::string> biomes;
    if (const Value* found = c.args.find("biomes"); found != nullptr && found->asList() != nullptr) {
        for (const Value& item : *found->asList()) {
            if (const std::string* text = item.asString()) {
                biomes.push_back(*text);
            }
        }
    }
    const auto found = c.world.locateBiome(biomes, at(c, "x"), at(c, "y"), at(c, "z"),
                                           static_cast<std::int32_t>(intField(c.args, "radius", 6400)),
                                           static_cast<std::int32_t>(intField(c.args, "step", 32)));
    return found ? finish(c, intList(*found)) : abi::kErrAbsent;
}

const std::array kOps{
    Op{"save", false, save},
    Op{"use_item", false, useItem},
    Op{"survives", true, survives},
    Op{"bone_meal", true, boneMeal},
    Op{"break_block", true, breakBlock},
    Op{"drops", true, drops},
    Op{"signal", true, signal},
    Op{"own_signal", true, ownSignal},
    Op{"analog_signal", true, analogSignal},
    Op{"fluid", true, fluid},
    Op{"fluid_flow", true, fluidFlow},
    Op{"highest", false, highest},
    Op{"random_tick", true, randomTick},
    Op{"grow_tree", true, growTree},
    Op{"place_tree", true, placeTree},
    Op{"place_feature", true, placeFeature},
    Op{"feature_ids", true, featureIds},
    Op{"pois", true, pois},
    Op{"climate", true, climate},
    Op{"humidity", true, humidity},
    Op{"temperature", true, temperature},
    Op{"beacon_scan", true, beaconScan},
    Op{"lightning_target", true, lightningTarget},
    Op{"raids", true, raids},
    Op{"raid_control", true, raidControl},
    Op{"raid_bar", true, raidBar},
    Op{"set_raid", true, setRaid},
    Op{"sculk", true, sculk},
    Op{"hive", true, hive},
    Op{"vault", true, vault},
    Op{"creaking_heart", true, creakingHeart},
    Op{"dragon_fight", false, dragonFight},
    Op{"dragon_fight_action", false, dragonFightAction},
    Op{"dragon_fight_bar", false, dragonFightBar},
    Op{"place_end_gateway", false, placeEndGateway},
    Op{"random_sequence", true, randomSequence},
    Op{"inhabited_time", true, inhabitedTime},
    Op{"force_chunk", true, forceChunk},
    Op{"move_to_time_marker", true, moveToMarker},
    Op{"chunk_holders", false, chunkHolders},
    Op{"break_speed", true, breakSpeed},
    Op{"structures", true, structures},
    Op{"locate_structure", false, locateStructure},
    Op{"locate_biome", false, locateBiome},
};

void worldCall(void* user, abi::Call& c) {
    withWorld(user, c, [&](State& state, PluginWorldRef& world) {
        const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
        const std::expected<Value, std::int32_t> args = mapArgument(c.memory(), c.i32(3), c.i32(4));
        if (!name) {
            return abi::kErrInvalid;
        }
        if (*name == "ops") {
            Value::List names;
            for (const Op& op : kOps) {
                names.emplace_back(std::string{op.name});
            }
            return putValue(c.memory(), c.i32(5), c.i32(6), Value{std::move(names)});
        }
        const auto found = std::ranges::find(kOps, *name, &Op::name);
        if (found == kOps.end()) {
            return abi::kErrAbsent;
        }
        if (!args) {
            return args.error();
        }
        Ctx context{state, world, world.env(), *args, Value{}};
        if (found->needsEnv && context.env == nullptr) {
            return abi::kErrAbsent;
        }
        const std::int32_t status = found->run(context);
        return status != abi::kOk ? status : putValue(c.memory(), c.i32(5), c.i32(6), context.out);
    });
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsCallImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    out.push_back({"world_call", worldCall, &state});
}
} // namespace basalt::server::plugins::wasm
