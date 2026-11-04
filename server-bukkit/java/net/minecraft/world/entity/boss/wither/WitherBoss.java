// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.wither;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class WitherBoss extends Monster implements RangedAttackMob {
    private final float[] headYRots = new float[2];
    private final float[] headXRots = new float[2];
    private final int[] alternativeTargets = new int[2];
    public WitherBoss(EntityType<? extends WitherBoss> type, Level level) {
        super(type, level);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void registerGoals() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wither.death");
    }
    public void aiStep() {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public static boolean canDestroy(BlockState state) {
        return !state.isAir() && !state.is(net.minecraft.tags.BlockTags.WITHER_IMMUNE);
    }
    public void makeInvulnerable() {
        setInvulnerableTicks(220);
        setHealth(getMaxHealth() / 3.0F);
    }
    public void makeStuckInBlock(BlockState blockState, Vec3 speedMultiplier) {

    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void dropCustomDeathLoot(ServerLevel level, DamageSource source, boolean killedByPlayer) {
    }
    public void checkDespawn() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 300.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.6)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FLYING_SPEED, 0.6)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 40.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ARMOR, 4.0);
    }
    public float[] getHeadYRots() {
        return headYRots;
    }
    public float[] getHeadXRots() {
        return headXRots;
    }
    public int getInvulnerableTicks() {
        return live() instanceof org.bukkit.entity.Wither api ? api.getInvulnerableTicks() : 0;
    }
    public void setInvulnerableTicks(int invulnerableTicks) {
        if (live() instanceof org.bukkit.entity.Wither api) { api.setInvulnerableTicks(invulnerableTicks); }
    }
    public int getAlternativeTarget(int headIndex) {
        return alternativeTargets[headIndex];
    }
    public void setAlternativeTarget(int headIndex, int entityId) {
        alternativeTargets[headIndex] = entityId;
    }
    public boolean isPowered() {
        return getHealth() <= getMaxHealth() / 2.0F;
    }
    protected boolean canRide(Entity vehicle) {
        return false;
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return live() instanceof org.bukkit.entity.Wither api && api.canTravelThroughPortals();
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return super.canBeAffected(newEffect);
    }
    class WitherDoNothingGoal extends Goal {
        public WitherDoNothingGoal() {
            super();
        }
        public boolean canUse() {
            return false;
        }
    }
}
