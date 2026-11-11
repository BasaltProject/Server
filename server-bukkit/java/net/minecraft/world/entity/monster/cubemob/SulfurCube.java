// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.cubemob;
import net.minecraft.world.item.Items;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.core.component.DataComponents;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Bucketable;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Shearable;
import net.minecraft.world.entity.SulfurCubeArchetype;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.TemptGoal;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class SulfurCube extends AbstractCubeMob implements Bucketable, Shearable {
    private int fuse = -1;
    public static final int SPLIT_COUNT = 2;
    public static final int MAX_SIZE = 2;
    public static final int MIN_SIZE = 1;
    public static final int PICKUP_TIMER_DURATION = 100;
    public static final double PUSH_DISTANCE_THRESHOLD = 1.3;
    public SulfurCube(EntityType<? extends SulfurCube> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addBehaviourGoals() {
        }
    public boolean fromBucket() {
        return false;
    }
    public int getFuse() {
        return fuse;
    }
    public boolean isPrimed() {
        return getFuse() >= 0;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public void setFromBucket(boolean fromBucket) {
        if (live() instanceof org.bukkit.entity.SulfurCube api) { api.setFromBucket(fromBucket); }
    }
    public SoundEvent getPickupSound() {
        return null;
    }
    public void saveToBucketTag(ItemStack bucket) {
        CustomData.update(DataComponents.BUCKET_ENTITY_DATA, bucket, tag -> {
            tag.putInt("age", getAge());
            tag.putBoolean("age_locked", isAgeLocked());
        });
    }
    public boolean canBreatheUnderwater() {
        return live() instanceof org.bukkit.entity.SulfurCube api ? (boolean) api.canBreatheUnderwater() : false;
    }
    protected void travelInFluid(Vec3 input) {
        }
    public double getFluidJumpThreshold() {
        return getBbHeight() * 0.2;
    }
    public void loadFromBucketTag(CompoundTag tag) {
        setAge(tag.getIntOr("age", 0));
        setAgeLocked(tag.getBooleanOr("age_locked", false));
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(Items.SULFUR_CUBE_BUCKET);
    }
    protected void addTargetingGoals() {
        }
    public float getLightLevelDependentMagicValue() {
        return 1.0F;
    }
    protected boolean canDealDamage() {
        return false;
    }
    public static boolean checkSulfurCubeSpawnRules(EntityType<SulfurCube> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean requiresCustomPersistence() {
        return super.requiresCustomPersistence() || hasBodyItem() || fromBucket();
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean hasBodyItem() {
        return !getItemBySlot(EquipmentSlot.BODY).isEmpty();
    }
    public boolean canExplode() {
        return live() instanceof org.bukkit.entity.SulfurCube api ? (boolean) api.canExplode() : false;
    }
    public List<SulfurCubeArchetype> matchingArchetypes(ItemStack stack) {
        List<SulfurCubeArchetype> hits = new ArrayList<>();
        level().registryAccess().lookupOrThrow(Registries.SULFUR_CUBE_ARCHETYPE).stream().forEach(archetype -> {
            if (stack.is(archetype.items())) {
                hits.add(archetype);
            }
        });
        return hits;
    }
    public void tick() {
    }
    public boolean primeTime(boolean imminent) {
        return false;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    protected Map<EquipmentSlot, ItemStack> collectEquipmentChanges(Map<EquipmentSlot, ItemStack> lastEquipmentItems) {
        return super.collectEquipmentChanges(lastEquipmentItems);
    }
    public float maxUpStep() {
        return hasBodyItem() ? 0.0F : super.maxUpStep();
    }
    protected boolean omnidirectionalAirMover() {
        return false;
    }
    public boolean canFreeze() {
        return hasBodyItem() ? false : super.canFreeze();
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean equipItem(ItemStack heldItem) {
        if (isBaby() || (hasBodyItem() && heldItem.is(getItemBySlot(EquipmentSlot.BODY).getItem()))) {
            return false;
        }
        if (!level().isClientSide()) {
            setItemSlotAndDropWhenKilled(EquipmentSlot.BODY, heldItem.copyWithCount(1));
        }
        return true;
    }
    protected void playEatingSound() {
        }
    public boolean canBePickedUpWithBucket(ItemStack itemStack) {
        return itemStack.getItem() == Items.BUCKET;
    }
    public EquipmentSlot getEquipmentSlotForItem(ItemStack itemStack) {
        return super.getEquipmentSlotForItem(itemStack);
    }
    public boolean isEquippableInSlot(ItemStack itemStack, EquipmentSlot slot) {
        return super.isEquippableInSlot(itemStack, slot);
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected SoundEvent getSquishSound() {
        return null;
    }
    protected SoundEvent getJumpSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected ParticleOptions getParticleType() {
        return ParticleTypes.SULFUR_CUBE_GOO;
    }
    public static AttributeSupplier.Builder createSulfurCubeAttributes() {
        return Mob.createMobAttributes();
    }
    public void shear(ServerLevel level, SoundSource soundSource, ItemStack tool) {
        ItemStack swallowed = getItemBySlot(EquipmentSlot.BODY);
        setItemSlot(EquipmentSlot.BODY, ItemStack.EMPTY);
        spawnAtLocation(level, swallowed);
    }
    public boolean readyForShearing() {
        return hasBodyItem();
    }
    public boolean canPickUpLoot() {
        return !hasBodyItem();
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    public boolean canHoldItem(ItemStack itemStack) {
        return getItemBySlot(EquipmentSlot.BODY).isEmpty() && !isBaby();
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    protected int getBaseExperienceReward(ServerLevel level) {
        return super.getBaseExperienceReward(level);
    }
    protected int getSplitCount() {
        return 0;
    }
    protected void setSpawnSize(ServerLevelAccessor level, DifficultyInstance difficulty) {
        }
    public void setSize(int size, boolean updateHealth) {
        super.setSize(size, updateHealth);
        if (updateHealth && size == 1 && !isBaby()) {
            setBaby(true);
        }
    }
    protected void setUpSplitCube(AbstractCubeMob cubeMob, int halfSize, float xd, float zd) {
        }
    public AbstractCubeMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected void ageBoundaryReached() {
        }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void doPush(Entity entity) {
    }
    public void playerTouch(Player player) {
        super.playerTouch(player);
    }
    public void knockback(double power, double xd, double zd, DamageSource source, float damage, boolean comesFromEffect) {
        super.knockback(power, xd, zd, source, damage, comesFromEffect);
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, getBbHeight() / 2.0F, 0.0);
    }
    protected void setCubeMobHealth(int actualSize) {
        }
    public boolean isInvulnerableToPiercingWeapon() {
        return super.isInvulnerableToPiercingWeapon() && !isPrimed();
    }
    public boolean canBePickedFromInside() {
        return !hasBodyItem();


    }
    static class SulfurCubeTemptGoal extends TemptGoal.ForNonPathfinders {
        public SulfurCubeTemptGoal(Mob mob, double speedModifier, Predicate<ItemStack> items, boolean canScare, double stopDistance) {
            super(mob, speedModifier, items, canScare, stopDistance);
        }
        protected void stopNavigation() {
                }
        protected void navigateTowards(Player player) {
                }
    }
    class SulfurCubeSearchForItemsGoal extends Goal {
        public SulfurCubeSearchForItemsGoal(SulfurCube sulfurCube) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void tick() {
        }
    }
    class SulfurCubeLookControl extends LookControl {
        private SulfurCubeLookControl() {
            super((net.minecraft.world.entity.Mob) null);
        }
        public void tick() {
        }
    }
    protected static class SulfurCubeMobMoveControl<T extends SulfurCube> extends AbstractCubeMob.CubeMobMoveControl<T> {
        public SulfurCubeMobMoveControl(T cubeMob) {
            super(cubeMob);
        }
        public void tick() {
        }
    }
}
