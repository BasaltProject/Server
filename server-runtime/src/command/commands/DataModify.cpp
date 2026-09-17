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
using wire::nbt::NbtType;
using wire::text::Component;

/// The three holders, named as vanilla names them on the reading side of a `modify`.
void sourceHolders(const CommandNodeBuilder& node, const std::function<void(const CommandNodeBuilder&)>& build) {
    node.literal("block",
                 [build](const CommandNodeBuilder& form) { form.argument("sourcePos", blockPosArgument(), build); });
    node.literal("entity",
                 [build](const CommandNodeBuilder& form) { form.argument("source", entitiesArgument(true), build); });
    node.literal("storage", [build](const CommandNodeBuilder& form) {
        form.argument("source", resourceLocationArgument(), build);
    });
}

[[nodiscard]] CommandSyntaxError noSuchPath(std::string_view path) {
    return CommandSyntaxError{
        .component = Component::translatable("arguments.nbtpath.nothing_found", {Component::text(std::string{path})})};
}

/// parity: a negative index counts back from the end of the string.
[[nodiscard]] std::int32_t stringIndex(std::int32_t value, std::size_t length) {
    return value < 0 ? static_cast<std::int32_t>(length) + value : value;
}

} // namespace

void DataCommand::buildModify(const CommandNodeBuilder& root) const {
    const DataAccess access{*storage_};

    /// The holder an argument set names, shared with the read half but looked up by its own names.
    const auto holderNamed = [](const CommandContext& context, std::string_view posName,
                                std::string_view targetName) -> std::expected<DataHolderRef, CommandSyntaxError> {
        if (const auto* coordinates = context.get<BlockCoordinates>(posName); coordinates != nullptr) {
            return DataHolderRef{DataHolderRef::Block{.pos = coordinates->resolve(context.source())}};
        }
        if (const auto* id = context.get<Identifier>(targetName); id != nullptr) {
            return DataHolderRef{DataHolderRef::Storage{.id = *id}};
        }
        const auto* matched = context.get<std::vector<CommandTarget>>(targetName);
        std::optional<DataHolderRef> holder =
            matched == nullptr || matched->size() != 1 ? std::nullopt : entityHolderOf(matched->front());
        if (!holder.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::text("No data holder was given.")}};
        }
        return *holder;
    };

    /// parity: `/data modify ... from`, which copies whatever is at that path, compound or not.
    const auto sourceValue = [access, holderNamed](const CommandContext& context,
                                                   bool pathed) -> std::expected<NbtTag, CommandSyntaxError> {
        const auto holder = holderNamed(context, "sourcePos", "source");
        if (!holder.has_value()) {
            return std::unexpected{holder.error()};
        }
        const std::optional<NbtCompound> root = access.read(context, *holder);
        if (!root.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.data.block.invalid")}};
        }
        if (!pathed) {
            return NbtTag::ofCompound(*root);
        }
        const auto* path = context.get<std::string>("sourcePath");
        const std::optional<NbtTag> found =
            path == nullptr ? std::nullopt : dataPaths::resolve(NbtTag::ofCompound(*root), *path);
        if (!found.has_value()) {
            return std::unexpected{noSuchPath(path == nullptr ? std::string{} : *path)};
        }
        return *found;
    };

    /// parity: `/data modify ... string`, which is the same value as text, optionally a slice of it.
    const auto sourceText = [sourceValue](const CommandContext& context,
                                          bool pathed) -> std::expected<NbtTag, CommandSyntaxError> {
        const auto value = sourceValue(context, pathed);
        if (!value.has_value()) {
            return std::unexpected{value.error()};
        }
        const auto* asString = value->get<std::string>();
        const std::string text = asString != nullptr ? *asString : wire::nbt::snbt::stringify(*value);
        const auto* from = context.get<std::int32_t>("start");
        const auto* to = context.get<std::int32_t>("end");
        const std::int32_t start = from == nullptr ? 0 : stringIndex(*from, text.size());
        const std::int32_t end = to == nullptr ? static_cast<std::int32_t>(text.size()) : stringIndex(*to, text.size());
        if (start > end || start < 0 || end < 0 || static_cast<std::size_t>(start) > text.size() ||
            static_cast<std::size_t>(end) > text.size()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.data.modify.invalid_substring",
                                                                        {Component::text(std::to_string(start)),
                                                                         Component::text(std::to_string(end))})}};
        }
        return NbtTag::ofString(text.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start)));
    };

    /// Which of the five `/data modify` operations to run with an already-resolved value.
    const auto apply = [this, access, holderNamed](const CommandContext& context, std::string_view mode,
                                                   const NbtTag& value) -> Parsed<std::int32_t> {
        const auto holder = holderNamed(context, "targetPos", "target");
        if (!holder.has_value()) {
            return std::unexpected{holder.error()};
        }
        const std::optional<NbtCompound> root = access.read(context, *holder);
        const auto* path = context.get<std::string>("targetPath");
        if (!root.has_value() || path == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.data.block.invalid")}};
        }
        const std::optional<NbtTag> existing = dataPaths::resolve(NbtTag::ofCompound(*root), *path);

        NbtTag next = value;
        std::int32_t result = 1;
        if (mode == "merge") {
            const auto* base = existing.has_value() ? existing->get<NbtCompound>() : nullptr;
            const auto* incoming = value.get<NbtCompound>();
            if (base != nullptr && incoming != nullptr) {
                next = NbtTag::ofCompound(dataPaths::merge(*base, *incoming));
            }
        } else if (mode != "set") {
            // `append`, `prepend` and `insert <index>`, which are the list forms.
            std::vector<NbtTag> elements;
            if (const auto* list = existing.has_value() ? existing->get<NbtList>() : nullptr; list != nullptr) {
                elements = list->elements();
            }
            const auto* asked = context.get<std::int32_t>("index");
            std::int32_t index = static_cast<std::int32_t>(elements.size());
            if (mode == "prepend") {
                index = 0;
            } else if (mode == "insert") {
                index = asked == nullptr ? 0 : *asked;
                if (index < 0) {
                    index = static_cast<std::int32_t>(elements.size()) + index + 1;
                }
            }
            if (index < 0 || static_cast<std::size_t>(index) > elements.size()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.data.modify.invalid_index",
                                                                            {Component::text(std::to_string(index))})}};
            }
            elements.insert(elements.begin() + index, value);
            const NbtType elementType = elements.front().type();
            for (const NbtTag& element : elements) {
                if (element.type() != elementType) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.data.modify.expected_list",
                                                                                {Component::text(*path)})}};
                }
            }
            result = static_cast<std::int32_t>(elements.size());
            next = NbtTag::ofList(NbtList{elementType, std::move(elements)});
        }

        const std::optional<NbtCompound> written = dataPaths::set(*root, *path, next);
        if (!written.has_value()) {
            return std::unexpected{noSuchPath(*path)};
        }
        if (const auto* named = holder->as<DataHolderRef::Storage>(); named != nullptr) {
            storage_->set(named->id, *written);
        } else if (const auto* entity = holder->as<DataHolderRef::Entity>(); entity != nullptr) {
            if (const std::optional<CommandSyntaxError> refused = writeEntityData(*entity, *written);
                refused.has_value()) {
                return std::unexpected{*refused};
            }
        } else {
            const auto* block = holder->as<DataHolderRef::Block>();
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(block->pos[0] >> 4, block->pos[2] >> 4);
            const server::world::ChunkBlockEntity* existingEntity =
                chunk == nullptr ? nullptr
                                 : chunk->blockEntityAt(block->pos[0] & 15, block->pos[1], block->pos[2] & 15);
            if (existingEntity == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.data.block.invalid")}};
            }
            // parity: `loadAdditional` ends in `setAutomatic`, which books a turn when `auto` has just
            // come on. Writing the data is what starts a repeating command block nobody has powered.
            const bool wasAutomatic = existingEntity->data().getBoolean("auto");
            chunk->setBlockEntity(server::world::ChunkBlockEntity{existingEntity->localX(), existingEntity->y(),
                                                                  existingEntity->localZ(), existingEntity->type(),
                                                                  *written});
            world::commandBlocks::onDataLoaded((*world)->state(), block->pos[0], block->pos[1], block->pos[2],
                                               wasAutomatic);
            // parity: `LecternBlockEntity.loadAdditional` resolves the book it was handed, which is
            // what marks a written one `resolved` even when it never went through a player's hand.
            static_cast<void>(
                world::lecterns::resolveBook((*world)->state(), block->pos[0], block->pos[1], block->pos[2]));
        }

        if (const auto* named = holder->as<DataHolderRef::Storage>(); named != nullptr) {
            context.success(
                Component::translatable("commands.data.storage.modified", {Component::text(named->id.toString())}),
                true);
        } else if (const auto* entity = holder->as<DataHolderRef::Entity>(); entity != nullptr) {
            context.success(Component::translatable("commands.data.entity.modified", {Component::text(entity->name)}),
                            true);
        } else {
            const auto* block = holder->as<DataHolderRef::Block>();
            context.success(Component::translatable("commands.data.block.modified",
                                                    {Component::text(std::to_string(block->pos[0])),
                                                     Component::text(std::to_string(block->pos[1])),
                                                     Component::text(std::to_string(block->pos[2]))}),
                            true);
        }
        return result;
    };

    /// Where a modified value comes from: a literal tag, another holder, or one as text.
    const auto sources = [apply, sourceValue, sourceText](const CommandNodeBuilder& node, std::string_view mode) {
        node.literal("value", [apply, mode](const CommandNodeBuilder& form) {
            form.argument("value", nbtTagArgument(), [apply, mode](const CommandNodeBuilder& value) {
                value.executes([apply, mode](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* tag = context.get<NbtTag>("value");
                    return tag == nullptr ? Parsed<std::int32_t>{0} : apply(context, mode, *tag);
                });
            });
        });
        node.literal("from", [apply, mode, sourceValue](const CommandNodeBuilder& form) {
            sourceHolders(form, [apply, mode, sourceValue](const CommandNodeBuilder& holder) {
                holder.executes([apply, mode, sourceValue](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto value = sourceValue(context, false);
                    return value.has_value() ? apply(context, mode, *value)
                                             : Parsed<std::int32_t>{std::unexpected{value.error()}};
                });
                holder.argument("sourcePath", nbtPathArgument(),
                                [apply, mode, sourceValue](const CommandNodeBuilder& path) {
                                    path.executes([apply, mode,
                                                   sourceValue](const CommandContext& context) -> Parsed<std::int32_t> {
                                        const auto value = sourceValue(context, true);
                                        return value.has_value() ? apply(context, mode, *value)
                                                                 : Parsed<std::int32_t>{std::unexpected{value.error()}};
                                    });
                                });
            });
        });
        node.literal("string", [apply, mode, sourceText](const CommandNodeBuilder& form) {
            sourceHolders(form, [apply, mode, sourceText](const CommandNodeBuilder& holder) {
                const auto run = [apply, mode, sourceText](const CommandContext& context,
                                                           bool pathed) -> Parsed<std::int32_t> {
                    const auto value = sourceText(context, pathed);
                    return value.has_value() ? apply(context, mode, *value)
                                             : Parsed<std::int32_t>{std::unexpected{value.error()}};
                };
                holder.executes([run](const CommandContext& context) { return run(context, false); });
                holder.argument("sourcePath", nbtPathArgument(), [run](const CommandNodeBuilder& path) {
                    path.executes([run](const CommandContext& context) { return run(context, true); });
                    path.argument("start", integerArgument(), [run](const CommandNodeBuilder& start) {
                        start.executes([run](const CommandContext& context) { return run(context, true); });
                        start.argument("end", integerArgument(), [run](const CommandNodeBuilder& end) {
                            end.executes([run](const CommandContext& context) { return run(context, true); });
                        });
                    });
                });
            });
        });
    };

    root.literal("modify", [sources](const CommandNodeBuilder& form) {
        const auto holders = [](const CommandNodeBuilder& node,
                                const std::function<void(const CommandNodeBuilder&)>& build) {
            node.literal("block", [build](const CommandNodeBuilder& one) {
                one.argument("targetPos", blockPosArgument(), build);
            });
            node.literal("entity", [build](const CommandNodeBuilder& one) {
                one.argument("target", entitiesArgument(true), build);
            });
            node.literal("storage", [build](const CommandNodeBuilder& one) {
                one.argument("target", resourceLocationArgument(), build);
            });
        };
        holders(form, [sources](const CommandNodeBuilder& holder) {
            holder.argument("targetPath", nbtPathArgument(), [sources](const CommandNodeBuilder& path) {
                for (const std::string_view mode : {"append", "insert", "merge", "prepend", "set"}) {
                    path.literal(std::string{mode}, [sources, mode](const CommandNodeBuilder& one) {
                        if (mode == "insert") {
                            one.argument("index", integerArgument(),
                                         [sources, mode](const CommandNodeBuilder& index) { sources(index, mode); });
                            return;
                        }
                        sources(one, mode);
                    });
                }
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
