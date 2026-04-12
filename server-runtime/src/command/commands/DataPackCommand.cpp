#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/DataPackCommand.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using datapack::Pack;
using datapack::PackRepository;
using wire::common::Identifier;
using wire::text::Component;

[[nodiscard]] std::unexpected<CommandSyntaxError> error(std::string key, std::vector<Component> arguments) {
    return std::unexpected{
        CommandSyntaxError{.component = Component::translatable(std::move(key), std::move(arguments))}};
}

/// parity: `ComponentUtils.formatList` with its grey comma.
[[nodiscard]] Component formatList(const std::vector<Component>& values) {
    if (values.size() == 1) {
        return values.front();
    }
    Component joined = Component::empty();
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            joined = joined.append(Component::text(", ", wire::text::kGray));
        }
        joined = joined.append(values[index]);
    }
    return joined;
}

/// parity: `StringArgumentType.escapeIfRequired`, which is how a pack id is suggested.
[[nodiscard]] std::string escaped(std::string_view id) {
    const bool plain = std::ranges::all_of(id, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-' ||
               c == '.' || c == '+';
    });
    if (plain) {
        return std::string{id};
    }
    std::string quoted{"\""};
    for (const char c : id) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
        }
        quoted += c;
    }
    return quoted + "\"";
}

/// parity: `FileUtil.isValidPathSegment` and `isPathPartPortable`, near enough: one plain segment.
[[nodiscard]] bool validSegment(std::string_view id) {
    return !id.empty() && id != "." && id != ".." && id.find_first_of("/\\:*?\"<>|") == std::string_view::npos &&
           std::ranges::none_of(id, [](char c) { return static_cast<unsigned char>(c) < ' '; });
}

} // namespace

DataPackCommand::DataPackCommand(DataPackControl control)
    : Command("datapack", permissionLevel::kGamemasters), control_(std::move(control)) {}

