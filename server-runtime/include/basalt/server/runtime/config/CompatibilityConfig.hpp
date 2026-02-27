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

/// Which older versions to accept: everything this build can translate, or a written list.
///
/// The list being absent is "all", which is not the same as an empty list: an operator who wrote
/// `backwards = []` said only the native version, and meant it.
struct BackwardsSelection {
    bool all = true;
    std::set<std::int32_t> protocols;
};

/// The Bedrock half of the same file. A server accepts both editions out of the box, so every
/// value here has an answer that needs no editing.
/// What versions of the other edition may in. Which port it listens on and what a Bedrock player
/// is called live in `server.toml`, under `[network.bedrock]`: those are the door, and this file is
/// only about who is old enough to come through it.
struct BedrockSettings {
    /// Which Bedrock protocols may in. Absent means all of them this build speaks, which is what
    /// makes updating the server add versions without editing this file.
    std::optional<std::set<std::int32_t>> protocols;

    /// What happens to a client newer than anything this build speaks.
    wire::bedrock::BedrockFuturePolicy futureProtocols = wire::bedrock::BedrockFuturePolicy::Warn;
};

/// How old a client of either edition may be, read from `config/server.toml`.
///
/// It lived in a file of its own while Bedrock was something bolted on the side. It is not: the two
/// editions are two doors into the same server, so each one's port and each one's answer to this
/// question sit together, under `[network.java]` and `[network.bedrock]`.
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

    /// Whether what stands in for content a client is too old for says what it really is.
    [[nodiscard]] bool nameSubstitutes() const noexcept { return nameSubstitutes_; }

    [[nodiscard]] const BedrockSettings& bedrock() const noexcept { return bedrock_; }

    /// The door, built from what this build can actually translate.
    ///
    /// `translatable` is what the chain of translators covers, so `all` grows on its own when a
    /// link is added and an operator who wrote a list gets exactly the list, minus anything this
    /// build cannot do, which is said out loud rather than accepted and then failed at.
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
