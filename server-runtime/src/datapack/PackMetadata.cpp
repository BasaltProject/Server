#include <basalt/server/runtime/datapack/PackMetadata.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>
#include <regex>
#include <utility>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

/// A range of plain majors, as `supported_formats` and `formats` write it.
struct IntRange {
    std::int32_t min = 0;
    std::int32_t max = 0;
};

/// parity: `PackFormat.IntermediaryFormat`, what the four optional fields leave behind.
struct Intermediary {
    std::optional<PackFormat> min;
    std::optional<PackFormat> max;
    std::optional<std::int32_t> format;
    std::optional<IntRange> supported;
};

using Result = std::expected<FormatRange, MetadataError>;

[[nodiscard]] std::unexpected<MetadataError> fail(std::string message) {
    return std::unexpected{MetadataError{std::move(message)}};
}

/// parity: `Codec.INT`, which takes any JSON number and truncates it.
[[nodiscard]] std::optional<std::int32_t> intOf(const JsonValue& value) {
    if (const auto* whole = value.get<std::int64_t>(); whole != nullptr) {
        return static_cast<std::int32_t>(*whole);
    }
    if (const auto* real = value.get<double>(); real != nullptr) {
        return static_cast<std::int32_t>(*real);
    }
    return std::nullopt;
}

/// parity: `PackFormat.fullCodec`: a number, or a list of one or two, never negative.
[[nodiscard]] std::expected<PackFormat, MetadataError> formatOf(const JsonValue& value, std::int32_t defaultMinor) {
    std::vector<std::int32_t> parts;
    if (const std::optional<std::int32_t> single = intOf(value); single.has_value()) {
        parts.push_back(*single);
    } else if (const auto* list = value.get<std::vector<JsonValue>>(); list != nullptr) {
        if (list->empty() || list->size() > 256) {
            return fail("Pack format list must hold between 1 and 256 numbers");
        }
        for (const JsonValue& one : *list) {
            const std::optional<std::int32_t> part = intOf(one);
            if (!part.has_value()) {
                return fail("Pack format must be a number");
            }
            parts.push_back(*part);
        }
    } else {
        return fail("Pack format must be a number or a list of numbers");
    }
    if (std::ranges::any_of(parts, [](std::int32_t part) { return part < 0; })) {
        return fail("Pack format must not be negative");
    }
    return PackFormat{.major = parts[0], .minor = parts.size() > 1 ? parts[1] : defaultMinor};
}

/// parity: `InclusiveRange.codec(Codec.INT)`: a number, `[min, max]` or `{min_inclusive, max_inclusive}`.
[[nodiscard]] std::expected<IntRange, MetadataError> intRangeOf(const JsonValue& value) {
    std::optional<std::int32_t> low;
    std::optional<std::int32_t> high;
    if (const std::optional<std::int32_t> single = intOf(value); single.has_value()) {
        low = single;
        high = single;
    } else if (const auto* list = value.get<std::vector<JsonValue>>(); list != nullptr && list->size() == 2) {
        low = intOf((*list)[0]);
        high = intOf((*list)[1]);
    } else if (const auto* object = value.get<JsonObject>(); object != nullptr) {
        const JsonValue* min = object->find("min_inclusive");
        const JsonValue* max = object->find("max_inclusive");
        low = min == nullptr ? std::nullopt : intOf(*min);
        high = max == nullptr ? std::nullopt : intOf(*max);
    }
    if (!low.has_value() || !high.has_value()) {
        return fail("Not a valid range of formats");
    }
    if (*low > *high) {
        return fail("Range bounds are out of order: " + std::to_string(*low) + " > " + std::to_string(*high));
    }
    return IntRange{.min = *low, .max = *high};
}

[[nodiscard]] std::string toString(const PackFormat& format) {
    return std::to_string(format.major) + "." +
           (format.minor == kUnknownVersion ? std::string{"*"} : std::to_string(format.minor));
}

