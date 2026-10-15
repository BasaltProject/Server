#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::registry::tags {
// parity: `TagEntry`/`ExtraCodecs.TAG_OR_ELEMENT_ID`.
struct TagEntry {
    wire::common::Identifier id;
    bool isTag = false;
    bool required = true;

    friend bool operator==(const TagEntry&, const TagEntry&) = default;
};

// parity: vanilla's `TagFile` record.
struct TagFile {
    std::vector<TagEntry> entries;
    bool replace = false;

    friend bool operator==(const TagFile&, const TagFile&) = default;
};

struct TagError {
    enum class Kind : std::uint8_t { Cycle, UnknownTag, MissingTag, MissingElement };

    Kind kind = Kind::UnknownTag;
    wire::common::Identifier tag;
    wire::common::Identifier reference;

    [[nodiscard]] std::string message() const;
};

using ElementExists = std::function<bool(const wire::common::Identifier&)>;

// parity: `TagLoader.build`.
[[nodiscard]] std::expected<std::map<wire::common::Identifier, std::set<wire::common::Identifier>>, TagError>
resolve(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists);

// parity: the `LinkedHashSet` `TagLoader.build` fills.
[[nodiscard]] std::expected<std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>, TagError>
resolveOrdered(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists);

struct TagFailure {
    wire::common::Identifier tag;
    std::vector<std::string> missing;

    // parity: `TagLoader.build`'s "Couldn't load tag {} as it is missing following references.
    [[nodiscard]] std::string message() const;
};

using TagFailed = std::function<void(const TagFailure&)>;

// parity: `TagLoader.build`.
[[nodiscard]] std::map<wire::common::Identifier, std::set<wire::common::Identifier>>
resolveEach(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists,
            const TagFailed& failed = {});

[[nodiscard]] std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>
resolveEachOrdered(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists,
                   const TagFailed& failed = {});

// parity: `Codec.either(TAG_OR_ELEMENT_ID, FULL_CODEC)`.
namespace tagFileJson {
enum class ParseError : std::uint8_t {
    NotAnObject,
    MissingValues,
    UnsupportedEntry,
    BadIdentifier,
};

[[nodiscard]] std::expected<TagFile, ParseError> parse(const wire::common::JsonValue& root);

[[nodiscard]] std::expected<TagFile, ParseError> parse(std::string_view json);
} // namespace tagFileJson
} // namespace basalt::server::registry::tags
