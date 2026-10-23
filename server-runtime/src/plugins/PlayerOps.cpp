#include <basalt/server/plugins/PluginPlayerOps.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

namespace basalt::server::runtime::plugins::playerOps {

const Value& arg(const Args& args, const std::size_t index) {
    static const Value kNone;
    return index < args.size() ? args[index] : kNone;
}

std::int64_t asInt(const Value& value, const std::int64_t fallback) {
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole;
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return static_cast<std::int64_t>(*real);
    }
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag ? 1 : 0;
    }
    return fallback;
}

double asReal(const Value& value, const double fallback) {
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return *real;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

bool asBool(const Value& value, const bool fallback) {
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole != 0;
    }
    return fallback;
}

std::string asText(const Value& value, const std::string_view fallback) {
    const auto* text = std::get_if<std::string>(&value);
    return text == nullptr ? std::string{fallback} : *text;
}

bool isNone(const Value& value) { return std::holds_alternative<std::monostate>(value); }

world::LoadedWorld* worldOf(Context& context) {
    return context.hooks.worldFor ? context.hooks.worldFor(context.session.worldName) : nullptr;
}

std::optional<Value> dispatch(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    const server::plugins::EntityPropertySpec* property = server::plugins::playerPropertyNamed(name);
    if (verb == Verb::Get   ? property == nullptr
        : verb == Verb::Set ? property == nullptr || !property->writable
                            : property == nullptr && !server::plugins::isPlayerOperation(name)) {
        return std::nullopt;
    }
    if (auto found = state(context, verb, name, args); found.has_value()) {
        return found;
    }
    if (auto found = hud(context, verb, name, args); found.has_value()) {
        return found;
    }
    if (auto found = view(context, verb, name, args); found.has_value()) {
        return found;
    }
    return misc(context, verb, name, args);
}

} // namespace basalt::server::runtime::plugins::playerOps
