package org.basaltmc.bukkit.facade;

import com.destroystokyo.paper.entity.ai.Goal;
import com.destroystokyo.paper.entity.ai.GoalKey;
import com.destroystokyo.paper.entity.ai.GoalType;
import java.util.EnumSet;
import java.util.concurrent.atomic.AtomicInteger;
import org.bukkit.NamespacedKey;
import org.bukkit.entity.Mob;

/** Runs a goal written against the internal goal API inside the native mob AI. */
public final class NmsGoalAdapter implements Goal<Mob> {
    private static final AtomicInteger kCounter = new AtomicInteger();

    private final net.minecraft.world.entity.ai.goal.Goal goal;
    private final GoalKey<Mob> key;
    private final EnumSet<GoalType> types = EnumSet.noneOf(GoalType.class);
    private int ticks;
    private boolean broken;

    public NmsGoalAdapter(net.minecraft.world.entity.ai.goal.Goal goal) {
        this.goal = goal;
        String name = goal.getClass().getName().toLowerCase(java.util.Locale.ROOT).replaceAll("[^a-z0-9_./-]", "_");
        this.key = GoalKey.of(Mob.class, new NamespacedKey("basalt", "nms/" + name + "/" + kCounter.incrementAndGet()));
        for (net.minecraft.world.entity.ai.goal.Goal.Flag flag : goal.getFlags()) {
            types.add(GoalType.valueOf(flag.name()));
        }
        if (types.isEmpty()) {
            types.add(GoalType.UNKNOWN_BEHAVIOR);
        }
    }

    private void fail(Throwable failure) {
        if (!broken) {
            broken = true;
            org.basaltmc.bukkit.BasaltServer.instance().getLogger().severe("Goal " + goal + " failed: " + failure);
        }
    }

    @Override
    public boolean shouldActivate() {
        try {
            return !broken && goal.canUse();
        } catch (Throwable failure) {
            fail(failure);
            return false;
        }
    }

    @Override
    public boolean shouldStayActive() {
        try {
            return !broken && goal.canContinueToUse();
        } catch (Throwable failure) {
            fail(failure);
            return false;
        }
    }

    @Override
    public void start() {
        ticks = 0;
        try {
            goal.start();
        } catch (Throwable failure) {
            fail(failure);
        }
    }

    @Override
    public void stop() {
        try {
            goal.stop();
        } catch (Throwable failure) {
            fail(failure);
        }
    }

    @Override
    public void tick() {
        if (!goal.requiresUpdateEveryTick() && (ticks++ & 1) != 0) {
            return;
        }
        try {
            goal.tick();
        } catch (Throwable failure) {
            fail(failure);
        }
    }

    @Override
    public GoalKey<Mob> getKey() {
        return key;
    }

    @Override
    public EnumSet<GoalType> getTypes() {
        return types;
    }
}
