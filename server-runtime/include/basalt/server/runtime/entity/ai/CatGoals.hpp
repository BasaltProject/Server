#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity::ai {

/// parity: `MoveToBlockGoal`: every so often looks outwards from its feet for a block it wants, walks
/// on top of it, and stays a random while before giving up on it.
class MoveToBlockGoal : public Goal {
public:
    MoveToBlockGoal(double speedModifier, std::int32_t searchRange, std::int32_t verticalSearchRange = 1);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Jump}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

protected:
    [[nodiscard]] virtual std::int32_t nextStartTick(MobContext& mob);

    [[nodiscard]] virtual bool isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) = 0;

    [[nodiscard]] bool reachedTarget() const { return reachedTarget_; }

    std::int32_t verticalSearchStart_ = 0;

private:
    [[nodiscard]] bool findNearestBlock(MobContext& mob);

    double speedModifier_;
    std::int32_t searchRange_;
    std::int32_t verticalSearchRange_;
    std::int32_t nextStartTick_ = 0;
    std::int32_t tryTicks_ = 0;
    std::int32_t maxStayTicks_ = 0;
    std::array<std::int32_t, 3> blockPos_{};
    bool reachedTarget_ = false;
};

/// parity: `CatLieOnBlockGoal`, 26.3's tag-driven successor to lying on a bed.
class CatLieOnBlockGoal final : public MoveToBlockGoal {
public:
    CatLieOnBlockGoal(double speedModifier, std::int32_t searchRange);

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

protected:
    [[nodiscard]] std::int32_t nextStartTick(MobContext& mob) override;

    [[nodiscard]] bool isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;
};

/// parity: `CatSitOnBlockGoal`: a closed chest, a lit furnace or the foot of a bed, from the
/// `cats_can_sit_on` tag.
class CatSitOnBlockGoal final : public MoveToBlockGoal {
public:
    explicit CatSitOnBlockGoal(double speedModifier);

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

protected:
    [[nodiscard]] bool isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;
};

/// parity: `Cat.CatRelaxOnOwnerGoal`: to the bed its owner sleeps in, curled up there, and on
/// waking perhaps a gift from `gameplay/cat_morning_gift`.
class CatRelaxOnOwnerGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] bool spaceIsOccupied(MobContext& mob) const;

    std::optional<std::int32_t> owner_;
    std::optional<std::array<std::int32_t, 3>> goalPos_;
    std::int32_t onBedTicks_ = 0;
};

} // namespace basalt::server::runtime::entity::ai
