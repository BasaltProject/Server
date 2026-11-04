// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.monster.CrossbowAttackMob;
import net.minecraft.world.entity.npc.InventoryCarrier;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Pillager extends AbstractIllager implements CrossbowAttackMob, InventoryCarrier {
    public Pillager(EntityType<? extends Pillager> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.monster.Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.35)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 24.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 5.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 32.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isChargingCrossbow() {
        return chargingCrossbow;
    }
    public void setChargingCrossbow(boolean isCharging) {
        this.chargingCrossbow = isCharging;
    }
    public void onCrossbowAttackPerformed() {
        this.noActionTime = 0;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public AbstractIllager.IllagerArmPose getArmPose() {
        if (isChargingCrossbow()) {
            return AbstractIllager.IllagerArmPose.CROSSBOW_CHARGE;
        }
        if (getMainHandItem().getItem() instanceof net.minecraft.world.item.CrossbowItem) {
            return AbstractIllager.IllagerArmPose.CROSSBOW_HOLD;
        }
        return isAggressive() ? AbstractIllager.IllagerArmPose.ATTACKING : AbstractIllager.IllagerArmPose.NEUTRAL;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected void enchantSpawnedWeapon(ServerLevelAccessor level, RandomSource random, DifficultyInstance difficulty) {
        }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.pillager.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.pillager.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.pillager.hurt");
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public SimpleContainer getInventory() {
        return inventory;
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        net.minecraft.world.entity.raid.Raid raid = getCurrentRaid();
        if (raid != null && random.nextFloat() <= raid.getEnchantOdds()) {
            ItemStack crossbow = new ItemStack(net.minecraft.world.item.Items.CROSSBOW);
            net.minecraft.resources.ResourceKey<net.minecraft.world.item.enchantment.providers.EnchantmentProvider> provider = wave > raid.getNumGroups(net.minecraft.world.Difficulty.NORMAL)
                ? net.minecraft.world.item.enchantment.providers.VanillaEnchantmentProviders.RAID_PILLAGER_POST_WAVE_5
                : wave > raid.getNumGroups(net.minecraft.world.Difficulty.EASY) ? net.minecraft.world.item.enchantment.providers.VanillaEnchantmentProviders.RAID_PILLAGER_POST_WAVE_3 : null;
            if (provider != null) {
                net.minecraft.world.item.enchantment.EnchantmentHelper.enchantItemFromProvider(crossbow, level.registryAccess(), provider, level.getCurrentDifficultyAt(blockPosition()), getRandom());
                setItemSlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND, crossbow);
            }
        }
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    private boolean chargingCrossbow;
    private final SimpleContainer inventory = new SimpleContainer(5);

}
