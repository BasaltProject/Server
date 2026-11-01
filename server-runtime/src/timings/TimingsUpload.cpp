#include <basalt/server/runtime/timings/TimingsUpload.hpp>
#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <span>

namespace basalt::server::runtime::timings {
namespace {

[[nodiscard]] std::string trimmed(std::string_view url) {
    while (!url.empty() && url.back() == '/') {
        url.remove_suffix(1);
    }
    return std::string{url};
}

[[nodiscard]] std::string field(const wire::common::JsonObject& object, std::string_view name) {
    const wire::common::JsonValue* value = object.find(name);
    const std::string* text = value == nullptr ? nullptr : value->get<std::string>();
    return text == nullptr ? std::string{} : *text;
}

} // namespace

std::expected<UploadedReport, std::string> uploadReport(std::string_view baseUrl, std::string_view json,
                                                        std::string_view trustPem) {
    const std::string base = trimmed(baseUrl);
    if (!base.starts_with("https://")) {
        return std::unexpected{"the timings viewer address must start with https://"};
    }
    const std::span<const std::uint8_t> body{reinterpret_cast<const std::uint8_t*>(json.data()), json.size()};
    // The client appends one CRLF after these, so the last header carries its own to leave the blank line.
    const auto reply = wire::auth::httpsPut(base + "/api/timings", body,
                                            "Content-Type: application/json\r\nAccept: application/json\r\n",
                                            wire::auth::HttpsOptions{.trustPem = trustPem});
    if (!reply.has_value()) {
        return std::unexpected{reply.error().message};
    }
    if (!reply->ok()) {
        return std::unexpected{"the viewer answered " + std::to_string(reply->status)};
    }
    const auto parsed = wire::common::parseJson(reply->text());
    const wire::common::JsonObject* object = parsed.has_value() ? parsed->get<wire::common::JsonObject>() : nullptr;
    if (object == nullptr) {
        return std::unexpected{"the viewer's answer was not understood"};
    }
    UploadedReport report;
    report.key = field(*object, "key");
    report.url = field(*object, "url");
    if (report.key.empty()) {
        return std::unexpected{"the viewer did not hand back a key"};
    }
    if (report.url.empty()) {
        report.url = base + "/timings/" + report.key;
    }
    return report;
}

} // namespace basalt::server::runtime::timings
