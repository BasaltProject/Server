#pragma once

#include <basalt/server/remote/ManagementBackend.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::remote::managementMethods {

using wire::common::JsonValue;

enum class FailureKind : std::uint8_t { InvalidParams, InvalidRequest, MethodNotFound, Internal };

struct Failure {
    FailureKind kind = FailureKind::Internal;
    std::string detail;
};

using Outcome = std::expected<JsonValue, Failure>;
using Handler = std::function<Outcome(ManagementBackend&, const JsonValue* param)>;

struct IncomingMethod {
    std::string name;
    std::string description;
    std::string paramName;
    JsonValue paramSchema;
    std::string resultName;
    JsonValue resultSchema;
    bool discoverable = true;
    Handler handler;
};

struct OutgoingMethod {
    std::string name;
    std::string description;
    std::string paramName;
    JsonValue paramSchema;
};

[[nodiscard]] inline Failure invalid(std::string detail) {
    return Failure{FailureKind::InvalidParams, std::move(detail)};
}

[[nodiscard]] const std::vector<IncomingMethod>& incomingMethods();

[[nodiscard]] const std::vector<OutgoingMethod>& outgoingMethods();

[[nodiscard]] const IncomingMethod* findIncoming(std::string_view name);

void addSettingsMethods(std::vector<IncomingMethod>& out);

void addListMethods(std::vector<IncomingMethod>& out);

void addServerMethods(std::vector<IncomingMethod>& out);

} // namespace basalt::server::remote::managementMethods