/// parity: `IntermediaryFormat.validatePackFormatForRange`.
[[nodiscard]] std::optional<std::string> packFormatError(std::int32_t format, std::int32_t min, std::int32_t max) {
    if (format >= min && format <= max) {
        if (format < 15) {
            return "Multi-version packs cannot support minimum version of less than 15, since this will leave "
                   "versions in range unable to load pack.";
        }
        return std::nullopt;
    }
    return "Pack declared support for versions " + std::to_string(min) + " to " + std::to_string(max) +
           " but declared main format is " + std::to_string(format);
}

[[nodiscard]] Result validateNew(const Intermediary& in, bool hasPackFormatField, bool requireOldField,
                                 const std::string& context, const std::string& oldField) {
    const std::int32_t majorMin = in.min->major;
    const std::int32_t majorMax = in.max->major;
    if (*in.min > *in.max) {
        return fail(context + " min_format (" + toString(*in.min) + ") is greater than max_format (" +
                    toString(*in.max) + ")");
    }
    const std::string last = std::to_string(kLastPreMinorVersion);
    if (majorMin > kLastPreMinorVersion && !requireOldField) {
        if (in.supported.has_value()) {
            return fail(context + " key " + oldField + " is deprecated starting from pack format " +
                        std::to_string(kLastPreMinorVersion + 1) + ". Remove " + oldField + " from your pack.mcmeta.");
        }
        if (hasPackFormatField && in.format.has_value()) {
            if (std::optional<std::string> error = packFormatError(*in.format, majorMin, majorMax); error.has_value()) {
                return fail(std::move(*error));
            }
        }
    } else {
        if (!in.supported.has_value()) {
            return fail(context + " declares support for format " + std::to_string(majorMin) +
                        ", but game versions supporting formats 15 to " + last + " require a " + oldField +
                        " field. Add \"" + oldField + "\": [" + std::to_string(majorMin) + ", " + last +
                        "] or require a version greater or equal to " + std::to_string(kLastPreMinorVersion + 1) +
                        ".0.");
        }
        if (in.supported->min != majorMin) {
            return fail(context + " version declaration mismatch between " + oldField + " (from " +
                        std::to_string(in.supported->min) + ") and min_format (" + toString(*in.min) + ")");
        }
        if (in.supported->max != majorMax && in.supported->max != kLastPreMinorVersion) {
            return fail(context + " version declaration mismatch between " + oldField + " (up to " +
                        std::to_string(in.supported->max) + ") and max_format (" + toString(*in.max) + ")");
        }
        if (hasPackFormatField) {
            if (!in.format.has_value()) {
                return fail(context + " declares support for formats up to " + last +
                            ", but game versions supporting formats 15 to " + last +
                            " require a pack_format field. Add \"pack_format\": " + std::to_string(majorMin) +
                            " or require a version greater or equal to " + std::to_string(kLastPreMinorVersion + 1) +
                            ".0.");
            }
            if (std::optional<std::string> error = packFormatError(*in.format, majorMin, majorMax); error.has_value()) {
                return fail(std::move(*error));
            }
        }
    }
    return FormatRange{.min = *in.min, .max = *in.max};
}

[[nodiscard]] Result validateOld(const Intermediary& in, bool hasPackFormatField, const std::string& context,
                                 const std::string& /*oldField*/) {
    const std::int32_t min = in.supported->min;
    const std::int32_t max = in.supported->max;
    const std::string last = std::to_string(kLastPreMinorVersion);
    if (max > kLastPreMinorVersion) {
        return fail(context + " declares support for version newer than " + last +
                    ", but is missing mandatory fields min_format and max_format");
    }
    if (hasPackFormatField) {
        if (!in.format.has_value()) {
            return fail(context + " declares support for formats up to " + last +
                        ", but game versions supporting formats 15 to " + last +
                        " require a pack_format field. Add \"pack_format\": " + std::to_string(min) +
                        " or require a version greater or equal to " + std::to_string(kLastPreMinorVersion + 1) +
                        ".0.");
        }
        if (std::optional<std::string> error = packFormatError(*in.format, min, max); error.has_value()) {
            return fail(std::move(*error));
        }
    }
    return FormatRange{.min = PackFormat{.major = min, .minor = 0}, .max = PackFormat{.major = max, .minor = 0}};
}

