// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.phys.Vec3;

public class SpearRetreat extends Behavior<PathfinderMob> {
    public static final int MIN_COOLDOWN_DISTANCE = 9;
    public static final int MAX_COOLDOWN_DISTANCE = 11;
    public static final int MAX_FLEEING_TIME = 100;
    private final double speedModifierWhenRepositioning;

    public SpearRetreat(double speedModifierWhenRepositioning) {
        super(Map.of(MemoryModuleType.SPEAR_STATUS, MemoryStatus.VALUE_PRESENT), MAX_FLEEING_TIME);
        this.speedModifierWhenRepositioning = speedModifierWhenRepositioning;
    }

    private LivingEntity getTarget(PathfinderMob mob) {
        return mob.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
    }

    private boolean ableToAttack(PathfinderMob mob) {
        return getTarget(mob) != null && mob.getMainHandItem().has(DataComponents.KINETIC_WEAPON);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        SpearAttack.SpearStatus status = body.getBrain().getMemory(MemoryModuleType.SPEAR_STATUS).orElse(SpearAttack.SpearStatus.APPROACH);
        if (status != SpearAttack.SpearStatus.RETREAT || body.isUsingItem() || !ableToAttack(body)) {
            return false;
        }
        LivingEntity foe = getTarget(body);
        double gap = Math.sqrt(body.distanceToSqr(foe.getX(), foe.getY(), foe.getZ()));
        int seat = body.isPassenger() ? 2 : 0;
        double nearest = Math.max(0.0, MIN_COOLDOWN_DISTANCE + seat - gap);
        double farthest = Math.max(1.0, MAX_COOLDOWN_DISTANCE + seat - gap);
        Vec3 spot = LandRandomPos.getPosAway(body, nearest, farthest, 7, foe.position());
        if (spot != null) {
            body.getBrain().setMemory(MemoryModuleType.SPEAR_FLEEING_POSITION, spot);
        }
        return spot != null;
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        body.setAggressive(true);
        body.getBrain().setMemory(MemoryModuleType.SPEAR_FLEEING_TIME, 0);
        super.start(level, body, timestamp);
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return body.getBrain().getMemory(MemoryModuleType.SPEAR_FLEEING_TIME).orElse(MAX_FLEEING_TIME) < MAX_FLEEING_TIME
            && body.getBrain().getMemory(MemoryModuleType.SPEAR_FLEEING_POSITION).isPresent() && !body.getNavigation().isDone() && ableToAttack(body);
    }

    protected void tick(ServerLevel level, PathfinderMob mob, long timestamp) {
        Brain<?> mind = mob.getBrain();
        float chargeScale = mob.getRootVehicle() instanceof Mob steed ? steed.chargeSpeedModifier() : 1.0F;
        mind.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(getTarget(mob), true));
        int elapsed = mind.getMemory(MemoryModuleType.SPEAR_FLEEING_TIME).orElse(0);
        mind.setMemory(MemoryModuleType.SPEAR_FLEEING_TIME, elapsed + 1);
        mind.getMemory(MemoryModuleType.SPEAR_FLEEING_POSITION)
            .ifPresent(spot -> mob.getNavigation().moveTo(spot.x, spot.y, spot.z, chargeScale * speedModifierWhenRepositioning));
    }

    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        body.setAggressive(false);
        body.stopUsingItem();
        body.getNavigation().stop();
        Brain<?> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.SPEAR_STATUS);
        mind.eraseMemory(MemoryModuleType.SPEAR_FLEEING_POSITION);
        mind.eraseMemory(MemoryModuleType.SPEAR_FLEEING_TIME);
    }
}
