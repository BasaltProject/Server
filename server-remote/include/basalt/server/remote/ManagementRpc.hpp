#pragma once

#include <basalt/server/remote/ManagementBackend.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <string>
#include <string_view>

namespace basalt::server::remote::managementRpc {

/// True for a JSON-RPC message: an object that carries "jsonrpc", or a batch.
[[nodiscard]] bool isRpc(const wire::common::JsonValue& message);

/// Answers one request or one batch on the server thread; empty when the message needs no answer.
[[nodiscard]] std::string handle(ManagementBackend& backend, const wire::common::JsonValue& message);

[[nodiscard]] std::string parseErrorFrame(std::string_view detail);

[[nodiscard]] std::string internalErrorFrame(std::string_view detail);

[[nodiscard]] std::string notificationFrame(std::string_view method, const wire::common::JsonValue* param);

} // namespace basalt::server::remote::managementRpc
