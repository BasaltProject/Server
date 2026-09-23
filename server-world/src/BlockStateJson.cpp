#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/BlockStateJson.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <map>
#include <string>

namespace basalt::server::world::blockStateJson {

std::optional<std::int32_t> parse(const wire::common::JsonValue& json) {
    using wire::common::JsonObject;
    using wire::common::JsonValue;
    const std::string* name = json.get<std::string>();
    const JsonObject* propertiesObject = nullptr;
    if (const auto* object = json.get<JsonObject>(); object != nullptr) {
        const JsonValue* id = object->find("id");
        const JsonValue* properties = object->find("properties");
        if (id == nullptr) {
            id = object->find("Name");
            properties = object->find("Properties");
        }
        name = id == nullptr ? nullptr : id->get<std::string>();
        propertiesObject = properties == nullptr ? nullptr : properties->get<JsonObject>();
    }
    if (name == nullptr) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> block =
        wire::common::Identifier::parse(*name);
    if (!block.has_value()) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (propertiesObject != nullptr) {
        for (std::size_t index = 0; index < propertiesObject->size(); ++index) {
            if (const std::string* value = propertiesObject->values()[index].get<std::string>(); value != nullptr) {
                properties.emplace(propertiesObject->names()[index], *value);
            }
        }
    }
    return registry::blocks::stateIdOf(*block, properties);
}

} // namespace basalt::server::world::blockStateJson
