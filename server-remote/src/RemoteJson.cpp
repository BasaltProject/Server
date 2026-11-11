#include "RemoteJson.hpp"

#include <algorithm>

namespace basalt::server::remote::remoteJson {

using wire::common::JsonObject;
using wire::common::JsonValue;

bool constantTimeEquals(std::string_view a, std::string_view b) {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t length = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const auto left = static_cast<unsigned char>(i < a.size() ? a[i] : 0);
        const auto right = static_cast<unsigned char>(i < b.size() ? b[i] : 0);
        diff = static_cast<unsigned char>(diff | (left ^ right));
    }
    return diff == 0;
}

std::string jsonText(const JsonObject& object) { return wire::common::writeJson(JsonValue::ofObject(object)); }

std::string errorJson(std::string_view message) {
    JsonObject object;
    object.set("error", JsonValue::of(std::string{message}));
    return jsonText(object);
}

std::string resultJson(const CommandResult& result, const JsonValue* id) {
    JsonObject object;
    if (id != nullptr) {
        object.set("type", JsonValue::of(std::string{"result"}));
        object.set("id", *id);
    }
    object.set("cancelled", JsonValue::of(result.cancelled));
    object.set("output", JsonValue::of(result.output));
    return jsonText(object);
}

} // namespace basalt::server::remote::remoteJson
