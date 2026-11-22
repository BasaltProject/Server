#include "WasmWorlds.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace basalt::server::plugins::wasm::worlds {
namespace {
struct Query {
    PluginWorlds& worlds;
    const Value& args;
    Value out;
};

using Table = std::int32_t (*)(Query&);

struct Entry {
    std::string_view name;
    Table run;
};

[[nodiscard]] std::int32_t finish(Query& q, Value value) {
    q.out = std::move(value);
    return abi::kOk;
}

[[nodiscard]] std::int32_t stateArg(const Query& q) { return static_cast<std::int32_t>(intField(q.args, "state", -1)); }

[[nodiscard]] Value reals(const std::vector<double>& values) {
    Value::List list;
    for (const double value : values) {
        list.emplace_back(value);
    }
    return Value{std::move(list)};
}

[[nodiscard]] std::vector<std::string> split(std::string_view text) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('|', start);
        parts.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

[[nodiscard]] Value pairRows(const std::vector<std::string>& flat, std::string_view first, std::string_view second,
                             bool secondIsList) {
    Value::List rows;
    for (std::size_t at = 0; at + 1 < flat.size(); at += 2) {
        Value::Map row;
        put(row, std::string{first}, Value{flat[at]});
        put(row, std::string{second}, secondIsList ? textList(split(flat[at + 1])) : Value{flat[at + 1]});
        rows.emplace_back(std::move(row));
    }
    return Value{std::move(rows)};
}

std::int32_t blocks(Query& q) {
    const std::vector<std::string> flat = q.worlds.blockTable();
    Value::List rows;
    for (std::size_t at = 0; at + 2 < flat.size(); at += 3) {
        Value::Map row;
        put(row, "id", Value{flat[at]});
        put(row, "first_state", Value{static_cast<std::int64_t>(std::atoll(flat[at + 1].c_str()))});
        put(row, "state_count", Value{static_cast<std::int64_t>(std::atoll(flat[at + 2].c_str()))});
        rows.emplace_back(std::move(row));
    }
    return finish(q, Value{std::move(rows)});
}

std::int32_t blockProperties(Query& q) {
    return finish(q, pairRows(q.worlds.blockProperties(textField(q.args, "block")), "name", "values", true));
}

std::int32_t stateFlags(Query& q) { return finish(q, Value{q.worlds.stateFlags(stateArg(q))}); }

std::int32_t blockHardness(Query& q) {
    const std::vector<double> facts = q.worlds.blockHardness(textField(q.args, "block"));
    if (facts.size() < 3) {
        return abi::kErrAbsent;
    }
    Value::Map row;
    put(row, "destroy_time", Value{facts[0]});
    put(row, "explosion_resistance", Value{facts[1]});
    put(row, "requires_tool", Value{facts[2] != 0.0});
    return finish(q, Value{std::move(row)});
}

std::int32_t blockInfo(Query& q) { return finish(q, reals(q.worlds.blockInfo(stateArg(q)))); }

std::int32_t blockShape(Query& q) {
    const std::vector<std::int32_t> boxes = q.worlds.collisionBoxes(stateArg(q));
    return finish(q, intList(boxes));
}

std::int32_t blockOutline(Query& q) {
    return finish(q, reals(q.worlds.outlineBoxes(stateArg(q), static_cast<std::int32_t>(intField(q.args, "x")),
                                                 static_cast<std::int32_t>(intField(q.args, "z")))));
}

std::int32_t blockVisual(Query& q) { return finish(q, reals(q.worlds.visualBoxes(stateArg(q)))); }

std::int32_t blockEntityTypes(Query& q) {
    return finish(q, pairRows(q.worlds.blockEntityTypes(), "block", "type", false));
}

std::int32_t blockSounds(Query& q) { return finish(q, textList(q.worlds.blockSounds(stateArg(q)))); }

std::int32_t blockInstrument(Query& q) { return finish(q, Value{q.worlds.blockInstrument(stateArg(q))}); }

std::int32_t toolFacts(Query& q) {
    return finish(q, reals(q.worlds.toolFacts(textField(q.args, "item"), stateArg(q))));
}

std::int32_t transformState(Query& q) {
    return finish(q, Value{q.worlds.transformState(stateArg(q), static_cast<std::int32_t>(intField(q.args, "rotation")),
                                                   static_cast<std::int32_t>(intField(q.args, "mirror")))});
}

std::int32_t poiType(Query& q) { return finish(q, Value{q.worlds.poiTypeOf(stateArg(q))}); }

std::int32_t poiTickets(Query& q) { return finish(q, Value{q.worlds.poiTickets(textField(q.args, "type"))}); }

std::int32_t gameRules(Query& q) {
    const std::vector<std::string> flat = q.worlds.gameRuleSpecs();
    Value::List rows;
    for (std::size_t at = 0; at + 2 < flat.size(); at += 3) {
        Value::Map row;
        put(row, "id", Value{flat[at]});
        put(row, "type", Value{flat[at + 1]});
        put(row, "default", Value{flat[at + 2]});
        rows.emplace_back(std::move(row));
    }
    return finish(q, Value{std::move(rows)});
}

std::int32_t gameEventFrequency(Query& q) {
    return finish(q, Value{q.worlds.gameEventFrequency(textField(q.args, "event"))});
}

std::int32_t dataVersion(Query& q) { return finish(q, Value{q.worlds.dataVersion()}); }

std::int32_t fixNbt(Query& q) {
    const Value* nbt = q.args.find("nbt");
    const wire::wasm::Blob* blob = nbt == nullptr ? nullptr : nbt->asBytes();
    if (blob == nullptr) {
        return abi::kErrInvalid;
    }
    const auto fixed = q.worlds.fixNbt(textField(q.args, "kind"), blob->data,
                                       static_cast<std::int32_t>(intField(q.args, "from_version")));
    return fixed ? finish(q, bytesValue(*fixed)) : abi::kErrAbsent;
}

std::int32_t fixName(Query& q) {
    const auto fixed = q.worlds.fixName(textField(q.args, "kind"), textField(q.args, "text"),
                                        static_cast<std::int32_t>(intField(q.args, "from_version")));
    return fixed ? finish(q, Value{*fixed}) : abi::kErrAbsent;
}

std::int32_t biomes(Query& q) { return finish(q, textList(q.worlds.biomeIds())); }

std::int32_t items(Query& q) { return finish(q, textList(q.worlds.itemIds())); }

std::int32_t entityTypes(Query& q) { return finish(q, textList(q.worlds.entityTypeIds())); }

std::int32_t tags(Query& q) {
    return finish(q, pairRows(q.worlds.tags(textField(q.args, "registry")), "tag", "members", true));
}

std::int32_t registryIds(Query& q) { return finish(q, textList(q.worlds.registryIds(textField(q.args, "registry")))); }

std::int32_t registryData(Query& q) {
    const auto data = q.worlds.registryData(textField(q.args, "registry"), textField(q.args, "id"));
    return data ? finish(q, bytesValue(*data)) : abi::kErrAbsent;
}

const std::array kTables{
    Entry{"blocks", blocks},
    Entry{"block_properties", blockProperties},
    Entry{"state_flags", stateFlags},
    Entry{"block_hardness", blockHardness},
    Entry{"block_info", blockInfo},
    Entry{"block_shape", blockShape},
    Entry{"block_outline", blockOutline},
    Entry{"block_visual", blockVisual},
    Entry{"block_entity_types", blockEntityTypes},
    Entry{"block_sounds", blockSounds},
    Entry{"block_instrument", blockInstrument},
    Entry{"tool_facts", toolFacts},
    Entry{"transform_state", transformState},
    Entry{"poi_type", poiType},
    Entry{"poi_tickets", poiTickets},
    Entry{"game_rules", gameRules},
    Entry{"game_event_frequency", gameEventFrequency},
    Entry{"data_version", dataVersion},
    Entry{"fix_nbt", fixNbt},
    Entry{"fix_name", fixName},
    Entry{"biomes", biomes},
    Entry{"items", items},
    Entry{"entity_types", entityTypes},
    Entry{"tags", tags},
    Entry{"registry", registryIds},
    Entry{"registry_data", registryData},
};

void worldsData(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (*name == "tables") {
        Value::List names;
        for (const Entry& entry : kTables) {
            names.emplace_back(std::string{entry.name});
        }
        c.ret(putValue(c.memory(), c.i32(4), c.i32(5), Value{std::move(names)}));
        return;
    }
    const auto found = std::ranges::find(kTables, *name, &Entry::name);
    if (found == kTables.end()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Value args{Value::Map{}};
    if (c.i32(3) != 0) {
        std::expected<Value, std::int32_t> read = readValue(c.memory(), c.i32(2), c.i32(3));
        if (!read || (!read->isNull() && read->asMap() == nullptr)) {
            c.ret(abi::kErrInvalid);
            return;
        }
        if (!read->isNull()) {
            args = std::move(*read);
        }
    }
    Query query{state.host->services().worlds(), args, Value{}};
    const std::int32_t status = found->run(query);
    c.ret(status != abi::kOk ? status : putValue(c.memory(), c.i32(4), c.i32(5), query.out));
}

void stateId(void* user, abi::Call& c) {
    const std::optional<std::string_view> text = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t id = text ? stateOf(user).host->services().worlds().stateId(*text) : -1;
    c.ret(id < 0 ? abi::kErrAbsent : id);
}

void stateText(void* user, abi::Call& c) {
    const std::string text = stateOf(user).host->services().worlds().stateText(c.i32(0));
    c.ret(text.empty() ? abi::kErrAbsent : c.memory().put(c.i32(1), c.i32(2), text));
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsDataImports(State& state, std::vector<abi::HostImport>& out) {
    using namespace worlds;
    void* user = &state;
    out.insert(out.end(), {
                              {"worlds_data", worldsData, user},
                              {"block_state_id", stateId, user},
                              {"block_state_text", stateText, user},
                          });
}
} // namespace basalt::server::plugins::wasm
