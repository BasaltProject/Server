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

inline constexpr PackFormat kCurrentDataFormat{.major = 121, .minor = 0};

/// parity: `PackFormat.lastPreMinorVersion(SERVER_DATA)`.
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

/// parity: `IdentifierPattern`, each half a regex found anywhere in its part.
struct IdentifierPattern {
    std::optional<std::string> namespacePattern;
    std::optional<std::string> pathPattern;
};

struct PackMetadata {
    wire::text::Component description = wire::text::Component::empty();
    FormatRange supported;
    std::vector<wire::common::Identifier> requestedFeatures;
    std::vector<std::string> overlays;
};

struct MetadataError {
    std::string message;
};

[[nodiscard]] const std::vector<wire::common::Identifier>& knownFeatures();

/// parity: `Pack.readPackMetadata`.
[[nodiscard]] std::expected<PackMetadata, MetadataError> parsePackMetadata(std::string_view text);

/// parity: `ResourceFilterSection`.
[[nodiscard]] std::expected<std::vector<IdentifierPattern>, MetadataError> parseFilter(std::string_view text);

/// parity: `PackFormat.IntermediaryFormat.validate` for a pack (`pack_format` expected).
[[nodiscard]] std::expected<FormatRange, MetadataError> packRange(const wire::common::JsonValue& pack);

} // namespace basalt::server::runtime::datapack
