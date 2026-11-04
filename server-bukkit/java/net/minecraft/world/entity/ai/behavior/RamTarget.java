// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.ToDoubleFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.BlockTags;
import net.minecraft.util.Mth;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.goat.Goat;
import net.minecraft.world.phys.Vec3;

public class RamTarget extends Behavior<Goat> {
    public static final int TIME_OUT_DURATION = 200;
    public static final float RAM_SPEED_FORCE_FACTOR = 1.65F;
    private final Function<Goat, UniformInt> getTimeBetweenRams;
    private final TargetingConditions ramTargeting;
    private final float speed;
    private final ToDoubleFunction<Goat> getKnockbackForce;
    private Vec3 ramDirection = Vec3.ZERO;
    private final Function<Goat, SoundEvent> getImpactSound;
    private final Function<Goat, SoundEvent> getHornBreakSound;

    public RamTarget(Function<Goat, UniformInt> getTimeBetweenRams, TargetingConditions ramTargeting, float speed,
            ToDoubleFunction<Goat> getKnockbackForce, Function<Goat, SoundEvent> getImpactSound, Function<Goat, SoundEvent> getHornBreakSound) {
        super(Map.of(MemoryModuleType.RAM_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT, MemoryModuleType.RAM_TARGET, MemoryStatus.VALUE_PRESENT), TIME_OUT_DURATION);
        this.getTimeBetweenRams = getTimeBetweenRams;
        this.ramTargeting = ramTargeting;
        this.speed = speed;
        this.getKnockbackForce = getKnockbackForce;
        this.getImpactSound = getImpactSound;
        this.getHornBreakSound = getHornBreakSound;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Goat body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.RAM_TARGET);
    }

    protected boolean canStillUse(ServerLevel level, Goat body, long timestamp) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.RAM_TARGET);
    }

    protected void start(ServerLevel level, Goat body, long timestamp) {
        BlockPos at = body.blockPosition();
        Vec3 target = body.getBrain().getMemory(MemoryModuleType.RAM_TARGET).get();
        ramDirection = new Vec3(at.getX() - target.x(), 0.0, at.getZ() - target.z()).normalize();
        body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target, speed, 0));
    }

    protected void tick(ServerLevel level, Goat body, long timestamp) {
        List<LivingEntity> hit = level.getNearbyEntities(LivingEntity.class, ramTargeting, body, body.getBoundingBox());
        var brain = body.getBrain();
        if (!hit.isEmpty()) {
            LivingEntity victim = hit.get(0);
            DamageSource silent = level.damageSources().noAggroMobAttack(body);
            float damage = (float) body.getAttributeValue(Attributes.ATTACK_DAMAGE);
            if (victim.hurtServer(level, silent, damage)) {
                net.minecraft.world.item.enchantment.EnchantmentHelper.doPostAttackEffects(level, victim, silent);
            }
            int speedLevel = body.hasEffect(MobEffects.SPEED) ? body.getEffect(MobEffects.SPEED).getAmplifier() + 1 : 0;
            int slowLevel = body.hasEffect(MobEffects.SLOWNESS) ? body.getEffect(MobEffects.SLOWNESS).getAmplifier() + 1 : 0;
            float factor = Mth.clamp(body.getSpeed() * RAM_SPEED_FORCE_FACTOR, 0.2F, 3.0F) + 0.25F * (speedLevel - slowLevel);
            DamageSource source = level.damageSources().mobAttack(body);
            float blocked = victim.applyItemBlocking(level, source, damage);
            float blocking = blocked > 0.0F ? 0.5F : 1.0F;
            victim.knockback(blocking * factor * getKnockbackForce.applyAsDouble(body), ramDirection.x(), ramDirection.z(), source, damage);
            finishRam(level, body);
            level.playSound(null, body, getImpactSound.apply(body), SoundSource.NEUTRAL, 1.0F, 1.0F);
        } else if (hasRammedHornBreakingBlock(level, body)) {
            level.playSound(null, body, getImpactSound.apply(body), SoundSource.NEUTRAL, 1.0F, 1.0F);
            if (body.dropHorn()) {
                level.playSound(null, body, getHornBreakSound.apply(body), SoundSource.NEUTRAL, 1.0F, 1.0F);
            }
            finishRam(level, body);
        } else {
            Optional<WalkTarget> walk = brain.getMemory(MemoryModuleType.WALK_TARGET);
            Optional<Vec3> ram = brain.getMemory(MemoryModuleType.RAM_TARGET);
            if (walk.isEmpty() || ram.isEmpty() || walk.get().getTarget().currentPosition().closerThan(ram.get(), 0.25)) {
                finishRam(level, body);
            }
        }
    }

    private boolean hasRammedHornBreakingBlock(ServerLevel level, Goat body) {
        Vec3 heading = body.getDeltaMovement().multiply(1.0, 0.0, 1.0).normalize();
        BlockPos ahead = BlockPos.containing(body.position().add(heading));
        return level.getBlockState(ahead).is(BlockTags.SNAPS_GOAT_HORN) || level.getBlockState(ahead.above()).is(BlockTags.SNAPS_GOAT_HORN);
    }

    protected void finishRam(ServerLevel level, Goat body) {
        level.broadcastEntityEvent(body, (byte) 59);
        body.getBrain().setMemory(MemoryModuleType.RAM_COOLDOWN_TICKS, getTimeBetweenRams.apply(body).sample(level.getRandom()));
        body.getBrain().eraseMemory(MemoryModuleType.RAM_TARGET);
    }
}
