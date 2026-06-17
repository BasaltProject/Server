#pragma once

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::plugins::playerOps {

using Value = server::plugins::PluginEntityValue;
using Args = std::vector<Value>;

enum class Verb : std::uint8_t { Get, Set, Call };

struct Context {
    player::PlayerSession& session;
    player::PlayerRegistry& registry;
    const ModelHooks& hooks;
};

[[nodiscard]] const Value& arg(const Args& args, std::size_t index);
[[nodiscard]] std::int64_t asInt(const Value& value, std::int64_t fallback = 0);
[[nodiscard]] double asReal(const Value& value, double fallback = 0.0);
[[nodiscard]] bool asBool(const Value& value, bool fallback = false);
[[nodiscard]] std::string asText(const Value& value, std::string_view fallback = {});
[[nodiscard]] bool isNone(const Value& value);

[[nodiscard]] world::LoadedWorld* worldOf(Context& context);

[[nodiscard]] std::optional<Value> state(Context& context, Verb verb, std::string_view name, const Args& args);
[[nodiscard]] std::optional<Value> hud(Context& context, Verb verb, std::string_view name, const Args& args);
[[nodiscard]] std::optional<Value> view(Context& context, Verb verb, std::string_view name, const Args& args);
[[nodiscard]] std::optional<Value> misc(Context& context, Verb verb, std::string_view name, const Args& args);

[[nodiscard]] std::optional<Value> dispatch(Context& context, Verb verb, std::string_view name, const Args& args);

} // namespace basalt::server::runtime::plugins::playerOps
