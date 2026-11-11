#pragma once

#include <basalt/server/remote/RemoteHost.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <string>
#include <string_view>

namespace basalt::server::remote::remoteJson {

[[nodiscard]] bool constantTimeEquals(std::string_view a, std::string_view b);

[[nodiscard]] std::string jsonText(const wire::common::JsonObject& object);

[[nodiscard]] std::string errorJson(std::string_view message);

[[nodiscard]] std::string resultJson(const CommandResult& result, const wire::common::JsonValue* id);

} // namespace basalt::server::remote::remoteJson
