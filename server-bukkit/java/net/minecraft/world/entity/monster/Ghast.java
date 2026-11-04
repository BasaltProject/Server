// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attributes;
import java.util.function.BooleanSupplier;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Ghast extends Mob implements Enemy {
    public Ghast(EntityType<? extends Ghast> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {

    }
    public boolean isCharging() {
        return live() instanceof org.bukkit.entity.Ghast api ? (boolean) api.isCharging() : false;
    }
    public void setCharging(boolean onOff) {
        if (live() instanceof org.bukkit.entity.Ghast api) { api.setCharging(onOff); }
    }
    public int getExplosionPower() {
        return live() instanceof org.bukkit.entity.Ghast api ? (int) api.getExplosionPower() : 0;
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return super.isInvulnerableTo(level, source);
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public void travel(Vec3 input) {
        move(net.minecraft.world.entity.MoverType.SELF, getDeltaMovement());
        setDeltaMovement(getDeltaMovement().scale(0.91));
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 100.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.CAMERA_DISTANCE, 8.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FLYING_SPEED, 0.06);
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ghast.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.ghast.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ghast.death");
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Ghast api ? (float) api.getSoundVolume() : 0;
    }
    public static boolean checkGhastSpawnRules(EntityType<Ghast> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public double leashElasticDistance() {
        return 10.0;
    }
    public double leashSnapDistance() {
        return 16.0;
    }
    public static void faceMovementDirection(Mob ghast) {
        if (ghast.getTarget() == null) {
            Vec3 motion = ghast.getDeltaMovement();
            ghast.setYRot(-((float) Math.atan2(motion.x, motion.z)) * (180.0F / (float) Math.PI));
            ghast.yBodyRot = ghast.getYRot();
        } else {
            LivingEntity target = ghast.getTarget();
            if (target.distanceToSqr(ghast) < 4096.0) {
                ghast.setYRot(-((float) Math.atan2(target.getX() - ghast.getX(), target.getZ() - ghast.getZ())) * (180.0F / (float) Math.PI));
                ghast.yBodyRot = ghast.getYRot();
            }
        }
    }
    public static class GhastMoveControl<T extends Mob> extends MoveControl<T> {
        private final boolean careful;
        private final java.util.function.BooleanSupplier shouldBeStopped;
        private int floatDuration;
        public GhastMoveControl(T ghast, boolean careful, java.util.function.BooleanSupplier shouldBeStopped) {
            super(ghast);
            this.careful = careful;
            this.shouldBeStopped = shouldBeStopped;
        }
        public void tick() {
            if (shouldBeStopped.getAsBoolean()) {
                operation = Operation.WAIT;
                mob.setDeltaMovement(mob.getDeltaMovement().scale(0.5));
                return;
            }
            if (operation != Operation.MOVE_TO || floatDuration-- > 0) {
                return;
            }
            floatDuration += mob.getRandom().nextInt(5) + 2;
            Vec3 direction = new Vec3(wantedX - mob.getX(), wantedY - mob.getY(), wantedZ - mob.getZ());
            if (canReach(direction)) {
                mob.setDeltaMovement(mob.getDeltaMovement().add(direction.normalize().scale(mob.getAttributeValue(Attributes.FLYING_SPEED) * 5.0 / 3.0)));
            } else {
                operation = Operation.WAIT;
            }
        }
        private boolean canReach(Vec3 direction) {
            Vec3 step = direction.normalize();
            int steps = (int) Math.ceil(direction.length());
            for (int i = 1; i < steps; i++) {
                BlockPos pos = BlockPos.containing(mob.getX() + step.x * i, mob.getY() + step.y * i, mob.getZ() + step.z * i);
                if (mob.level().getBlockState(pos).isSolid() && (!careful || i < 8)) {
                    return false;
                }
            }
            return true;
        }
    }
    public static class RandomFloatAroundGoal extends Goal {
        private final Mob ghast;
        private final int distanceToBlocks;
        public RandomFloatAroundGoal(Mob ghast) {
            this(ghast, 0);
        }
        public RandomFloatAroundGoal(Mob ghast, int distanceToBlocks) {
            this.ghast = ghast;
            this.distanceToBlocks = distanceToBlocks;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            MoveControl control = ghast.getMoveControl();
            if (!control.hasWanted()) {
                return true;
            }
            double dx = control.getWantedX() - ghast.getX();
            double dy = control.getWantedY() - ghast.getY();
            double dz = control.getWantedZ() - ghast.getZ();
            double distance = dx * dx + dy * dy + dz * dz;
            return distance < 1.0 || distance > 3600.0;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public static Vec3 getSuitableFlyToPosition(Mob mob, int distanceToBlocks) {
            RandomSource random = mob.getRandom();
            for (int i = 0; i < 64; i++) {
                double x = mob.getX() + (random.nextFloat() * 2.0F - 1.0F) * 16.0F;
                double y = mob.getY() + (random.nextFloat() * 2.0F - 1.0F) * 16.0F;
                double z = mob.getZ() + (random.nextFloat() * 2.0F - 1.0F) * 16.0F;
                BlockPos pos = BlockPos.containing(x, y, z);
                boolean clear = true;
                for (int below = 0; below <= distanceToBlocks && clear; below++) {
                    clear = !mob.level().getBlockState(pos.below(below)).isSolid();
                }
                if (clear) {
                    return new Vec3(x, y, z);
                }
            }
            return null;
        }
        public void start() {
            Vec3 pos = getSuitableFlyToPosition(ghast, distanceToBlocks);
            if (pos != null) {
                ghast.getMoveControl().setWantedPosition(pos.x, pos.y, pos.z, 1.0);
            }
        }
    }
    public static class GhastLookGoal extends Goal {
        private final Mob ghast;
        public GhastLookGoal(Mob ghast) {
            this.ghast = ghast;
            setFlags(java.util.EnumSet.of(Goal.Flag.LOOK));
        }
        public boolean canUse() {
            return true;
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            faceMovementDirection(ghast);
        }
    }
    static class GhastShootFireballGoal extends Goal {
        public int chargeTime;
        private final Ghast ghast;
        public GhastShootFireballGoal(Ghast ghast) {
            this.ghast = ghast;
        }
        public boolean canUse() {
            return ghast.getTarget() != null;
        }
        public void start() {
            chargeTime = 0;
        }
        public void stop() {
            ghast.setCharging(false);
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            LivingEntity target = ghast.getTarget();
            if (target == null) {
                return;
            }
            if (target.distanceToSqr(ghast) < 4096.0 && ghast.hasLineOfSight(target)) {
                chargeTime++;
                if (chargeTime == 20) {
                    Vec3 view = ghast.getViewVector(1.0F);
                    org.bukkit.entity.Ghast api = (org.bukkit.entity.Ghast) ghast.live();
                    Vec3 aim = new Vec3(target.getX() - (ghast.getX() + view.x * 4.0), target.getY(0.5) - (0.5 + ghast.getY(0.5)),
                        target.getZ() - (ghast.getZ() + view.z * 4.0));
                    org.bukkit.Location where = api.getLocation().add(view.x * 4.0, api.getHeight() / 2.0 + 0.5, view.z * 4.0);
                    int power = ghast.getExplosionPower();
                    api.getWorld().spawn(where, org.bukkit.entity.LargeFireball.class, fireball -> {
                        fireball.setShooter(api);
                        fireball.setDirection(new org.bukkit.util.Vector(aim.x, aim.y, aim.z));
                        fireball.setYield(power);
                    });
                    chargeTime = -40;
                }
            } else if (chargeTime > 0) {
                chargeTime--;
            }
            ghast.setCharging(chargeTime > 10);
        }
    }
}
