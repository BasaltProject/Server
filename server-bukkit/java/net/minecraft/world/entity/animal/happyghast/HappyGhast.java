// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.happyghast;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.util.Mth;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.Leashable;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.goal.FloatGoal;
import net.minecraft.world.entity.ai.navigation.FlyingPathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
public class HappyGhast extends Animal {
    private int leashHolderTime;
    private int serverStillTimeout;
    private boolean leashHolderFlag;
    private boolean staysStillFlag;
    public static final float BABY_SCALE = 0.2375F;
    public static final int WANDER_GROUND_DISTANCE = 16;
    public static final int SMALL_RESTRICTION_RADIUS = 32;
    public static final int LARGE_RESTRICTION_RADIUS = 64;
    public static final int RESTRICTION_RADIUS_BUFFER = 16;
    public static final int FAST_HEALING_TICKS = 20;
    public static final int SLOW_HEALING_TICKS = 600;
    public static final int MAX_PASSANGERS = 4;
    public static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.0F;
    public HappyGhast(EntityType<? extends HappyGhast> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void ageBoundaryReached() {
        }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected float sanitizeScale(float scale) {
        return 0;
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean onClimbable() {
        return false;
    }
    public void travel(Vec3 input) {
        float speed = (float) getAttributeValue(Attributes.FLYING_SPEED) * 5.0F / 3.0F;
        travelFlying(input, speed, speed, speed);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public boolean canBreatheUnderwater() {
        return live() instanceof org.bukkit.entity.HappyGhast api ? (boolean) api.canBreatheUnderwater() : false;
    }
    protected boolean shouldStayCloseToLeashHolder() {
        return false;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    public float getVoicePitch() {
        return 1.0F;
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.HappyGhast api ? (float) api.getSoundVolume() : 0;
    }
    public int getMaxSpawnClusterSize() {
        return 1;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean canFallInLove() {
        return false;
    }
    public float getAgeScale() {
        return isBaby() ? 0.2375F : 1.0F;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void addPassenger(Entity passenger) {
        }
    protected void removePassenger(Entity passenger) {
        }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    public LivingEntity getControllingPassenger() {
        if (isWearingBodyArmor() && !isOnStillTimeout() && getFirstPassenger() instanceof Player rider) {
            return rider;
        }
        return super.getControllingPassenger();
    }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        float forward = 0.0F;
        float up = 0.0F;
        if (controller.zza != 0.0F) {
            float pitch = controller.getXRot() * ((float) Math.PI / 180F);
            forward = Mth.cos((double) pitch);
            up = -Mth.sin((double) pitch);
            if (controller.zza < 0.0F) {
                forward *= -0.5F;
                up *= -0.5F;
            }
        }
        if (controller.isJumping()) {
            up += 0.5F;
        }
        return new Vec3(controller.xxa, up, forward).scale(3.9F * getAttributeValue(Attributes.FLYING_SPEED));
    }
    protected Vec2 getRiddenRotation(LivingEntity controller) {
        return new Vec2(controller.getXRot() * 0.5F, controller.getYRot());
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    protected Brain<HappyGhast> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<HappyGhast> made = (Brain<HappyGhast>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<HappyGhast> getBrain() {
        @SuppressWarnings("unchecked") Brain<HappyGhast> made = (Brain<HappyGhast>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public void aiStep() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isLeashHolder() {
        return leashHolderFlag;
    }
    public boolean staysStill() {
        return staysStillFlag;
    }
    public boolean supportQuadLeashAsHolder() {
        return true;
    }
    public Vec3[] getQuadLeashHolderOffsets() {
        return Leashable.createQuadLeashOffsets(this, -0.03125, 0.4375, 0.46875, 0.03125);
    }
    public Vec3 getLeashOffset() {
        return Vec3.ZERO;
    }
    public double leashElasticDistance() {
        return 10.0;
    }
    public double leashSnapDistance() {
        return 16.0;
    }
    public void onElasticLeashPull() {
        super.onElasticLeashPull();
        getMoveControl().setWait();
    }
    public void notifyLeashHolder(Leashable entity) {
        if (entity.supportQuadLeash()) {
            leashHolderTime = 5;
        }
    }
    public void addAdditionalSaveData(ValueOutput tag) {
    }
    public void readAdditionalSaveData(ValueInput tag) {
    }
    public boolean isOnStillTimeout() {
        return staysStill() || serverStillTimeout > 0;
    }
    protected BodyRotationControl createBodyControl() {
        return new HappyGhastBodyRotationControl();
    }
    public boolean canBeCollidedWith(Entity other) {
        return false;
    }
    public boolean isFlyingVehicle() {
        return !isBaby();
    }
    public Vec3 getDismountLocationForPassenger(LivingEntity passenger) {
        return new Vec3(getX(), getBoundingBox().maxY, getZ());
    }
    static class BabyFlyingPathNavigation extends FlyingPathNavigation {
        public BabyFlyingPathNavigation(HappyGhast mob, Level level) {
            super((net.minecraft.world.entity.Mob) null, (net.minecraft.world.level.Level) null);
        }
        protected boolean canMoveDirectly(Vec3 startPos, Vec3 stopPos) {
            return false;
        }
    }
    class HappyGhastFloatGoal extends FloatGoal {
        public HappyGhastFloatGoal() {
            super((net.minecraft.world.entity.Mob) null);
        }
        public boolean canUse() {
            return false;
        }
    }
    class HappyGhastLookControl extends LookControl {
        private HappyGhastLookControl() {
            super((net.minecraft.world.entity.Mob) null);
        }
        public void tick() {
        }
    }
    class HappyGhastBodyRotationControl extends BodyRotationControl {
        public HappyGhastBodyRotationControl() {
            super((net.minecraft.world.entity.Mob) null);
        }
        public void clientTick() {
            if (isVehicle()) {
                yHeadRot = getYRot();
                yBodyRot = yHeadRot;
            }
            super.clientTick();


        }
    }
}
