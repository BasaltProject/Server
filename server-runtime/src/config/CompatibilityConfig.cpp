#include <basalt/server/runtime/config/CompatibilityConfig.hpp>
#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;
using wire::multiversion::UnknownProtocolPolicy;

[[nodiscard]] std::string_view trim(std::string_view value) {
    while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ') {
        value.remove_prefix(1);
    }
    while (!value.empty() && static_cast<unsigned char>(value.back()) <= ' ') {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] std::expected<UnknownProtocolPolicy, std::string>
policyOf(std::optional<std::string_view> written, std::string_view key, const std::string& path) {
    if (!written.has_value()) {
        return UnknownProtocolPolicy::Refuse;
    }
    const std::optional<UnknownProtocolPolicy> parsed = wire::multiversion::parseUnknownProtocolPolicy(*written);
    if (!parsed.has_value()) {
        return std::unexpected(path + ": " + std::string{key} + " must be accept, warn or refuse, not \"" +
                               std::string{*written} + "\"");
    }
    return *parsed;
}

} // namespace

std::expected<CompatibilityConfig, std::string> CompatibilityConfig::load(const ConfigDirectory& directory,
                                                                          const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, kCompatibilityDefaultToml);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    const TomlTable* java = root->table("java");

    BackwardsSelection backwards;
    if (java != nullptr) {
        if (const std::optional<std::string_view> written = java->string("backwards"); written.has_value()) {
            if (lowercaseAscii(trim(*written)) != "all") {
                // Not a guess: "backwards = none" and "backwards = 775" are both someone meaning
                // something specific, and quietly reading either as "all" is the worst answer.
                return std::unexpected(path + ": backwards must be \"all\" or a list of protocol numbers");
            }
        } else if (java->find("backwards") != nullptr) {
            backwards.all = false;
            for (const std::int64_t protocol : java->integerList("backwards")) {
                backwards.protocols.insert(static_cast<std::int32_t>(protocol));
            }
        }
    }

    const std::expected<UnknownProtocolPolicy, std::string> future =
        policyOf(java == nullptr ? std::nullopt : java->string("future-protocols"), "future-protocols", path);
    if (!future.has_value()) {
        return std::unexpected(future.error());
    }
    const std::expected<UnknownProtocolPolicy, std::string> snapshots =
        policyOf(java == nullptr ? std::nullopt : java->string("snapshots"), "snapshots", path);
    if (!snapshots.has_value()) {
        return std::unexpected(snapshots.error());
    }
    const bool nameSubstitutes = java == nullptr ? true : java->boolean("name-substitutes").value_or(true);

    BedrockSettings bedrock;
    if (const TomlTable* table = root->table("bedrock"); table != nullptr) {
        bedrock.enabled = table->boolean("enabled").value_or(bedrock.enabled);
        if (const std::optional<std::int64_t> port = table->integer("port"); port.has_value()) {
            if (*port <= 0 || *port > 65535) {
                return std::unexpected(path + ": the bedrock port must be between 1 and 65535");
            }
            bedrock.port = static_cast<std::uint16_t>(*port);
        }
        if (const std::optional<std::string_view> prefix = table->string("prefix"); prefix.has_value()) {
            bedrock.prefix = std::string{*prefix};
        }
        if (const std::optional<std::string_view> written = table->string("protocols"); written.has_value()) {
            if (lowercaseAscii(trim(*written)) != "all") {
                return std::unexpected(path + ": the bedrock protocols must be \"all\" or a list of numbers");
            }
        } else if (table->find("protocols") != nullptr) {
            std::set<std::int32_t> chosen;
            for (const std::int64_t protocol : table->integerList("protocols")) {
                chosen.insert(static_cast<std::int32_t>(protocol));
            }
            bedrock.protocols = std::move(chosen);
        }
        if (const std::optional<std::string_view> written = table->string("future-protocols"); written.has_value()) {
            const std::optional<wire::bedrock::BedrockFuturePolicy> parsed =
                wire::bedrock::parseBedrockFuturePolicy(*written);
            if (!parsed.has_value()) {
                return std::unexpected(path + ": the bedrock future-protocols must be accept, warn or refuse, not \"" +
                                       std::string{*written} + "\"");
            }
            bedrock.futureProtocols = *parsed;
        }
    }

    logger.debug([&path] { return "Loaded " + path + "."; });
    return CompatibilityConfig{std::move(backwards), *future, *snapshots, nameSubstitutes, std::move(bedrock)};
}

wire::multiversion::ProtocolGate CompatibilityConfig::gate(const std::set<std::int32_t>& translatable,
                                                           const wire::common::Logger& logger) const {
    std::set<std::int32_t> supported = translatable;
    supported.insert(wire::multiversion::protocolVersions::kNative);
    if (backwards_.all) {
        return wire::multiversion::ProtocolGate{std::move(supported), futureProtocols_, snapshots_};
    }

    std::vector<std::int32_t> missing;
    for (const std::int32_t protocol : backwards_.protocols) {
        if (!supported.contains(protocol)) {
            missing.push_back(protocol);
        }
    }
    if (!missing.empty()) {
        logger.warn([&missing] {
            std::string named;
            for (const std::int32_t protocol : missing) {
                if (!named.empty()) {
                    named += ", ";
                }
                named += wire::multiversion::protocolVersions::describe(protocol);
            }
            return "compatibility.toml asks for protocols this build cannot translate: " + named;
        });
    }

    std::set<std::int32_t> chosen;
    for (const std::int32_t protocol : backwards_.protocols) {
        if (supported.contains(protocol)) {
            chosen.insert(protocol);
        }
    }
    // The native version is never off: a server that could not be joined by its own clients is a
    // configuration nobody meant to write.
    chosen.insert(wire::multiversion::protocolVersions::kNative);
    return wire::multiversion::ProtocolGate{std::move(chosen), futureProtocols_, snapshots_};
}

} // namespace basalt::server::runtime::config
