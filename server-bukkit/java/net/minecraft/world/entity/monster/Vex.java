// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.OwnableEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.TraceableEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.target.TargetGoal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Vex extends Monster implements TraceableEntity, OwnableEntity {
    public static final float FLAP_DEGREES_PER_TICK = 45.836624F;
    public static final int TICKS_PER_FLAP = 0;
    protected static final EntityDataAccessor<Byte> DATA_FLAGS_ID = null;
    public Vex(EntityType<? extends Vex> type, Level level) {
        super(type, level);
        this.moveControl = new VexMoveControl<>(this);
    }
    private Mob owner;
    private BlockPos boundOrigin;
    public boolean isFlapping() {
        return super.isFlapping();
    }
    protected boolean isAffectedByBlocks() {
        return false;
    }
    public void tick() {
    }
    protected void registerGoals() {
        goalSelector.addGoal(0, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(9, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 3.0F, 1.0F));
        targetSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.player.Player.class, true));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 14.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 4.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void restoreFrom(Entity oldEntity) {
        super.restoreFrom(oldEntity);
        if (oldEntity instanceof Vex vex) {
            owner = vex.owner;
        }
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public LivingEntity getOwner() {
        return owner;
    }
    public EntityReference<LivingEntity> getOwnerReference() {
        return owner == null ? null : EntityReference.of(owner);
    }
    public BlockPos getBoundOrigin() {
        return boundOrigin;
    }
    public void setBoundOrigin(BlockPos boundOrigin) {
        this.boundOrigin = boundOrigin;
    }
    public boolean isCharging() {
        return live() instanceof org.bukkit.entity.Vex api ? (boolean) api.isCharging() : false;
    }
    public void setIsCharging(boolean value) {
        if (live() instanceof org.bukkit.entity.Vex api) { api.setCharging(value); }
    }
    public void setOwner(Mob owner) {
        this.owner = owner;
        if (live() instanceof org.bukkit.entity.Vex api && owner != null) { api.setSummoner((org.bukkit.entity.Mob) owner.getBukkitEntity()); }
    }
    public void setLimitedLife(int lifeTicks) {
        if (live() instanceof org.bukkit.entity.Vex api) { api.setLimitedLifetimeTicks(lifeTicks); }
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.vex.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.vex.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.vex.hurt");
    }
    public float getLightLevelDependentMagicValue() {
        return 1.0F;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    class VexMoveControl<T extends Mob> extends MoveControl<T> {
        public VexMoveControl(T vex) {
            super(vex);
        }
        public void tick() {
            if (operation != Operation.MOVE_TO) {
                return;
            }
            double dx = wantedX - mob.getX();
            double dy = wantedY - mob.getY();
            double dz = wantedZ - mob.getZ();
            double dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < mob.getBoundingBox().getSize()) {
                operation = Operation.WAIT;
                mob.setDeltaMovement(mob.getDeltaMovement().scale(0.5));
                return;
            }
            double step = 0.05 * speedModifier / dist;
            mob.setDeltaMovement(mob.getDeltaMovement().add(dx * step, dy * step, dz * step));
            if (mob.getTarget() == null) {
                mob.setYRot(-((float) Math.atan2(mob.getDeltaMovement().x, mob.getDeltaMovement().z)) * (180.0F / (float) Math.PI));
            } else {
                mob.setYRot(-((float) Math.atan2(mob.getTarget().getX() - mob.getX(), mob.getTarget().getZ() - mob.getZ())) * (180.0F / (float) Math.PI));
            }
            mob.yBodyRot = mob.getYRot();
        }
    }
    class VexChargeAttackGoal extends Goal {
        public VexChargeAttackGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            LivingEntity target = getTarget();
            return target != null && !getMoveControl().hasWanted() && random.nextInt(reducedTickDelay(7)) == 0
                && distanceToSqr(target) > 4.0;
        }
        public boolean canContinueToUse() {
            LivingEntity target = getTarget();
            return getMoveControl().hasWanted() && isCharging() && target != null && target.isAlive();
        }
        public void start() {
            LivingEntity target = getTarget();
            if (target != null) {
                net.minecraft.world.phys.Vec3 eye = target.getEyePosition();
                getMoveControl().setWantedPosition(eye.x, eye.y, eye.z, 1.0);
            }
            setIsCharging(true);
        }
        public void stop() {
            setIsCharging(false);
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            LivingEntity target = getTarget();
            if (target == null) {
                return;
            }
            if (getBoundingBox().intersects(target.getBoundingBox())) {
                doHurtTarget(null, target);
                setIsCharging(false);
            } else if (distanceToSqr(target) < 9.0) {
                net.minecraft.world.phys.Vec3 eye = target.getEyePosition();
                getMoveControl().setWantedPosition(eye.x, eye.y, eye.z, 1.0);
            }
        }
    }
    class VexRandomMoveGoal extends Goal {
        public VexRandomMoveGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            return !getMoveControl().hasWanted() && random.nextInt(7) == 0;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public void tick() {
            BlockPos origin = getBoundOrigin() != null ? getBoundOrigin() : blockPosition();
            for (int i = 0; i < 3; i++) {
                BlockPos pos = origin.offset(random.nextInt(15) - 7, random.nextInt(11) - 5, random.nextInt(15) - 7);
                if (level().isEmptyBlock(pos)) {
                    getMoveControl().setWantedPosition(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5, 0.25);
                    if (getTarget() == null) {
                        getLookControl().setLookAt(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5, 180.0F, 20.0F);
                    }
                    break;
                }
            }
        }
    }
    class VexCopyOwnerTargetGoal extends TargetGoal {
        public VexCopyOwnerTargetGoal(PathfinderMob mob) {
            super(Vex.this, false);
        }
        public boolean canUse() {
            return owner != null && owner.getTarget() != null && Vex.this.canAttack(owner.getTarget());
        }
        public void start() {
            Vex.this.setTarget(owner.getTarget());
            super.start();
        }
    }
}
