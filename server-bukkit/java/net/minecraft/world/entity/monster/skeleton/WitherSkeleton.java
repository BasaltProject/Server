// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.skeleton;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
public class WitherSkeleton extends AbstractSkeleton {
    public WitherSkeleton(EntityType<? extends WitherSkeleton> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither_skeleton.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither_skeleton.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither_skeleton.death");
    }
    protected SoundEvent getStepSound() {
        return net.minecraft.sounds.SoundEvents.WITHER_SKELETON_STEP;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected void populateDefaultEquipmentEnchantments(ServerLevelAccessor level, RandomSource random, DifficultyInstance localDifficulty) {
        }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    protected AbstractArrow getArrow(ItemStack projectile, float power, ItemStack firingWeapon) {
        AbstractArrow arrow = super.getArrow(projectile, power, firingWeapon);
        arrow.igniteForSeconds(100.0F);
        return arrow;
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return super.canBeAffected(newEffect);
    }
}
