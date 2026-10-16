#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::storage {
struct StoredProgress {
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> criteria;
    std::map<std::string, std::map<std::string, std::int64_t, std::less<>>, std::less<>> obtained;
    std::set<std::string, std::less<>> recipes;
    std::set<std::string, std::less<>> highlighted;
    /// open and filtering per RecipeBookType ordinal.
    std::map<std::int32_t, std::pair<bool, bool>> bookSettings;
};

class ProgressRepository {
public:
    explicit ProgressRepository(wire::sql::Database& database) : database_(&database) {}

    [[nodiscard]] StoredProgress load(std::string_view uuid);

    void grantCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion,
                        std::int64_t obtainedMillis = 0);

    void revokeCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion);

    void revokeAdvancement(std::string_view uuid, std::string_view advancement);

    void learnRecipe(std::string_view uuid, std::string_view recipe);

    void forgetRecipe(std::string_view uuid, std::string_view recipe);

    void setBookSetting(std::string_view uuid, std::int32_t book, bool open, bool filtering);

    void addHighlight(std::string_view uuid, std::string_view recipe);

    void removeHighlight(std::string_view uuid, std::string_view recipe);

private:
    wire::sql::Database* database_;
};
} // namespace basalt::server::storage
