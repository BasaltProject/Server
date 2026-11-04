// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.animal.Animal;

public class FollowParentGoal extends Goal {
    public static final int HORIZONTAL_SCAN_RANGE = 8;
    public static final int VERTICAL_SCAN_RANGE = 4;
    public static final int DONT_FOLLOW_IF_CLOSER_THAN = 3;
    private final Animal animal;
    private Animal parent;
    private final double speedModifier;
    private int timeToRecalcPath;

    public FollowParentGoal(Animal animal, double speedModifier) {
        this.animal = animal;
        this.speedModifier = speedModifier;
    }

    @Override
    public boolean canUse() {
        if (animal.getAge() >= 0 || !(animal.live() instanceof org.bukkit.entity.Animals live)) {
            return false;
        }
        Animal best = null;
        double bestDistance = Double.MAX_VALUE;
        for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(HORIZONTAL_SCAN_RANGE, VERTICAL_SCAN_RANGE, HORIZONTAL_SCAN_RANGE)) {
            Entity handle = Entity.fromBukkit(candidate);
            if (handle instanceof Animal other && other.getAge() >= 0 && other.getClass() == animal.getClass()) {
                double distance = animal.distanceToSqr(other);
                if (distance >= DONT_FOLLOW_IF_CLOSER_THAN * DONT_FOLLOW_IF_CLOSER_THAN && distance < bestDistance) {
                    best = other;
                    bestDistance = distance;
                }
            }
        }
        parent = best;
        return parent != null;
    }

    @Override
    public boolean canContinueToUse() {
        if (animal.getAge() >= 0 || parent == null || !parent.isAlive()) {
            return false;
        }
        double distance = animal.distanceToSqr(parent);
        return distance >= 9.0 && distance <= 256.0;
    }

    @Override
    public void start() {
        timeToRecalcPath = 0;
    }

    @Override
    public void stop() {
        parent = null;
    }

    @Override
    public void tick() {
        if (--timeToRecalcPath <= 0) {
            timeToRecalcPath = adjustedTickDelay(10);
            animal.getNavigation().moveTo(parent, speedModifier);
        }
    }
}
