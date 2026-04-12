#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/text/Component.hpp>

#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common {
class JsonValue;
}

namespace basalt::server::runtime::datapack {

/// parity: `PackFormat`, a major and a minor compared in that order.
struct PackFormat {
    std::int32_t major = 0;
    std::int32_t minor = 0;

    friend auto operator<=>(const PackFormat&, const PackFormat&) = default;
    friend bool operator==(const PackFormat&, const PackFormat&) = default;
};

/// The data format this build reads: `version.json`'s `data_major`/`data_minor` for 26.3.
inline constexpr PackFormat kCurrentDataFormat{.major = 121, .minor = 0};

/// parity: `PackFormat.lastPreMinorVersion(SERVER_DATA)`, the last format written without a minor.
inline constexpr std::int32_t kLastPreMinorVersion = 81;

inline constexpr std::int32_t kUnknownVersion = std::numeric_limits<std::int32_t>::max();

struct FormatRange {
    PackFormat min;
    PackFormat max;

    [[nodiscard]] bool contains(const PackFormat& format) const noexcept { return min <= format && format <= max; }
};

/// parity: `PackCompatibility`.
enum class Compatibility : std::uint8_t { TooOld, TooNew, Unknown, Compatible };

[[nodiscard]] Compatibility compatibilityOf(const FormatRange& declared,
                                            const PackFormat& current = kCurrentDataFormat);

/// One `filter.block` entry. parity: `IdentifierPattern`, each half a regex found anywhere in its part.
struct IdentifierPattern {
    std::optional<std::string> namespacePattern;
    std::optional<std::string> pathPattern;
};

/// What a pack's `pack.mcmeta` says, already checked the way vanilla checks it.
struct PackMetadata {
    wire::text::Component description = wire::text::Component::empty();
    FormatRange supported;
    /// The `features.enabled` flags, by id.
    std::vector<wire::common::Identifier> requestedFeatures;
    /// The overlay directories that apply to this build, in the order the file lists them.
    std::vector<std::string> overlays;
};

/// Why a `pack.mcmeta` was refused, in vanilla's words where it has them.
struct MetadataError {
    std::string message;
};

/// The feature flags 26.3 knows. An unknown one makes the whole metadata unreadable.
[[nodiscard]] const std::vector<wire::common::Identifier>& knownFeatures();

/// parity: `Pack.readPackMetadata`. A `pack` section the full codec refuses falls back to its
/// description alone, which lists the pack as of unknown compatibility; no section at all, or a
/// file that is not JSON, is an error and the pack is not listed.
[[nodiscard]] std::expected<PackMetadata, MetadataError> parsePackMetadata(std::string_view text);

/// The `filter.block` list of a `pack.mcmeta`, empty when it has none. An error when the section
/// is there but broken. parity: `ResourceFilterSection`.
[[nodiscard]] std::expected<std::vector<IdentifierPattern>, MetadataError> parseFilter(std::string_view text);

/// A pack format range as the `pack` section declares it, or why it cannot be read.
/// parity: `PackFormat.IntermediaryFormat.validate` for a pack (`pack_format` expected).
[[nodiscard]] std::expected<FormatRange, MetadataError> packRange(const wire::common::JsonValue& pack);

} // namespace basalt::server::runtime::datapack
