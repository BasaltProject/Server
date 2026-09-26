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

/// One entry in a tag file's `values` list: either a concrete element or a reference to another tag
/// (a leading `#`), optionally required. parity: `TagEntry`/`ExtraCodecs.TAG_OR_ELEMENT_ID`.
struct TagEntry {
    wire::common::Identifier id;
    bool isTag = false;
    bool required = true;

    friend bool operator==(const TagEntry&, const TagEntry&) = default;
};

/// One `tags/<registry>/<path>.json` file. parity: vanilla's `TagFile` record.
struct TagFile {
    std::vector<TagEntry> entries;
    bool replace = false;

    friend bool operator==(const TagFile&, const TagFile&) = default;
};

/// Why a tag graph could not be resolved.
struct TagError {
    enum class Kind : std::uint8_t { Cycle, UnknownTag, MissingTag, MissingElement };

    Kind kind = Kind::UnknownTag;
    /// The tag being resolved when it went wrong.
    wire::common::Identifier tag;
    /// What it referred to, for everything but a cycle.
    wire::common::Identifier reference;

    [[nodiscard]] std::string message() const;
};

/// Whether a concrete element exists in the registry these tags belong to.
using ElementExists = std::function<bool(const wire::common::Identifier&)>;

/// Flattens one registry's tag files into resolved element sets.
///
/// parity: `TagLoader.build`. Referenced tags resolve first; a missing required element or reference
/// is an error, a missing optional one is dropped. Depth first with memoisation matches vanilla's sort.
[[nodiscard]] std::expected<std::map<wire::common::Identifier, std::set<wire::common::Identifier>>, TagError>
resolve(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists);

/// The same, keeping each tag's members in the order the files list them, first occurrence winning.
/// parity: the `LinkedHashSet` `TagLoader.build` fills, which is the order a random pick draws over.
[[nodiscard]] std::expected<std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>, TagError>
resolveOrdered(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists);

/// One tag that could not be built, and every required reference it was missing.
struct TagFailure {
    wire::common::Identifier tag;
    std::vector<std::string> missing;

    /// parity: `TagLoader.build`'s "Couldn't load tag {} as it is missing following references: {}".
    [[nodiscard]] std::string message() const;
};

using TagFailed = std::function<void(const TagFailure&)>;

/// parity: `TagLoader.build`. A tag missing a required element or tag is left out and reported,
/// with everything that refers to it in turn, and every other tag still loads. A tag reached again
/// while it is being built counts as missing, which is how a cycle fails.
[[nodiscard]] std::map<wire::common::Identifier, std::set<wire::common::Identifier>>
resolveEach(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists,
            const TagFailed& failed = {});

/// The same in file order, first occurrence winning.
[[nodiscard]] std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>
resolveEachOrdered(const std::map<wire::common::Identifier, TagFile>& files, const ElementExists& elementExists,
                   const TagFailed& failed = {});

/// Reads the real vanilla tag file schema, `{"replace": bool, "values": [...]}`.
///
/// Each value is either a plain string (an element, or `#id` for a tag reference, always required)
/// or `{"id": "...", "required": bool}`, with the same `#` rule and `required` defaulting to true.
/// parity: `Codec.either(TAG_OR_ELEMENT_ID, FULL_CODEC)`.
namespace tagFileJson {

/// Why a tag file could not be read. Malformed JSON is refused before this, by the parser.
enum class ParseError : std::uint8_t {
    /// The document is not an object at all.
    NotAnObject,
    /// No `values`, or it is not an array.
    MissingValues,
    /// An entry that is neither a string nor an object with an `id`.
    UnsupportedEntry,
    /// An entry naming something that is not a valid identifier.
    BadIdentifier,
};

[[nodiscard]] std::expected<TagFile, ParseError> parse(const wire::common::JsonValue& root);

/// The same from the text of the file, which is how one arrives from disk.
[[nodiscard]] std::expected<TagFile, ParseError> parse(std::string_view json);

} // namespace tagFileJson

} // namespace basalt::server::registry::tags
