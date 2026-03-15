#include <basalt/server/registry/Tags.hpp>

#include <algorithm>

namespace basalt::server::registry::tags {
namespace {

using wire::common::Identifier;

using Resolved = std::map<Identifier, std::set<Identifier>>;
using ResolvedOrder = std::map<Identifier, std::vector<Identifier>>;

void addTo(std::set<Identifier>& values, const Identifier& id) { values.insert(id); }

/// parity: the jar's `LinkedHashSet`, first occurrence wins.
void addTo(std::vector<Identifier>& values, const Identifier& id) {
    if (std::ranges::find(values, id) == values.end()) {
        values.push_back(id);
    }
}

/// Depth-first with the answer remembered, which is what makes each tag resolve exactly once.
template <typename Values>
struct Resolver {
    const std::map<Identifier, TagFile>& files;
    const ElementExists& elementExists;
    std::map<Identifier, Values> resolved;
    std::set<Identifier> inProgress;

    [[nodiscard]] std::expected<const Values*, TagError> resolve(const Identifier& id) {
        if (const auto done = resolved.find(id); done != resolved.end()) {
            return &done->second;
        }
        if (inProgress.contains(id)) {
            return std::unexpected(TagError{TagError::Kind::Cycle, id, id});
        }
        const auto file = files.find(id);
        if (file == files.end()) {
            return std::unexpected(TagError{TagError::Kind::UnknownTag, id, id});
        }
        inProgress.insert(id);
        Values values;
        for (const TagEntry& entry : file->second.entries) {
            if (entry.isTag) {
                if (!files.contains(entry.id)) {
                    if (entry.required) {
                        return std::unexpected(TagError{TagError::Kind::MissingTag, id, entry.id});
                    }
                    continue;
                }
                const std::expected<const Values*, TagError> referenced = resolve(entry.id);
                if (!referenced.has_value()) {
                    return std::unexpected(referenced.error());
                }
                for (const Identifier& member : **referenced) {
                    addTo(values, member);
                }
            } else {
                if (!elementExists(entry.id)) {
                    if (entry.required) {
                        return std::unexpected(TagError{TagError::Kind::MissingElement, id, entry.id});
                    }
                    continue;
                }
                addTo(values, entry.id);
            }
        }
        inProgress.erase(id);
        return &resolved.emplace(id, std::move(values)).first->second;
    }
};

} // namespace

std::string TagError::message() const {
    switch (kind) {
    case Kind::Cycle:
        return "Cycle detected while resolving tag " + tag.toString();
    case Kind::UnknownTag:
        return "Unknown tag " + tag.toString();
    case Kind::MissingTag:
        return "Tag " + tag.toString() + " references missing tag " + reference.toString();
    default:
        return "Tag " + tag.toString() + " references missing element " + reference.toString();
    }
}

namespace {

template <typename Values>
[[nodiscard]] std::expected<std::map<Identifier, Values>, TagError>
resolveAll(const std::map<Identifier, TagFile>& files, const ElementExists& elementExists) {
    Resolver<Values> resolver{files, elementExists, {}, {}};
    for (const auto& [id, unused] : files) {
        const std::expected<const Values*, TagError> one = resolver.resolve(id);
        if (!one.has_value()) {
            return std::unexpected(one.error());
        }
    }
    return std::move(resolver.resolved);
}

} // namespace

std::expected<Resolved, TagError> resolve(const std::map<Identifier, TagFile>& files,
                                          const ElementExists& elementExists) {
    return resolveAll<std::set<Identifier>>(files, elementExists);
}

std::expected<ResolvedOrder, TagError> resolveOrdered(const std::map<Identifier, TagFile>& files,
                                                      const ElementExists& elementExists) {
    return resolveAll<std::vector<Identifier>>(files, elementExists);
}

namespace tagFileJson {
namespace {

/// A leading `#` is what makes an entry a reference to another tag rather than an element.
[[nodiscard]] std::expected<TagEntry, ParseError> idOrTag(std::string_view raw, bool required) {
    const bool isTag = raw.starts_with('#');
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(isTag ? raw.substr(1) : raw);
    if (!id.has_value()) {
        return std::unexpected(ParseError::BadIdentifier);
    }
    return TagEntry{*id, isTag, required};
}

[[nodiscard]] std::expected<TagEntry, ParseError> parseEntry(const wire::common::JsonValue& element) {
    if (const auto* text = element.get<std::string>()) {
        return idOrTag(*text, true);
    }
    const auto* object = element.get<wire::common::JsonObject>();
    if (object == nullptr) {
        return std::unexpected(ParseError::UnsupportedEntry);
    }
    const wire::common::JsonValue* id = object->find("id");
    if (id == nullptr) {
        return std::unexpected(ParseError::UnsupportedEntry);
    }
    const auto* text = id->get<std::string>();
    if (text == nullptr) {
        return std::unexpected(ParseError::UnsupportedEntry);
    }
    const wire::common::JsonValue* required = object->find("required");
    const bool* flag = required == nullptr ? nullptr : required->get<bool>();
    return idOrTag(*text, flag == nullptr || *flag);
}

} // namespace

std::expected<TagFile, ParseError> parse(const wire::common::JsonValue& root) {
    const auto* object = root.get<wire::common::JsonObject>();
    if (object == nullptr) {
        return std::unexpected(ParseError::NotAnObject);
    }
    const wire::common::JsonValue* values = object->find("values");
    if (values == nullptr) {
        return std::unexpected(ParseError::MissingValues);
    }
    const auto* entries = values->get<std::vector<wire::common::JsonValue>>();
    if (entries == nullptr) {
        return std::unexpected(ParseError::MissingValues);
    }
    TagFile file;
    file.entries.reserve(entries->size());
    for (const wire::common::JsonValue& element : *entries) {
        const std::expected<TagEntry, ParseError> entry = parseEntry(element);
        if (!entry.has_value()) {
            return std::unexpected(entry.error());
        }
        file.entries.push_back(*entry);
    }
    const wire::common::JsonValue* replace = object->find("replace");
    const bool* flag = replace == nullptr ? nullptr : replace->get<bool>();
    file.replace = flag != nullptr && *flag;
    return file;
}

std::expected<TagFile, ParseError> parse(std::string_view json) {
    const std::expected<wire::common::JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    if (!root.has_value()) {
        return std::unexpected(ParseError::NotAnObject);
    }
    return parse(*root);
}

} // namespace tagFileJson

} // namespace basalt::server::registry::tags
