// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.spider;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.MeleeAttackGoal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
public class Spider extends Monster {
    public Spider(EntityType<? extends Spider> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.LeapAtTargetGoal(this, 0.4F));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 0.8));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 16.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.spider.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.spider.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.spider.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.spider.step"), 0.15F, 1.0F);
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return super.canBeAffected(newEffect);
    }
    public boolean isClimbing() {
        return live() instanceof org.bukkit.entity.Spider api ? (boolean) api.isClimbing() : false;
    }
    public void setClimbing(boolean value) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.spider.Spider#setClimbing");
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static class SpiderEffectsGroupData implements SpawnGroupData {
        public Holder<MobEffect> effect;
        public void setRandomEffect(RandomSource random) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.spider.Spider.SpiderEffectsGroupData#setRandomEffect");
        }
    }
    static class SpiderAttackGoal extends MeleeAttackGoal {
        public SpiderAttackGoal(Spider mob) {
            super(mob, 1.0, true);
        }
        public boolean canUse() {
            return super.canUse() && !mob.isVehicle();
        }
        public boolean canContinueToUse() {
            if (mob.getLightLevelDependentMagicValue() >= 0.5F && mob.getRandom().nextInt(100) == 0) {
                mob.setTarget(null);
                return false;
            }
            return super.canContinueToUse();
        }
    }
    static class SpiderTargetGoal<T extends LivingEntity> extends NearestAttackableTargetGoal<T> {
        public SpiderTargetGoal(Spider mob, Class<T> targetType) {
            super(mob, targetType, true);
        }
        public boolean canUse() {
            return mob.getLightLevelDependentMagicValue() < 0.5F && super.canUse();
        }
    }
}
