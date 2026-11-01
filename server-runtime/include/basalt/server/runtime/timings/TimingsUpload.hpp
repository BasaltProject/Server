#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::runtime::timings {

struct UploadedReport {
    std::string key;
    std::string url;
};

/// Sends a finished report to the viewer at `baseUrl` and returns where it can be read. Blocks on the network, so
/// callers keep it off the tick thread.
[[nodiscard]] std::expected<UploadedReport, std::string> uploadReport(std::string_view baseUrl, std::string_view json,
                                                                      std::string_view trustPem = {});

} // namespace basalt::server::runtime::timings
