#pragma once

#include <basalt/server/runtime/datapack/PackFiles.hpp>
#include <basalt/server/runtime/datapack/PackMetadata.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::datapack {

/// parity: `PackSource`.
enum class PackSource : std::uint8_t { BuiltIn, Feature, World };

/// parity: `Pack.Position`.
enum class PackPosition : std::uint8_t { Top, Bottom };

/// parity: `Pack` with its `PackLocationInfo`.
struct Pack {
    std::string id;
    wire::text::Component title = wire::text::Component::empty();
    wire::text::Component description = wire::text::Component::empty();
    Compatibility compatibility = Compatibility::Compatible;
    std::vector<wire::common::Identifier> requestedFeatures;
    std::vector<std::string> overlays;
    PackSource source = PackSource::World;
    PackPosition defaultPosition = PackPosition::Top;
    bool required = false;
    bool fixedPosition = false;
    std::string knownPackId;
    PackFiles files;

    /// parity: `Pack.getChatLink`.
    [[nodiscard]] wire::text::Component chatLink(bool enabled) const;

    /// parity: `PackRepository.displayPackList`'s " (incompatible)".
    [[nodiscard]] bool isCompatible() const noexcept { return compatibility == Compatibility::Compatible; }
};

/// parity: `PackRepository` fed by `ServerPacksSource` and a `FolderRepositorySource`.
class PackRepository {
public:
    struct Sources {
        PackFiles jar;
        std::string worldPacks;
    };

    explicit PackRepository(Sources sources, wire::common::Logger logger = wire::common::Logger{"Datapacks"});

    /// parity: `reload`.
    void reload();

    /// parity: `setSelected`: what is present, in the order given, plus whatever is required.
    void setSelected(const std::vector<std::string>& ids);

    [[nodiscard]] const Pack* pack(std::string_view id) const;

    [[nodiscard]] bool isAvailable(std::string_view id) const { return pack(id) != nullptr; }

    [[nodiscard]] std::vector<const Pack*> availablePacks() const;

    [[nodiscard]] std::vector<std::string> availableIds() const;

    [[nodiscard]] std::vector<const Pack*> selectedPacks() const;

    [[nodiscard]] std::vector<std::string> selectedIds() const { return selected_; }

    [[nodiscard]] bool isSelected(std::string_view id) const;

    /// parity: `getRequestedFeatureFlags`.
    [[nodiscard]] std::vector<wire::common::Identifier> requestedFeatures() const;

    [[nodiscard]] std::shared_ptr<const ResourceStack> open(const std::vector<std::string>& ids) const;

    [[nodiscard]] std::shared_ptr<const ResourceStack> openSelected() const { return open(selected_); }

    [[nodiscard]] const std::string& worldPacksDirectory() const noexcept { return sources_.worldPacks; }

private:
    void discoverBuiltIn(std::map<std::string, Pack, std::less<>>& found) const;
    void discoverWorld(std::map<std::string, Pack, std::less<>>& found) const;
    [[nodiscard]] std::vector<std::string> rebuildSelected(const std::vector<std::string>& ids) const;

    Sources sources_;
    wire::common::Logger logger_;
    std::map<std::string, Pack, std::less<>> available_;
    std::vector<std::string> selected_;
};

/// parity: `Pack.readMetaAndCreate`.
[[nodiscard]] std::optional<Pack> readPack(std::string id, wire::text::Component title, PackSource source,
                                           PackFiles files, const wire::common::Logger& logger);

} // namespace basalt::server::runtime::datapack
