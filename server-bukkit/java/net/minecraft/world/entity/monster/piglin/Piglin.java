// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.monster.CrossbowAttackMob;
import net.minecraft.world.entity.npc.InventoryCarrier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Piglin extends AbstractPiglin implements CrossbowAttackMob, InventoryCarrier {
    public Piglin(EntityType<? extends AbstractPiglin> type, Level level) {
        super(type, level);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    private final SimpleContainer inventory = new SimpleContainer(8);
    public SimpleContainer getInventory() {
        return inventory;
    }
    protected void dropCustomDeathLoot(ServerLevel level, DamageSource source, boolean killedByPlayer) {
    }
    protected ItemStack addToInventory(ItemStack itemStack) {
        return inventory.addItem(itemStack);
    }
    protected boolean canAddToInventory(ItemStack itemStack) {
        return false;
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.monster.Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 16.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.35)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 5.0);
    }
    public static boolean checkPiglinSpawnRules(EntityType<Piglin> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected Brain<Piglin> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Piglin> made = (Brain<Piglin>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Piglin> getBrain() {
        @SuppressWarnings("unchecked") Brain<Piglin> made = (Brain<Piglin>) (Brain<?>) super.getBrain(); return made;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public void setBaby(boolean baby) {
        if (live() instanceof org.bukkit.entity.Piglin api) { api.setBaby(baby); }
    }
    public boolean isBaby() {
        return live() instanceof org.bukkit.entity.Piglin api ? (boolean) api.isBaby() : false;
    }
    protected boolean canHunt() {
        return false;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return super.getBaseExperienceReward(level);
    }
    protected void finishConversion(ServerLevel level) {
        }
    public void setChargingCrossbow(boolean isCharging) {
        if (live() instanceof org.bukkit.entity.Piglin api) { api.setChargingCrossbow(isCharging); }
    }
    public void onCrossbowAttackPerformed() {
        this.noActionTime = 0;
    }
    public PiglinArmPose getArmPose() {
        if (isDancing()) {
            return PiglinArmPose.DANCING;
        }
        if (getOffhandItem().is(net.minecraft.world.item.Items.GOLD_INGOT)) {
            return PiglinArmPose.ADMIRING_ITEM;
        }
        if (isAggressive() && getMainHandItem().getItem() instanceof net.minecraft.world.item.CrossbowItem) {
            return PiglinArmPose.CROSSBOW_HOLD;
        }
        return isAggressive() ? PiglinArmPose.ATTACKING_WITH_MELEE_WEAPON : PiglinArmPose.DEFAULT;
    }
    public boolean isDancing() {
        return live() instanceof org.bukkit.entity.Piglin api ? (boolean) api.isDancing() : false;
    }
    public void setDancing(boolean dancing) {
        if (live() instanceof org.bukkit.entity.Piglin api) { api.setDancing(dancing); }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    protected void holdInMainHand(ItemStack itemStack) {
        }
    protected void holdInOffHand(ItemStack itemStack) {
        }
    protected boolean canReplaceCurrentItem(ItemStack newItemStack) {
        return false;
    }
    protected boolean canReplaceCurrentItem(ItemStack newItemStack, ItemStack currentItemStack, EquipmentSlot slot) {
        return false;
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.piglin.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.piglin.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.piglin.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.piglin.step"), 0.15F, 1.0F);
    }
    protected void playConvertedSound() {
        }
}
