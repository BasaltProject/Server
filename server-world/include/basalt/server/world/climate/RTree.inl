#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace basalt::server::world::climate {
namespace detail {

inline constexpr std::size_t kChildrenPerNode = 6;

/// The previous leaf each tree found, per thread.
///
/// Kept beside the trees rather than inside them because it is thread state, not tree state: two
/// generation workers descending the same tree must not seed each other's searches. Slots are handed
/// out when a tree is built, which happens while a biome source is constructed, before any worker
/// exists.
[[nodiscard]] const void* lastLeaf(std::size_t slot);
void setLastLeaf(std::size_t slot, const void* leaf);
[[nodiscard]] std::size_t nextTreeSlot();

} // namespace detail

template <typename T>
std::int64_t RTree<T>::Node::distance(const std::array<std::int64_t, kDimensions>& target) const {
    std::int64_t total = 0;
    for (std::size_t i = 0; i < kDimensions; ++i) {
        const std::int64_t gap = parameterSpace[i].distance(target[i]);
        total += gap * gap;
    }
    return total;
}

template <typename T>
RTree<T>::SubTree::SubTree(std::vector<std::unique_ptr<Node>> children)
    : Node([&children] {
          if (children.empty()) {
              throw std::invalid_argument{"a subtree needs at least one child"};
          }
          std::array<Parameter, kDimensions> bounds{};
          bool first = true;
          for (const std::unique_ptr<Node>& child : children) {
              for (std::size_t axis = 0; axis < kDimensions; ++axis) {
                  bounds[axis] = first ? child->parameterSpace[axis] : bounds[axis].span(child->parameterSpace[axis]);
              }
              first = false;
          }
          return bounds;
      }()),
      children(std::move(children)) {}

template <typename T>
const typename RTree<T>::Leaf* RTree<T>::SubTree::search(const std::array<std::int64_t, kDimensions>& target,
                                                         const Leaf* candidate) const {
    // No candidate means no distance computed for one, which is vanilla's own null branch.
    std::int64_t minDistance =
        candidate == nullptr ? std::numeric_limits<std::int64_t>::max() : candidate->distance(target);
    const Leaf* closest = candidate;
    for (const std::unique_ptr<Node>& child : children) {
        const std::int64_t childDistance = child->distance(target);
        if (minDistance <= childDistance) {
            continue;
        }
        const Leaf* leaf = child->search(target, closest);
        const std::int64_t leafDistance =
            static_cast<const Node*>(leaf) == child.get() ? childDistance : leaf->distance(target);
        if (minDistance > leafDistance) {
            minDistance = leafDistance;
            closest = leaf;
        }
    }
    if (closest == nullptr) {
        throw std::logic_error{"a subtree with children returned no leaf"};
    }
    return closest;
}

template <typename T>
const T& RTree<T>::search(const std::array<std::int64_t, kDimensions>& target) const {
    const auto* previous = static_cast<const Leaf*>(detail::lastLeaf(slot_));
    const Leaf* leaf = root_->search(target, previous);
    detail::setLastLeaf(slot_, leaf);
    return leaf->value;
}

namespace detail {

/// Vanilla chains a comparator per axis, starting at one and wrapping around.
template <typename Node>
void sortNodes(std::vector<std::unique_ptr<Node>>& children, std::size_t dimension, bool absolute) {
    const auto centre = [absolute](const Node& node, std::size_t axis) {
        const Parameter& parameter = node.parameterSpace[axis];
        const std::int64_t middle = (parameter.min + parameter.max) / 2;
        return absolute ? std::abs(middle) : middle;
    };
    std::ranges::stable_sort(children, [&](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
        for (std::size_t offset = 0; offset < kDimensions; ++offset) {
            const std::size_t axis = (dimension + offset) % kDimensions;
            const std::int64_t left = centre(*a, axis);
            const std::int64_t right = centre(*b, axis);
            if (left != right) {
                return left < right;
            }
        }
        return false;
    });
}

template <typename Node>
[[nodiscard]] std::int64_t totalMagnitude(const Node& node) {
    std::int64_t total = 0;
    for (std::size_t axis = 0; axis < kDimensions; ++axis) {
        total += std::abs((node.parameterSpace[axis].min + node.parameterSpace[axis].max) / 2);
    }
    return total;
}

[[nodiscard]] inline std::int64_t cost(const std::array<Parameter, kDimensions>& space) {
    std::int64_t result = 0;
    for (const Parameter& parameter : space) {
        result += std::abs(parameter.max - parameter.min);
    }
    return result;
}

[[nodiscard]] inline std::size_t pow6(int exponent) {
    std::size_t result = 1;
    for (int i = 0; i < std::max(exponent, 0); ++i) {
        result *= kChildrenPerNode;
    }
    return result;
}

} // namespace detail

