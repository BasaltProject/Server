// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.raid.Raider;
import net.minecraft.world.level.Level;
public class Witch extends Raider implements RangedAttackMob {
    public Witch(EntityType<? extends Witch> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.witch.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.witch.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.witch.death");
    }
    public void setUsingItem(boolean using) {
        this.usingItem = using;
    }
    private boolean usingItem;
    public boolean isDrinkingPotion() {
        return usingItem || live() instanceof org.bukkit.entity.Witch api && api.isDrinkingPotion();
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 26.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25);
    }
    public void aiStep() {
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    public void handleEntityEvent(byte id) {
    }
    protected float getDamageAfterMagicAbsorb(DamageSource damageSource, float damage) {
        return 0;
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public float rangedAttackUncertainty(Level level) {
        return 8.0F;
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        
    }
    public boolean canBeLeader() {
        return false;
    }
}
