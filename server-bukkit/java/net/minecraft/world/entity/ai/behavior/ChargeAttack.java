// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.Mth;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.phys.Vec3;

public class ChargeAttack extends Behavior<Animal> {
    private final int timeBetweenAttacks;
    private final TargetingConditions chargeTargeting;
    private final float speed;
    private final float knockbackForce;
    private final double maxTargetDetectionDistance;
    private final double maxChargeDistance;
    private final SoundEvent chargeSound;
    private Vec3 chargeVelocityVector = Vec3.ZERO;
    private Vec3 startPosition = Vec3.ZERO;

    public ChargeAttack(int timeBetweenAttacks, TargetingConditions chargeTargeting, float speed, float knockbackForce, double maxChargeDistance,
            double maxTargetDetectionDistance, SoundEvent chargeSound) {
        super(Map.of(MemoryModuleType.CHARGE_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT, MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT));
        this.timeBetweenAttacks = timeBetweenAttacks;
        this.chargeTargeting = chargeTargeting;
        this.speed = speed;
        this.knockbackForce = knockbackForce;
        this.maxChargeDistance = maxChargeDistance;
        this.maxTargetDetectionDistance = maxTargetDetectionDistance;
        this.chargeSound = chargeSound;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Animal body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET);
    }

    protected boolean canStillUse(ServerLevel level, Animal body, long timestamp) {
        Brain<?> brain = body.getBrain();
        Optional<LivingEntity> candidate = brain.getMemory(MemoryModuleType.ATTACK_TARGET);
        if (candidate.isEmpty()) {
            return false;
        }
        LivingEntity target = candidate.get();
        if (body instanceof TamableAnimal tamed && tamed.isTame()) {
            return false;
        }
        if (body.position().subtract(startPosition).lengthSqr() >= maxChargeDistance * maxChargeDistance) {
            return false;
        }
        if (target.position().subtract(body.position()).lengthSqr() >= maxTargetDetectionDistance * maxTargetDetectionDistance) {
            return false;
        }
        return body.hasLineOfSight(target) && !brain.hasMemoryValue(MemoryModuleType.CHARGE_COOLDOWN_TICKS);
    }

    protected void start(ServerLevel level, Animal body, long timestamp) {
        startPosition = body.position();
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).get();
        chargeVelocityVector = target.position().subtract(body.position()).normalize().scale(speed);
        if (canStillUse(level, body, timestamp)) {
            body.playSound(chargeSound);
        }
    }

    protected void tick(ServerLevel level, Animal body, long timestamp) {
        LivingEntity target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElseThrow();
        body.lookAt(target, 360.0F, 360.0F);
        body.setDeltaMovement(chargeVelocityVector);
        List<LivingEntity> colliding = new ArrayList<>(1);
        for (LivingEntity e : level.getEntitiesOfClass(LivingEntity.class, body.getBoundingBox())) {
            if (chargeTargeting.test(level, body, e)) {
                colliding.add(e);
                break;
            }
        }
        if (!colliding.isEmpty()) {
            LivingEntity closest = colliding.get(0);
            if (body.hasPassenger(closest)) {
                return;
            }
            dealDamageToTarget(level, body, closest);
            dealKnockBack(level, body, closest);
            stop(level, body, timestamp);
        }
    }

    private void dealDamageToTarget(ServerLevel level, Animal body, LivingEntity target) {
        DamageSource source = level.damageSources().mobAttack(body);
        float damage = (float) body.getAttributeValue(Attributes.ATTACK_DAMAGE);
        if (target.hurtServer(level, source, damage)) {
            EnchantmentHelper.doPostAttackEffects(level, target, source);
        }
    }

    private void dealKnockBack(ServerLevel level, Animal body, LivingEntity target) {
        int speedLevel = body.hasEffect(MobEffects.SPEED) ? body.getEffect(MobEffects.SPEED).getAmplifier() + 1 : 0;
        int slowLevel = body.hasEffect(MobEffects.SLOWNESS) ? body.getEffect(MobEffects.SLOWNESS).getAmplifier() + 1 : 0;
        float boost = 0.25F * (speedLevel - slowLevel);
        float factor = Mth.clamp(speed * (float) body.getAttributeValue(Attributes.MOVEMENT_SPEED), 0.2F, 2.0F) + boost;
        DamageSource source = level.damageSources().mobAttack(body);
        float damage = (float) body.getAttributeValue(Attributes.ATTACK_DAMAGE);
        body.causeExtraKnockback(target, factor * knockbackForce, body.getDeltaMovement(), source, damage, false);
    }

    protected void stop(ServerLevel level, Animal body, long timestamp) {
        body.getBrain().setMemory(MemoryModuleType.CHARGE_COOLDOWN_TICKS, timeBetweenAttacks);
        body.getBrain().eraseMemory(MemoryModuleType.ATTACK_TARGET);
    }
}
