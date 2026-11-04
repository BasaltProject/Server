// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.player.Player;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.RandomStrollGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.phys.Vec3;
public class Guardian extends Monster {
    protected static final int ATTACK_TIME = 80;
    protected RandomStrollGoal randomStrollGoal;
    public Guardian(EntityType<? extends Guardian> type, Level level) {
        super(type, level);
    }
    private boolean moving;
    protected void registerGoals() {
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(9, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 6.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.5)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 30.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isMoving() {
        return live() instanceof org.bukkit.entity.Guardian api ? (boolean) api.isMoving() : false;
    }
    public void setMoving(boolean value) {
        this.moving = value;
    }
    public int getAttackDuration() {
        return live() instanceof org.bukkit.entity.Guardian api ? api.getLaserDuration() : 80;
    }
    public boolean hasActiveAttackTarget() {
        return live() instanceof org.bukkit.entity.Guardian api && api.hasLaser();
    }
    public LivingEntity getActiveAttackTarget() {
        return hasActiveAttackTarget() ? getTarget() : null;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.guardian.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.guardian.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.guardian.death");
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public void aiStep() {
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
    public float getTailAnimation(float a) {
        return 0.0F;
    }
    public float getSpikesAnimation(float a) {
        return moving ? 1.0F : 0.0F;
    }
    public float getAttackAnimationScale(float a) {
        return getClientSideAttackTime() / (float) getAttackDuration();
    }
    public float getClientSideAttackTime() {
        return live() instanceof org.bukkit.entity.Guardian api ? api.getLaserTicks() : 0;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public static boolean checkGuardianSpawnRules(EntityType<? extends Guardian> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    static class GuardianAttackSelector implements TargetingConditions.Selector {
        private final Guardian guardian;
        public GuardianAttackSelector(Guardian guardian) {
            this.guardian = guardian;
        }
        public boolean test(LivingEntity target, ServerLevel level) {
            return (target instanceof Player || target instanceof net.minecraft.world.entity.animal.squid.Squid
                || target instanceof net.minecraft.world.entity.animal.axolotl.Axolotl) && target.distanceToSqr(guardian) > 9.0;
        }
    }
    static class GuardianAttackGoal extends Goal {
        private final Guardian guardian;
        private int attackTime;
        public GuardianAttackGoal(Guardian guardian) {
            this.guardian = guardian;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            LivingEntity target = guardian.getTarget();
            return target != null && target.isAlive();
        }
        public boolean canContinueToUse() {
            return super.canContinueToUse() && (guardian.live() instanceof org.bukkit.entity.Guardian api && api.isElder()
                || guardian.getTarget() != null && guardian.distanceToSqr(guardian.getTarget()) > 9.0);
        }
        public void start() {
            attackTime = -10;
            guardian.getNavigation().stop();
            LivingEntity target = guardian.getTarget();
            if (target != null) {
                guardian.getLookControl().setLookAt(target, 90.0F, 90.0F);
            }
        }
        public void stop() {
            if (guardian.live() instanceof org.bukkit.entity.Guardian api) {
                api.setLaser(false);
                api.setLaserTicks(0);
            }
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            LivingEntity target = guardian.getTarget();
            if (target == null || !(guardian.live() instanceof org.bukkit.entity.Guardian api)) {
                return;
            }
            guardian.getNavigation().stop();
            guardian.getLookControl().setLookAt(target, 90.0F, 90.0F);
            if (!guardian.hasLineOfSight(target)) {
                guardian.setTarget(null);
                return;
            }
            attackTime++;
            api.setLaserTicks(Math.max(attackTime, 0));
            if (attackTime == 0) {
                api.setLaser(true);
            } else if (attackTime >= guardian.getAttackDuration()) {
                float damage = 1.0F;
                if (api.getWorld().getDifficulty() == org.bukkit.Difficulty.HARD) {
                    damage += 2.0F;
                }
                if (api.isElder()) {
                    damage += 2.0F;
                }
                if (target.live() instanceof org.bukkit.entity.Damageable victim) {
                    victim.damage(damage, api);
                }
                guardian.doHurtTarget(null, target);
                guardian.setTarget(null);
            }
        }
    }
    static class GuardianMoveControl<T extends Guardian> extends MoveControl<T> {
        public GuardianMoveControl(T guardian) {
            super(guardian);
        }
        public void tick() {
            if (operation != Operation.MOVE_TO || mob.getNavigation().isDone()) {
                mob.setSpeed(0.0F);
                mob.setMoving(false);
                return;
            }
            Vec3 delta = new Vec3(wantedX - mob.getX(), wantedY - mob.getY(), wantedZ - mob.getZ());
            double distance = delta.length();
            Vec3 direction = delta.scale(1.0 / distance);
            float yaw = (float) (Mth.atan2(delta.z, delta.x) * (180.0F / (float) Math.PI)) - 90.0F;
            mob.setYRot(rotlerp(mob.getYRot(), yaw, 90.0F));
            mob.yBodyRot = mob.getYRot();
            float speed = (float) (speedModifier * mob.getAttributeValue(Attributes.MOVEMENT_SPEED));
            float blended = Mth.lerp(0.125F, mob.getSpeed(), speed);
            mob.setSpeed(blended);
            mob.setDeltaMovement(mob.getDeltaMovement().add(direction.x * blended * 0.05, direction.y * blended * 0.1, direction.z * blended * 0.05));
            mob.setMoving(true);
        }
    }
}
