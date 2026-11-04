// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.zombie;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.MoveToBlockGoal;
import net.minecraft.world.entity.ai.goal.RangedAttackGoal;
import net.minecraft.world.entity.ai.goal.ZombieAttackGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.phys.Vec3;
public class Drowned extends Zombie implements RangedAttackMob {
    public static final float NAUTILUS_SHELL_CHANCE = 0.03F;
    public Drowned(EntityType<? extends Drowned> type, Level level) {
        super(type, level);
        this.moveControl = new DrownedMoveControl<>(this);
    }
    private boolean searchingForLand;
    public static AttributeSupplier.Builder createAttributes() {
        return Zombie.createAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void addBehaviourGoals() {
        }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkDrownedSpawnRules(EntityType<Drowned> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.drowned.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.drowned.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.drowned.death");
    }
    protected SoundEvent getStepSound() {
        return net.minecraft.sounds.SoundEvents.DROWNED_STEP;
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected boolean canSpawnInLiquids() {
        return false;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected boolean canReplaceCurrentItem(ItemStack newItemStack, ItemStack currentItemStack, EquipmentSlot slot) {
        return false;
    }
    protected boolean convertsInWater() {
        return false;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public boolean okTarget(LivingEntity target) {
        if (target == null) {
            return false;
        }
        return !level().isBrightOutside() || target.isInWater();
    }
    public boolean wantsToSwim() {
        LivingEntity target = getTarget();
        return searchingForLand ? false : target != null && target.isInWater() ? isInWater() : isInWater() && !level().isBrightOutside();
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    public void updateSwimming() {
    }
    protected boolean closeToNextPos() {
        return false;
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public float rangedAttackUncertainty(Level level) {
        return 14 - (live().getWorld().getDifficulty().ordinal()) * 4;
    }
    public void setRangedAttackUncertainty(float rangedAttackUncertainty) {

    }
    public void setSearchingForLand(boolean searchingForLand) {
        this.searchingForLand = searchingForLand;
    }
    public boolean isSearchingForLand() {
        return searchingForLand;
    }
    public void rideTick() {
    }
    static class DrownedTridentAttackGoal extends RangedAttackGoal {
        private final Drowned drowned;
        public DrownedTridentAttackGoal(RangedAttackMob mob, double speedModifier, int attackInterval, float attackRadius) {
            super(mob, speedModifier, attackInterval, attackRadius);
            this.drowned = (Drowned) mob;
        }
        public boolean canUse() {
            return super.canUse() && drowned.getMainHandItem().is(net.minecraft.world.item.Items.TRIDENT);
        }
        public void start() {
            super.start();
            drowned.setAggressive(true);
            drowned.startUsingItem(net.minecraft.world.InteractionHand.MAIN_HAND);
        }
        public void stop() {
            super.stop();
            drowned.stopUsingItem();
            drowned.setAggressive(false);
        }
    }
    static class DrownedSwimUpGoal extends Goal {
        private final Drowned drowned;
        private final double speedModifier;
        private final int seaLevel;
        private boolean stuck;
        public DrownedSwimUpGoal(Drowned drowned, double speedModifier, int seaLevel) {
            this.drowned = drowned;
            this.speedModifier = speedModifier;
            this.seaLevel = seaLevel;
        }
        public boolean canUse() {
            return !drowned.level().isBrightOutside() && drowned.isInWater() && drowned.getY() < seaLevel - 2;
        }
        public boolean canContinueToUse() {
            return canUse() && !stuck;
        }
        public void tick() {
            if (drowned.getY() < seaLevel - 1 && drowned.getNavigation().isDone()) {
                Vec3 pos = DefaultRandomPos.getPosTowards(drowned, 4, 8, new Vec3(drowned.getX(), seaLevel - 1, drowned.getZ()), Math.PI / 2);
                if (pos == null) {
                    stuck = true;
                    return;
                }
                drowned.getNavigation().moveTo(pos.x, pos.y, pos.z, speedModifier);
            }
        }
        public void start() {
            drowned.setSearchingForLand(true);
            stuck = false;
        }
        public void stop() {
            drowned.setSearchingForLand(false);
        }
    }
    static class DrownedGoToBeachGoal extends MoveToBlockGoal {
        private final Drowned drowned;
        public DrownedGoToBeachGoal(Drowned drowned, double speedModifier) {
            super(drowned, speedModifier, 8, 2);
            this.drowned = drowned;
        }
        public boolean canUse() {
            return super.canUse() && !drowned.level().isBrightOutside() && drowned.isInWater() && drowned.getY() >= ((org.bukkit.entity.Entity) drowned.getBukkitEntity()).getWorld().getSeaLevel() - 3;
        }
        public boolean canContinueToUse() {
            return super.canContinueToUse();
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            return level.isEmptyBlock(pos.above()) && level.isEmptyBlock(pos.above(2)) && level.getBlockState(pos).isSolid();
        }
        public void start() {
            drowned.setSearchingForLand(false);
            super.start();
        }
        public void stop() {
            super.stop();
        }
    }
    static class DrownedGoToWaterGoal extends Goal {
        private final PathfinderMob mob;
        private final double speedModifier;
        private BlockPos water;
        public DrownedGoToWaterGoal(PathfinderMob mob, double speedModifier) {
            this.mob = mob;
            this.speedModifier = speedModifier;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            if (!mob.level().isBrightOutside() || mob.isInWater()) {
                return false;
            }
            BlockPos origin = mob.blockPosition();
            for (int dx = -5; dx <= 5; dx++) {
                for (int dz = -5; dz <= 5; dz++) {
                    for (int dy = -2; dy <= 2; dy++) {
                        BlockPos pos = origin.offset(dx, dy, dz);
                        if (mob.level().getBlockState(pos).is(Blocks.WATER)) {
                            water = pos;
                            return true;
                        }
                    }
                }
            }
            return false;
        }
        public boolean canContinueToUse() {
            return !mob.getNavigation().isDone();
        }
        public void start() {
            if (water != null) {
                mob.getNavigation().moveTo(water.getX() + 0.5, water.getY(), water.getZ() + 0.5, speedModifier);
            }
        }
    }
    static class DrownedAttackGoal extends ZombieAttackGoal {
        public DrownedAttackGoal(Drowned drowned, double speedModifier, boolean trackTarget) {
            super(drowned, speedModifier, trackTarget);
        }
    }
    static class DrownedMoveControl<T extends Drowned> extends MoveControl<T> {
        public DrownedMoveControl(T drowned) {
            super(drowned);
        }
        public void tick() {
            LivingEntity target = mob.getTarget();
            if (mob.wantsToSwim() && mob.isInWater()) {
                if (target != null && target.getY() > mob.getY() || mob.isSearchingForLand()) {
                    mob.setDeltaMovement(mob.getDeltaMovement().add(0.0, 0.002, 0.0));
                }
                if (operation != Operation.MOVE_TO || mob.getNavigation().isDone()) {
                    mob.setSpeed(0.0F);
                    return;
                }
                Vec3 delta = new Vec3(wantedX - mob.getX(), wantedY - mob.getY(), wantedZ - mob.getZ());
                double distance = delta.length();
                if (distance < 1.0E-5) {
                    return;
                }
                float speed = (float) (speedModifier * mob.getAttributeValue(Attributes.MOVEMENT_SPEED));
                mob.setSpeed(net.minecraft.util.Mth.lerp(0.125F, mob.getSpeed(), speed));
                mob.setDeltaMovement(mob.getDeltaMovement().add(delta.scale(mob.getSpeed() * 0.005 / distance)));
            } else {
                if (!mob.onGround()) {
                    mob.setDeltaMovement(mob.getDeltaMovement().add(0.0, -0.008, 0.0));
                }
                super.tick();
            }
        }
    }
}
