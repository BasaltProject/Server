// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Shearable;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class SnowGolem extends AbstractGolem implements RangedAttackMob, Shearable {
    public SnowGolem(EntityType<? extends SnowGolem> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0, 1.0000001E-5F));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 6.0F));
        goalSelector.addGoal(4, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 4.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.2);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean isSensitiveToWater() {
        return super.isSensitiveToWater();
    }
    public void aiStep() {
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public float rangedAttackUncertainty(Level level) {
        return 10.0F;
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void shear(ServerLevel level, SoundSource soundSource, ItemStack tool) {
        setPumpkin(false);
    }
    public boolean readyForShearing() {
        return isAlive() && hasPumpkin();
    }
    public boolean hasPumpkin() {
        return live() instanceof org.bukkit.entity.Snowman api && !api.isDerp();
    }
    public void setPumpkin(boolean pumpkin) {
        if (live() instanceof org.bukkit.entity.Snowman api) { api.setDerp(!pumpkin); }
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.snow_golem.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.snow_golem.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.snow_golem.death");
    }
    public Vec3 getLeashOffset() {
        return new net.minecraft.world.phys.Vec3(0.0, 0.75F * getEyeHeight(), getBbWidth() * 0.4F);
    }
}
