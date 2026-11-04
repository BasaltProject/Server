// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.function.Predicate;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class TemptGoal extends Goal {
    protected final Mob mob;
    protected final double speedModifier;
    private final Predicate<ItemStack> items;
    private final boolean canScare;
    private final double stopDistance;
    protected Player player;
    private int calmDown;
    private boolean running;

    public TemptGoal(PathfinderMob mob, double speedModifier, Predicate<ItemStack> items, boolean canScare) {
        this(mob, speedModifier, items, canScare, 2.5);
    }

    public TemptGoal(PathfinderMob mob, double speedModifier, Predicate<ItemStack> items, boolean canScare, double stopDistance) {
        this((Mob) mob, speedModifier, items, canScare, stopDistance);
    }

    private TemptGoal(Mob mob, double speedModifier, Predicate<ItemStack> items, boolean canScare, double stopDistance) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.items = items;
        this.canScare = canScare;
        this.stopDistance = stopDistance;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        if (calmDown > 0) {
            calmDown--;
            return false;
        }
        player = null;
        if (mob.live() instanceof org.bukkit.entity.Mob live) {
            double reach = 10.0;
            double best = Double.MAX_VALUE;
            for (org.bukkit.entity.Player candidate : live.getWorld().getNearbyPlayers(live.getLocation(), reach)) {
                Player handle = (Player) net.minecraft.world.entity.Entity.fromBukkit(candidate);
                if (shouldFollow(handle) && mob.distanceToSqr(handle) < best) {
                    best = mob.distanceToSqr(handle);
                    player = handle;
                }
            }
        }
        return player != null;
    }

    private boolean shouldFollow(Player candidate) {
        return items.test(candidate.getMainHandItem()) || items.test(candidate.getOffhandItem());
    }

    @Override
    public boolean canContinueToUse() {
        if (canScare()) {
            if (mob.distanceToSqr(player) < 36.0 && player.distanceToSqr(mob) > 0.01) {
                return false;
            }
        }
        return canUse() || (player != null && shouldFollow(player) && mob.distanceToSqr(player) < 100.0);
    }

    protected boolean canScare() {
        return canScare;
    }

    @Override
    public void start() {
        running = true;
    }

    @Override
    public void stop() {
        player = null;
        stopNavigation();
        calmDown = reducedTickDelay(100);
        running = false;
    }

    @Override
    public void tick() {
        if (player == null) {
            return;
        }
        mob.getLookControl().setLookAt(player, mob.getMaxHeadYRot() + 20, mob.getMaxHeadXRot());
        if (mob.distanceToSqr(player) < stopDistance * stopDistance) {
            stopNavigation();
        } else {
            navigateTowards(player);
        }
    }

    protected void stopNavigation() {
        mob.getNavigation().stop();
    }

    protected void navigateTowards(Player player) {
        mob.getNavigation().moveTo(player, speedModifier);
    }

    public boolean isRunning() {
        return running;
    }

    public static class ForNonPathfinders extends TemptGoal {
        public ForNonPathfinders(Mob mob, double speedModifier, Predicate<ItemStack> items, boolean canScare, double stopDistance) {
            super(mob, speedModifier, items, canScare, stopDistance);
        }

        @Override
        protected void stopNavigation() {
            mob.getMoveControl().setWait();
        }

        @Override
        protected void navigateTowards(Player player) {
            mob.getMoveControl().setWantedPosition(player.getX(), player.getY(), player.getZ(), speedModifier);
        }
    }
}
