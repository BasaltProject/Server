#include <basalt/server/registry/Tags.hpp>

namespace basalt::server::registry::tags {
namespace {

using wire::common::Identifier;

using Resolved = std::map<Identifier, std::set<Identifier>>;

/// Depth-first with the answer remembered, which is what makes each tag resolve exactly once.
struct Resolver {
    const std::map<Identifier, TagFile>& files;
    const ElementExists& elementExists;
    Resolved resolved;
    std::set<Identifier> inProgress;

    [[nodiscard]] std::expected<const std::set<Identifier>*, TagError> resolve(const Identifier& id) {
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
        std::set<Identifier> values;
        for (const TagEntry& entry : file->second.entries) {
            if (entry.isTag) {
                if (!files.contains(entry.id)) {
                    if (entry.required) {
                        return std::unexpected(TagError{TagError::Kind::MissingTag, id, entry.id});
                    }
                    continue;
                }
                const std::expected<const std::set<Identifier>*, TagError> referenced = resolve(entry.id);
                if (!referenced.has_value()) {
                    return std::unexpected(referenced.error());
                }
                values.insert((*referenced)->begin(), (*referenced)->end());
            } else {
                if (!elementExists(entry.id)) {
                    if (entry.required) {
                        return std::unexpected(TagError{TagError::Kind::MissingElement, id, entry.id});
                    }
                    continue;
                }
                values.insert(entry.id);
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

std::expected<Resolved, TagError> resolve(const std::map<Identifier, TagFile>& files,
                                          const ElementExists& elementExists) {
    Resolver resolver{files, elementExists, {}, {}};
    for (const auto& [id, unused] : files) {
        const std::expected<const std::set<Identifier>*, TagError> one = resolver.resolve(id);
        if (!one.has_value()) {
            return std::unexpected(one.error());
        }
    }
    return std::move(resolver.resolved);
}

} // namespace basalt::server::registry::tags
