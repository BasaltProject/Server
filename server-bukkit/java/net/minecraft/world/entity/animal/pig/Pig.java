// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.pig;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.ItemSteerable;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Pig extends Animal implements ItemSteerable {
    public Pig(EntityType<? extends Pig> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(0, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.PanicGoal(this, 1.25));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 1.0));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.FollowParentGoal(this, 1.1));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, Player.class, 6.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes().add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25);
    }
    public LivingEntity getControllingPassenger() {
        if (isSaddled() && getFirstPassenger() instanceof Player player
            && player.isHolding(net.minecraft.world.item.Items.CARROT_ON_A_STICK)) {
            return player;
        }
        return super.getControllingPassenger();
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId(isBaby() ? "entity.baby_pig.ambient" : "entity.pig.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId(isBaby() ? "entity.baby_pig.hurt" : "entity.pig.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId(isBaby() ? "entity.baby_pig.death" : "entity.pig.death");
    }
    protected void playEatingSound() {
        }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId(isBaby() ? "entity.baby_pig.step" : "entity.pig.step"), 0.15F, 1.0F);
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return slot == EquipmentSlot.SADDLE ? isAlive() && !isBaby() : super.canUseSlot(slot);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return slot == EquipmentSlot.SADDLE;
    }
    protected Holder<SoundEvent> getEquipSound(EquipmentSlot slot, ItemStack stack, Equippable equippable) {
        return slot == EquipmentSlot.SADDLE ? Holder.direct(net.minecraft.sounds.SoundEvents.byId("entity.pig.saddle")) : super.getEquipSound(slot, stack, equippable);
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        return new Vec3(0.0, 0.0, 1.0);
    }
    protected float getRiddenSpeed(Player controller) {
        return (float) (getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED) * 0.225);
    }
    public boolean boost() {
        if (live() instanceof org.bukkit.entity.Steerable steerable && steerable.hasSaddle() && steerable.getCurrentBoostTicks() == 0) {
            steerable.setBoostTicks(140 + getRandom().nextInt(841));
            return true;
        }
        return false;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public Pig getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.6F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public Holder<PigVariant> getVariant() {
        return live() instanceof org.bukkit.entity.Pig api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.PIG_VARIANT, net.minecraft.world.entity.animal.pig.PigVariant.class, api.getVariant()) : null;
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
}
