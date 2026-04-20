#include <basalt/server/world/crafting/StackedContents.hpp>

#include <algorithm>
#include <limits>

namespace basalt::server::world::crafting {

class RecipePicker {
public:
    RecipePicker(StackedContents& contents, std::span<const Ingredient* const> ingredients)
        : contents_(contents), ingredients_(ingredients),
          ingredientCount_(static_cast<std::int32_t>(ingredients.size())) {
        for (const auto& [item, amount] : contents_.amounts_) {
            if (amount > 0 && std::ranges::any_of(
                                  ingredients_, [&item](const Ingredient* one) { return one->items.contains(item); })) {
                items_.push_back(item);
            }
        }
        itemCount_ = static_cast<std::int32_t>(items_.size());
        visitedIngredient_.assign(static_cast<std::size_t>(ingredientCount_), false);
        visitedItem_.assign(static_cast<std::size_t>(itemCount_), false);
        satisfied_.assign(static_cast<std::size_t>(ingredientCount_), false);
        connection_.assign(static_cast<std::size_t>(ingredientCount_ * itemCount_), false);
        residual_.assign(static_cast<std::size_t>(ingredientCount_ * itemCount_), false);
        for (std::int32_t ingredient = 0; ingredient < ingredientCount_; ++ingredient) {
            for (std::int32_t item = 0; item < itemCount_; ++item) {
                if (ingredients_[static_cast<std::size_t>(ingredient)]->items.contains(items_[idx(item)])) {
                    connection_[cell(item, ingredient)] = true;
                }
            }
        }
    }

    bool tryPick(std::int32_t capacity, std::vector<std::string>* output) {
        if (capacity <= 0) {
            return true;
        }
        std::int32_t satisfiedCount = 0;
        while (true) {
            const std::vector<std::int32_t>* path = tryAssigningNewItem(capacity);
            if (path == nullptr) {
                const bool valid = satisfiedCount == ingredientCount_;
                const bool hasOutput = valid && output != nullptr;
                clearVisited();
                std::ranges::fill(satisfied_, false);
                for (std::int32_t ingredient = 0; ingredient < ingredientCount_; ++ingredient) {
                    for (std::int32_t item = 0; item < itemCount_; ++item) {
                        if (residual_[cell(item, ingredient)]) {
                            residual_[cell(item, ingredient)] = false;
                            amountOf(items_[idx(item)]) += capacity;
                            if (hasOutput) {
                                output->push_back(items_[idx(item)]);
                            }
                            break;
                        }
                    }
                }
                return valid;
            }
            const std::vector<std::int32_t> taken = *path;
            amountOf(items_[idx(taken.front())]) -= capacity;
            satisfied_[idx(taken.back())] = true;
            ++satisfiedCount;
            for (std::size_t i = 0; i + 1 < taken.size(); ++i) {
                if ((i & 1U) == 0) {
                    residual_[cell(taken[i], taken[i + 1])] = true;
                } else {
                    residual_[cell(taken[i + 1], taken[i])] = false;
                }
            }
        }
    }

    std::int32_t tryPickAll(std::int32_t maxSize, std::vector<std::string>* output) {
        std::int32_t min = 0;
        std::int32_t max = std::min(maxSize, upperBound()) + 1;
        while (true) {
            const std::int32_t mid = (min + max) / 2;
            if (tryPick(mid, nullptr)) {
                if (max - min <= 1) {
                    if (mid > 0) {
                        static_cast<void>(tryPick(mid, output));
                    }
                    return mid;
                }
                min = mid;
            } else {
                max = mid;
            }
        }
    }

private:
    [[nodiscard]] static std::size_t idx(std::int32_t value) { return static_cast<std::size_t>(value); }

    [[nodiscard]] std::size_t cell(std::int32_t item, std::int32_t ingredient) const {
        return idx((item * ingredientCount_) + ingredient);
    }

    [[nodiscard]] std::int32_t& amountOf(const std::string& item) {
        return std::ranges::find_if(contents_.amounts_, [&item](const auto& entry) { return entry.first == item; })
            ->second;
    }

    [[nodiscard]] std::int32_t upperBound() const {
        std::int32_t min = std::numeric_limits<std::int32_t>::max();
        for (const Ingredient* ingredient : ingredients_) {
            std::int32_t max = 0;
            for (const auto& [item, amount] : contents_.amounts_) {
                if (amount > max && ingredient->items.contains(item)) {
                    max = amount;
                }
            }
            min = std::min(min, max);
            if (max == 0) {
                break;
            }
        }
        return min;
    }

    void clearVisited() {
        std::ranges::fill(visitedIngredient_, false);
        std::ranges::fill(visitedItem_, false);
    }

    [[nodiscard]] const std::vector<std::int32_t>* tryAssigningNewItem(std::int32_t capacity) {
        clearVisited();
        for (std::int32_t item = 0; item < itemCount_; ++item) {
            if (amountOf(items_[idx(item)]) >= capacity && findPath(item)) {
                return &path_;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool findPath(std::int32_t startingItem) {
        path_.clear();
        visitedItem_[idx(startingItem)] = true;
        path_.push_back(startingItem);
        while (!path_.empty()) {
            const std::size_t length = path_.size();
            if (((length - 1) & 1U) == 0) {
                const std::int32_t item = path_.back();
                for (std::int32_t ingredient = 0; ingredient < ingredientCount_; ++ingredient) {
                    if (!visitedIngredient_[idx(ingredient)] && connection_[cell(item, ingredient)] &&
                        !residual_[cell(item, ingredient)]) {
                        visitedIngredient_[idx(ingredient)] = true;
                        path_.push_back(ingredient);
                        break;
                    }
                }
            } else {
                const std::int32_t last = path_.back();
                if (!satisfied_[idx(last)]) {
                    return true;
                }
                for (std::int32_t item = 0; item < itemCount_; ++item) {
                    if (!visitedItem_[idx(item)] && residual_[cell(item, last)]) {
                        visitedItem_[idx(item)] = true;
                        path_.push_back(item);
                        break;
                    }
                }
            }
            if (path_.size() == length) {
                path_.pop_back();
            }
        }
        return false;
    }

    StackedContents& contents_;
    std::span<const Ingredient* const> ingredients_;
    std::int32_t ingredientCount_;
    std::int32_t itemCount_ = 0;
    std::vector<std::string> items_;
    std::vector<bool> visitedIngredient_;
    std::vector<bool> visitedItem_;
    std::vector<bool> satisfied_;
    std::vector<bool> connection_;
    std::vector<bool> residual_;
    std::vector<std::int32_t> path_;
};

void StackedContents::account(std::string_view item, std::int32_t count) {
    for (auto& [name, amount] : amounts_) {
        if (name == item) {
            amount += count;
            return;
        }
    }
    amounts_.emplace_back(std::string{item}, count);
}

bool StackedContents::tryPick(std::span<const Ingredient* const> ingredients, std::int32_t amount,
                              std::vector<std::string>* output) {
    return RecipePicker{*this, ingredients}.tryPick(amount, output);
}

std::int32_t StackedContents::tryPickAll(std::span<const Ingredient* const> ingredients, std::int32_t maxSize,
                                         std::vector<std::string>* output) {
    return RecipePicker{*this, ingredients}.tryPickAll(maxSize, output);
}

} // namespace basalt::server::world::crafting
