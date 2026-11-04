// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.Items;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Enderman extends Monster implements NeutralMob {
    private long angerEndTime = -1L;
    private net.minecraft.world.entity.EntityReference<net.minecraft.world.entity.LivingEntity> angerTarget;
    public static final int MAX_TELEPORT_RADIUS = 32;
    public static final int MAX_TELEPORT_DIAMETER = 64;
    public Enderman(EntityType<? extends Enderman> type, Level level) {
        super(type, level);
    }
    private BlockState carriedBlock;
    private boolean staredAt;
    private boolean creepy;
    public boolean isLookingAtMe(Player player) {
        if (player.getItemBySlot(EquipmentSlot.HEAD).is(Items.CARVED_PUMPKIN)) {
            return false;
        }
        return isLookingAtMe(player, 0.025, true, false, getEyeY());
    }
    boolean teleportTowards(net.minecraft.world.entity.Entity target) {
        Vec3 away = new Vec3(getX() - target.getX(), getY(0.5) - target.getEyeY(), getZ() - target.getZ()).normalize();
        double x = getX() + (random.nextDouble() - 0.5) * 8.0 - away.x * 16.0;
        double y = getY() + (random.nextInt(16) - 8) - away.y * 16.0;
        double z = getZ() + (random.nextDouble() - 0.5) * 8.0 - away.z * 16.0;
        return teleport(x, y, z);
    }
    protected void registerGoals() {
        goalSelector.addGoal(0, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.MeleeAttackGoal(this, 1.0, false));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0, 0.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 40.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 7.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 64.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    public void setTarget(LivingEntity target) {
        super.setTarget(target);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
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
    public void playStareSound() {
        if (live() != null) { live().getWorld().playSound(live().getLocation(), org.bukkit.Sound.ENTITY_ENDERMAN_STARE, 2.5F, 1.0F); }
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void aiStep() {
    }
    public boolean isSensitiveToWater() {
        return super.isSensitiveToWater();
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    protected boolean teleport() {
        if (!isAlive()) {
            return false;
        }
        double x = getX() + (random.nextDouble() - 0.5) * 64.0;
        double y = getY() + (random.nextInt(64) - 32);
        double z = getZ() + (random.nextDouble() - 0.5) * 64.0;
        return teleport(x, y, z);
    }
    public boolean teleport(double x, double y, double z) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos(x, y, z);
        int min = live().getWorld().getMinHeight();
        while (pos.getY() > min && !level().getBlockState(pos).isSolid()) {
            pos.move(Direction.DOWN);
        }
        BlockState ground = level().getBlockState(pos);
        if (!ground.isSolid() || !ground.getFluidState().isEmpty()) {
            return false;
        }
        BlockPos feet = pos.above();
        if (!level().getBlockState(feet).getFluidState().isEmpty() || level().getBlockState(feet).isSolid()
            || level().getBlockState(feet.above()).isSolid()) {
            return false;
        }
        snapTo(x, feet.getY(), z);
        return true;
    }
    public boolean canRandomlyTeleportTo(double x, double y, double z) {
        BlockPos pos = BlockPos.containing(x, y, z);
        BlockState ground = level().getBlockState(pos.below());
        return ground.isSolid() && !level().getBlockState(pos).isSolid() && !level().getBlockState(pos.above()).isSolid();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.enderman.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.enderman.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.enderman.death");
    }
    protected void dropCustomDeathLoot(ServerLevel level, DamageSource source, boolean killedByPlayer) {
    }
    public void setCarriedBlock(BlockState carryingBlock) {
        this.carriedBlock = carryingBlock;
    }
    public BlockState getCarriedBlock() {
        return carriedBlock;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean projectileReceivesSideEffectsOnHit(boolean wasHurt) {
        return true;
    }
    public boolean isCreepy() {
        return getTarget() != null || creepy;
    }
    public boolean hasBeenStaredAt() {
        return staredAt;
    }
    public void setBeingStaredAt() {
        this.staredAt = true;
    }
    public boolean requiresCustomPersistence() {
        return super.requiresCustomPersistence() || getCarriedBlock() != null;
    }
    static class EndermanLookForPlayerGoal extends NearestAttackableTargetGoal<Player> {
        private final Enderman enderman;
        private Player pendingTarget;
        private int aggroTime;
        private int teleportTime;
        public EndermanLookForPlayerGoal(Enderman enderman, TargetingConditions.Selector isAngryAt) {
            super(enderman, Player.class, 10, false, false, isAngryAt);
            this.enderman = enderman;
        }
        public boolean canUse() {
            pendingTarget = enderman.level().getNearestPlayer(enderman, getFollowDistance());
            if (pendingTarget != null && !enderman.isLookingAtMe(pendingTarget)) {
                pendingTarget = null;
            }
            return pendingTarget != null;
        }
        public void start() {
            aggroTime = adjustedTickDelay(5);
            teleportTime = 0;
            enderman.setBeingStaredAt();
        }
        public void stop() {
            pendingTarget = null;
            super.stop();
        }
        public boolean canContinueToUse() {
            if (pendingTarget != null) {
                if (!enderman.isLookingAtMe(pendingTarget)) {
                    return false;
                }
                enderman.lookAt(pendingTarget, 10.0F, 10.0F);
                return true;
            }
            return super.canContinueToUse();
        }
        public void tick() {
            if (enderman.getTarget() == null) {
                super.setTarget(null);
            }
            if (pendingTarget != null) {
                if (--aggroTime <= 0) {
                    target = pendingTarget;
                    pendingTarget = null;
                    super.start();
                }
                return;
            }
            LivingEntity current = enderman.getTarget();
            if (current != null && !enderman.isPassenger()) {
                if (current instanceof Player player && enderman.isLookingAtMe(player)) {
                    if (current.distanceToSqr(enderman) < 16.0) {
                        enderman.teleport();
                    }
                    teleportTime = 0;
                } else if (current.distanceToSqr(enderman) > 256.0 && teleportTime++ >= adjustedTickDelay(30)
                    && enderman.teleportTowards(current)) {
                    teleportTime = 0;
                }
            }
        }
    }
    static class EndermanFreezeWhenLookedAt extends Goal {
        private final Enderman enderman;
        private LivingEntity target;
        public EndermanFreezeWhenLookedAt(Enderman enderman) {
            this.enderman = enderman;
            setFlags(java.util.EnumSet.of(Goal.Flag.JUMP, Goal.Flag.MOVE));
        }
        public boolean canUse() {
            target = enderman.getTarget();
            return target instanceof Player player && target.distanceToSqr(enderman) <= 256.0 && enderman.isLookingAtMe(player);
        }
        public void start() {
            enderman.getNavigation().stop();
        }
        public void tick() {
            enderman.getLookControl().setLookAt(target.getX(), target.getEyeY(), target.getZ());
        }
    }
    static class EndermanLeaveBlockGoal extends Goal {
        private final Enderman enderman;
        public EndermanLeaveBlockGoal(Enderman enderman) {
            this.enderman = enderman;
        }
        public boolean canUse() {
            return enderman.getCarriedBlock() != null && enderman.getRandom().nextInt(reducedTickDelay(2000)) == 0;
        }
        public void tick() {
            RandomSource random = enderman.getRandom();
            BlockPos pos = BlockPos.containing(enderman.getX() - 1.0 + random.nextDouble() * 2.0,
                enderman.getY() + random.nextDouble() * 2.0, enderman.getZ() - 1.0 + random.nextDouble() * 2.0);
            if (enderman.level().getBlockState(pos).isAir() && enderman.level().getBlockState(pos.below()).isSolid()) {
                enderman.level().setBlock(pos, enderman.getCarriedBlock(), 3);
                enderman.setCarriedBlock(null);
            }
        }
    }
    static class EndermanTakeBlockGoal extends Goal {
        private final Enderman enderman;
        public EndermanTakeBlockGoal(Enderman enderman) {
            this.enderman = enderman;
        }
        public boolean canUse() {
            return enderman.getCarriedBlock() == null && enderman.getRandom().nextInt(reducedTickDelay(20)) == 0;
        }
        public void tick() {
            RandomSource random = enderman.getRandom();
            BlockPos pos = BlockPos.containing(enderman.getX() - 2.0 + random.nextDouble() * 4.0,
                enderman.getY() + random.nextDouble() * 3.0, enderman.getZ() - 2.0 + random.nextDouble() * 4.0);
            BlockState state = enderman.level().getBlockState(pos);
            if (state.is(net.minecraft.tags.BlockTags.ENDERMAN_HOLDABLE)) {
                enderman.level().removeBlock(pos, false);
                enderman.setCarriedBlock(state);
            }
        }
    }
}
