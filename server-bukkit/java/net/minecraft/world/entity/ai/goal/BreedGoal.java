// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.animal.Animal;

public class BreedGoal extends Goal {
    protected final Animal animal;
    private final Class<? extends Animal> partnerClass;
    protected final ServerLevel level;
    protected Animal partner;
    private int loveTime;
    private final double speedModifier;

    public BreedGoal(Animal animal, double speedModifier) {
        this(animal, speedModifier, animal.getClass());
    }

    public BreedGoal(Animal animal, double speedModifier, Class<? extends Animal> clazz) {
        this.animal = animal;
        this.level = (ServerLevel) animal.level();
        this.partnerClass = clazz;
        this.speedModifier = speedModifier;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        if (!animal.isInLove() || !(animal.live() instanceof org.bukkit.entity.Animals live)) {
            return false;
        }
        partner = null;
        double best = Double.MAX_VALUE;
        for (org.bukkit.entity.Entity candidate : live.getNearbyEntities(8.0, 8.0, 8.0)) {
            Entity handle = Entity.fromBukkit(candidate);
            if (partnerClass.isInstance(handle) && handle instanceof Animal other && animal.canMate(other)
                && animal.distanceToSqr(other) < best) {
                partner = other;
                best = animal.distanceToSqr(other);
            }
        }
        return partner != null;
    }

    @Override
    public boolean canContinueToUse() {
        return partner != null && partner.isAlive() && partner.isInLove() && loveTime < 60;
    }

    @Override
    public void stop() {
        partner = null;
        loveTime = 0;
    }

    @Override
    public void tick() {
        animal.getLookControl().setLookAt(partner, 10.0F, animal.getMaxHeadXRot());
        animal.getNavigation().moveTo(partner, speedModifier);
        loveTime++;
        if (loveTime >= adjustedTickDelay(60) && animal.distanceToSqr(partner) < 9.0) {
            breed();
        }
    }

    protected void breed() {
        animal.spawnChildFromBreeding(level, partner);
    }
}
