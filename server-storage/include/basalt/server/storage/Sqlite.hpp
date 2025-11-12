#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace basalt::server::storage::sql {

class Database;

/// One prepared statement, bound and stepped. Parameters are one-based, columns zero-based, the way
/// SQLite itself numbers them.
class Statement {
public:
    Statement() = default;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) noexcept;
    Statement& operator=(Statement&&) noexcept;
    ~Statement();

    [[nodiscard]] bool isValid() const noexcept { return handle_ != nullptr; }

    void bind(std::int32_t parameter, std::int64_t value);
    void bind(std::int32_t parameter, double value);
    void bind(std::int32_t parameter, std::string_view value);

    /// Its own overload, or a `std::string` is ambiguous between a view and an optional.
    void bind(std::int32_t parameter, const std::string& value) { bind(parameter, std::string_view{value}); }

    void bind(std::int32_t parameter, std::span<const std::uint8_t> value);
    void bind(std::int32_t parameter, const std::optional<std::string>& value);
    void bind(std::int32_t parameter, const std::optional<std::int64_t>& value);
    /// An empty blob is a null column: what the Kotlin wrote when there were no stats to store.
    void bindBlobOrNull(std::int32_t parameter, std::span<const std::uint8_t> value);

    /// Advances to the next row. False once there are none left, or on failure, which raises the
    /// database's sticky error instead of throwing.
    [[nodiscard]] bool step();

    /// Runs a statement that answers no rows.
    bool execute();

    [[nodiscard]] bool isNull(std::int32_t column) const;
    [[nodiscard]] std::int64_t integer(std::int32_t column) const;
    [[nodiscard]] double real(std::int32_t column) const;
    [[nodiscard]] std::string text(std::int32_t column) const;
    [[nodiscard]] std::optional<std::string> textOrNull(std::int32_t column) const;
    [[nodiscard]] std::optional<std::int64_t> integerOrNull(std::int32_t column) const;
    [[nodiscard]] std::vector<std::uint8_t> blob(std::int32_t column) const;

private:
    friend class Database;

    Statement(sqlite3_stmt* handle, Database* owner) : handle_(handle), owner_(owner) {}

    sqlite3_stmt* handle_ = nullptr;
    Database* owner_ = nullptr;
};

/// One open database file.
///
/// Failures raise a sticky error rather than propagating through every call: a query that fails on a
/// healthy file means the disk or the schema is wrong, and threading an error through twenty
/// repository methods buys nothing a single check after a batch does not.
class Database {
public:
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    ~Database();

    /// Opens, creating the file if it is not there.
    [[nodiscard]] static std::optional<Database> open(const std::string& path);

    [[nodiscard]] bool isOpen() const noexcept { return handle_ != nullptr; }

    [[nodiscard]] Statement prepare(std::string_view sql);

    /// Runs one or more statements that answer no rows, such as the schema.
    bool exec(std::string_view sql);

    /// Wraps `action` in one transaction, rolled back if it answers false or raises the error.
    template <typename Action>
    bool transaction(Action&& action) {
        if (!exec("BEGIN")) {
            return false;
        }
        if (!action() || hasError()) {
            exec("ROLLBACK");
            return false;
        }
        return exec("COMMIT");
    }

    /// The tables this file has, `sqlite_` internals excluded.
    [[nodiscard]] std::vector<std::string> tables();

    /// The columns of one table, in declaration order.
    [[nodiscard]] std::vector<std::string> columns(std::string_view table);

    [[nodiscard]] bool hasError() const noexcept { return !error_.empty(); }

    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    void clearError() { error_.clear(); }

    void close();

private:
    friend class Statement;

    void raise(std::string_view what);

    sqlite3* handle_ = nullptr;
    std::string error_;
};

} // namespace basalt::server::storage::sql
