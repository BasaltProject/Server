#include <basalt/server/runtime/plugins/ServerDatapacks.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

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

} // namespace basalt::server::runtime::plugins
