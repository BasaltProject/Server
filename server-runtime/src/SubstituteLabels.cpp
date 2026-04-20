#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/SubstituteLabels.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace reg = server::registry;

[[nodiscard]] std::string titleCase(std::string_view path) {
    std::string title;
    bool starting = true;
    for (const char c : path) {
        if (c == '_') {
            if (!title.empty()) {
                title += ' ';
            }
            starting = true;
            continue;
        }
        title += starting ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        starting = false;
    }
    return title;
}

} // namespace

SubstituteLabels::SubstituteLabels(std::string version) : version_(std::move(version)) {}

SubstituteLabels::SubstituteLabels() : SubstituteLabels(std::string{buildInfo::kMinecraftVersion}) {}

std::optional<wire::text::Component> SubstituteLabels::itemLabel(std::int32_t nativeItemId) const {
    const reg::Item* item = reg::items::registry().byId(nativeItemId);
    if (item == nullptr) {
        return std::nullopt;
    }
    return wire::text::Component::text(titleCase(item->id.path()) + " (" + version_ + ")");
}

std::optional<wire::text::Component> SubstituteLabels::entityLabel(std::int32_t nativeEntityTypeId) const {
    const reg::EntityTypeEntry* type = reg::entityTypes::byProtocolId(nativeEntityTypeId);
    if (type == nullptr) {
        return std::nullopt;
    }
    return wire::text::Component::text(titleCase(type->id.path()) + " (" + version_ + ")");
}

} // namespace basalt::server::runtime
