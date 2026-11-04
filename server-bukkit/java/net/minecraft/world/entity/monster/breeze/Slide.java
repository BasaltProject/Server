// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;

import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.phys.Vec3;

public class Slide extends Behavior<Breeze> {
    public Slide() {
        super(Map.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_JUMP_COOLDOWN, MemoryStatus.VALUE_ABSENT, MemoryModuleType.BREEZE_SHOOT, MemoryStatus.VALUE_ABSENT));
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Breeze breeze) {
        return breeze.onGround() && !breeze.isInWater() && breeze.getPose() == Pose.STANDING;
    }

    protected void start(ServerLevel level, Breeze breeze, long timestamp) {
        LivingEntity enemy = breeze.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
        if (enemy == null) {
            return;
        }
        Vec3 position = null;
        if (breeze.withinInnerCircleRange(enemy.position())) {
            Vec3 away = DefaultRandomPos.getPosAway(breeze, 5, 5, enemy.position());
            if (away != null && BreezeUtil.hasLineOfSight(breeze, away) && enemy.distanceToSqr(away.x, away.y, away.z) > enemy.distanceToSqr(breeze)) {
                position = away;
            }
        }
        if (position == null) {
            position = breeze.getRandom().nextBoolean() ? BreezeUtil.randomPointBehindTarget(enemy, breeze.getRandom()) : randomPointInMiddleCircle(breeze, enemy);
        }
        breeze.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(BlockPos.containing(position), BreezeAi.SPEED_MULTIPLIER_WHEN_SLIDING, 1));
    }

    private static Vec3 randomPointInMiddleCircle(Breeze breeze, LivingEntity enemy) {
        Vec3 direction = enemy.position().subtract(breeze.position());
        double distance = direction.length() - Mth.lerp(breeze.getRandom().nextDouble(), 8.0, 4.0);
        return breeze.position().add(direction.normalize().multiply(distance, distance, distance));
    }
}
