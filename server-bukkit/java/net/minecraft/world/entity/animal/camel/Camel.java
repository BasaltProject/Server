// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.camel;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
public class Camel extends AbstractHorse {
    public static final float BABY_SCALE = 0.6F;
    public static final int DASH_COOLDOWN_TICKS = 55;
    public static final int MAX_HEAD_Y_ROT = 30;
    public static final EntityDataAccessor<Boolean> DASH = null;
    public static final EntityDataAccessor<Long> LAST_POSE_CHANGE_TICK = null;
    public final AnimationState sitAnimationState = null;
    public final AnimationState sitPoseAnimationState = null;
    public final AnimationState sitUpAnimationState = null;
    public final AnimationState idleAnimationState = null;
    public final AnimationState dashAnimationState = null;
    public Camel(EntityType<? extends Camel> type, Level level) {
        super(type, level);
        this.moveControl = new CamelMoveControl<>(this);
        this.lookControl = new CamelLookControl();
    }
    private long lastPoseChangeTick;
    private int jumpCooldown;
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkCamelSpawnRules(EntityType<Camel> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected Brain<Camel> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Camel> made = (Brain<Camel>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Camel> getBrain() {
        @SuppressWarnings("unchecked") Brain<Camel> made = (Brain<Camel>) (Brain<?>) super.getBrain(); return made;
    }
    protected void registerGoals() {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    protected void updateWalkAnimation(float distance) {
        }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    public boolean refuseToMove() {
        return isCamelSitting() || isInPoseTransition();
    }
    protected float getRiddenSpeed(Player controller) {
        return 0;
    }
    protected Vec2 getRiddenRotation(LivingEntity controller) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.camel.Camel#getRiddenRotation");
    }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.camel.Camel#getRiddenInput");
    }
    public boolean canJump() {
        return !refuseToMove();
    }
    public void onPlayerJump(int jumpAmount) {
        jumpCooldown = Math.max(jumpCooldown, 55);
    }
    public boolean canSprint() {
        return true;
    }
    protected void executeRidersJump(float amount, Vec3 input) {
        }
    public boolean isDashing() {
        return live() instanceof org.bukkit.entity.Camel api ? (boolean) api.isDashing() : false;
    }
    public void setDashing(boolean isDashing) {
        if (live() instanceof org.bukkit.entity.Camel api) { api.setDashing(isDashing); }
    }
    public void handleStartJump(int jumpScale) {
        setDashing(true);
    }
    protected SoundEvent getDashingSound() {
        return null;
    }
    protected SoundEvent getDashReadySound() {
        return null;
    }
    public void handleStopJump() {
        setDashing(false);
    }
    public int getJumpCooldown() {
        return jumpCooldown;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.camel.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.camel.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.camel.hurt");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.camel.step"), 0.15F, 1.0F);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void onElasticLeashPull() {
        super.onElasticLeashPull();
    }
    public Vec3[] getQuadLeashOffsets() {
        return new Vec3[] {new Vec3(0.0, 1.1, 0.0), new Vec3(0.0, 1.1, 0.0), new Vec3(0.0, 1.1, 0.0), new Vec3(0.0, 1.1, 0.0)};
    }
    public boolean canCamelChangePose() {
        return !isInWater() && !isPassenger();
    }
    protected boolean handleEating(Player player, ItemStack itemStack) {
        return false;
    }
    protected boolean canPerformRearing() {
        return false;
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public Camel getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected SoundEvent getEatingSound() {
        return null;
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float dmg) {
        }
    protected Vec3 getPassengerAttachmentPoint(Entity passenger, EntityDimensions dimensions, float scale) {
        return new Vec3(0.0, dimensions.height() - (isBaby() ? 0.35F : 0.6F), -0.5F * scale);
    }
    public float getAgeScale() {
        return isBaby() ? 0.45F : 1.0F;
    }
    public Vec3 getLeashOffset(float partialTicks) {
        return new Vec3(0.0, getBbHeight() - 0.6F, getBbWidth() * 0.5F);
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    public boolean isCamelSitting() {
        return lastPoseChangeTick < 0L;
    }
    public boolean isCamelVisuallySitting() {
        return getPoseTime() < 40L ? !isCamelSitting() : isCamelSitting();
    }
    public boolean isInPoseTransition() {
        return getPoseTime() < (isCamelSitting() ? 40L : 52L);
    }
    public void sitDown() {
        if (!isCamelSitting()) {
            lastPoseChangeTick = -level().getGameTime();
        }
    }
    public void standUp() {
        if (isCamelSitting()) {
            lastPoseChangeTick = level().getGameTime();
        }
    }
    protected SoundEvent getStandUpSound() {
        return null;
    }
    protected SoundEvent getSitDownSound() {
        return null;
    }
    public void standUpInstantly() {
        lastPoseChangeTick = level().getGameTime() - 52L;
    }
    public void resetLastPoseChangeTick(long syncedPoseTickTime) {
        lastPoseChangeTick = syncedPoseTickTime;
    }
    public long getPoseTime() {
        return level().getGameTime() - Math.abs(lastPoseChangeTick);
    }
    protected Holder.Reference<SoundEvent> getSaddleSound() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.camel.Camel#getSaddleSound");
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public boolean isTamed() {
        return live() instanceof org.bukkit.entity.Camel api ? (boolean) api.isTamed() : false;
    }
    protected BodyRotationControl createBodyControl() {
        return new CamelBodyRotationControl(this);
    }
    class CamelBodyRotationControl extends BodyRotationControl {
        public CamelBodyRotationControl(Camel camel) {
            super(camel);
        }
        public void clientTick() {
            if (!Camel.this.refuseToMove()) {
                super.clientTick();
            }
        }
    }
    class CamelLookControl extends LookControl {
        private CamelLookControl() {
            super(Camel.this);
        }
        public void tick() {
            if (!Camel.this.isCamelSitting() || !Camel.this.isInPoseTransition()) {
                super.tick();
            }
        }
    }
    static class CamelMoveControl<T extends Camel> extends MoveControl<T> {
        public CamelMoveControl(T camel) {
            super(camel);
        }
        public void tick() {
            if (mob.refuseToMove() && mob.onGround()) {
                mob.setDeltaMovement(mob.getDeltaMovement().multiply(0.0, 1.0, 0.0));
                mob.getNavigation().stop();
                return;
            }
            super.tick();
        }
    }
}