template <typename T>
std::unique_ptr<RTree<T>> RTree<T>::create(std::vector<std::pair<ParameterPoint, T>> values) {
    if (values.empty()) {
        throw std::invalid_argument{"a climate tree needs at least one value"};
    }

    std::vector<std::unique_ptr<Node>> leaves;
    leaves.reserve(values.size());
    for (auto& [point, value] : values) {
        leaves.push_back(std::make_unique<Leaf>(point, std::move(value)));
    }

    // Grouped into buckets of `6^floor(log6(n - 0.01))`, matching vanilla's own sizing exactly, and
    // rebuilt from their own children so the tree keeps splitting until every node fits.
    const auto build = [](auto&& self, std::vector<std::unique_ptr<Node>> children) -> std::unique_ptr<Node> {
        if (children.size() == 1) {
            return std::move(children.front());
        }
        if (children.size() <= detail::kChildrenPerNode) {
            std::ranges::stable_sort(children, [](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
                return detail::totalMagnitude(*a) < detail::totalMagnitude(*b);
            });
            return std::make_unique<SubTree>(std::move(children));
        }

        const std::size_t expected = detail::pow6(
            static_cast<int>(std::floor(std::log(static_cast<double>(children.size()) - 0.01) / std::log(6.0))));
        std::int64_t minCost = std::numeric_limits<std::int64_t>::max();
        std::size_t minDimension = 0;
        for (std::size_t dimension = 0; dimension < kDimensions; ++dimension) {
            detail::sortNodes(children, dimension, false);
            std::int64_t totalCost = 0;
            for (std::size_t start = 0; start < children.size(); start += expected) {
                const std::size_t end = std::min(start + expected, children.size());
                std::array<Parameter, kDimensions> bounds{};
                for (std::size_t i = start; i < end; ++i) {
                    for (std::size_t axis = 0; axis < kDimensions; ++axis) {
                        bounds[axis] = i == start ? children[i]->parameterSpace[axis]
                                                  : bounds[axis].span(children[i]->parameterSpace[axis]);
                    }
                }
                totalCost += detail::cost(bounds);
            }
            if (minCost > totalCost) {
                minCost = totalCost;
                minDimension = dimension;
            }
        }

        // Bucket along the cheapest axis, then order the buckets by the absolute centre.
        detail::sortNodes(children, minDimension, false);
        std::vector<std::unique_ptr<Node>> buckets;
        for (std::size_t start = 0; start < children.size(); start += expected) {
            const std::size_t end = std::min(start + expected, children.size());
            std::vector<std::unique_ptr<Node>> bucket;
            for (std::size_t i = start; i < end; ++i) {
                bucket.push_back(std::move(children[i]));
            }
            buckets.push_back(std::make_unique<SubTree>(std::move(bucket)));
        }
        detail::sortNodes(buckets, minDimension, true);

        std::vector<std::unique_ptr<Node>> rebuilt;
        rebuilt.reserve(buckets.size());
        for (std::unique_ptr<Node>& bucket : buckets) {
            auto* subtree = static_cast<SubTree*>(bucket.get());
            rebuilt.push_back(self(self, std::move(subtree->children)));
        }
        return std::make_unique<SubTree>(std::move(rebuilt));
    };

    auto tree = std::unique_ptr<RTree<T>>{new RTree<T>{build(build, std::move(leaves))}};
    tree->slot_ = detail::nextTreeSlot();
    return tree;
}

} // namespace basalt::server::world::climate
