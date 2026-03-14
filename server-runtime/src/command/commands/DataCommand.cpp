#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/DataPaths.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/DataCommand.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::text::Component;

[[nodiscard]] CommandSyntaxError noSuchPath(std::string_view path) {
    return CommandSyntaxError{
        .component = Component::translatable("arguments.nbtpath.nothing_found", {Component::text(std::string{path})})};
}

/// The block coordinates an argument named, resolved against whoever is running the command.
[[nodiscard]] std::array<std::int32_t, 3> blockAt(const CommandContext& context, std::string_view name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

/// The one entity a selector matched, as a holder. An item on the floor has no data view here.
[[nodiscard]] std::expected<DataHolderRef, CommandSyntaxError> entityOf(const CommandContext& context,
                                                                        std::string_view name) {
    const auto* matched = context.get<std::vector<CommandTarget>>(name);
    if (matched == nullptr || matched->size() != 1) {
        return std::unexpected{CommandSyntaxError{.component = Component::text("No data holder was given.")}};
    }
    std::optional<DataHolderRef> holder = entityHolderOf(matched->front());
    if (!holder.has_value()) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::text("An item on the floor carries no data in this build; " +
                                                            matched->front().name() + " does not.")}};
    }
    return *holder;
}

/// Which of the three holders an argument set names. Vanilla calls both the storage id and the
/// entity `target`, so which one it is follows from what parsed there.
[[nodiscard]] std::expected<DataHolderRef, CommandSyntaxError>
holderOf(const CommandContext& context, std::string_view posName, std::string_view targetName) {
    if (context.get<BlockCoordinates>(posName) != nullptr) {
        return DataHolderRef{DataHolderRef::Block{.pos = blockAt(context, posName)}};
    }
    if (const auto* id = context.get<Identifier>(targetName); id != nullptr) {
        return DataHolderRef{DataHolderRef::Storage{.id = *id}};
    }
    return entityOf(context, targetName);
}

/// parity: each holder has its own pair of strings, and the block ones take the three coordinates
/// rather than a description, so there is no generic form to share.
[[nodiscard]] Component holderMessage(const DataHolderRef& holder, std::string_view verb, std::vector<Component> tail) {
    if (const auto* named = holder.as<DataHolderRef::Storage>(); named != nullptr) {
        std::vector<Component> args{Component::text(named->id.toString())};
        args.insert(args.end(), tail.begin(), tail.end());
        return Component::translatable("commands.data.storage." + std::string{verb}, std::move(args));
    }
    if (const auto* entity = holder.as<DataHolderRef::Entity>(); entity != nullptr) {
        std::vector<Component> args{Component::text(entity->name)};
        args.insert(args.end(), tail.begin(), tail.end());
        return Component::translatable("commands.data.entity." + std::string{verb}, std::move(args));
    }
    const auto* block = holder.as<DataHolderRef::Block>();
    std::vector<Component> args{Component::text(std::to_string(block->pos[0])),
                                Component::text(std::to_string(block->pos[1])),
                                Component::text(std::to_string(block->pos[2]))};
    args.insert(args.end(), tail.begin(), tail.end());
    return Component::translatable("commands.data.block." + std::string{verb}, std::move(args));
}

/// The result `/data get` reports, which for a number is the number itself.
[[nodiscard]] std::optional<std::int32_t> numeric(const NbtTag& tag) {
    if (const auto* value = tag.get<std::int32_t>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag.get<float>(); value != nullptr) {
        return static_cast<std::int32_t>(*value);
    }
    if (const auto* value = tag.get<double>(); value != nullptr) {
        return static_cast<std::int32_t>(*value);
    }
    return std::nullopt;
}

} // namespace

DataCommand::DataCommand(CommandStorage& storage)
    : Command("data", permissionLevel::kGamemasters), storage_(&storage) {}

