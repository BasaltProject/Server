// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.breeze;
import java.util.Optional;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.entity.projectile.ProjectileDeflection;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public class Breeze extends Monster {
    private int jumpTrailStartedTick;
    public final AnimationState idle = null;
    public final AnimationState slide = null;
    public final AnimationState slideBack = null;
    public final AnimationState longJump = null;
    public final AnimationState shoot = null;
    public final AnimationState inhale = null;
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.63)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 30.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 24.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0);
    }
    public Breeze(EntityType<? extends Monster> type, Level level) {
        super(type, level);
    }
    public Brain<Breeze> getBrain() {
        @SuppressWarnings("unchecked") Brain<Breeze> made = (Brain<Breeze>) (Brain<?>) super.getBrain(); return made;
    }
    protected Brain<Breeze> makeBrain(Brain.Packed input) {
        @SuppressWarnings("unchecked") Brain<Breeze> made = (Brain<Breeze>) (Brain<?>) super.makeBrain(input); return made;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public void tick() {
    }
    public Breeze resetJumpTrail() {
        this.jumpTrailStartedTick = 0;
        return this;
    }
    public void emitJumpTrailParticles() {
        this.jumpTrailStartedTick++;
    }
    public void emitGroundParticles(int amount) {
        
    }
    public void playAmbientSound() {
    }
    public void playWhirlSound() {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.breeze.whirl"), 0.8F + 0.2F * random.nextFloat(), 0.7F + 0.4F * random.nextFloat());
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.breeze.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.breeze.hurt");
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    public Optional<LivingEntity> getHurtBy() {
        return getBrain().getMemory(net.minecraft.world.entity.ai.memory.MemoryModuleType.HURT_BY).map(DamageSource::getEntity).filter(e -> e instanceof LivingEntity).map(e -> (LivingEntity) e);
    }
    public boolean withinInnerCircleRange(Vec3 target) {
        return target.closerThan(Vec3.atCenterOf(blockPosition()), 4.0, 10.0);
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public double getFiringYPosition() {
        return getY() + getBbHeight() / 2.0F + 0.3F;
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return super.isInvulnerableTo(level, source);
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
}
