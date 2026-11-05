// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.polarbear;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.MeleeAttackGoal;
import net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class PolarBear extends Animal implements NeutralMob {
    private long angerEndTime = -1L;
    private net.minecraft.world.entity.EntityReference<net.minecraft.world.entity.LivingEntity> angerTarget;
    public PolarBear(EntityType<? extends PolarBear> type, Level level) {
        super(type, level);
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected void registerGoals() {
        goalSelector.addGoal(0, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(4, new net.minecraft.world.entity.ai.goal.FollowParentGoal(this, 1.25));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.RandomStrollGoal(this, 1.0));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 6.0F));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 30.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 20.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 6.0);
    }
    public static boolean checkPolarBearSpawnRules(EntityType<PolarBear> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void startPersistentAngerTimer() {
        setTimeToRemainAngry(400 + random.nextInt(381));
    }
    public void setPersistentAngerEndTime(long endTime) {
        this.angerEndTime = endTime;
    }
    public long getPersistentAngerEndTime() {
        return angerEndTime;
    }
    public void setPersistentAngerTarget(EntityReference<LivingEntity> persistentAngerTarget) {
        this.angerTarget = persistentAngerTarget;
    }
    public EntityReference<LivingEntity> getPersistentAngerTarget() {
        return angerTarget;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.polar_bear.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.polar_bear.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.polar_bear.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.polar_bear.step"), 0.15F, 1.0F);
    }
    protected void playWarningSound() {
        }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public boolean isStanding() {
        return live() instanceof org.bukkit.entity.PolarBear api ? (boolean) api.isStanding() : false;
    }
    public void setStanding(boolean value) {
        if (live() instanceof org.bukkit.entity.PolarBear api) { api.setStanding(value); }
    }
    public float getStandingAnimationScale(float a) {
        return isStanding() ? 1.0F : 0.0F;
    }
    protected float getWaterSlowDown() {
        return 0;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    class PolarBearHurtByTargetGoal extends HurtByTargetGoal {
        public PolarBearHurtByTargetGoal() {
            super(PolarBear.this);
        }
        public void start() {
            super.start();
            if (isBaby()) {
                alertOthers();
                stop();
            }
        }
        protected void alertOther(Mob other, LivingEntity hurtByMob) {
            if (other instanceof PolarBear && !other.isBaby()) {
                super.alertOther(other, hurtByMob);
            }
        }
    }
    class PolarBearAttackPlayersGoal extends NearestAttackableTargetGoal<Player> {
        public PolarBearAttackPlayersGoal() {
            super(PolarBear.this, Player.class, 20, true, true, null);
        }
        public boolean canUse() {
            if (isBaby() || !super.canUse()) {
                return false;
            }
            for (PolarBear bear : level().getEntitiesOfClass(PolarBear.class, getBoundingBox().inflate(8.0, 4.0, 8.0))) {
                if (bear.isBaby()) {
                    return true;
                }
            }
            return false;
        }
        protected double getFollowDistance() {
            return super.getFollowDistance() * 0.5;
        }
    }
    class PolarBearMeleeAttackGoal extends MeleeAttackGoal {
        public PolarBearMeleeAttackGoal() {
            super(PolarBear.this, 1.25, true);
        }
        protected void checkAndPerformAttack(LivingEntity target) {
            if (canPerformAttack(target)) {
                resetAttackCooldown();
                doHurtTarget(null, target);
                setStanding(false);
            } else if (distanceToSqr(target) < (target.getBbWidth() + 3.0F) * (target.getBbWidth() + 3.0F)) {
                if (isTimeToAttack()) {
                    setStanding(false);
                    resetAttackCooldown();
                }
                if (getTicksUntilNextAttack() <= adjustedTickDelay(10)) {
                    setStanding(true);
                    playWarningSound();
                }
            } else {
                resetAttackCooldown();
                setStanding(false);
            }
        }
        public void stop() {
            setStanding(false);
            super.stop();
        }
    }
}