void DataCommand::build(const CommandNodeBuilder& root) const {
    const DataAccess access{*storage_};

    /// Reading a holder, refusing an unloaded chunk and a block with nothing in it by name.
    const auto readHolder = [access](const CommandContext& context,
                                     const DataHolderRef& holder) -> std::expected<NbtCompound, CommandSyntaxError> {
        if (const auto* block = holder.as<DataHolderRef::Block>(); block != nullptr) {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            if ((*world)->state().chunkIfLoaded(block->pos[0] >> 4, block->pos[2] >> 4) == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")}};
            }
        }
        const std::optional<NbtCompound> read = access.read(context, holder);
        if (!read.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.data.block.invalid")}};
        }
        return *read;
    };

    const auto writeHolder = [this](const CommandContext& context, const DataHolderRef& holder,
                                    const NbtCompound& value) -> std::optional<CommandSyntaxError> {
        if (const auto* named = holder.as<DataHolderRef::Storage>(); named != nullptr) {
            storage_->set(named->id, value);
            return std::nullopt;
        }
        if (const auto* entity = holder.as<DataHolderRef::Entity>(); entity != nullptr) {
            return writeEntityData(*entity, value);
        }
        const auto* block = holder.as<DataHolderRef::Block>();
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return world.error();
        }
        server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(block->pos[0] >> 4, block->pos[2] >> 4);
        if (chunk == nullptr) {
            return CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")};
        }
        const server::world::ChunkBlockEntity* existing =
            chunk->blockEntityAt(block->pos[0] & 15, block->pos[1], block->pos[2] & 15);
        if (existing == nullptr) {
            return CommandSyntaxError{.component = Component::translatable("commands.data.block.invalid")};
        }
        // parity: `loadAdditional` runs on what a command writes, and two block entities do
        // something with it: a command block that has just become automatic books a turn, and a
        // lectern resolves the book it was handed.
        const bool wasAutomatic = existing->data().getBoolean("auto");
        chunk->setBlockEntity(server::world::ChunkBlockEntity{existing->localX(), existing->y(), existing->localZ(),
                                                              existing->type(), value});
        world::commandBlocks::onDataLoaded((*world)->state(), block->pos[0], block->pos[1], block->pos[2],
                                           wasAutomatic);
        static_cast<void>(world::lecterns::resolveBook((*world)->state(), block->pos[0], block->pos[1], block->pos[2]));
        return std::nullopt;
    };

    /// The three `(block|entity|storage)` forms, declared once per verb the way vanilla does.
    const auto holders = [](const CommandNodeBuilder& node, std::string_view posName, std::string_view targetName,
                            const std::function<void(const CommandNodeBuilder&)>& build) {
        node.literal("block", [posName, build](const CommandNodeBuilder& form) {
            form.argument(std::string{posName}, blockPosArgument(), build);
        });
        node.literal("entity", [targetName, build](const CommandNodeBuilder& form) {
            form.argument(std::string{targetName}, entitiesArgument(true), build);
        });
        node.literal("storage", [targetName, build](const CommandNodeBuilder& form) {
            form.argument(std::string{targetName}, resourceLocationArgument(), build);
        });
    };

    root.literal("get", [holders, readHolder](const CommandNodeBuilder& form) {
        const auto get = [readHolder](const CommandContext& context, bool pathed, bool scaled) -> Parsed<std::int32_t> {
            const auto holder = holderOf(context, "targetPos", "target");
            if (!holder.has_value()) {
                return std::unexpected{holder.error()};
            }
            const auto root = readHolder(context, *holder);
            if (!root.has_value()) {
                return std::unexpected{root.error()};
            }
            const auto* path = pathed ? context.get<std::string>("path") : nullptr;
            std::optional<NbtTag> value = NbtTag::ofCompound(*root);
            if (path != nullptr) {
                value = dataPaths::resolve(*value, *path);
                if (!value.has_value()) {
                    return std::unexpected{noSuchPath(*path)};
                }
            }
            const std::optional<std::int32_t> number = numeric(*value);
            if (path != nullptr && number.has_value() && scaled) {
                const auto* scale = context.get<double>("scale");
                const double result = static_cast<double>(*number) * (scale == nullptr ? 1.0 : *scale);
                context.success(
                    Component::translatable("commands.data.get.query.scaled",
                                            {Component::text(*path), Component::text(std::to_string(result))}));
                return static_cast<std::int32_t>(result);
            }
            context.success(holderMessage(*holder, "query", {Component::text(wire::nbt::snbt::stringify(*value))}));
            return number.value_or(1);
        };
        holders(form, "targetPos", "target", [get](const CommandNodeBuilder& holder) {
            holder.executes([get](const CommandContext& context) { return get(context, false, false); });
            holder.argument("path", nbtPathArgument(), [get](const CommandNodeBuilder& path) {
                path.executes([get](const CommandContext& context) { return get(context, true, false); });
                path.argument("scale", doubleArgument(), [get](const CommandNodeBuilder& scale) {
                    scale.executes([get](const CommandContext& context) { return get(context, true, true); });
                });
            });
        });
    });

    root.literal("merge", [holders, readHolder, writeHolder](const CommandNodeBuilder& form) {
        holders(form, "targetPos", "target", [readHolder, writeHolder](const CommandNodeBuilder& holder) {
            holder.argument("nbt", nbtCompoundArgument(), [readHolder, writeHolder](const CommandNodeBuilder& nbt) {
                nbt.executes([readHolder, writeHolder](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto which = holderOf(context, "targetPos", "target");
                    if (!which.has_value()) {
                        return std::unexpected{which.error()};
                    }
                    const auto root = readHolder(context, *which);
                    if (!root.has_value()) {
                        return std::unexpected{root.error()};
                    }
                    const auto* incoming = context.get<NbtCompound>("nbt");
                    if (incoming == nullptr) {
                        return 0;
                    }
                    if (const std::optional<CommandSyntaxError> failure =
                            writeHolder(context, *which, dataPaths::merge(*root, *incoming));
                        failure.has_value()) {
                        return std::unexpected{*failure};
                    }
                    context.success(holderMessage(*which, "modified", {}), true);
                    return 1;
                });
            });
        });
    });

    buildModify(root);

    root.literal("remove", [holders, readHolder, writeHolder](const CommandNodeBuilder& form) {
        holders(form, "targetPos", "target", [readHolder, writeHolder](const CommandNodeBuilder& holder) {
            holder.argument("path", nbtPathArgument(), [readHolder, writeHolder](const CommandNodeBuilder& path) {
                path.executes([readHolder, writeHolder](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto which = holderOf(context, "targetPos", "target");
                    if (!which.has_value()) {
                        return std::unexpected{which.error()};
                    }
                    const auto root = readHolder(context, *which);
                    if (!root.has_value()) {
                        return std::unexpected{root.error()};
                    }
                    const auto* named = context.get<std::string>("path");
                    const std::optional<NbtCompound> removed =
                        named == nullptr ? std::nullopt : dataPaths::remove(*root, *named);
                    if (!removed.has_value()) {
                        return std::unexpected{noSuchPath(named == nullptr ? std::string{} : *named)};
                    }
                    if (const std::optional<CommandSyntaxError> failure = writeHolder(context, *which, *removed);
                        failure.has_value()) {
                        return std::unexpected{*failure};
                    }
                    context.success(holderMessage(*which, "modified", {}), true);
                    return 1;
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
