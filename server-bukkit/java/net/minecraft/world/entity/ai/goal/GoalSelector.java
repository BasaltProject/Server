// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import com.google.common.collect.Sets;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

public class GoalSelector {
    private static final WrappedGoal NO_GOAL = new WrappedGoal(Integer.MAX_VALUE, new Goal() {
        @Override
        public boolean canUse() {
            return false;
        }
    }) {
        @Override
        public boolean isRunning() {
            return false;
        }
    };

    private final Map<Goal.Flag, WrappedGoal> lockedFlags = new EnumMap<>(Goal.Flag.class);
    private final Set<WrappedGoal> availableGoals = Sets.newLinkedHashSet();
    private final EnumSet<Goal.Flag> disabledFlags = EnumSet.noneOf(Goal.Flag.class);

    private final net.minecraft.world.entity.Mob owner;
    private final Set<Goal> pluginGoals = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
    private final Map<Goal, org.basaltmc.bukkit.facade.NmsGoalAdapter> bridged = new java.util.IdentityHashMap<>();

    public GoalSelector() {
        this(null);
    }

    public GoalSelector(net.minecraft.world.entity.Mob owner) {
        this.owner = owner;
    }

    public void addGoal(int prio, Goal goal) {
        availableGoals.add(new WrappedGoal(prio, goal));
        boolean fromPlugin = !builtInCaller(goal);
        if (fromPlugin) {
            pluginGoals.add(goal);
            bridge(prio, goal);
        }
    }

    /** Registers the goals added so far with the native mob AI, once the owner is a live entity. */
    public void syncNative() {
        for (WrappedGoal wrapped : availableGoals) {
            bridge(wrapped.getPriority(), wrapped.getGoal());
        }
    }

    private static boolean builtInCaller(Goal goal) {
        StackTraceElement[] frames = new Throwable().getStackTrace();
        for (int i = 2; i < frames.length; i++) {
            String name = frames[i].getClassName();
            if (!name.equals(GoalSelector.class.getName())) {
                return name.startsWith("net.minecraft.");
            }
        }
        return goal.getClass().getName().startsWith("net.minecraft.");
    }

    private void bridge(int prio, Goal goal) {
        if (owner == null || bridged.containsKey(goal) || !pluginGoals.contains(goal)
            || !(owner.live() instanceof org.bukkit.entity.Mob live)) {
            return;
        }
        org.basaltmc.bukkit.facade.NmsGoalAdapter adapter = new org.basaltmc.bukkit.facade.NmsGoalAdapter(goal);
        bridged.put(goal, adapter);
        org.basaltmc.bukkit.BasaltMobGoals.INSTANCE.addGoal(live, prio, adapter);
    }

    private void unbridge(Goal goal) {
        org.basaltmc.bukkit.facade.NmsGoalAdapter adapter = bridged.remove(goal);
        if (adapter != null && owner != null && owner.live() instanceof org.bukkit.entity.Mob live) {
            org.basaltmc.bukkit.BasaltMobGoals.INSTANCE.removeGoal(live, adapter);
        }
    }

    public void removeAllGoals(Predicate<Goal> predicate) {
        for (WrappedGoal wrapped : availableGoals) {
            if (predicate.test(wrapped.getGoal())) {
                unbridge(wrapped.getGoal());
            }
        }
        availableGoals.removeIf(wrapped -> predicate.test(wrapped.getGoal()));
    }

    public void removeGoal(Goal toRemove) {
        for (WrappedGoal wrapped : availableGoals) {
            if (wrapped.getGoal() == toRemove && wrapped.isRunning()) {
                wrapped.stop();
            }
        }
        unbridge(toRemove);
        availableGoals.removeIf(wrapped -> wrapped.getGoal() == toRemove);
    }

    private static boolean goalContainsAnyFlags(WrappedGoal goal, EnumSet<Goal.Flag> flags) {
        for (Goal.Flag flag : goal.getFlags()) {
            if (flags.contains(flag)) {
                return true;
            }
        }
        return false;
    }

    private static boolean goalCanBeReplacedForAllFlags(WrappedGoal goal, Map<Goal.Flag, WrappedGoal> locks) {
        for (Goal.Flag flag : goal.getFlags()) {
            if (!locks.getOrDefault(flag, NO_GOAL).canBeReplacedBy(goal)) {
                return false;
            }
        }
        return true;
    }

    public void tick() {
        for (WrappedGoal wrapped : availableGoals) {
            if (wrapped.isRunning() && (goalContainsAnyFlags(wrapped, disabledFlags) || !wrapped.canContinueToUse())) {
                wrapped.stop();
            }
        }
        lockedFlags.entrySet().removeIf(entry -> !entry.getValue().isRunning());
        for (WrappedGoal wrapped : availableGoals) {
            if (!wrapped.isRunning() && !goalContainsAnyFlags(wrapped, disabledFlags)
                && goalCanBeReplacedForAllFlags(wrapped, lockedFlags) && wrapped.canUse()) {
                for (Goal.Flag flag : wrapped.getFlags()) {
                    lockedFlags.getOrDefault(flag, NO_GOAL).stop();
                    lockedFlags.put(flag, wrapped);
                }
                wrapped.start();
            }
        }
        tickRunningGoals(true);
    }

    public void tickRunningGoals(boolean forceTickAllRunningGoals) {
        for (WrappedGoal wrapped : availableGoals) {
            if (wrapped.isRunning() && (forceTickAllRunningGoals || wrapped.requiresUpdateEveryTick())) {
                wrapped.tick();
            }
        }
    }

    public Set<WrappedGoal> getAvailableGoals() {
        return availableGoals;
    }

    public void disableControlFlag(Goal.Flag flag) {
        disabledFlags.add(flag);
    }

    public void enableControlFlag(Goal.Flag flag) {
        disabledFlags.remove(flag);
    }

    public void setControlFlag(Goal.Flag flag, boolean enabled) {
        if (enabled) {
            enableControlFlag(flag);
        } else {
            disableControlFlag(flag);
        }
    }
}
