// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.illager;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.BreakDoorGoal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Vindicator extends AbstractIllager {
    public Vindicator(EntityType<? extends Vindicator> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.monster.Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.35)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 12.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 24.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 5.0);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public AbstractIllager.IllagerArmPose getArmPose() {
        return isAggressive() ? AbstractIllager.IllagerArmPose.ATTACKING : isCelebrating() ? AbstractIllager.IllagerArmPose.CELEBRATING : AbstractIllager.IllagerArmPose.CROSSED;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.vindicator.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.vindicator.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.vindicator.hurt");
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        net.minecraft.world.item.ItemStack axe = new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.IRON_AXE);
        net.minecraft.world.entity.raid.Raid raid = getCurrentRaid();
        if (raid != null && random.nextFloat() <= raid.getEnchantOdds()) {
            net.minecraft.world.item.enchantment.EnchantmentHelper.enchantItemFromProvider(axe, level.registryAccess(),
                wave > raid.getNumGroups(net.minecraft.world.Difficulty.NORMAL) ? net.minecraft.world.item.enchantment.providers.VanillaEnchantmentProviders.RAID_VINDICATOR_POST_WAVE_5
                    : net.minecraft.world.item.enchantment.providers.VanillaEnchantmentProviders.RAID_VINDICATOR,
                level.getCurrentDifficultyAt(blockPosition()), random);
        }
        setItemSlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND, axe);
    }
    static class VindicatorBreakDoorGoal extends BreakDoorGoal {
        public VindicatorBreakDoorGoal(Mob mob) {
            super(mob, 6, d -> d == net.minecraft.world.Difficulty.NORMAL || d == net.minecraft.world.Difficulty.HARD);
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
    }
    static class VindicatorJohnnyAttackGoal extends NearestAttackableTargetGoal<LivingEntity> {
        public VindicatorJohnnyAttackGoal(Vindicator mob) {
            super(mob, LivingEntity.class, 0, true, true, (target, level) -> target.attackable());
        }
    }
}
