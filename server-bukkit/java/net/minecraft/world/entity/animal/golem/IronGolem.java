// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Crackiness;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class IronGolem extends AbstractGolem implements NeutralMob {
    private long angerEndTime = -1L;
    private net.minecraft.world.entity.EntityReference<net.minecraft.world.entity.LivingEntity> angerTarget;
    protected static final EntityDataAccessor<Byte> DATA_FLAGS_ID = null;
    public IronGolem(EntityType<? extends IronGolem> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.MeleeAttackGoal(this, 1.0, true));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 6.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 100.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.KNOCKBACK_RESISTANCE, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 15.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    protected int decreaseAirSupply(int currentSupply) {
        return 0;
    }
    protected void doPush(Entity entity) {
    }
    public void aiStep() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void startPersistentAngerTimer() {
        setTimeToRemainAngry(400 + random.nextInt(381));
    }
    public void setPersistentAngerEndTime(long endTime) {
        this.angerEndTime = endTime;
    }
    public long getPersistentAngerEndTime() {
        return angerEndTime;
    }
    public void setPersistentAngerTarget(EntityReference<LivingEntity> persistentAngerTarget) {
        this.angerTarget = persistentAngerTarget;
    }
    public EntityReference<LivingEntity> getPersistentAngerTarget() {
        return angerTarget;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public Crackiness.Level getCrackiness() {
        float ratio = getHealth() / getMaxHealth();
        return ratio < 0.25F ? Crackiness.Level.HIGH : ratio < 0.5F ? Crackiness.Level.MEDIUM : ratio < 0.75F ? Crackiness.Level.LOW : Crackiness.Level.NONE;
    }
    public void handleEntityEvent(byte id) {
    }
    public int getAttackAnimationTick() {
        return 0;
    }
    public void offerFlower(boolean offer) {
        offerFlowerTick = offer ? 400 : 0;
        level().broadcastEntityEvent(this, (byte) (offer ? 11 : 34));
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.iron_golem.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.iron_golem.death");
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.iron_golem.step"), 1.0F, 1.0F);
    }
    public int getOfferFlowerTick() {
        return offerFlowerTick;
    }
    public boolean isPlayerCreated() {
        return live() instanceof org.bukkit.entity.IronGolem api ? (boolean) api.isPlayerCreated() : false;
    }
    public void setPlayerCreated(boolean value) {
        if (live() instanceof org.bukkit.entity.IronGolem api) { api.setPlayerCreated(value); }
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public Vec3 getLeashOffset() {
        return new net.minecraft.world.phys.Vec3(0.0, 0.875F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    private int offerFlowerTick;
}