/// parity: `IntermediaryFormat.validate`.
[[nodiscard]] Result validate(const Intermediary& in, bool hasPackFormatField, bool requireOldField,
                              const std::string& context, const std::string& oldField) {
    if (in.min.has_value() != in.max.has_value()) {
        return fail(context + " missing field, must declare both min_format and max_format");
    }
    if (requireOldField && !in.supported.has_value()) {
        return fail(context + " missing required field " + oldField +
                    ", must be present in all overlays for any overlays to work across game versions");
    }
    if (in.min.has_value()) {
        return validateNew(in, hasPackFormatField, requireOldField, context, oldField);
    }
    if (in.supported.has_value()) {
        return validateOld(in, hasPackFormatField, context, oldField);
    }
    if (hasPackFormatField && in.format.has_value()) {
        if (*in.format > kLastPreMinorVersion) {
            return fail(context + " declares support for version newer than " + std::to_string(kLastPreMinorVersion) +
                        ", but is missing mandatory fields min_format and max_format");
        }
        const PackFormat only{.major = *in.format, .minor = 0};
        return FormatRange{.min = only, .max = only};
    }
    return fail(context + " could not be parsed, missing format version information");
}

/// Reads the optional format fields shared by a pack and an overlay; `supportedKey` is the old one.
[[nodiscard]] std::expected<Intermediary, MetadataError> intermediaryOf(const JsonObject& object,
                                                                        std::string_view supportedKey) {
    Intermediary in;
    if (const JsonValue* min = object.find("min_format"); min != nullptr) {
        std::expected<PackFormat, MetadataError> read = formatOf(*min, 0);
        if (!read.has_value()) {
            return std::unexpected{read.error()};
        }
        in.min = *read;
    }
    if (const JsonValue* max = object.find("max_format"); max != nullptr) {
        std::expected<PackFormat, MetadataError> read = formatOf(*max, kUnknownVersion);
        if (!read.has_value()) {
            return std::unexpected{read.error()};
        }
        in.max = *read;
    }
    if (const JsonValue* supported = object.find(supportedKey); supported != nullptr) {
        std::expected<IntRange, MetadataError> read = intRangeOf(*supported);
        if (!read.has_value()) {
            return std::unexpected{read.error()};
        }
        in.supported = *read;
    }
    return in;
}

/// parity: `OverlayMetadataSection.DIR_VALIDATOR`, `[-_a-zA-Z0-9.]+`.
[[nodiscard]] bool validOverlayDirectory(std::string_view directory) {
    return !directory.empty() && std::ranges::all_of(directory, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
               c == '.';
    });
}

