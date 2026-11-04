// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.raid.Raider;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
public class Ravager extends Raider {
    public static final int ATTACK_DURATION = 10;
    public static final int STUN_DURATION = 40;
    public Ravager(EntityType<? extends Ravager> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void updateControlFlags() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SoundEvent getCelebrateSound() {
        return null;
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public void aiStep() {
    }
    protected boolean isImmobile() {
        return false;
    }
    protected void blockedByItem(LivingEntity defender, DamageSource source, float damage, boolean fullyBlocked) {
        }
    public void handleEntityEvent(byte id) {
    }
    public int getAttackTick() {
        return live() instanceof org.bukkit.entity.Ravager api ? api.getAttackTicks() : 0;
    }
    public int getStunnedTick() {
        return live() instanceof org.bukkit.entity.Ravager api ? api.getStunnedTicks() : 0;
    }
    public int getRoarTick() {
        return live() instanceof org.bukkit.entity.Ravager api ? api.getRoarTicks() : 0;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ravager.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.ravager.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ravager.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.ravager.step"), 0.15F, 1.0F);
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.Ravager#applyRaidBuffs");
    }
    public boolean canBeLeader() {
        return false;
    }
}
