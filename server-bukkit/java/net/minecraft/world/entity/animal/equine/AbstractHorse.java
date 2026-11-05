// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.equine;
import java.util.function.DoubleSupplier;
import java.util.function.IntUnaryOperator;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.Container;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.HasCustomInventoryScreen;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.OwnableEntity;
import net.minecraft.world.entity.PlayerRideableJumping;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.SoundType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractHorse extends Animal implements PlayerRideableJumping, HasCustomInventoryScreen, OwnableEntity {
    public static final int CHEST_SLOT_OFFSET = 499;
    public static final int INVENTORY_SLOT_OFFSET = 500;
    public static final double BREEDING_CROSS_FACTOR = 0.15;
    protected static final float BABY_SCALE = 0.7F;
    public static final int INVENTORY_ROWS = 3;
    public int tailCounter;
    public int sprintCounter;
    protected SimpleContainer inventory;
    protected int temper;
    protected float playerJumpPendingScale;
    protected boolean allowStandSliding;
    protected boolean canGallop;
    protected int gallopSoundCounter;
    protected AbstractHorse(EntityType<? extends AbstractHorse> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void addBehaviourGoals() {
        }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected boolean getFlag(int flag) {
        return false;
    }
    protected void setFlag(int flag, boolean value) {
        }
    public boolean isTamed() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? (boolean) api.isTamed() : false;
    }
    public EntityReference<LivingEntity> getOwnerReference() {
        return live() instanceof org.bukkit.entity.AbstractHorse api && api.getOwnerUniqueId() != null
            ? EntityReference.of(api.getOwnerUniqueId()) : null;
    }
    public void setOwner(LivingEntity owner) {
        if (live() instanceof org.bukkit.entity.AbstractHorse api) {
            org.bukkit.entity.Entity target = owner == null ? null : owner.live();
            api.setOwner(target instanceof org.bukkit.entity.AnimalTamer tamer ? tamer : null);
        }
    }
    public void setTamed(boolean flag) {
        if (live() instanceof org.bukkit.entity.AbstractHorse api) { api.setTamed(flag); }
    }
    public void onElasticLeashPull() {

    }
    public boolean supportQuadLeash() {
        return false;
    }
    public Vec3[] getQuadLeashOffsets() {
        return net.minecraft.world.entity.Leashable.createQuadLeashOffsets(this, 0.04, 0.52, 0.23, 0.87);
    }
    public boolean isEating() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? (boolean) api.isEating() : false;
    }
    public boolean isStanding() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? api.isRearing() : standing;
    }
    public boolean isBred() {
        return bred;
    }
    public void setBred(boolean flag) {
        bred = flag;
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    public void equipBodyArmor(Player player, ItemStack itemStack) {

    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    public int getTemper() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? api.getDomestication() : 0;
    }
    public void setTemper(int temper) {
        if (live() instanceof org.bukkit.entity.AbstractHorse api) {
            api.setDomestication(temper);
        }
    }
    public int modifyTemper(int amount) {
        int clamped = net.minecraft.util.Mth.clamp(getTemper() + amount, 0, getMaxTemper());
        setTemper(clamped);
        return clamped;
    }
    public boolean isPushable() {
        return !isVehicle() && super.isPushable();
    }
    public boolean causeFallDamage(double fallDistance, float damageModifier, DamageSource damageSource) {
        return false;
    }
    public final int getInventorySize() {
        return getInventoryColumns() * 3 + 2;
    }
    protected void createInventory() {
        }
    protected Holder<SoundEvent> getEquipSound(EquipmentSlot slot, ItemStack stack, Equippable equippable) {
        return Holder.direct(net.minecraft.sounds.SoundEvents.byId("item.armor.equip_generic"));
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected boolean canPerformRearing() {
        return false;
    }
    protected SoundEvent getEatingSound() {
        return null;
    }
    protected SoundEvent getAngrySound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected void playGallopSound(SoundType soundType) {
        }
    public static AttributeSupplier.Builder createBaseHorseAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.JUMP_STRENGTH, 0.7)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 53.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.225)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.SAFE_FALL_DISTANCE, 6.0);
    }
    public int getMaxSpawnClusterSize() {
        return 6;
    }
    public int getMaxTemper() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? api.getMaxDomestication() : 100;
    }
    public float getSoundVolume() {
        return live() instanceof org.bukkit.entity.AbstractHorse api ? (float) api.getSoundVolume() : 0;
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    public void openCustomInventoryScreen(Player player) {

    }
    public InteractionResult fedFood(Player player, ItemStack itemStack) {
        boolean ate = handleEating(player, itemStack);
        if (ate) {
            itemStack.consume(1, player);
        }
        return !ate && !level().isClientSide() ? InteractionResult.PASS : InteractionResult.SUCCESS_SERVER;
    }
    protected boolean handleEating(Player player, ItemStack itemStack) {
        return false;
    }
    protected void doPlayerRide(Player player) {
        }
    public boolean isImmobile() {
        return super.isImmobile() || isEating() || isStanding();
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected void dropEquipment(ServerLevel level) {
    }
    public void aiStep() {
    }
    protected void followMommy(ServerLevel level) {
        }
    public boolean canEatGrass() {
        return true;
    }
    public void tick() {
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void setEating(boolean flag) {
        if (live() instanceof org.bukkit.entity.AbstractHorse api) { api.setEating(flag); }
    }
    public void setStanding(int ticks) {
        standCounter = 1;
        standing = true;
        if (live() instanceof org.bukkit.entity.AbstractHorse api) {
            api.setRearing(true);
        }
    }
    public void clearStanding() {
        standing = false;
        standCounter = 0;
        if (live() instanceof org.bukkit.entity.AbstractHorse api) {
            api.setRearing(false);
        }
    }
    public SoundEvent getAmbientStandSound() {
        return null;
    }
    public void standIfPossible() {
        if (canPerformRearing() && isEffectiveAi()) {
            setStanding(20);
        }
    }
    public void makeMad() {
        standIfPossible();
    }
    public boolean tameWithName(Player player) {
        if (live() instanceof org.bukkit.entity.AbstractHorse api) {
            api.setTamed(true);
            api.setOwner((org.bukkit.entity.AnimalTamer) player.live());
            return true;
        }
        return false;
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    protected Vec2 getRiddenRotation(LivingEntity controller) {
        return new Vec2(controller.getXRot() * 0.5F, controller.getYRot());
    }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        if (onGround() && playerJumpPendingScale == 0.0F && isStanding() && !allowStandSliding) {
            return Vec3.ZERO;
        }
        float forward = controller.zza;
        return new Vec3(controller.xxa * 0.5F, 0.0, forward <= 0.0F ? forward * 0.25F : forward);
    }
    protected void addPassenger(Entity passenger) {
        }
    protected float getRiddenSpeed(Player controller) {
        return 0;
    }
    protected void executeRidersJump(float amount, Vec3 input) {
        }
    protected void playJumpSound() {
        }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    protected boolean canParent() {
        return false;
    }
    public boolean isMobControlled() {
        return getControllingPassenger() instanceof net.minecraft.world.entity.Mob;
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected void setOffspringAttributes(AgeableMob partner, AbstractHorse baby) {
        }
    public static double createOffspringAttribute(double parentAValue, double parentBValue, double attributeRangeMin, double attributeRangeMax, RandomSource random) {
        if (attributeRangeMax <= attributeRangeMin) {
            throw new IllegalArgumentException("Incorrect range for an attribute");
        }
        double first = random.nextDouble();
        double second = random.nextDouble();
        double third = random.nextDouble();
        return org.basaltmc.bukkit.MechanicsNatives.offspringAttribute(parentAValue, parentBValue, attributeRangeMin,
            attributeRangeMax, first, second, third);
    }
    public float getEatAnim(float a) {
        return 0.0F;
    }
    public float getStandAnim(float a) {
        return standing ? 1.0F : 0.0F;
    }
    public float getMouthAnim(float a) {
        return 0.0F;
    }
    public void onPlayerJump(int jumpAmount) {

    }
    public boolean canJump() {
        return false;
    }
    public void handleStartJump(int jumpScale) {

    }
    public void handleStopJump() {

    }
    protected void spawnTamingParticles(boolean success) {
        }
    public void handleEntityEvent(byte id) {
    }
    protected void positionRider(Entity passenger, Entity.MoveFunction moveFunction) {
    }
    protected static float generateMaxHealth(IntUnaryOperator integerByBoundProvider) {
        return 0;
    }
    protected static double generateJumpStrength(DoubleSupplier probabilityProvider) {
        return 0;
    }
    protected static double generateSpeed(DoubleSupplier probabilityProvider) {
        return 0;
    }
    public boolean onClimbable() {
        return false;
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public LivingEntity getControllingPassenger() {
        return getFirstPassenger() instanceof LivingEntity rider ? rider : null;
    }
    protected void randomizeAttributes(RandomSource random) {
        }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public boolean hasInventoryChanged(Container oldInventory) {
        return false;
    }
    public int getAmbientStandInterval() {
        return getAmbientSoundInterval();
    }
    public int getInventoryColumns() {
        return 5;
    }
    class MountPanicGoal extends PanicGoal {
        public MountPanicGoal(double speedModifier) {
            super(AbstractHorse.this, speedModifier);
        }
        public boolean shouldPanic() {
            return !AbstractHorse.this.isMobControlled() && super.shouldPanic();
        }
    }
    private boolean standing;
    private boolean bred;
    private int standCounter;
}