/// parity: `OverlayEntry.listCodecForPackType` then `overlaysForVersion`.
[[nodiscard]] std::expected<std::vector<std::string>, MetadataError> overlaysOf(const JsonValue& section) {
    const auto* object = section.get<JsonObject>();
    const JsonValue* entries = object == nullptr ? nullptr : object->find("entries");
    const auto* list = entries == nullptr ? nullptr : entries->get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return fail("Overlay section has no entries list");
    }

    struct Read {
        Intermediary format;
        std::string directory;
    };

    std::vector<Read> read;
    for (const JsonValue& entry : *list) {
        const auto* fields = entry.get<JsonObject>();
        const JsonValue* directory = fields == nullptr ? nullptr : fields->find("directory");
        const auto* name = directory == nullptr ? nullptr : directory->get<std::string>();
        if (name == nullptr) {
            return fail("Overlay entry has no directory");
        }
        if (!validOverlayDirectory(*name)) {
            return fail(*name + " is not accepted directory name");
        }
        std::expected<Intermediary, MetadataError> format = intermediaryOf(*fields, "formats");
        if (!format.has_value()) {
            return std::unexpected{format.error()};
        }
        // parity: the overlay codec fills the main format in from `min_format`.
        format->format = format->min.has_value() ? std::optional{format->min->major} : std::nullopt;
        read.push_back(Read{.format = *format, .directory = *name});
    }
    std::int32_t minVersion = kUnknownVersion;
    for (const Read& one : read) {
        std::int32_t effective = kUnknownVersion;
        if (one.format.min.has_value()) {
            effective = one.format.supported.has_value() ? std::min(one.format.min->major, one.format.supported->min)
                                                         : one.format.min->major;
        } else if (one.format.supported.has_value()) {
            effective = one.format.supported->min;
        }
        minVersion = std::min(minVersion, effective);
    }
    std::vector<std::string> applicable;
    for (const Read& one : read) {
        if (!one.format.min.has_value() && !one.format.max.has_value() && !one.format.supported.has_value()) {
            // parity: vanilla warns about a broken entry and drops just that one.
            continue;
        }
        Result range = validate(one.format, false, minVersion <= kLastPreMinorVersion,
                                "Overlay \"" + one.directory + "\"", "formats");
        if (!range.has_value()) {
            return std::unexpected{range.error()};
        }
        if (range->contains(kCurrentDataFormat)) {
            applicable.push_back(one.directory);
        }
    }
    return applicable;
}

[[nodiscard]] std::expected<std::vector<Identifier>, MetadataError> featuresOf(const JsonValue& section) {
    const auto* object = section.get<JsonObject>();
    const JsonValue* enabled = object == nullptr ? nullptr : object->find("enabled");
    const auto* list = enabled == nullptr ? nullptr : enabled->get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return fail("Feature section has no enabled list");
    }
    std::vector<Identifier> flags;
    std::string unknown;
    for (const JsonValue& one : *list) {
        const auto* text = one.get<std::string>();
        const auto id = text == nullptr ? std::nullopt : std::optional{Identifier::parse(*text)};
        if (!id.has_value() || !id->has_value()) {
            return fail("Not a valid feature id");
        }
        if (!std::ranges::contains(knownFeatures(), **id)) {
            unknown += unknown.empty() ? (*id)->toString() : ", " + (*id)->toString();
            continue;
        }
        if (!std::ranges::contains(flags, **id)) {
            flags.push_back(**id);
        }
    }
    if (!unknown.empty()) {
        return fail("Unknown feature ids: " + unknown);
    }
    return flags;
}

[[nodiscard]] std::expected<JsonObject, MetadataError> rootOf(std::string_view text) {
    std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(text);
    if (!root.has_value()) {
        return fail("pack.mcmeta is not valid JSON");
    }
    const auto* object = root->get<JsonObject>();
    if (object == nullptr) {
        return fail("pack.mcmeta is not a JSON object");
    }
    return *object;
}

} // namespace

Compatibility compatibilityOf(const FormatRange& declared, const PackFormat& current) {
    if (declared.min.major == kUnknownVersion) {
        return Compatibility::Unknown;
    }
    if (declared.max < current) {
        return Compatibility::TooOld;
    }
    return current < declared.min ? Compatibility::TooNew : Compatibility::Compatible;
}

const std::vector<Identifier>& knownFeatures() {
    static const std::vector<Identifier> kFeatures{
        Identifier::vanilla("vanilla"), Identifier::vanilla("trade_rebalance"),
        Identifier::vanilla("redstone_experiments"), Identifier::vanilla("minecart_improvements")};
    return kFeatures;
}

