#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/text/HoverEvent.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::text::Component;

constexpr std::string_view kVanillaId = "vanilla";
constexpr std::string_view kFeaturePacksRoot = "data/minecraft/datapacks/";
constexpr std::string_view kMetadataFile = "pack.mcmeta";
constexpr std::string_view kZipSuffix = ".zip";

/// parity: `StringArgumentType.escapeIfRequired`.
[[nodiscard]] std::string escapeIfRequired(std::string_view text) {
    const bool plain = std::ranges::all_of(text, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-' ||
               c == '.' || c == '+';
    });
    if (plain) {
        return std::string{text};
    }
    std::string quoted{"\""};
    for (const char c : text) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
        }
        quoted += c;
    }
    return quoted + "\"";
}

/// parity: `PackSource.decorate`.
[[nodiscard]] Component decorate(PackSource source, Component name) {
    const char* key = source == PackSource::BuiltIn   ? "pack.source.builtin"
                      : source == PackSource::Feature ? "pack.source.feature"
                                                      : "pack.source.world";
    return Component::translatable("pack.nameAndSource", {std::move(name), Component::translatable(key)})
        .color(wire::text::kGray);
}

/// parity: `ServerPacksSource`'s built-in metadata, since the jar has no `pack.mcmeta` of its own.
[[nodiscard]] Pack vanillaPack(const PackFiles& jar) {
    Pack pack;
    pack.id = std::string{kVanillaId};
    pack.title = Component::translatable("dataPack.vanilla.name");
    pack.description = Component::translatable("dataPack.vanilla.description");
    pack.compatibility = Compatibility::Compatible;
    pack.requestedFeatures = {Identifier::vanilla("vanilla")};
    pack.source = PackSource::BuiltIn;
    pack.defaultPosition = PackPosition::Bottom;
    pack.knownPackId = "core";
    pack.files = subtreeFiles(jar, "", {std::string{kFeaturePacksRoot}});
    return pack;
}

[[nodiscard]] std::optional<std::string> readText(const PackFiles& files, std::string_view path) {
    if (!files.read) {
        return std::nullopt;
    }
    std::optional<std::vector<std::uint8_t>> bytes = files.read(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return std::string{bytes->begin(), bytes->end()};
}

/// parity: `Pack.Position.insert`, which keeps fixed packs at their own end of the list.
void insertAtDefault(std::vector<std::string>& ids, const std::map<std::string, Pack, std::less<>>& available,
                     const Pack& pack) {
    const auto fixedAt = [&](std::size_t index, PackPosition side) {
        const auto found = available.find(ids[index]);
        return found != available.end() && found->second.fixedPosition && found->second.defaultPosition == side;
    };
    if (pack.defaultPosition == PackPosition::Bottom) {
        std::size_t index = 0;
        while (index < ids.size() && fixedAt(index, PackPosition::Bottom)) {
            ++index;
        }
        ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(index), pack.id);
        return;
    }
    std::size_t index = ids.size();
    while (index > 0 && fixedAt(index - 1, PackPosition::Top)) {
        --index;
    }
    ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(index), pack.id);
}

} // namespace

Component Pack::chatLink(bool enabled) const {
    const Component hover = Component::empty().append(title).append(Component::text("\n")).append(description);
    Component link = Component::translatable("chat.square_brackets", {decorate(source, Component::text(id))});
    wire::text::Style style = link.style();
    style.color = enabled ? wire::text::kGreen : wire::text::kRed;
    style.insertion = escapeIfRequired(id);
    style.hoverEvent = wire::text::HoverEvent::showText(hover);
    return link.withStyle(std::move(style));
}

std::optional<Pack> readPack(std::string id, Component title, PackSource source, PackFiles files,
                             const wire::common::Logger& logger) {
    const std::optional<std::string> text = readText(files, kMetadataFile);
    if (!text.has_value()) {
        logger.warn([&id] { return "Missing metadata in pack " + id; });
        return std::nullopt;
    }
    std::expected<PackMetadata, MetadataError> metadata = parsePackMetadata(*text);
    if (!metadata.has_value()) {
        logger.warn([&] { return "Failed to read pack " + id + " metadata: " + metadata.error().message; });
        return std::nullopt;
    }
    Pack pack;
    pack.id = std::move(id);
    pack.title = std::move(title);
    pack.description = std::move(metadata->description);
    pack.compatibility = compatibilityOf(metadata->supported);
    pack.requestedFeatures = std::move(metadata->requestedFeatures);
    pack.overlays = std::move(metadata->overlays);
    pack.source = source;
    pack.defaultPosition = PackPosition::Top;
    pack.files = std::move(files);
    return pack;
}

PackRepository::PackRepository(Sources sources, wire::common::Logger logger)
    : sources_(std::move(sources)), logger_(std::move(logger)) {}

