#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>

namespace basalt::server::runtime::command {

/// A live entity as NBT, in the shape vanilla's own entity data uses for the same fields.
///
/// One view for `/data get entity` and for a selector's `nbt=`, because a player who reads a field
/// out of the first and filters on it with the second has to see the same name and the same type.
/// Only what this build models: everything else arrives with the system that makes it exist.
[[nodiscard]] wire::nbt::NbtCompound entityNbtOf(const player::PlayerSession& player);

[[nodiscard]] wire::nbt::NbtCompound entityNbtOf(const entity::ServerEntity& entity);

/// Named NBT compounds a command can keep things in. parity: `CommandStorage`.
class CommandStorage {
public:
    /// Beside `worlds/` and `players.db`: it belongs to the server, not to one world.
    static constexpr std::string_view kDefaultPath = "command_storage.nbt";

    explicit CommandStorage(std::string path = std::string{kDefaultPath}) : path_(std::move(path)) {}

    [[nodiscard]] wire::nbt::NbtCompound& get(const wire::common::Identifier& id);

    void set(const wire::common::Identifier& id, wire::nbt::NbtCompound value);

    [[nodiscard]] std::set<wire::common::Identifier> ids() const;

    /// Reads whatever the last run left. A missing or unreadable file simply starts empty.
    void load();

    void save() const;

private:
    /// Where it is kept between runs. parity: vanilla's `command_storage_<namespace>.dat`, in one
    /// file.
    std::string path_;
    std::map<wire::common::Identifier, wire::nbt::NbtCompound> compounds_;
};

/// Which holder a `/data` or `/execute ... data` clause is talking about.
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

/// The slice of `/data` that `/execute` and `/function` need: does this path exist, what compound is
/// there, and write this number into it.
///
/// A separate surface rather than a reference to the command, so neither depends on a command's
/// tree to read a compound.
class DataAccess {
public:
    explicit DataAccess(CommandStorage& storage) : storage_(&storage) {}

    [[nodiscard]] bool pathExists(const CommandContext& context, const DataHolderRef& holder,
                                  std::string_view path) const;

    /// The compound at this holder and path, for `/function ... with`. Nothing when there is none.
    [[nodiscard]] std::optional<wire::nbt::NbtCompound>
    readCompound(const CommandContext& context, const DataHolderRef& holder, const std::string* path) const;

    /// `/execute store ... storage <id> <path> <type> <scale>`, which writes the typed number.
    void store(const wire::common::Identifier& id, std::string_view path, double value, std::string_view type) const;

    /// `/execute store ... block <pos> <path> <type> <scale>`, which writes into a block entity.
    void storeInBlock(const CommandContext& context, const std::array<std::int32_t, 3>& pos, std::string_view path,
                      double value, std::string_view type) const;

    /// The whole compound a holder carries, or nothing when it has none to read.
    [[nodiscard]] std::optional<wire::nbt::NbtCompound> read(const CommandContext& context,
                                                             const DataHolderRef& holder) const;

private:
    CommandStorage* storage_;
};

} // namespace basalt::server::runtime::command
