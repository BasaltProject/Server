#include <basalt/server/runtime/datapack/DataFiles.hpp>

#include <algorithm>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;

constexpr std::string_view kDataRoot = "data/";

[[nodiscard]] registry::tags::TagFailed logged(const wire::common::Logger& logger) {
    return [&logger](const registry::tags::TagFailure& failure) { logger.error([&] { return failure.message(); }); };
}

} // namespace

bool registryFileOrder(const Identifier& left, const Identifier& right, std::string_view extension) {
    const std::string a = left.path() + std::string{extension};
    const std::string b = right.path() + std::string{extension};
    return a != b ? a < b : left.namespaceName() < right.namespaceName();
}

std::vector<std::pair<Identifier, std::string>> dataFiles(const EntriesUnder& entriesUnder, std::string_view directory,
                                                          std::string_view extension) {
    std::vector<std::pair<Identifier, std::string>> found;
    if (!entriesUnder) {
        return found;
    }
    const std::string wanted = std::string{directory} + "/";
    for (const std::string& entry : entriesUnder(kDataRoot)) {
        const std::string_view rest = std::string_view{entry}.substr(kDataRoot.size());
        const std::size_t slash = rest.find('/');
        if (slash == std::string_view::npos || slash == 0) {
            continue;
        }
        const std::string_view inside = rest.substr(slash + 1);
        if (!inside.starts_with(wanted) || !inside.ends_with(extension) ||
            inside.size() <= wanted.size() + extension.size()) {
            continue;
        }
        const std::string_view path = inside.substr(wanted.size(), inside.size() - wanted.size() - extension.size());
        found.emplace_back(Identifier{std::string{rest.substr(0, slash)}, std::string{path}}, entry);
    }
    std::ranges::sort(found, [extension](const auto& left, const auto& right) {
        return registryFileOrder(left.first, right.first, extension);
    });
    return found;
}

std::map<Identifier, registry::tags::TagFile> tagFiles(const EntriesUnder& entriesUnder, const ReadText& readText,
                                                       std::string_view registry, const wire::common::Logger& logger) {
    std::map<Identifier, registry::tags::TagFile> files;
    if (!readText) {
        return files;
    }
    for (const auto& [id, entry] : dataFiles(entriesUnder, "tags/" + std::string{registry})) {
        const std::optional<std::string> text = readText(entry);
        if (!text.has_value()) {
            continue;
        }
        std::expected<registry::tags::TagFile, registry::tags::tagFileJson::ParseError> file =
            registry::tags::tagFileJson::parse(*text);
        if (!file.has_value()) {
            logger.error([&] { return "Couldn't read tag list " + id.toString() + " from " + entry; });
            continue;
        }
        files.insert_or_assign(id, std::move(*file));
    }
    return files;
}

std::map<Identifier, std::set<Identifier>> resolveTags(const EntriesUnder& entriesUnder, const ReadText& readText,
                                                       std::string_view registry,
                                                       const registry::tags::ElementExists& exists,
                                                       const wire::common::Logger& logger) {
    return registry::tags::resolveEach(tagFiles(entriesUnder, readText, registry, logger), exists, logged(logger));
}

std::map<Identifier, std::vector<Identifier>> resolveTagsOrdered(const EntriesUnder& entriesUnder,
                                                                 const ReadText& readText, std::string_view registry,
                                                                 const registry::tags::ElementExists& exists,
                                                                 const wire::common::Logger& logger) {
    return registry::tags::resolveEachOrdered(tagFiles(entriesUnder, readText, registry, logger), exists,
                                              logged(logger));
}

} // namespace basalt::server::runtime::datapack
