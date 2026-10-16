#include <basalt/server/storage/ProgressRepository.hpp>

namespace basalt::server::storage {
StoredProgress ProgressRepository::load(std::string_view uuid) {
    StoredProgress progress;
    wire::sql::Statement criteria =
        database_->prepare("SELECT advancement, criterion, obtained FROM advancement_criterion WHERE uuid = ?");
    criteria.bind(1, uuid);
    while (criteria.step()) {
        progress.criteria[criteria.text(0)].insert(criteria.text(1));
        progress.obtained[criteria.text(0)][criteria.text(1)] = criteria.integer(2);
    }
    wire::sql::Statement recipes = database_->prepare("SELECT recipe FROM known_recipe WHERE uuid = ?");
    recipes.bind(1, uuid);
    while (recipes.step()) {
        progress.recipes.insert(recipes.text(0));
    }
    wire::sql::Statement highlights = database_->prepare("SELECT recipe FROM recipe_highlight WHERE uuid = ?");
    highlights.bind(1, uuid);
    while (highlights.step()) {
        progress.highlighted.insert(highlights.text(0));
    }
    wire::sql::Statement settings =
        database_->prepare("SELECT book, open, filtering FROM recipe_book_setting WHERE uuid = ?");
    settings.bind(1, uuid);
    while (settings.step()) {
        progress.bookSettings[static_cast<std::int32_t>(settings.integer(0))] = {settings.integer(1) != 0,
                                                                                 settings.integer(2) != 0};
    }
    return progress;
}

void ProgressRepository::grantCriterion(std::string_view uuid, std::string_view advancement, std::string_view criterion,
                                        std::int64_t obtainedMillis) {
    wire::sql::Statement statement = database_->prepare(
        "INSERT OR IGNORE INTO advancement_criterion(uuid, advancement, criterion, obtained) VALUES (?, ?, ?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.bind(3, criterion);
    statement.bind(4, obtainedMillis);
    statement.execute();
}

void ProgressRepository::revokeCriterion(std::string_view uuid, std::string_view advancement,
                                         std::string_view criterion) {
    wire::sql::Statement statement =
        database_->prepare("DELETE FROM advancement_criterion WHERE uuid = ? AND advancement = ? AND criterion = ?");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.bind(3, criterion);
    statement.execute();
}

void ProgressRepository::revokeAdvancement(std::string_view uuid, std::string_view advancement) {
    wire::sql::Statement statement =
        database_->prepare("DELETE FROM advancement_criterion WHERE uuid = ? AND advancement = ?");
    statement.bind(1, uuid);
    statement.bind(2, advancement);
    statement.execute();
}

void ProgressRepository::learnRecipe(std::string_view uuid, std::string_view recipe) {
    wire::sql::Statement statement =
        database_->prepare("INSERT OR IGNORE INTO known_recipe(uuid, recipe) VALUES (?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
    addHighlight(uuid, recipe);
}

void ProgressRepository::forgetRecipe(std::string_view uuid, std::string_view recipe) {
    wire::sql::Statement statement = database_->prepare("DELETE FROM known_recipe WHERE uuid = ? AND recipe = ?");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
    removeHighlight(uuid, recipe);
}

void ProgressRepository::setBookSetting(std::string_view uuid, std::int32_t book, bool open, bool filtering) {
    wire::sql::Statement statement = database_->prepare(
        "INSERT OR REPLACE INTO recipe_book_setting(uuid, book, open, filtering) VALUES (?, ?, ?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, static_cast<std::int64_t>(book));
    statement.bind(3, static_cast<std::int64_t>(open ? 1 : 0));
    statement.bind(4, static_cast<std::int64_t>(filtering ? 1 : 0));
    statement.execute();
}

void ProgressRepository::addHighlight(std::string_view uuid, std::string_view recipe) {
    wire::sql::Statement statement =
        database_->prepare("INSERT OR IGNORE INTO recipe_highlight(uuid, recipe) VALUES (?, ?)");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
}

void ProgressRepository::removeHighlight(std::string_view uuid, std::string_view recipe) {
    wire::sql::Statement statement = database_->prepare("DELETE FROM recipe_highlight WHERE uuid = ? AND recipe = ?");
    statement.bind(1, uuid);
    statement.bind(2, recipe);
    statement.execute();
}
} // namespace basalt::server::storage
