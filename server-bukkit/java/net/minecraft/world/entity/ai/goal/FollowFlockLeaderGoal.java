// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.List;
import net.minecraft.world.entity.animal.fish.AbstractSchoolingFish;

public class FollowFlockLeaderGoal extends Goal {
    private final AbstractSchoolingFish mob;
    private int timer;

    public FollowFlockLeaderGoal(AbstractSchoolingFish mob) {
        this.mob = mob;
        this.timer = nextStartTick(mob);
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    protected int nextStartTick(AbstractSchoolingFish mob) {
        return reducedTickDelay(200 + mob.getRandom().nextInt(200) % 20);
    }

    @Override
    public boolean canUse() {
        if (mob.hasFollowers()) {
            return false;
        }
        if (mob.isFollower()) {
            return true;
        }
        if (timer > 0) {
            timer--;
            return false;
        }
        timer = nextStartTick(mob);
        List<AbstractSchoolingFish> others = (List<AbstractSchoolingFish>) (List<?>) mob.level().getEntitiesOfClass(mob.getClass(), mob.getBoundingBox().inflate(8.0, 8.0, 8.0),
            fish -> fish.canBeFollowed() || !fish.isFollower());
        AbstractSchoolingFish leader = others.stream().filter(AbstractSchoolingFish::canBeFollowed).findAny().orElse(mob);
        leader.addFollowers(others.stream().filter(fish -> !fish.isFollower()));
        return mob.isFollower();
    }

    @Override
    public boolean canContinueToUse() {
        return mob.isFollower() && mob.inRangeOfLeader();
    }

    @Override
    public void start() {
        timer = 0;
    }

    @Override
    public void stop() {
        mob.stopFollowing();
    }

    @Override
    public void tick() {
        if (--timer <= 0) {
            timer = nextStartTick(mob);
            mob.pathToLeader();
        }
    }
}
