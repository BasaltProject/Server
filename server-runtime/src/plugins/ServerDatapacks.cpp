#include <basalt/server/runtime/plugins/ServerDatapacks.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins {
namespace {

[[nodiscard]] std::string toJson(const wire::text::Component& component) {
    return wire::common::writeJson(wire::text::componentJson::serialize(component));
}

[[nodiscard]] std::string compatibilityName(datapack::Compatibility compatibility) {
    switch (compatibility) {
    case datapack::Compatibility::TooOld:
        return "too_old";
    case datapack::Compatibility::TooNew:
        return "too_new";
    case datapack::Compatibility::Unknown:
        return "unknown";
    case datapack::Compatibility::Compatible:
    default:
        return "compatible";
    }
}

[[nodiscard]] std::string sourceName(datapack::PackSource source) {
    switch (source) {
    case datapack::PackSource::BuiltIn:
        return "built_in";
    case datapack::PackSource::Feature:
        return "feature";
    case datapack::PackSource::World:
    default:
        return "world";
    }
}

} // namespace

void ServerDatapacks::refresh() {
    if (datapack::DataPacks* packs = packs_ ? packs_() : nullptr; packs != nullptr) {
        static_cast<void>(packs->discoverNewPacks());
    }
}

bool ServerDatapacks::reload() {
    datapack::DataPacks* packs = packs_ ? packs_() : nullptr;
    if (packs == nullptr || !reload_) {
        return false;
    }
    return reload_(packs->discoverNewPacks());
}

std::vector<server::plugins::PluginDatapack> ServerDatapacks::packs() {
    std::vector<server::plugins::PluginDatapack> out;
    datapack::DataPacks* packs = packs_ ? packs_() : nullptr;
    if (packs == nullptr) {
        return out;
    }
    for (const datapack::Pack* pack : std::as_const(packs->repository()).availablePacks()) {
        server::plugins::PluginDatapack one;
        one.id = pack->id;
        one.title = toJson(pack->title);
        one.description = toJson(pack->description);
        one.required = pack->required;
        one.enabled = packs->repository().isSelected(pack->id);
        one.compatibility = compatibilityName(pack->compatibility);
        one.source = sourceName(pack->source);
        for (const wire::common::Identifier& feature : pack->requestedFeatures) {
            one.features.push_back(feature.toString());
        }
        out.push_back(std::move(one));
    }
    return out;
}

std::vector<std::string> ServerDatapacks::functionIds() {
    std::vector<std::string> out;
    if (const function::FunctionLibrary* library = functions_ ? functions_() : nullptr; library != nullptr) {
        for (const wire::common::Identifier& id : library->functionIds()) {
            out.push_back(id.toString());
        }
    }
    return out;
}

std::vector<std::string> ServerDatapacks::functionTagIds() {
    std::vector<std::string> out;
    if (const function::FunctionLibrary* library = functions_ ? functions_() : nullptr; library != nullptr) {
        for (const wire::common::Identifier& id : library->tagIds()) {
            out.push_back(id.toString());
        }
    }
    return out;
}

std::vector<server::plugins::PluginScheduledFunction> ServerDatapacks::scheduled() {
    std::vector<server::plugins::PluginScheduledFunction> out;
    if (schedule_ == nullptr) {
        return out;
    }
    std::vector<command::commands::FunctionSchedule::Event> events = schedule_->events();
    std::ranges::stable_sort(events, {}, &command::commands::FunctionSchedule::Event::triggerTime);
    for (const command::commands::FunctionSchedule::Event& event : events) {
        out.push_back({.name = event.name, .triggerTime = event.triggerTime});
    }
    return out;
}

bool ServerDatapacks::schedule(const std::string_view function, const std::int64_t delay, const bool replace) {
    const function::FunctionLibrary* library = functions_ ? functions_() : nullptr;
    if (schedule_ == nullptr || library == nullptr || !gameTime_ || delay == 0) {
        return false;
    }
    const bool isTag = function.starts_with('#');
    const auto id = wire::common::Identifier::parse(isTag ? function.substr(1) : function);
    if (!id.has_value()) {
        return false;
    }
    if (isTag ? !std::ranges::contains(library->tagIds(), *id) : !std::ranges::contains(library->functionIds(), *id)) {
        return false;
    }
    if (replace) {
        static_cast<void>(schedule_->remove(function));
    }
    schedule_->schedule(*id, isTag, gameTime_() + delay);
    return true;
}

std::int32_t ServerDatapacks::unschedule(const std::string_view name) {
    return schedule_ == nullptr ? 0 : schedule_->remove(name);
}

std::optional<std::vector<std::uint8_t>> ServerDatapacks::resource(const std::string_view name) {
    datapack::DataPacks* packs = packs_ ? packs_() : nullptr;
    const auto stack = packs == nullptr ? nullptr : packs->resources();
    return stack == nullptr ? std::nullopt : stack->read(name);
}

std::vector<std::string> ServerDatapacks::resourcesUnder(const std::string_view prefix) {
    datapack::DataPacks* packs = packs_ ? packs_() : nullptr;
    const auto stack = packs == nullptr ? nullptr : packs->resources();
    return stack == nullptr ? std::vector<std::string>{} : stack->entriesUnder(prefix);
}

std::string ServerDatapacks::resourcePack(const std::string_view name) {
    datapack::DataPacks* packs = packs_ ? packs_() : nullptr;
    const auto stack = packs == nullptr ? nullptr : packs->resources();
    return stack == nullptr ? std::string{} : stack->packOf(name).value_or(std::string{});
}

} // namespace basalt::server::runtime::plugins
