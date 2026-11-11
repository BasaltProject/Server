#pragma once

#include <basalt/wire/common/json/Json.hpp>

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::remote::managementSchema {

using wire::common::JsonValue;

[[nodiscard]] JsonValue type(std::string_view name);
[[nodiscard]] JsonValue types(std::initializer_list<std::string_view> names);
[[nodiscard]] JsonValue ref(std::string_view component);
[[nodiscard]] JsonValue array(JsonValue items);
[[nodiscard]] JsonValue enumOf(std::initializer_list<std::string_view> values);
[[nodiscard]] JsonValue record(std::initializer_list<std::pair<std::string_view, JsonValue>> properties);

/// The OpenRPC document `rpc.discover` answers with.
[[nodiscard]] JsonValue discoverDocument();

} // namespace basalt::server::remote::managementSchema
