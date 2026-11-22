#include "WasmWorlds.hpp"

#include <basalt/server/plugins/PluginDatafix.hpp>

namespace basalt::server::plugins::wasm::worlds {
namespace {

[[nodiscard]] Value stateValue(const metamorph::datafix::LegacyBlockState& state) {
    Value::Map properties;
    for (const auto& [key, value] : state.properties) {
        put(properties, key, Value{value});
    }
    Value::Map map;
    put(map, "name", Value{state.name});
    put(map, "properties", Value{std::move(properties)});
    return Value{std::move(map)};
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> nbtField(const Value& args) {
    const Value* nbt = field(args, "nbt");
    if (nbt == nullptr || nbt->asBytes() == nullptr) {
        return std::nullopt;
    }
    return nbt->asBytes()->data;
}

[[nodiscard]] std::int32_t answer(abi::Call& c, const datafix::Outcome& outcome) {
    if (!outcome.nbt) {
        return abi::kErrInvalid;
    }
    return putValue(c.memory(), c.i32(4), c.i32(5), bytesValue(*outcome.nbt));
}

void datafixCall(void*, abi::Call& c) {
    const std::optional<std::string_view> op = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> args = mapArgument(c.memory(), c.i32(2), c.i32(3));
    if (!op || !args) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::string type = textField(*args, "type");
    const auto out = [&](Value value) { return putValue(c.memory(), c.i32(4), c.i32(5), std::move(value)); };
    if (*op == "types") {
        c.ret(out(textList(datafix::types())));
    } else if (*op == "legacy_state") {
        c.ret(out(stateValue(datafix::legacyState(static_cast<std::int32_t>(intField(*args, "id"))))));
    } else if (*op == "modern_state") {
        c.ret(out(stateValue(datafix::modernState(static_cast<std::int32_t>(intField(*args, "id"))))));
    } else if (*op == "supports") {
        c.ret(out(Value{datafix::supports(type)}));
    } else if (!datafix::supports(type)) {
        c.ret(abi::kErrAbsent);
    } else if (*op == "steps") {
        Value::List rows;
        for (const datafix::Step& step : datafix::steps(type)) {
            Value::Map row;
            put(row, "version", Value{step.version});
            put(row, "sub_version", Value{step.subVersion});
            put(row, "name", Value{step.name});
            rows.emplace_back(std::move(row));
        }
        c.ret(out(Value{std::move(rows)}));
    } else if (*op == "missing") {
        c.ret(out(Value{datafix::missing(type, static_cast<std::int32_t>(intField(*args, "from")),
                                         static_cast<std::int32_t>(intField(*args, "to")))}));
    } else if (*op == "upgrade" || *op == "step") {
        const std::optional<std::vector<std::uint8_t>> nbt = nbtField(*args);
        if (!nbt) {
            c.ret(abi::kErrInvalid);
        } else if (*op == "upgrade") {
            c.ret(answer(c, datafix::upgrade(type, *nbt, static_cast<std::int32_t>(intField(*args, "from")),
                                             static_cast<std::int32_t>(intField(*args, "to")))));
        } else {
            c.ret(answer(c, datafix::applyStep(type, textField(*args, "name"),
                                               static_cast<std::int32_t>(intField(*args, "version_key")), *nbt)));
        }
    } else {
        c.ret(abi::kErrAbsent);
    }
}
} // namespace
} // namespace basalt::server::plugins::wasm::worlds

namespace basalt::server::plugins::wasm {
void addWorldsDatafixImports(State& state, std::vector<abi::HostImport>& out) {
    out.push_back({"datafix_call", worlds::datafixCall, &state});
}
} // namespace basalt::server::plugins::wasm
