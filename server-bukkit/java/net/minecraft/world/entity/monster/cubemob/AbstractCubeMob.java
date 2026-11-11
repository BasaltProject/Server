// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.cubemob;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractCubeMob extends AgeableMob {
    protected static final EntityDataAccessor<Integer> ID_SIZE = net.minecraft.network.syncher.EntityDataSerializers.INT.createAccessor(18);
    public static final int MIN_SIZE = 1;
    public static final int MAX_SIZE = 127;
    public static final int MAX_NATURAL_SIZE = 4;
    public float targetSquish;
    public float squish;
    public float oSquish;
    protected AbstractCubeMob(EntityType<? extends AbstractCubeMob> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected abstract void addBehaviourGoals();
    protected abstract void addTargetingGoals();
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setSize(int size, boolean updateHealth) {
        int clamped = Math.max(1, Math.min(127, size));
        if (live() instanceof org.bukkit.entity.AbstractCubeMob api) {
            api.setSize(clamped);
        }
        refreshDimensions();
        if (updateHealth) {
            setHealth(getMaxHealth());
        }
    }
    protected void setCubeMobHealth(int actualSize) {
        }
    public int getSize() {
        return live() instanceof org.bukkit.entity.AbstractCubeMob api ? (int) api.getSize() : 0;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean isTiny() {
        return getSize() <= 1;
    }
    protected abstract ParticleOptions getParticleType();
    public void tick() {
    }
    protected void decreaseSquish() {
        }
    protected int getJumpDelay() {
        return 0;
    }
    public void refreshDimensions() {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public EntityType<? extends AbstractCubeMob> getType() {
        return super.getType();
    }
    public void remove(Entity.RemovalReason reason) {
        super.remove(reason);
    }
    protected void setUpSplitCube(AbstractCubeMob cubeMob, int halfSize, float xd, float zd) {
        }
    protected int getSplitCount() {
        return 0;
    }
    public void push(Entity entity) {
        
    }
    public void playerTouch(Player player) {
        if (canDealDamage()) {
            dealDamage(player);
        }
    }
    protected void dealDamage(LivingEntity target) {
        }
    protected Vec3 getPassengerAttachmentPoint(Entity passenger, EntityDimensions dimensions, float scale) {
        return new Vec3(0.0, dimensions.height() - 0.015625 * getSize() * scale, 0.0);
    }
    protected boolean canDealDamage() {
        return false;
    }
    protected float getAttackDamage() {
        return 0;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public void jumpFromGround() {
        Vec3 motion = getDeltaMovement();
        setDeltaMovement(motion.x, getJumpPower(), motion.z);
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.AbstractCubeMob api ? (float) api.getSoundVolume() : 0;
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    protected boolean doPlayJumpSound() {
        return false;
    }
    public float getSoundPitch() {
        return live() instanceof org.bukkit.entity.AbstractCubeMob api ? (float) api.getSoundPitch() : 0;
    }
    protected abstract SoundEvent getJumpSound();
    protected abstract SoundEvent getHurtSound(DamageSource source);
    protected abstract SoundEvent getDeathSound();
    protected abstract SoundEvent getSquishSound();
    public AbstractCubeMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void setSpawnSize(ServerLevelAccessor level, DifficultyInstance difficulty) {
        }
    protected static class CubeMobMoveControl<T extends AbstractCubeMob> extends MoveControl<T> {
        private float yRot;
        private boolean isAggressive;

        public CubeMobMoveControl(T cubeMob) {
            super(cubeMob);
        }
        public void setDirection(float yRot, boolean isAggressive) {
            this.yRot = yRot;
            this.isAggressive = isAggressive;
        }
        public void setWantedMovement(double speedModifier) {
            this.speedModifier = speedModifier;
            this.operation = MoveControl.Operation.MOVE_TO;
        }
        public void tick() {
        }
    }
    protected static class CubeMobAttackGoal extends Goal {
        public CubeMobAttackGoal(AbstractCubeMob cubeMob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void start() {
            
        }
        public boolean canContinueToUse() {
            return false;
        }
        public boolean requiresUpdateEveryTick() {
            return false;
        }
        public void tick() {
        }
    }
    static class CubeMobRandomDirectionGoal extends Goal {
        public CubeMobRandomDirectionGoal(AbstractCubeMob cubeMob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void tick() {
        }
    }
    static class CubeMobFloatGoal extends Goal {
        public CubeMobFloatGoal(AbstractCubeMob mob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public boolean requiresUpdateEveryTick() {
            return false;
        }
        public void tick() {
        }
    }
    static class CubeMobKeepOnJumpingGoal extends Goal {
        public CubeMobKeepOnJumpingGoal(AbstractCubeMob mob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void tick() {
        }
    }
}