std::expected<FormatRange, MetadataError> packRange(const JsonValue& pack) {
    const auto* object = pack.get<JsonObject>();
    if (object == nullptr) {
        return fail("Pack section is not an object");
    }
    std::expected<Intermediary, MetadataError> in = intermediaryOf(*object, "supported_formats");
    if (!in.has_value()) {
        return std::unexpected{in.error()};
    }
    if (const JsonValue* format = object->find("pack_format"); format != nullptr) {
        const std::optional<std::int32_t> value = intOf(*format);
        if (!value.has_value()) {
            return fail("pack_format is not a number");
        }
        in->format = value;
    }
    return validate(*in, true, false, "Pack", "supported_formats");
}

std::expected<PackMetadata, MetadataError> parsePackMetadata(std::string_view text) {
    std::expected<JsonObject, MetadataError> root = rootOf(text);
    if (!root.has_value()) {
        return std::unexpected{root.error()};
    }
    const JsonValue* pack = root->find("pack");
    if (pack == nullptr) {
        return fail("Missing metadata");
    }
    const auto* packObject = pack->get<JsonObject>();
    const JsonValue* description = packObject == nullptr ? nullptr : packObject->find("description");
    if (description == nullptr) {
        return fail("Pack section has no description");
    }
    PackMetadata metadata;
    metadata.description = wire::text::componentJson::deserialize(*description);
    // parity: the fallback codec, description only, declared as the unknown version.
    std::expected<FormatRange, MetadataError> range = packRange(*pack);
    metadata.supported = range.value_or(FormatRange{.min = PackFormat{.major = kUnknownVersion, .minor = 0},
                                                    .max = PackFormat{.major = kUnknownVersion, .minor = 0}});
    if (const JsonValue* features = root->find("features"); features != nullptr) {
        std::expected<std::vector<Identifier>, MetadataError> flags = featuresOf(*features);
        if (!flags.has_value()) {
            return std::unexpected{flags.error()};
        }
        metadata.requestedFeatures = std::move(*flags);
    }
    if (const JsonValue* overlays = root->find("overlays"); overlays != nullptr) {
        std::expected<std::vector<std::string>, MetadataError> applicable = overlaysOf(*overlays);
        if (!applicable.has_value()) {
            return std::unexpected{applicable.error()};
        }
        metadata.overlays = std::move(*applicable);
    }
    return metadata;
}

std::expected<std::vector<IdentifierPattern>, MetadataError> parseFilter(std::string_view text) {
    std::expected<JsonObject, MetadataError> root = rootOf(text);
    if (!root.has_value()) {
        return std::unexpected{root.error()};
    }
    const JsonValue* filter = root->find("filter");
    if (filter == nullptr) {
        return std::vector<IdentifierPattern>{};
    }
    const auto* object = filter->get<JsonObject>();
    const JsonValue* block = object == nullptr ? nullptr : object->find("block");
    const auto* list = block == nullptr ? nullptr : block->get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return fail("Filter section has no block list");
    }
    std::vector<IdentifierPattern> patterns;
    for (const JsonValue& entry : *list) {
        const auto* fields = entry.get<JsonObject>();
        if (fields == nullptr) {
            return fail("Filter entry is not an object");
        }
        IdentifierPattern pattern;
        for (const auto& [key, slot] :
             {std::pair{"namespace", &pattern.namespacePattern}, std::pair{"path", &pattern.pathPattern}}) {
            const JsonValue* value = fields->find(key);
            if (value == nullptr) {
                continue;
            }
            const auto* regex = value->get<std::string>();
            if (regex == nullptr) {
                return fail(std::string{"Filter "} + key + " is not a string");
            }
            try {
                static_cast<void>(std::regex{*regex});
            } catch (const std::regex_error&) {
                return fail("Invalid regex pattern '" + *regex + "'");
            }
            *slot = *regex;
        }
        patterns.push_back(std::move(pattern));
    }
    return patterns;
}

} // namespace basalt::server::runtime::datapack
