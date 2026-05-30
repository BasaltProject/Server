#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>

namespace basalt::server::runtime::command {

[[nodiscard]] wire::nbt::NbtCompound entityNbtOf(const player::PlayerSession& player);

[[nodiscard]] wire::nbt::NbtCompound entityNbtOf(const entity::ServerEntity& entity);

/// parity: `ItemEntity.addAdditionalSaveData` plus the base entity fields a `data get` shows.
[[nodiscard]] wire::nbt::NbtCompound entityNbtOf(const world::DroppedItem& item);

/// parity: `CommandStorage`.
class CommandStorage {
public:
    static constexpr std::string_view kDefaultPath = "command_storage.nbt";

    explicit CommandStorage(std::string path = std::string{kDefaultPath}) : path_(std::move(path)) {}

    [[nodiscard]] wire::nbt::NbtCompound& get(const wire::common::Identifier& id);

    void set(const wire::common::Identifier& id, wire::nbt::NbtCompound value);

    [[nodiscard]] std::set<wire::common::Identifier> ids() const;

    void load();

    void save() const;

private:
    std::string path_;
    std::map<wire::common::Identifier, wire::nbt::NbtCompound> compounds_;
};

class DataHolderRef {
public:
    struct Storage {
        wire::common::Identifier id;
    };

    struct Block {
        std::array<std::int32_t, 3> pos{};
    };

    struct Entity {
        player::PlayerSession* player = nullptr;
        entity::ServerEntity* mob = nullptr;
        const world::DroppedItem* item = nullptr;
        std::string name;
    };

    using Payload = std::variant<Storage, Block, Entity>;

    explicit DataHolderRef(Payload payload) : payload_(std::move(payload)) {}

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

private:
    Payload payload_;
};

[[nodiscard]] std::optional<DataHolderRef> entityHolderOf(const CommandTarget& target);

/// parity: `EntityDataAccessor.setData`, which is `Entity.load` on the merged tag.
[[nodiscard]] std::optional<CommandSyntaxError> writeEntityData(const DataHolderRef::Entity& holder,
                                                                wire::nbt::NbtCompound tag);

class DataAccess {
public:
    explicit DataAccess(CommandStorage& storage) : storage_(&storage) {}

    [[nodiscard]] bool pathExists(const CommandContext& context, const DataHolderRef& holder,
                                  std::string_view path) const;

    [[nodiscard]] std::optional<wire::nbt::NbtCompound>
    readCompound(const CommandContext& context, const DataHolderRef& holder, const std::string* path) const;

    void store(const wire::common::Identifier& id, std::string_view path, double value, std::string_view type) const;

    void storeInEntity(const DataHolderRef::Entity& holder, std::string_view path, double value,
                       std::string_view type) const;

    void storeInBlock(const CommandContext& context, const std::array<std::int32_t, 3>& pos, std::string_view path,
                      double value, std::string_view type) const;

    [[nodiscard]] std::optional<wire::nbt::NbtCompound> read(const CommandContext& context,
                                                             const DataHolderRef& holder) const;

private:
    CommandStorage* storage_;
};

} // namespace basalt::server::runtime::command
