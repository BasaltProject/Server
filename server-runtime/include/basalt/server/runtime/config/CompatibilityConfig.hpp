#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

struct BackwardsSelection {
    bool all = true;
    std::set<std::int32_t> protocols;
};

struct BedrockSettings {
    std::optional<std::set<std::int32_t>> protocols;

    wire::bedrock::BedrockFuturePolicy futureProtocols = wire::bedrock::BedrockFuturePolicy::Warn;
};

class CompatibilityConfig {
public:
    static constexpr std::string_view kFileName = "server.toml";

    CompatibilityConfig(BackwardsSelection backwards, wire::multiversion::UnknownProtocolPolicy futureProtocols,
                        wire::multiversion::UnknownProtocolPolicy snapshots, bool nameSubstitutes = true,
                        BedrockSettings bedrock = {})
        : backwards_(std::move(backwards)), futureProtocols_(futureProtocols), snapshots_(snapshots),
          nameSubstitutes_(nameSubstitutes), bedrock_(std::move(bedrock)) {}

    [[nodiscard]] static std::expected<CompatibilityConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const BackwardsSelection& backwards() const noexcept { return backwards_; }

    [[nodiscard]] wire::multiversion::UnknownProtocolPolicy futureProtocols() const noexcept {
        return futureProtocols_;
    }

    [[nodiscard]] wire::multiversion::UnknownProtocolPolicy snapshots() const noexcept { return snapshots_; }

    [[nodiscard]] bool nameSubstitutes() const noexcept { return nameSubstitutes_; }

    [[nodiscard]] const BedrockSettings& bedrock() const noexcept { return bedrock_; }

    [[nodiscard]] wire::multiversion::ProtocolGate gate(const std::set<std::int32_t>& translatable,
                                                        const wire::common::Logger& logger = wire::common::Logger{
                                                            "Compatibility"}) const;

private:
    BackwardsSelection backwards_;
    wire::multiversion::UnknownProtocolPolicy futureProtocols_;
    wire::multiversion::UnknownProtocolPolicy snapshots_;
    bool nameSubstitutes_ = true;
    BedrockSettings bedrock_;
};

} // namespace basalt::server::runtime::config