void DataPackCommand::build(const CommandNodeBuilder& root) const {
    datapack::DataPacks* packs = control_.packs;
    const auto features = [packs] { return packs->configuration().enabledFeatures; };

    const auto listEnabled = [packs](const CommandContext& context) -> std::int32_t {
        PackRepository& repository = packs->repository();
        repository.reload();
        const std::vector<const Pack*> selected = repository.selectedPacks();
        if (selected.empty()) {
            context.success(Component::translatable("commands.datapack.list.enabled.none"));
            return 0;
        }
        std::vector<Component> links;
        for (const Pack* pack : selected) {
            links.push_back(pack->chatLink(true));
        }
        context.success(Component::translatable("commands.datapack.list.enabled.success",
                                                {Component::text(std::to_string(selected.size())), formatList(links)}));
        return static_cast<std::int32_t>(selected.size());
    };
    const auto listAvailable = [packs, features](const CommandContext& context) -> std::int32_t {
        PackRepository& repository = packs->repository();
        repository.reload();
        std::vector<Component> links;
        for (const Pack* pack : repository.availablePacks()) {
            if (!repository.isSelected(pack->id) && datapack::featuresAllowed(pack->requestedFeatures, features())) {
                links.push_back(pack->chatLink(false));
            }
        }
        if (links.empty()) {
            context.success(Component::translatable("commands.datapack.list.available.none"));
            return 0;
        }
        context.success(Component::translatable("commands.datapack.list.available.success",
                                                {Component::text(std::to_string(links.size())), formatList(links)}));
        return static_cast<std::int32_t>(links.size());
    };

    /// parity: `getPack`, with every refusal `/datapack enable` and `disable` can give.
    const auto packNamed = [packs, features](const CommandContext& context, std::string_view argument,
                                             bool enabling) -> Parsed<const Pack*> {
        const auto* id = context.get<std::string>(argument);
        const std::string named = id == nullptr ? std::string{} : *id;
        const Pack* pack = packs->repository().pack(named);
        if (pack == nullptr) {
            return error("commands.datapack.unknown", {Component::text(named)});
        }
        const bool enabled = packs->repository().isSelected(named);
        if (enabling && enabled) {
            return error("commands.datapack.enable.failed", {Component::text(named)});
        }
        if (!enabling && !enabled) {
            return error("commands.datapack.disable.failed", {Component::text(named)});
        }
        if (!enabling && !pack->requestedFeatures.empty() && pack->source == datapack::PackSource::Feature) {
            return error("commands.datapack.disable.failed.feature", {Component::text(named)});
        }
        if (!datapack::featuresAllowed(pack->requestedFeatures, features())) {
            return error("commands.datapack.enable.failed.no_flags",
                         {Component::text(named),
                          Component::text(datapack::missingFeatures(features(), pack->requestedFeatures))});
        }
        return pack;
    };

    using Inserter = std::function<Parsed<void>(const CommandContext&, std::vector<std::string>&, const Pack&)>;
    const auto enable = [this, packs, packNamed](const CommandContext& context,
                                                 const Inserter& insert) -> Parsed<std::int32_t> {
        const Parsed<const Pack*> pack = packNamed(context, "name", true);
        if (!pack.has_value()) {
            return std::unexpected{pack.error()};
        }
        std::vector<std::string> selected = packs->repository().selectedIds();
        if (const Parsed<void> inserted = insert(context, selected, **pack); !inserted.has_value()) {
            return std::unexpected{inserted.error()};
        }
        context.success(Component::translatable("commands.datapack.modify.enable", {(*pack)->chatLink(true)}), true);
        if (control_.reload && !control_.reload(selected)) {
            context.dispatcher().feedback().failure(context.source(),
                                                    Component::translatable("commands.reload.failure"));
        }
        return static_cast<std::int32_t>(selected.size());
    };
    // parity: `Pack.Position.insert`, a new pack at the top unless a fixed one holds it.
    const Inserter atDefault = [packs](const CommandContext&, std::vector<std::string>& selected,
                                       const Pack& pack) -> Parsed<void> {
        const auto fixedAt = [&](std::size_t index, datapack::PackPosition side) {
            const Pack* other = packs->repository().pack(selected[index]);
            return other != nullptr && other->fixedPosition && other->defaultPosition == side;
        };
        if (pack.defaultPosition == datapack::PackPosition::Bottom) {
            std::size_t index = 0;
            while (index < selected.size() && fixedAt(index, datapack::PackPosition::Bottom)) {
                ++index;
            }
            selected.insert(selected.begin() + static_cast<std::ptrdiff_t>(index), pack.id);
        } else {
            std::size_t index = selected.size();
            while (index > 0 && fixedAt(index - 1, datapack::PackPosition::Top)) {
                --index;
            }
            selected.insert(selected.begin() + static_cast<std::ptrdiff_t>(index), pack.id);
        }
        return {};
    };

    const auto suggestUnselected = [packs, features](const SuggestionContext& context) {
        std::vector<std::string> ids;
        for (const Pack* pack : packs->repository().availablePacks()) {
            if (!packs->repository().isSelected(pack->id) &&
                datapack::featuresAllowed(pack->requestedFeatures, features())) {
                ids.push_back(escaped(pack->id));
            }
        }
        return context.matching(ids);
    };
    const auto suggestSelected = [packs](const SuggestionContext& context) {
        std::vector<std::string> ids;
        for (const std::string& id : packs->repository().selectedIds()) {
            ids.push_back(escaped(id));
        }
        return context.matching(ids);
    };

    root.literal("enable", [=](const CommandNodeBuilder& form) {
        form.argument("name", stringArgument(), [=](const CommandNodeBuilder& name) {
            name.suggestsFromServer(suggestUnselected);
            name.executes([=](const CommandContext& context) { return enable(context, atDefault); });
            for (const bool after : {true, false}) {
                name.literal(after ? "after" : "before", [=](const CommandNodeBuilder& one) {
                    one.argument("existing", stringArgument(), [=](const CommandNodeBuilder& existing) {
                        existing.suggestsFromServer(suggestSelected);
                        existing.executes([=](const CommandContext& context) {
                            return enable(context,
                                          [packNamed, after](const CommandContext& inner,
                                                             std::vector<std::string>& selected,
                                                             const Pack& pack) -> Parsed<void> {
                                              const Parsed<const Pack*> other = packNamed(inner, "existing", false);
                                              if (!other.has_value()) {
                                                  return std::unexpected{other.error()};
                                              }
                                              const auto at = std::ranges::find(selected, (*other)->id);
                                              selected.insert(after ? at + 1 : at, pack.id);
                                              return {};
                                          });
                        });
                    });
                });
            }
            name.literal("last", [=](const CommandNodeBuilder& one) {
                one.executes([=](const CommandContext& context) {
                    return enable(context,
                                  [](const CommandContext&, std::vector<std::string>& selected,
                                     const Pack& pack) -> Parsed<void> {
                                      selected.push_back(pack.id);
                                      return {};
                                  });
                });
            });
            name.literal("first", [=](const CommandNodeBuilder& one) {
                one.executes([=](const CommandContext& context) {
                    return enable(context,
                                  [](const CommandContext&, std::vector<std::string>& selected,
                                     const Pack& pack) -> Parsed<void> {
                                      selected.insert(selected.begin(), pack.id);
                                      return {};
                                  });
                });
            });
        });
    });

    root.literal("disable", [=, this](const CommandNodeBuilder& form) {
        form.argument("name", stringArgument(), [=, this](const CommandNodeBuilder& name) {
            name.suggestsFromServer(suggestSelected);
            name.executes([=, this](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<const Pack*> pack = packNamed(context, "name", false);
                if (!pack.has_value()) {
                    return std::unexpected{pack.error()};
                }
                std::vector<std::string> selected = packs->repository().selectedIds();
                std::erase(selected, (*pack)->id);
                context.success(Component::translatable("commands.datapack.modify.disable", {(*pack)->chatLink(true)}),
                                true);
                if (control_.reload && !control_.reload(selected)) {
                    context.dispatcher().feedback().failure(context.source(),
                                                            Component::translatable("commands.reload.failure"));
                }
                return static_cast<std::int32_t>(selected.size());
            });
        });
    });

    root.literal("list", [=](const CommandNodeBuilder& form) {
        form.executes([=](const CommandContext& context) { return listEnabled(context) + listAvailable(context); });
        form.literal("available", [=](const CommandNodeBuilder& one) {
            one.executes([=](const CommandContext& context) { return listAvailable(context); });
        });
        form.literal("enabled", [=](const CommandNodeBuilder& one) {
            one.executes([=](const CommandContext& context) { return listEnabled(context); });
        });
    });

    // parity: `/datapack create`, owners only: an empty pack with this build's format range.
    root.literal("create", [packs](const CommandNodeBuilder& form) {
        form.node().requiredLevel = permissionLevel::kOwners;
        form.argument("id", stringArgument(), [packs](const CommandNodeBuilder& id) {
            id.argument("description", componentArgument(), [packs](const CommandNodeBuilder& description) {
                description.executes([packs](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* name = context.get<std::string>("id");
                    const auto* text = context.get<Component>("description");
                    const std::string id = name == nullptr ? std::string{} : *name;
                    if (!validSegment(id)) {
                        return error("commands.datapack.create.invalid_name", {Component::text(id)});
                    }
                    const std::string directory = packs->repository().worldPacksDirectory() + "/" + id;
                    if (wire::io::fileIO::exists(directory)) {
                        return error("commands.datapack.create.already_exists", {Component::text(id)});
                    }
                    wire::common::JsonObject pack;
                    pack.set("description",
                             wire::text::componentJson::serialize(text == nullptr ? Component::empty() : *text));
                    pack.set("min_format", wire::common::JsonValue::of(
                                               static_cast<std::int64_t>(datapack::kCurrentDataFormat.major)));
                    pack.set("max_format", wire::common::JsonValue::of(
                                               static_cast<std::int64_t>(datapack::kCurrentDataFormat.major)));
                    wire::common::JsonObject top;
                    top.set("pack", wire::common::JsonValue::ofObject(std::move(pack)));
                    wire::io::fileIO::createDirectories(directory + "/data");
                    if (!wire::io::fileIO::writeText(
                            directory + "/pack.mcmeta",
                            wire::common::writeJson(wire::common::JsonValue::ofObject(std::move(top))))) {
                        return error("commands.datapack.create.io_failure", {Component::text(id)});
                    }
                    context.success(Component::translatable("commands.datapack.create.success", {Component::text(id)}),
                                    true);
                    return 1;
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