void PackRepository::discoverBuiltIn(std::map<std::string, Pack, std::less<>>& found) const {
    found.insert_or_assign(std::string{kVanillaId}, vanillaPack(sources_.jar));
    if (!sources_.jar.list) {
        return;
    }
    std::set<std::string> ids;
    for (const std::string& name : sources_.jar.list()) {
        if (!name.starts_with(kFeaturePacksRoot)) {
            continue;
        }
        const std::string_view rest = std::string_view{name}.substr(kFeaturePacksRoot.size());
        const std::size_t slash = rest.find('/');
        if (slash != std::string_view::npos && rest.substr(slash + 1) == kMetadataFile) {
            ids.emplace(rest.substr(0, slash));
        }
    }
    for (const std::string& id : ids) {
        std::optional<Pack> pack =
            readPack(id, Component::text(id), PackSource::Feature,
                     subtreeFiles(sources_.jar, std::string{kFeaturePacksRoot} + id + "/"), logger_);
        if (pack.has_value()) {
            pack->knownPackId = id;
            found.insert_or_assign(id, std::move(*pack));
        }
    }
}

void PackRepository::discoverWorld(std::map<std::string, Pack, std::less<>>& found) const {
    if (sources_.worldPacks.empty()) {
        return;
    }
    wire::io::fileIO::createDirectories(sources_.worldPacks);
    std::vector<std::string> entries = wire::io::fileIO::list(sources_.worldPacks);
    std::ranges::sort(entries);
    for (const std::string& entry : entries) {
        const std::string path = sources_.worldPacks + "/" + entry;
        std::optional<PackFiles> files;
        if (wire::io::fileIO::isDirectory(path)) {
            if (wire::io::fileIO::isDirectory(path + "/" + std::string{kMetadataFile}) ||
                !wire::io::fileIO::exists(path + "/" + std::string{kMetadataFile})) {
                logger_.info([&path] { return "Found non-pack entry '" + path + "', ignoring"; });
                continue;
            }
            files = directoryFiles(path);
        } else if (entry.ends_with(kZipSuffix)) {
            files = zipFiles(path);
            if (!files.has_value()) {
                logger_.warn([&path] { return "Failed to read properties of '" + path + "', ignoring"; });
                continue;
            }
        } else {
            logger_.info([&path] { return "Found non-pack entry '" + path + "', ignoring"; });
            continue;
        }
        const std::string id = "file/" + entry;
        std::optional<Pack> pack = readPack(id, Component::text(entry), PackSource::World, std::move(*files), logger_);
        if (pack.has_value()) {
            found.insert_or_assign(id, std::move(*pack));
        }
    }
}

void PackRepository::reload() {
    std::map<std::string, Pack, std::less<>> found;
    discoverBuiltIn(found);
    discoverWorld(found);
    available_ = std::move(found);
    selected_ = rebuildSelected(selected_);
}

std::vector<std::string> PackRepository::rebuildSelected(const std::vector<std::string>& ids) const {
    std::vector<std::string> present;
    for (const std::string& id : ids) {
        if (available_.contains(id) && !std::ranges::contains(present, id)) {
            present.push_back(id);
        }
    }
    for (const auto& [id, pack] : available_) {
        if (pack.required && !std::ranges::contains(present, id)) {
            insertAtDefault(present, available_, pack);
        }
    }
    return present;
}

void PackRepository::setSelected(const std::vector<std::string>& ids) { selected_ = rebuildSelected(ids); }

const Pack* PackRepository::pack(std::string_view id) const {
    const auto found = available_.find(id);
    return found == available_.end() ? nullptr : &found->second;
}

std::vector<const Pack*> PackRepository::availablePacks() const {
    std::vector<const Pack*> packs;
    for (const auto& [id, pack] : available_) {
        packs.push_back(&pack);
    }
    return packs;
}

std::vector<std::string> PackRepository::availableIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, pack] : available_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<const Pack*> PackRepository::selectedPacks() const {
    std::vector<const Pack*> packs;
    for (const std::string& id : selected_) {
        if (const Pack* one = pack(id); one != nullptr) {
            packs.push_back(one);
        }
    }
    return packs;
}

bool PackRepository::isSelected(std::string_view id) const { return std::ranges::contains(selected_, id); }

std::vector<Identifier> PackRepository::requestedFeatures() const {
    std::vector<Identifier> flags;
    for (const Pack* pack : selectedPacks()) {
        for (const Identifier& flag : pack->requestedFeatures) {
            if (!std::ranges::contains(flags, flag)) {
                flags.push_back(flag);
            }
        }
    }
    return flags;
}

std::shared_ptr<const ResourceStack> PackRepository::open(const std::vector<std::string>& ids) const {
    std::vector<ResourceStack::Layer> layers;
    for (const std::string& id : ids) {
        const Pack* one = pack(id);
        if (one == nullptr) {
            continue;
        }
        ResourceStack::Layer layer;
        layer.contents = std::make_shared<const PackContents>(one->id, one->files, one->overlays, logger_);
        if (const std::optional<std::string> text = readText(one->files, kMetadataFile); text.has_value()) {
            std::expected<std::vector<IdentifierPattern>, MetadataError> filter = parseFilter(*text);
            if (filter.has_value()) {
                layer.filter = std::move(*filter);
            } else {
                logger_.error([one] { return "Failed to get filter section from pack " + one->id; });
            }
        }
        layers.push_back(std::move(layer));
    }
    return std::make_shared<const ResourceStack>(std::move(layers), logger_);
}

} // namespace basalt::server::runtime::datapack
