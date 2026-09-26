#pragma once

#include <basalt/server/storage/Sqlite.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::storage {

/// One player's granted criteria, by advancement id, and the recipes in their book.
struct StoredProgress {
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>> criteria;
    /// When each criterion was granted, in epoch milliseconds, by advancement then criterion.
    std::map<std::string, std::map<std::string, std::int64_t, std::less<>>, std::less<>> obtained;
    std::set<std::string, std::less<>> recipes;
};

/// Advancement criteria and recipe books, written as they change rather than at save time.
///
/// A grant is one row, so there is nothing to lose to a crash between the command and the next save,
/// which is the failure `advancements/<uuid>.json` has: vanilla rewrites the whole file and a
/// truncated one is the whole player's progress.
class ProgressRepository {
public:
    explicit ProgressRepository(sql::Database& database) : database_(&database) {}

    [[nodiscard]] StoredProgress load(std::string_view uuid);

    void grantCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion,
                        std::int64_t obtainedMillis = 0);

    void revokeCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion);

    void revokeAdvancement(std::string_view uuid, std::string_view advancement);

    void learnRecipe(std::string_view uuid, std::string_view recipe);

    void forgetRecipe(std::string_view uuid, std::string_view recipe);

private:
    sql::Database* database_;
};

} // namespace basalt::server::storage
