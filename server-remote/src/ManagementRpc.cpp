#include "ManagementMethods.hpp"

#include <basalt/server/remote/ManagementRpc.hpp>

#include <algorithm>
#include <optional>

namespace basalt::server::remote::managementRpc {
namespace {

using managementMethods::Failure;
using managementMethods::FailureKind;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::int64_t kParseError = -32700;
constexpr std::int64_t kInvalidRequest = -32600;
constexpr std::int64_t kMethodNotFound = -32601;
constexpr std::int64_t kInvalidParams = -32602;
constexpr std::int64_t kInternalError = -32603;
constexpr std::string_view kNamespace = "minecraft";

[[nodiscard]] JsonValue text(std::string_view value) { return JsonValue::of(std::string{value}); }

[[nodiscard]] JsonValue errorObject(const JsonValue& id, const std::int64_t code, std::string_view message,
                                    std::string_view data) {
    JsonObject error;
    error.set("code", JsonValue::of(code));
    error.set("message", text(message));
    if (std::ranges::any_of(data, [](char c) { return c != ' ' && c != '\t' && c != '\n' && c != '\r'; })) {
        error.set("data", text(data));
    }
    JsonObject object;
    object.set("jsonrpc", text("2.0"));
    object.set("id", id);
    object.set("error", JsonValue::ofObject(std::move(error)));
    return JsonValue::ofObject(std::move(object));
}

[[nodiscard]] JsonValue failureObject(const JsonValue& id, const Failure& failure) {
    switch (failure.kind) {
    case FailureKind::InvalidParams:
        return errorObject(id, kInvalidParams, "Invalid params", failure.detail);
    case FailureKind::InvalidRequest:
        return errorObject(id, kInvalidRequest, "Invalid Request", failure.detail);
    case FailureKind::MethodNotFound:
        return errorObject(id, kMethodNotFound, "Method not found", failure.detail);
    case FailureKind::Internal:
        break;
    }
    return errorObject(id, kInternalError, "Internal error", failure.detail);
}

[[nodiscard]] bool validIdentifierPart(std::string_view part, bool path) {
    return std::ranges::all_of(part, [path](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
               (path && c == '/');
    });
}

/// parity: `Identifier.tryParse` plus the registry lookup, where a bare path lives in `minecraft`.
[[nodiscard]] std::expected<std::string, Failure> methodKey(std::string_view method) {
    std::string_view space = kNamespace;
    std::string_view path = method;
    if (const std::size_t colon = method.find(':'); colon != std::string_view::npos) {
        space = method.substr(0, colon);
        path = method.substr(colon + 1);
    }
    if (path.empty() || !validIdentifierPart(space, false) || !validIdentifierPart(path, true)) {
        return std::unexpected{
            Failure{FailureKind::InvalidRequest, "Failed to parse method value: " + std::string{method}}};
    }
    if (space != kNamespace) {
        return std::unexpected{Failure{FailureKind::MethodNotFound, "Method not found: " + std::string{method}}};
    }
    return std::string{path};
}

[[nodiscard]] std::expected<const JsonValue*, Failure> argumentOf(const managementMethods::IncomingMethod& method,
                                                                  const JsonValue* params) {
    const auto* list = params == nullptr ? nullptr : params->get<std::vector<JsonValue>>();
    const auto* named = params == nullptr ? nullptr : params->get<JsonObject>();
    if (method.paramName.empty()) {
        if (params == nullptr || (list != nullptr && list->empty())) {
            return nullptr;
        }
        return std::unexpected{managementMethods::invalid("Expected no params, or an empty array")};
    }
    if (list == nullptr && named == nullptr) {
        return std::unexpected{managementMethods::invalid("Expected params as array or named")};
    }
    if (named != nullptr) {
        const JsonValue* value = named->find(method.paramName);
        if (value == nullptr) {
            return std::unexpected{managementMethods::invalid("Params passed by-name, but expected param [" +
                                                              method.paramName + "] does not exist")};
        }
        return value;
    }
    if (list->size() != 1) {
        return std::unexpected{managementMethods::invalid("Expected exactly one element in the params array")};
    }
    return &list->front();
}

[[nodiscard]] std::optional<JsonValue> handleRequest(ManagementBackend& backend, const JsonValue* id,
                                                     std::string_view method, const JsonValue* params) {
    const bool respond = id != nullptr;
    const auto fail = [&](const Failure& failure) -> std::optional<JsonValue> {
        return respond ? std::optional{failureObject(*id, failure)} : std::nullopt;
    };
    const auto key = methodKey(method);
    if (!key.has_value()) {
        return fail(key.error());
    }
    const managementMethods::IncomingMethod* found = managementMethods::findIncoming(*key);
    if (found == nullptr) {
        return fail(Failure{FailureKind::MethodNotFound, "Method not found: " + std::string{method}});
    }
    const auto argument = argumentOf(*found, params);
    if (!argument.has_value()) {
        return fail(argument.error());
    }
    const managementMethods::Outcome outcome = found->handler(backend, *argument);
    if (!outcome.has_value()) {
        return fail(outcome.error());
    }
    if (!respond) {
        return std::nullopt;
    }
    JsonObject object;
    object.set("jsonrpc", text("2.0"));
    object.set("id", *id);
    object.set("result", *outcome);
    return JsonValue::ofObject(std::move(object));
}

[[nodiscard]] std::optional<JsonValue> handleObject(ManagementBackend& backend, const JsonObject& request) {
    const JsonValue* id = request.find("id");
    const JsonValue* method = request.find("method");
    const JsonValue* result = request.find("result");
    const JsonValue* error = request.find("error");
    const JsonValue unknown;
    if (method != nullptr && result == nullptr && error == nullptr) {
        const bool validId = id == nullptr || id->isNull() || id->get<std::int64_t>() != nullptr ||
                             id->get<double>() != nullptr || id->get<std::string>() != nullptr;
        if (!validId) {
            return errorObject(unknown, kInvalidRequest, "Invalid Request",
                               "Invalid request id - only String, Number and NULL supported");
        }
        const auto* name = method->get<std::string>();
        if (name == nullptr) {
            return errorObject(unknown, kInternalError, "Internal error",
                               "Unknown error handling request - check server logs for stack trace");
        }
        return handleRequest(backend, id, *name, request.find("params"));
    }
    if ((method == nullptr && result != nullptr && error == nullptr && id != nullptr) ||
        (method == nullptr && result == nullptr && error != nullptr)) {
        return std::nullopt;
    }
    return errorObject(id == nullptr ? unknown : *id, kInvalidRequest, "Invalid Request", "");
}

} // namespace

bool isRpc(const JsonValue& message) {
    if (message.get<std::vector<JsonValue>>() != nullptr) {
        return true;
    }
    const auto* object = message.get<JsonObject>();
    return object != nullptr && object->find("jsonrpc") != nullptr;
}

std::string handle(ManagementBackend& backend, const JsonValue& message) {
    if (const auto* batch = message.get<std::vector<JsonValue>>(); batch != nullptr) {
        std::vector<JsonValue> answers;
        for (const JsonValue& entry : *batch) {
            const auto* object = entry.get<JsonObject>();
            if (object == nullptr) {
                answers.push_back(errorObject(JsonValue{}, kInvalidRequest, "Invalid Request", ""));
            } else if (std::optional<JsonValue> answer = handleObject(backend, *object); answer.has_value()) {
                answers.push_back(std::move(*answer));
            }
        }
        return wire::common::writeJson(JsonValue::ofArray(std::move(answers)));
    }
    const auto* object = message.get<JsonObject>();
    if (object == nullptr) {
        return wire::common::writeJson(errorObject(JsonValue{}, kInvalidRequest, "Invalid Request", ""));
    }
    const std::optional<JsonValue> answer = handleObject(backend, *object);
    return answer.has_value() ? wire::common::writeJson(*answer) : std::string{};
}

std::string parseErrorFrame(const std::string_view detail) {
    return wire::common::writeJson(errorObject(JsonValue{}, kParseError, "Parse error", detail));
}

std::string internalErrorFrame(const std::string_view detail) {
    return wire::common::writeJson(errorObject(JsonValue{}, kInternalError, "Internal error", detail));
}

std::string notificationFrame(const std::string_view method, const JsonValue* param) {
    JsonObject object;
    object.set("jsonrpc", text("2.0"));
    object.set("method", text(std::string{kNamespace} + ":" + std::string{method}));
    if (param != nullptr) {
        object.set("params", JsonValue::ofArray(std::vector<JsonValue>{*param}));
    }
    return wire::common::writeJson(JsonValue::ofObject(std::move(object)));
}

} // namespace basalt::server::remote::managementRpc
