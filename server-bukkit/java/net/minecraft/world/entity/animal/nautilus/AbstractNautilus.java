// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;
import net.minecraft.world.inventory.AbstractMountInventoryMenu;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.syncher.EntityDataAccessor;
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
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.HasCustomInventoryScreen;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PlayerRideableJumping;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractNautilus extends TamableAnimal implements PlayerRideableJumping, HasCustomInventoryScreen {
    private boolean dashing;
    protected int dashCooldown;
    public static final int INVENTORY_SLOT_OFFSET = 500;
    public static final int INVENTORY_ROWS = 3;
    public static final int SMALL_RESTRICTION_RADIUS = 16;
    public static final int LARGE_RESTRICTION_RADIUS = 32;
    public static final int RESTRICTION_RADIUS_BUFFER = 8;
    protected float playerJumpPendingScale;
    protected SimpleContainer inventory;
    protected AbstractNautilus(EntityType<? extends AbstractNautilus> type, Level level) {
        super(type, level);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected void usePlayerItem(Player player, InteractionHand hand, ItemStack itemStack) {
        }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public boolean isPushedByFluid() {
        return false;
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public static boolean checkNautilusSpawnRules(EntityType<? extends AbstractNautilus> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    public LivingEntity getControllingPassenger() {
        Entity rider = getFirstPassenger();
        return isSaddled() && rider instanceof Player player ? player : super.getControllingPassenger();
    }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        float forward = 0.0F;
        float up = 0.0F;
        if (controller.zza != 0.0F) {
            double pitch = controller.getXRot() * (Math.PI / 180.0);
            forward = (float) Math.cos(pitch);
            up = (float) -Math.sin(pitch);
            if (controller.zza < 0.0F) {
                forward *= -0.5F;
                up *= -0.5F;
            }
        }
        return new Vec3(controller.xxa, up, forward);
    }
    protected Vec2 getRiddenRotation(LivingEntity controller) {
        return new Vec2(controller.getXRot() * 0.5F, controller.getYRot());
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    protected float getRiddenSpeed(Player controller) {
        return 0;
    }
    protected void doPlayerRide(Player player) {
        }
    protected void checkRestriction() {
        }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public boolean canJump() {
        return isSaddled();
    }
    public void onPlayerJump(int jumpAmount) {
        if (isSaddled() && dashCooldown <= 0) {
            playerJumpPendingScale = getPlayerJumpPendingScale(jumpAmount);
        }
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isDashing() {
        return dashing;
    }
    public void setDashing(boolean isDashing) {
        this.dashing = isDashing;
    }
    protected void executeRidersJump(float amount, Player controller) {
        }
    public void handleStartJump(int jumpScale) {
        setDashing(true);
    }
    public int getJumpCooldown() {
        return dashCooldown;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public void handleStopJump() {
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected SoundEvent getDashSound() {
        return null;
    }
    protected SoundEvent getDashReadySound() {
        return null;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return super.canBeAffected(newEffect);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected Holder<SoundEvent> getEquipSound(EquipmentSlot slot, ItemStack stack, Equippable equippable) {
        return super.getEquipSound(slot, stack, equippable);
    }
    public final int getInventorySize() {
        return AbstractMountInventoryMenu.getInventorySize(getInventoryColumns());
    }
    protected void createInventory() {
        }
    public void openCustomInventoryScreen(Player player) {
        if (!level().isClientSide() && (!isVehicle() || hasPassenger(player)) && isTame()) {
            player.openNautilusInventory(this, inventory);
        }
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public boolean hasInventoryChanged(Container oldInventory) {
        return inventory != oldInventory;
    }
    public int getInventoryColumns() {
        return 0;
    }
    protected boolean isMobControlled() {
        return false;
    }
    protected boolean isAggravated() {
        return false;
    }
    public boolean requiresCustomPersistence() {
        return super.requiresCustomPersistence() || isTame();
    }
}
