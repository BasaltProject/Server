// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.hoglin;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.monster.Enemy;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Hoglin extends Animal implements Enemy, HoglinBase {
    public static final int CONVERSION_TIME = 300;
    private int timeInOverworld;
    private int attackAnimationRemainingTicks;
    public Hoglin(EntityType<? extends Hoglin> type, Level level) {
        super(type, level);
    }
    public void setTimeInOverworld(int timeInOverworld) {
        this.timeInOverworld = timeInOverworld;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.monster.Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 40.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.KNOCKBACK_RESISTANCE, 0.6)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_KNOCKBACK, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 6.0);
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    protected void blockedByItem(LivingEntity defender, DamageSource source, float damage, boolean fullyBlocked) {
        }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected Brain<Hoglin> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Hoglin> made = (Brain<Hoglin>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Hoglin> getBrain() {
        @SuppressWarnings("unchecked") Brain<Hoglin> made = (Brain<Hoglin>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void aiStep() {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void ageBoundaryReached() {
        }
    public static boolean checkHoglinSpawnRules(EntityType<Hoglin> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void handleEntityEvent(byte id) {
    }
    public int getAttackAnimationRemainingTicks() {
        return attackAnimationRemainingTicks;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return super.getBaseExperienceReward(level);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public boolean isAdult() {
        return live() instanceof org.bukkit.entity.Hoglin api ? (boolean) api.isAdult() : false;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void setImmuneToZombification(boolean isImmuneToZombification) {
        if (live() instanceof org.bukkit.entity.Hoglin api) { api.setImmuneToZombification(isImmuneToZombification); }
    }
    public boolean isConverting() {
        return live() instanceof org.bukkit.entity.Hoglin api ? (boolean) api.isConverting() : false;
    }
    public boolean canBeHunted() {
        return live() instanceof org.bukkit.entity.Hoglin api && api.isAbleToBeHunted();
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.hoglin.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.hoglin.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.hoglin.death");
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected SoundEvent getSwimSplashSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.hoglin.step"), 0.15F, 1.0F);
    }
}
