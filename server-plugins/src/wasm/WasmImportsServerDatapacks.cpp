#include "WasmServerSupport.hpp"

namespace basalt::server::plugins::wasm::server {
namespace {
constexpr std::array kNames{std::string_view{"datapacks.list"},          std::string_view{"datapacks.refresh"},
                            std::string_view{"datapacks.resource"},      std::string_view{"datapacks.resources"},
                            std::string_view{"datapacks.resource_pack"}, std::string_view{"datapacks.functions"},
                            std::string_view{"datapacks.function_tags"}, std::string_view{"datapacks.scheduled"},
                            std::string_view{"datapacks.schedule"},      std::string_view{"datapacks.unschedule"}};
} // namespace

std::vector<std::string> datapackOperationNames() { return {kNames.begin(), kNames.end()}; }

Value runDatapackOperation(State& state, std::string_view op, const Value& args, std::int32_t& status) {
    PluginDatapacks& packs = state.host->services().datapacks();
    const std::string_view name = textOf(args.find("name"));
    if (op == "datapacks.list") {
        Value::List list;
        for (const PluginDatapack& pack : packs.packs()) {
            list.push_back(mapOf({{"id", Value{pack.id}},
                                  {"enabled", Value{pack.enabled}},
                                  {"required", Value{pack.required}},
                                  {"compatibility", Value{pack.compatibility}},
                                  {"source", Value{pack.source}}}));
        }
        return Value{std::move(list)};
    }
    if (op == "datapacks.refresh") {
        packs.refresh();
        return Value{};
    }
    if (op == "datapacks.resource") {
        std::optional<std::vector<std::uint8_t>> bytes = name.empty() ? std::nullopt : packs.resource(name);
        return bytes ? Value{wire::wasm::Blob{std::move(*bytes)}} : Value{};
    }
    if (op == "datapacks.resources") {
        return listOfText(packs.resourcesUnder(textOf(args.find("prefix"))));
    }
    if (op == "datapacks.resource_pack") {
        const std::string pack = name.empty() ? std::string{} : packs.resourcePack(name);
        return pack.empty() ? Value{} : Value{pack};
    }
    if (op == "datapacks.functions") {
        return listOfText(packs.functionIds());
    }
    if (op == "datapacks.function_tags") {
        return listOfText(packs.functionTagIds());
    }
    if (op == "datapacks.scheduled") {
        Value::List list;
        for (const PluginScheduledFunction& entry : packs.scheduled()) {
            list.push_back(mapOf({{"name", Value{entry.name}}, {"trigger_time", Value{entry.triggerTime}}}));
        }
        return Value{std::move(list)};
    }
    if (op == "datapacks.schedule") {
        const std::string_view function = textOf(args.find("function"));
        const std::optional<std::int64_t> delay = integerOf(args.find("delay"));
        if (function.empty() || !delay) {
            status = abi::kErrInvalid;
            return Value{};
        }
        return Value{packs.schedule(function, *delay, flagOf(args.find("replace")).value_or(false))};
    }
    if (op == "datapacks.unschedule") {
        if (name.empty()) {
            status = abi::kErrInvalid;
            return Value{};
        }
        return Value{static_cast<std::int64_t>(packs.unschedule(name))};
    }
    status = abi::kErrInvalid;
    return Value{};
}
} // namespace basalt::server::plugins::wasm::server

namespace basalt::server::plugins::wasm {
void addServerDatapacksImports(State&, std::vector<abi::HostImport>&) {}
} // namespace basalt::server::plugins::wasm
