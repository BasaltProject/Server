#include <basalt/server/storage/ProgressRepository.hpp>

namespace basalt::server::storage {
StoredProgress ProgressRepository::load(std::string_view uuid) {
    StoredProgress progress;
    sql::Statement criteria =
        database_->prepare("SELECT advancement, criterion, obtained FROM advancement_criterion WHERE uuid = ?");
    criteria.bind(1, uuid);
    while (criteria.step()) {
        progress.criteria[criteria.text(0)].insert(criteria.text(1));
        progress.obtained[criteria.text(0)][criteria.text(1)] = criteria.integer(2);
    }
    sql::Statement recipes = database_->prepare("SELECT recipe FROM known_recipe WHERE uuid = ?");
    recipes.bind(1, uuid);
    while (recipes.step()) {
        progress.recipes.insert(recipes.text(0));
    }
    return progress;
}

void ProgressRepository::grantCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion,
                                        std::int64_t obtainedMillis) {
    sql::Statement statement = database_->prepare(
        "INSERT OR IGNORE INTO advancement_criterion(uuid, advancement, criterion, obtained) VALUES (?, ?, ?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.bind(3, criterion);
    statement.bind(4, obtainedMillis);
    statement.execute();
}

void ProgressRepository::revokeCriterion(std::string_view uuid, std::string_view advancement,
                                         std::string_view criterion) {
    sql::Statement statement =
        database_->prepare("DELETE FROM advancement_criterion WHERE uuid = ? AND advancement = ? AND criterion = ?");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.bind(3, criterion);
    statement.execute();
}

void ProgressRepository::revokeAdvancement(std::string_view uuid, std::string_view advancement) {
    sql::Statement statement =
        database_->prepare("DELETE FROM advancement_criterion WHERE uuid = ? AND advancement = ?");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.execute();
}

void ProgressRepository::learnRecipe(std::string_view uuid, std::string_view recipe) {
    sql::Statement statement = database_->prepare("INSERT OR IGNORE INTO known_recipe(uuid, recipe) VALUES (?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
}

void ProgressRepository::forgetRecipe(std::string_view uuid, std::string_view recipe) {
    sql::Statement statement = database_->prepare("DELETE FROM known_recipe WHERE uuid = ? AND recipe = ?");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
}
} // namespace basalt::server::storage
