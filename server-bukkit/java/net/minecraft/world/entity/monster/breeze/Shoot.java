// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.util.Unit;
import java.util.Optional;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.phys.Vec3;

public class Shoot extends Behavior<Breeze> {
    private static final int ATTACK_RANGE_MAX_SQRT = 256;
    private static final float PROJECTILE_MOVEMENT_SCALE = 0.7F;
    private static final int SHOOT_INITIAL_DELAY_TICKS = Math.round(15.0F);
    private static final int SHOOT_RECOVER_DELAY_TICKS = Math.round(4.0F);
    private static final int SHOOT_COOLDOWN_TICKS = Math.round(10.0F);

    public Shoot() {
        super(Map.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT, MemoryModuleType.BREEZE_SHOOT_COOLDOWN, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_SHOOT_CHARGING, MemoryStatus.VALUE_ABSENT, MemoryModuleType.BREEZE_SHOOT_RECOVERING, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_SHOOT, MemoryStatus.VALUE_PRESENT, MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.BREEZE_JUMP_TARGET, MemoryStatus.VALUE_ABSENT), SHOOT_INITIAL_DELAY_TICKS + 1 + SHOOT_RECOVER_DELAY_TICKS);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Breeze breeze) {
        if (breeze.getPose() != Pose.STANDING) {
            return false;
        }
        Optional<LivingEntity> prey = breeze.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET);
        if (prey.isEmpty()) {
            return false;
        }
        boolean inRange = isTargetWithinRange(breeze, prey.get());
        if (!inRange) {
            breeze.getBrain().eraseMemory(MemoryModuleType.BREEZE_SHOOT);
        }
        return inRange;
    }

    protected boolean canStillUse(ServerLevel level, Breeze body, long timestamp) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET) && body.getBrain().hasMemoryValue(MemoryModuleType.BREEZE_SHOOT);
    }

    protected void start(ServerLevel level, Breeze breeze, long timestamp) {
        breeze.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).ifPresent(target -> breeze.setPose(Pose.SHOOTING));
        breeze.getBrain().setMemoryWithExpiry(MemoryModuleType.BREEZE_SHOOT_CHARGING, Unit.INSTANCE, SHOOT_INITIAL_DELAY_TICKS);
        breeze.playSound(SoundEvents.BREEZE_INHALE, 1.0F, 1.0F);
    }

    protected void stop(ServerLevel level, Breeze breeze, long timestamp) {
        if (breeze.getPose() == Pose.SHOOTING) {
            breeze.setPose(Pose.STANDING);
        }
        breeze.getBrain().setMemoryWithExpiry(MemoryModuleType.BREEZE_SHOOT_COOLDOWN, Unit.INSTANCE, SHOOT_COOLDOWN_TICKS);
        breeze.getBrain().eraseMemory(MemoryModuleType.BREEZE_SHOOT);
    }

    protected void tick(ServerLevel level, Breeze breeze, long timestamp) {
        Brain<Breeze> mind = breeze.getBrain();
        Optional<LivingEntity> prey = mind.getMemory(MemoryModuleType.ATTACK_TARGET);
        if (prey.isEmpty()) {
            return;
        }
        LivingEntity target = prey.get();
        breeze.lookAt(net.minecraft.commands.arguments.EntityAnchorArgument.Anchor.EYES, target.position());
        boolean busy = mind.getMemory(MemoryModuleType.BREEZE_SHOOT_CHARGING).isPresent()
            || mind.getMemory(MemoryModuleType.BREEZE_SHOOT_RECOVERING).isPresent();
        if (busy) {
            return;
        }
        mind.setMemoryWithExpiry(MemoryModuleType.BREEZE_SHOOT_RECOVERING, Unit.INSTANCE, SHOOT_RECOVER_DELAY_TICKS);
        if (breeze.live() instanceof org.bukkit.entity.Breeze api) {
            org.bukkit.util.Vector aim = new org.bukkit.util.Vector(
                target.getX() - breeze.getX(),
                target.getY(target.isPassenger() ? 0.8 : 0.3) - breeze.getFiringYPosition(),
                target.getZ() - breeze.getZ());
            org.bukkit.entity.BreezeWindCharge charge = api.launchProjectile(org.bukkit.entity.BreezeWindCharge.class,
                aim.normalize().multiply(PROJECTILE_MOVEMENT_SCALE));
            charge.setShooter(api);
        }
        breeze.playSound(SoundEvents.BREEZE_SHOOT, 1.5F, 1.0F);
    }

    private static boolean isTargetWithinRange(Breeze body, LivingEntity target) {
        return body.position().distanceToSqr(target.position()) < ATTACK_RANGE_MAX_SQRT;
    }
}
