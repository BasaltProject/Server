// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ambient;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Bat extends AmbientCreature {
    public static final float FLAP_LENGTH_SECONDS = 0.5F;
    public static final float TICKS_PER_FLAP = 10.0F;
    public final AnimationState flyAnimationState = new AnimationState();
    public final AnimationState restAnimationState = new AnimationState();
    public Bat(EntityType<? extends Bat> type, Level level) {
        super(type, level);
    }
    public boolean isFlapping() {
        return super.isFlapping();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Bat api ? (float) api.getSoundVolume() : 0;
    }
    public float getVoicePitch() {
        return super.getVoicePitch() * 0.95F;
    }
    public SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    public boolean isPushable() {
        return false;
    }
    protected void doPush(Entity entity) {
    }
    protected void pushEntities() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public boolean isResting() {
        return live() instanceof org.bukkit.entity.Bat api && !api.isAwake();
    }
    public void setResting(boolean value) {
        if (live() instanceof org.bukkit.entity.Bat api) {
            api.setAwake(!value);
        }
    }
    public void tick() {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean isIgnoringBlockTriggers() {
        return super.isIgnoringBlockTriggers();
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public static boolean checkBatSpawnRules(EntityType<Bat> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
}
