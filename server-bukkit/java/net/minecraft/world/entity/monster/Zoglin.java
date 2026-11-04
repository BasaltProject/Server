// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.monster.hoglin.HoglinBase;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Zoglin extends Monster implements HoglinBase {
    public Zoglin(EntityType<? extends Zoglin> type, Level level) {
        super(type, level);
    }
    protected Brain<Zoglin> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Zoglin> made = (Brain<Zoglin>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    protected static List<ActivityData<Zoglin>> getActivities() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.Zoglin#getActivities");
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public boolean isAdult() {
        return live() instanceof org.bukkit.entity.Zoglin api ? (boolean) api.isAdult() : false;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    protected void blockedByItem(LivingEntity defender, DamageSource source, float damage, boolean fullyBlocked) {
        }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public Brain<Zoglin> getBrain() {
        @SuppressWarnings("unchecked") Brain<Zoglin> made = (Brain<Zoglin>) (Brain<?>) super.getBrain(); return made;
    }
    protected void updateActivity() {
        }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void setBaby(boolean baby) {
        if (live() instanceof org.bukkit.entity.Zoglin api) { api.setBaby(baby); }
    }
    public boolean isBaby() {
        return live() instanceof org.bukkit.entity.Zoglin api ? (boolean) api.isBaby() : false;
    }
    public void aiStep() {
    }
    public void handleEntityEvent(byte id) {
    }
    public int getAttackAnimationRemainingTicks() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.Zoglin#getAttackAnimationRemainingTicks");
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.zoglin.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.zoglin.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.zoglin.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.zoglin.step"), 0.15F, 1.0F);
    }
    protected void playAngrySound() {
        }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
}
