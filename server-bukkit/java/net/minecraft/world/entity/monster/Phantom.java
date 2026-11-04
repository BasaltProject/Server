// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.animal.feline.Cat;
import net.minecraft.world.entity.player.Player;
import net.minecraft.util.Mth;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Phantom extends Mob implements Enemy {
    public static final float FLAP_DEGREES_PER_TICK = 7.448451F;
    public static final int TICKS_PER_FLAP = 0;
    public Phantom(EntityType<? extends Phantom> type, Level level) {
        super(type, level);
        this.moveControl = new PhantomMoveControl<>(this);
        this.lookControl = new PhantomLookControl(this);
    }
    public boolean isFlapping() {
        return super.isFlapping();
    }
    protected BodyRotationControl createBodyControl() {
        return new PhantomBodyRotationControl(this);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new PhantomAttackStrategyGoal());
        goalSelector.addGoal(2, new PhantomSweepAttackGoal());
        goalSelector.addGoal(3, new PhantomCircleAroundAnchorGoal());
        targetSelector.addGoal(1, new PhantomAttackPlayerTargetGoal());
    }
    Vec3 moveTargetPoint = Vec3.ZERO;
    BlockPos anchorPoint = BlockPos.ZERO;
    AttackPhase attackPhase = AttackPhase.CIRCLE;
    private int seaLevel() {
        return live().getWorld().getSeaLevel();
    }
    void anchorAboveTarget() {
        BlockPos above = getTarget().blockPosition().above(20 + random.nextInt(20));
        anchorPoint = above.getY() < seaLevel() ? new BlockPos(above.getX(), seaLevel() + 1, above.getZ()) : above;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setPhantomSize(int size) {
        if (live() instanceof org.bukkit.entity.Phantom api) { api.setSize(size); }
    }
    public int getPhantomSize() {
        return live() instanceof org.bukkit.entity.Phantom api ? api.getSize() : 0;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public int getUniqueFlapTickOffset() {
        return getId() * 3;
    }
    public void tick() {
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean onClimbable() {
        return false;
    }
    public void travel(Vec3 input) {
        move(net.minecraft.world.entity.MoverType.SELF, getDeltaMovement());
        setDeltaMovement(getDeltaMovement().scale(0.99));
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.phantom.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.phantom.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.phantom.death");
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    static enum AttackPhase {
        CIRCLE, SWOOP;
    }
    class PhantomMoveControl<T extends Mob> extends MoveControl<T> {
        private float speed = 0.1F;
        public PhantomMoveControl(T mob) {
            super(mob);
        }
        public void tick() {
            if (horizontalCollision) {
                setYRot(getYRot() + 180.0F);
                speed = 0.1F;
            }
            double dx = moveTargetPoint.x - getX();
            double dy = moveTargetPoint.y - getY();
            double dz = moveTargetPoint.z - getZ();
            double flat = Math.sqrt(dx * dx + dz * dz);
            if (Math.abs(flat) <= 1.0E-5F) {
                return;
            }
            double ratio = 1.0 - Math.abs(dy * 0.7F) / flat;
            dx *= ratio;
            dz *= ratio;
            flat = Math.sqrt(dx * dx + dz * dz);
            double total = Math.sqrt(dx * dx + dz * dz + dy * dy);
            float oldYaw = getYRot();
            float bearing = (float) Math.atan2(dz, dx);
            float current = Mth.wrapDegrees(getYRot() + 90.0F);
            float wanted = Mth.wrapDegrees(bearing * (180.0F / (float) Math.PI));
            setYRot(Mth.approachDegrees(current, wanted, 4.0F) - 90.0F);
            yBodyRot = getYRot();
            if (Mth.degreesDifferenceAbs(oldYaw, getYRot()) < 3.0F) {
                speed = Mth.approach(speed, 1.8F, 0.005F * (1.8F / speed));
            } else {
                speed = Mth.approach(speed, 0.2F, 0.025F);
            }
            float pitch = (float) -(Math.atan2(-dy, flat) * (180.0F / (float) Math.PI));
            setXRot(pitch);
            float yawRad = (getYRot() + 90.0F) * ((float) Math.PI / 180.0F);
            double xa = speed * Math.cos(yawRad) * Math.abs(dx / total);
            double za = speed * Math.sin(yawRad) * Math.abs(dz / total);
            double ya = speed * Math.sin(pitch * ((float) Math.PI / 180.0F)) * Math.abs(dy / total);
            Vec3 movement = getDeltaMovement();
            setDeltaMovement(movement.add(new Vec3(xa, ya, za).subtract(movement).scale(0.2)));
        }
    }
    class PhantomBodyRotationControl extends BodyRotationControl {
        public PhantomBodyRotationControl(Mob mob) {
            super(Phantom.this);
        }
        public void clientTick() {
            yHeadRot = yBodyRot;
            yBodyRot = getYRot();
        }
    }
    static class PhantomLookControl extends LookControl {
        public PhantomLookControl(Mob mob) {
            super(mob);
        }
        public void tick() {
        }
    }
    abstract class PhantomMoveTargetGoal extends Goal {
        public PhantomMoveTargetGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        protected boolean touchingTarget() {
            return moveTargetPoint.distanceToSqr(getX(), getY(), getZ()) < 4.0;
        }
    }
    class PhantomCircleAroundAnchorGoal extends PhantomMoveTargetGoal {
        private float angle;
        private float distance;
        private float height;
        private float clockwise;
        private PhantomCircleAroundAnchorGoal() {
        }
        public boolean canUse() {
            return getTarget() == null || attackPhase == AttackPhase.CIRCLE;
        }
        public void start() {
            distance = 5.0F + random.nextFloat() * 10.0F;
            height = -4.0F + random.nextFloat() * 9.0F;
            clockwise = random.nextBoolean() ? 1.0F : -1.0F;
            selectNext();
        }
        public void tick() {
            if (random.nextInt(adjustedTickDelay(350)) == 0) {
                height = -4.0F + random.nextFloat() * 9.0F;
            }
            if (random.nextInt(adjustedTickDelay(250)) == 0) {
                distance++;
                if (distance > 15.0F) {
                    distance = 5.0F;
                    clockwise = -clockwise;
                }
            }
            if (random.nextInt(adjustedTickDelay(450)) == 0) {
                angle = random.nextFloat() * 2.0F * (float) Math.PI;
                selectNext();
            }
            if (touchingTarget()) {
                selectNext();
            }
            if (moveTargetPoint.y < getY() && !level().isEmptyBlock(blockPosition().below())) {
                height = Math.max(1.0F, height);
                selectNext();
            }
            if (moveTargetPoint.y > getY() && !level().isEmptyBlock(blockPosition().above())) {
                height = Math.min(-1.0F, height);
                selectNext();
            }
        }
        private void selectNext() {
            if (BlockPos.ZERO.equals(anchorPoint)) {
                anchorPoint = blockPosition();
            }
            angle += clockwise * 15.0F * ((float) Math.PI / 180.0F);
            moveTargetPoint = Vec3.atLowerCornerOf(anchorPoint).add(distance * Math.cos(angle), -4.0F + height, distance * Math.sin(angle));
        }
    }
    class PhantomSweepAttackGoal extends PhantomMoveTargetGoal {
        private PhantomSweepAttackGoal() {
        }
        public boolean canUse() {
            return getTarget() != null && attackPhase == AttackPhase.SWOOP;
        }
        public boolean canContinueToUse() {
            LivingEntity target = getTarget();
            if (target == null || !target.isAlive()) {
                return false;
            }
            if (target instanceof Player player && (player.isSpectator() || player.isCreative())) {
                return false;
            }
            if (!canUse()) {
                return false;
            }
            if (tickCount % 20 == 0) {
                return level().getEntitiesOfClass(Cat.class, getBoundingBox().inflate(16.0), cat -> true).isEmpty();
            }
            return true;
        }
        public void stop() {
            setTarget(null);
            attackPhase = AttackPhase.CIRCLE;
        }
        public void tick() {
            LivingEntity target = getTarget();
            if (target == null) {
                return;
            }
            moveTargetPoint = new Vec3(target.getX(), target.getY(0.5), target.getZ());
            if (getBoundingBox().inflate(0.2F).intersects(target.getBoundingBox())) {
                doHurtTarget(null, target);
                attackPhase = AttackPhase.CIRCLE;
            } else if (horizontalCollision || hurtTime > 0) {
                attackPhase = AttackPhase.CIRCLE;
            }
        }
    }
    class PhantomAttackStrategyGoal extends Goal {
        private int nextSweepTick;
        private PhantomAttackStrategyGoal() {
        }
        public boolean canUse() {
            LivingEntity target = getTarget();
            return target != null && canAttack(target);
        }
        public void start() {
            nextSweepTick = adjustedTickDelay(10);
            attackPhase = AttackPhase.CIRCLE;
            anchorAboveTarget();
        }
        public void stop() {
            int top = live().getWorld().getHighestBlockYAt(anchorPoint.getX(), anchorPoint.getZ());
            anchorPoint = new BlockPos(anchorPoint.getX(), top + 10 + random.nextInt(20), anchorPoint.getZ());
        }
        public void tick() {
            if (attackPhase == AttackPhase.CIRCLE && --nextSweepTick <= 0 && getTarget() != null) {
                attackPhase = AttackPhase.SWOOP;
                anchorAboveTarget();
                nextSweepTick = adjustedTickDelay((8 + random.nextInt(4)) * 20);
            }
        }
    }
    class PhantomAttackPlayerTargetGoal extends Goal {
        private int nextScanTick = reducedTickDelay(20);
        private PhantomAttackPlayerTargetGoal() {
        }
        public boolean canUse() {
            if (nextScanTick > 0) {
                nextScanTick--;
                return false;
            }
            nextScanTick = reducedTickDelay(60);
            Player best = null;
            for (org.bukkit.entity.Player candidate : live().getWorld().getNearbyPlayers(live().getLocation(), 64.0)) {
                Player player = (Player) Entity.fromBukkit(candidate);
                if (!player.isSpectator() && !player.isCreative() && canAttack(player)
                    && (best == null || player.getY() > best.getY())) {
                    best = player;
                }
            }
            if (best == null) {
                return false;
            }
            setTarget(best);
            return true;
        }
        public boolean canContinueToUse() {
            LivingEntity target = getTarget();
            return target != null && canAttack(target);
        }
    }
}
